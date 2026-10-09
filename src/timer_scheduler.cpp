// ╔══════════════════════════════════════════════════════════════════╗
// ║  timer_scheduler.cpp — Реализация таймеров полива v6.0          ║
// ║                                                                  ║
// ║  v6.0 ИЗМЕНЕНИЯ:                                                ║
// ║   • Все времена через Unix timestamp (time_t), не "секунды от   ║
// ║     полуночи" — корректно при ребутах между сутками.            ║
// ║   • Восстановление активного полива после ребута: если          ║
// ║     остаток durationMin > 0 — реле включается, доигрывает       ║
// ║     остаток.                                                     ║
// ║   • Поддержка интервалов > 24 часов (до 168 ч = 1 неделя).     ║
// ║   • Сохранение startUnix в NVS немедленно при каждом запуске   ║
// ║     (для надёжного восстановления).                             ║
// ║   • При переходе Manual→Auto: реле выключаются, пересчитывается║
// ║     состояние "должен ли быть активен сейчас".                  ║
// ╚══════════════════════════════════════════════════════════════════╝

#include "timer_scheduler.h"
#include "settings.h"
#include "relay_manager.h"
#include "ws_handler.h"
#include "network_manager.h"
#include "config.h"
#include <Arduino.h>
#include <time.h>
#include <atomic>

// ═══════════════════════════════════════════════════════════════════
//  ВНУТРЕННЕЕ СОСТОЯНИЕ
// ═══════════════════════════════════════════════════════════════════
enum TimerState : uint8_t {
    TSTATE_IDLE   = 0,
    TSTATE_ACTIVE = 1
};

struct TimerRuntime {
    TimerState state;
    time_t     startUnix;       // Unix timestamp начала полива (0 = неактивен)
    uint64_t   startMs;         // steadyMs() для расчёта оставшегося в активном
    time_t     lastStartUnix;   // Когда был последний запуск (для расчёта следующего)
};

static TimerRuntime s_rt[NUM_IRRIGATION];
static bool s_timeAvailable = false;
static uint64_t s_lastDbgMs = 0;

// Запросы из чужих задач (веб-сокет, MQTT). s_rt[] и реле полива меняет
// только taskCore1: раньше onModeChanged() из AsyncTCP сбрасывал s_rt[]
// параллельно со startTimer() в update(), и при неудачном чередовании реле
// оставалось включённым при таймере в IDLE — клапан открыт до следующего окна.
static std::atomic<bool>    s_modeAutoReq{false};
static std::atomic<uint8_t> s_forceOffReq{0};   // битовая маска таймеров

// ═══════════════════════════════════════════════════════════════════
//  УТИЛИТЫ ВРЕМЕНИ
// ═══════════════════════════════════════════════════════════════════
// Текущий Unix timestamp (или 0 если время не установлено)
static time_t nowUnix() {
    if (!NetMgr::isTimeSet()) return 0;
    time_t t;
    time(&t);
    return t;
}

// v6.1: удалена функция secOfDay() — не вызывалась ниоткуда. Осталась от
// версии 5, где расписание считалось в секундах от полуночи; в v6.0 расчёт
// перевели на Unix-время, и она стала не нужна.

// Полночь "сегодня" в локальном TZ как Unix timestamp
static time_t midnightToday() {
    time_t t = nowUnix();
    if (t == 0) return 0;
    struct tm tm_local;
    localtime_r(&t, &tm_local);
    tm_local.tm_hour = 0;
    tm_local.tm_min  = 0;
    tm_local.tm_sec  = 0;
    return mktime(&tm_local);
}

// Индекс реле для таймера
static uint8_t relayIdxForTimer(uint8_t idx) {
    return RELAY_IDX_IRR1 + idx;
}

// ═══════════════════════════════════════════════════════════════════
//  СЕТКА ЗАПУСКОВ
// ═══════════════════════════════════════════════════════════════════
// v6.3: раньше опорной точкой служила «сегодняшняя полночь + startMin»,
// то есть она пересчитывалась каждые сутки. Для интервалов длиннее суток
// и не делящих 24 часа это ломало расписание: 48- и 72-часовой полив
// срабатывал ежедневно, а пятичасовой «сбивался» в полночь (21:00 → 01:00).
//
// Теперь:
//   • интервал 0 или 24 ч — как и прежде, по местным часам: startMin
//     каждого дня (верно и при переходе на летнее время);
//   • любой другой — сетка anchor + n*интервал от ПЕРВОГО запуска. Опорная
//     точка (timerAnchorGet) создаётся при первом расчёте — ближайший
//     startMin, чьё окно ещё не закрыто — и хранится в NVS, поэтому
//     переживает перезагрузки. Правка времени старта или интервала
//     в панели сбрасывает её (см. ws_handler).
static int32_t periodSecFor(const TimerCfg& cfg) {
    return (cfg.intervalHours == 0) ? 86400 : (int32_t)cfg.intervalHours * 3600;
}

// Опорная точка сетки для интервалов, отличных от суток. persist=false —
// только посмотреть (вызывается из обработчика веб-сокета, писать во флеш
// оттуда незачем: запишет следующий проход update()).
static time_t scheduleAnchor(uint8_t idx, const TimerCfg& cfg, time_t now, bool persist) {
    time_t stored = (time_t)timerAnchorGet(idx);
    if (stored != 0) return stored;

    time_t midnight = midnightToday();
    if (midnight == 0) return 0;

    time_t  base   = midnight + (int32_t)cfg.startMin * 60;
    int32_t durSec = (int32_t)cfg.durationMin * 60;
    if ((int32_t)(now - base) >= durSec) base += 86400;   // сегодняшнее окно прошло
    if (persist) timerAnchorSet(idx, (int64_t)base);
    return base;
}

// Ближайшее будущее срабатывание (Unix) или 0, если таймер не задан.
static time_t computeNextFire(uint8_t idx, const TimerCfg& cfg, time_t fromUnix, bool persist) {
    if (cfg.durationMin == 0) return 0;
    if (fromUnix == 0)        return 0;

    const int32_t period = periodSecFor(cfg);

    if (period == 86400) {
        time_t midnight = midnightToday();
        if (midnight == 0) return 0;
        time_t base = midnight + (int32_t)cfg.startMin * 60;
        if (base > fromUnix) return base;
        int64_t n = ((int64_t)(fromUnix - base) / period) + 1;
        return base + (time_t)(n * period);
    }

    time_t anchor = scheduleAnchor(idx, cfg, fromUnix, persist);
    if (anchor == 0)       return 0;
    if (anchor > fromUnix) return anchor;
    int64_t n = ((int64_t)(fromUnix - anchor) / period) + 1;
    return anchor + (time_t)(n * period);
}

// Момент последнего по сетке запуска (Unix) или 0, если сетка ещё не
// началась. Нужен для проверки «полив должен идти прямо сейчас»:
// lastFire <= now < lastFire + duration.
static time_t computeLastFire(uint8_t idx, const TimerCfg& cfg, time_t fromUnix, bool persist) {
    if (cfg.durationMin == 0) return 0;
    if (fromUnix == 0)        return 0;

    const int32_t period = periodSecFor(cfg);

    if (period == 86400) {
        time_t midnight = midnightToday();
        if (midnight == 0) return 0;
        time_t base = midnight + (int32_t)cfg.startMin * 60;
        if (base > fromUnix) return base - period;   // сегодняшний старт ещё впереди — последний был вчера
        int64_t n = (int64_t)(fromUnix - base) / period;
        return base + (time_t)(n * period);
    }

    time_t anchor = scheduleAnchor(idx, cfg, fromUnix, persist);
    if (anchor == 0 || anchor > fromUnix) return 0;
    int64_t n = (int64_t)(fromUnix - anchor) / period;
    return anchor + (time_t)(n * period);
}

// ═══════════════════════════════════════════════════════════════════
//  СТАРТ / СТОП ТАЙМЕРА
// ═══════════════════════════════════════════════════════════════════
// Не раньше какого момента (steadyMs) пробовать запуск снова после отказа реле.
static uint64_t s_nextStartTryMs[NUM_IRRIGATION] = {};

static void startTimer(uint8_t idx, time_t startUnix, uint64_t nowMs) {
    // v6.3: реле включаем ПЕРВЫМ и смотрим на результат. Раньше таймер
    // объявлялся активным, даже если setRelay() отказал (антидребезг,
    // занятый мьютекс): полив пропускался молча, и повторной попытки не было.
    // Теперь при отказе состояние не меняется, и update() попробует снова
    // на следующем тике — окно запуска ещё открыто.
    uint8_t relay = relayIdxForTimer(idx);
    if (!RelayMgr::setRelay(relay, true) && !RelayMgr::getState(relay)) {
        // Повтор не чаще раза в секунду (период антидребезга реле): иначе
        // update() дёргал бы setRelay каждые 50 мс, и каждый отказ печатал
        // бы строку в Serial под мьютексом настроек.
        s_nextStartTryMs[idx] = nowMs + RELAY_MIN_TOGGLE_INTERVAL_MS;
        Serial.printf("[TIMER%u] Start deferred: relay not switched, retry in %u ms\n",
            (unsigned)idx, (unsigned)RELAY_MIN_TOGGLE_INTERVAL_MS);
        return;
    }

    s_rt[idx].state         = TSTATE_ACTIVE;
    s_rt[idx].startUnix     = startUnix;
    s_rt[idx].startMs       = nowMs;
    s_rt[idx].lastStartUnix = startUnix;

    // Сохраняем в NVS немедленно (для восстановления после ребута)
    // v6.2: settingsSave() пишет во flash и делает это ВНЕ мьютекса.
    // Раньше вызов стоял внутри блока с g_settingsMutex, а RelayMgr::
    // setRelayDirect() ждёт тот же мьютекс всего 20 мс. Запись в NVS
    // легко превышает этот срок, и тогда учёт состояния реле молча
    // терялся: GPIO переключён, а g_settings.relayState[] — нет, и
    // панель с MQTT показывали неправду. Мьютекс нужен только для
    // изменения поля; сама запись в нём не нуждается.
    bool needSave = false;
    {
        MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(50));
        if (lock.locked()) {
            g_settings.timerActiveStartSec[idx] = (int32_t)startUnix;
            needSave = true;
        }
    }
    if (needSave) settingsSave();   // НЕ deferred — нужно прямо сейчас

    Serial.printf("[TIMER%u] STARTED at unix=%ld\n", (unsigned)idx, (long)startUnix);
    WsHandler::notifyRelayChange(relay);
}

static void stopTimer(uint8_t idx) {
    if (s_rt[idx].state != TSTATE_ACTIVE) return;

    s_rt[idx].state     = TSTATE_IDLE;
    s_rt[idx].startUnix = 0;
    s_rt[idx].startMs   = 0;

    bool needSave = false;
    {
        MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(50));
        if (lock.locked()) {
            g_settings.timerActiveStartSec[idx] = -1;
            needSave = true;
        }
    }
    if (needSave) settingsSave();   // см. комментарий в startTimer()

    // v6.3: forceOff(), а не setRelay(false). setRelay() отказывает по
    // антидребезгу и при занятом мьютексе, а состояние таймера здесь уже
    // IDLE — повторить закрытие клапана было бы некому.
    uint8_t relay = relayIdxForTimer(idx);
    RelayMgr::forceOff(relay);

    Serial.printf("[TIMER%u] STOPPED\n", (unsigned)idx);
    WsHandler::notifyRelayChange(relay);
}

// ═══════════════════════════════════════════════════════════════════
//  ВОССТАНОВЛЕНИЕ ПОСЛЕ РЕБУТА
// ═══════════════════════════════════════════════════════════════════
// Сценарий: ESP32 ребутнулся посреди полива.
//   В NVS лежит timerActiveStartSec[idx] = unixStart.
//   Если unixStart + durationMin*60 > nowUnix() — полив ещё должен идти.
//   Иначе — он уже завершился (даже если в NVS ещё активен флаг).
static void tryRestore(uint8_t idx, uint64_t nowMs, const TimerCfg& cfg) {
    int32_t saved = g_settings.timerActiveStartSec[idx];
    if (saved < 0) return;

    // v6.0: проверка что значение это Unix timestamp (после 2023-11-14),
    // а не старый формат "секунд от полуночи" (< 86400 в v5.x)
    if (saved < 1700000000) {
        Serial.printf("[TIMER%u] Restore: legacy format detected, clearing\n", (unsigned)idx);
        MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(50));
        if (lock.locked()) {
            g_settings.timerActiveStartSec[idx] = -1;
            settingsSaveDeferred();
        }
        return;
    }

    time_t startUnix = (time_t)saved;
    time_t now       = nowUnix();
    if (now == 0) return;

    int32_t durSec = (int32_t)cfg.durationMin * 60;
    int32_t elapsed = (int32_t)(now - startUnix);

    if (elapsed < 0 || elapsed >= durSec) {
        // Уже должно было завершиться (или какая-то ошибка с временем)
        Serial.printf("[TIMER%u] Restore: timer already finished (%d sec elapsed, dur=%d)\n",
            (unsigned)idx, elapsed, durSec);
        MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(50));
        if (lock.locked()) {
            g_settings.timerActiveStartSec[idx] = -1;
            settingsSaveDeferred();
        }
        s_rt[idx].lastStartUnix = startUnix;
        return;
    }

    // Полив ещё должен идти — включаем реле, восстанавливаем состояние
    uint8_t relay = relayIdxForTimer(idx);
    if (!RelayMgr::setRelay(relay, true) && !RelayMgr::getState(relay)) {
        // Не включилось — не объявляем таймер активным; окно ещё открыто,
        // и update() запустит полив обычным путём на ближайшем тике.
        Serial.printf("[TIMER%u] Restore: relay not switched, update() will retry\n",
            (unsigned)idx);
        return;
    }
    s_rt[idx].state         = TSTATE_ACTIVE;
    s_rt[idx].startUnix     = startUnix;
    s_rt[idx].startMs       = nowMs - (uint64_t)elapsed * 1000ULL;
    s_rt[idx].lastStartUnix = startUnix;

    Serial.printf("[TIMER%u] RESTORED: %d sec elapsed, %d sec remaining\n",
        (unsigned)idx, elapsed, durSec - elapsed);
    WsHandler::notifyRelayChange(relay);
}

// ═══════════════════════════════════════════════════════════════════
//  ОСНОВНОЙ ЦИКЛ UPDATE
// ═══════════════════════════════════════════════════════════════════
static void applyModeAuto();
static void applyForceOff(uint8_t timerIdx);

void TimerSched::update(uint64_t nowMs) {
    // ── Запросы из чужих задач — до любых проверок режима ───────
    // Ручной режим ставится раньше, чем приходит forceOff, и update()
    // ниже вернётся рано; выключение при этом должно состояться.
    if (s_modeAutoReq.exchange(false)) applyModeAuto();
    const uint8_t offMask = s_forceOffReq.exchange(0);
    for (uint8_t i = 0; i < NUM_IRRIGATION; i++) {
        if (offMask & (1u << i)) applyForceOff(i);
    }

    // ── Авто-активация при NTP sync ─────────────────────────────
    if (!s_timeAvailable) {
        if (NetMgr::isTimeSet()) {
            Serial.println(F("[TIMER] Auto-activated via fallback (NetMgr::isTimeSet)"));
            onTimeAvailable();
        } else {
            return;
        }
    }

    // ── Проверка режима AUTO ────────────────────────────────────
    bool autoMode;
    {
        MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(10));
        if (!lock.locked()) return;
        autoMode = (g_settings.modeTimers != 0);
    }
    if (!autoMode) return;

    time_t now = nowUnix();
    if (now == 0) return;

    for (uint8_t i = 0; i < NUM_IRRIGATION; i++) {
        TimerCfg cfg;
        {
            MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(10));
            if (!lock.locked()) continue;
            cfg = g_settings.timer[i];
        }
        // Длительность 0 — таймер выключен. Но идущий полив всё равно обязан
        // пройти проверку завершения ниже (durSec = 0 → стоп сразу), иначе
        // обнуление длительности посреди полива оставляло клапан открытым
        // навсегда. Новый запуск при 0 невозможен: computeLastFire вернёт 0.

        if (s_rt[i].state == TSTATE_ACTIVE) {
            // Проверка на завершение
            int32_t durSec  = (int32_t)cfg.durationMin * 60;
            int32_t elapsed = (int32_t)(now - s_rt[i].startUnix);
            if (elapsed >= durSec) {
                stopTimer(i);
            }
        } else {
            // Проверка на запуск.
            // Логика: запускать если "сейчас попадает в активное окно",
            // т.е. computeLastFire + durationMin > now И мы это ещё не запускали.
            time_t lastFire = computeLastFire(i, cfg, now, true);
            if (lastFire == 0) continue;

            int32_t sinceFire = (int32_t)(now - lastFire);
            int32_t durSec    = (int32_t)cfg.durationMin * 60;

            if (sinceFire >= 0 && sinceFire < durSec) {
                // Сейчас попадаем в активное окно
                // Защита: не запускать тот же самый старт повторно
                if (s_rt[i].lastStartUnix != lastFire && nowMs >= s_nextStartTryMs[i]) {
                    Serial.printf("[TIMER%u] Window match: %d sec into duration %d\n",
                        (unsigned)i, sinceFire, durSec);
                    startTimer(i, lastFire, nowMs);
                }
            }
        }
    }

    // ── Лог состояния раз в минуту ──────────────────────────────
    if (nowMs - s_lastDbgMs >= 60000ULL) {
        s_lastDbgMs = nowMs;
        for (uint8_t i = 0; i < NUM_IRRIGATION; i++) {
            int32_t next = getNextStartSec(i);
            Serial.printf("[TIMER%u] state=%s next=%ds duration=%dmin interval=%dh\n",
                (unsigned)i,
                s_rt[i].state == TSTATE_ACTIVE ? "ACTIVE" : "IDLE",
                next,
                (int)g_settings.timer[i].durationMin,
                (int)g_settings.timer[i].intervalHours);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
//  COUNTDOWN — секунды до следующего срабатывания
// ═══════════════════════════════════════════════════════════════════
int32_t TimerSched::getNextStartSec(uint8_t timerIdx) {
    if (timerIdx >= NUM_IRRIGATION) return -1;
    if (!s_timeAvailable && !NetMgr::isTimeSet()) return -1;

    TimerCfg cfg;
    {
        MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(10));
        if (!lock.locked()) return -1;
        cfg = g_settings.timer[timerIdx];
    }
    if (cfg.durationMin == 0) return -1;

    time_t now = nowUnix();
    if (now == 0) return -1;

    time_t next = computeNextFire(timerIdx, cfg, now, false);
    if (next == 0) return -1;

    int64_t diff = (int64_t)(next - now);
    if (diff < 0)        return 0;
    if (diff > 0x7FFFFFFF) return -1;
    return (int32_t)diff;
}

// ═══════════════════════════════════════════════════════════════════
//  СОСТОЯНИЕ ДЛЯ WS
// ═══════════════════════════════════════════════════════════════════
bool TimerSched::isActive(uint8_t timerIdx) {
    if (timerIdx >= NUM_IRRIGATION) return false;
    return s_rt[timerIdx].state == TSTATE_ACTIVE;
}

uint32_t TimerSched::getRemainingSec(uint8_t timerIdx) {
    if (timerIdx >= NUM_IRRIGATION) return 0;
    if (s_rt[timerIdx].state != TSTATE_ACTIVE) return 0;

    uint16_t durMin;
    {
        MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(10));
        if (!lock.locked()) return 0;
        durMin = g_settings.timer[timerIdx].durationMin;
    }

    uint64_t elapsedMs = steadyMs() - s_rt[timerIdx].startMs;
    uint64_t durMs     = (uint64_t)durMin * 60ULL * 1000ULL;
    if (elapsedMs >= durMs) return 0;
    return (uint32_t)((durMs - elapsedMs) / 1000ULL);
}

// Вызывается из AsyncTCP и taskCore1 — только ставит запрос.
void TimerSched::forceOff(uint8_t timerIdx) {
    if (timerIdx >= NUM_IRRIGATION) return;
    s_forceOffReq.fetch_or((uint8_t)(1u << timerIdx));
}

static void applyForceOff(uint8_t timerIdx) {
    if (s_rt[timerIdx].state == TSTATE_ACTIVE) {
        stopTimer(timerIdx);
    } else {
        // Реле могло быть включено вручную — выключаем
        RelayMgr::setRelay(relayIdxForTimer(timerIdx), false);
    }
}

// ═══════════════════════════════════════════════════════════════════
//  ПЕРЕХОД MANUAL→AUTO — пересчёт состояния
// ═══════════════════════════════════════════════════════════════════
// Вызывается из ws_handler при изменении modeTimers.
// При переходе Manual→Auto:
//   1. Все реле полива выключаем (могли быть вручную включены).
//   2. Сбрасываем s_rt[*] в IDLE.
//   3. На следующем тике update() сам решит — если попадаем
//      в активное окно по расписанию, запустит.
// Вызывается из AsyncTCP и loopTask — только ставит запрос.
void TimerSched::onModeChanged(bool autoMode) {
    if (!autoMode) {
        // Manual: ничего не делаем (пользователь сам управляет)
        return;
    }
    s_modeAutoReq.store(true);
}

static void applyModeAuto() {
    Serial.println(F("[TIMER] Mode -> AUTO: resetting all relays, recalculating"));
    uint64_t nowMs = steadyMs();
    for (uint8_t i = 0; i < NUM_IRRIGATION; i++) {
        // Принудительно выключаем реле (v6.3: forceOff — антидребезг не
        // должен оставить вручную включённый клапан открытым)
        uint8_t relay = relayIdxForTimer(i);
        RelayMgr::forceOff(relay);
        WsHandler::notifyRelayChange(relay);

        // Сбрасываем runtime
        s_rt[i].state     = TSTATE_IDLE;
        s_rt[i].startUnix = 0;
        s_rt[i].startMs   = 0;
        // lastStartUnix НЕ трогаем — чтобы при пересчёте update()
        // не запустил тот же самый старт

        // Очищаем NVS флаг активного полива
        MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(20));
        if (lock.locked()) {
            g_settings.timerActiveStartSec[i] = -1;
        }
    }
    settingsSaveDeferred();

    // Принудительный лог-цикл, чтобы сразу было видно
    s_lastDbgMs = nowMs - 60001;
    (void)nowMs;
}

// ═══════════════════════════════════════════════════════════════════
//  ИНИЦИАЛИЗАЦИЯ
// ═══════════════════════════════════════════════════════════════════
void TimerSched::init() {
    for (uint8_t i = 0; i < NUM_IRRIGATION; i++) {
        s_rt[i].state         = TSTATE_IDLE;
        s_rt[i].startUnix     = 0;
        s_rt[i].startMs       = 0;
        s_rt[i].lastStartUnix = 0;
    }
    s_timeAvailable = false;
    Serial.println(F("[TIMER] Scheduler initialized (awaiting NTP sync)"));
}

void TimerSched::onTimeAvailable() {
    if (s_timeAvailable) return;
    s_timeAvailable = true;
    Serial.println(F("[TIMER] Time available — checking restore"));

    uint64_t nowMs = steadyMs();
    for (uint8_t i = 0; i < NUM_IRRIGATION; i++) {
        TimerCfg cfg;
        {
            MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(50));
            if (!lock.locked()) continue;
            cfg = g_settings.timer[i];
        }
        if (cfg.durationMin == 0) continue;
        tryRestore(i, nowMs, cfg);
    }
}
