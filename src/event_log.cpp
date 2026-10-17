// ╔══════════════════════════════════════════════════════════════════╗
// ║  event_log.cpp — Журнал событий v6.3 (см. event_log.h)          ║
// ╚══════════════════════════════════════════════════════════════════╝

#include "event_log.h"
#include "config.h"
#include "settings.h"
#include "relay_manager.h"
#include "vent_controller.h"
#include "water_pump_controller.h"
#include "sensor_reader.h"
#include "network_manager.h"
#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <atomic>
#include <string.h>
#include <time.h>

// ═══════════════════════════════════════════════════════════════════
//  ХРАНЕНИЕ
// ═══════════════════════════════════════════════════════════════════
#define EV_MAGIC        0x45564C47UL   // "EVLG"
#define EV_VERSION      2
#define EV_NVS_NS       "gevt"
#define EV_NVS_KEY      "ring"
#define EV_SAVE_MS      600000ULL      // не чаще раза в 10 минут
#define EV_OBSERVE_MS   1000ULL        // наблюдатель — раз в секунду
#define EV_SETTLE_MS    30000ULL       // раньше после запуска исходное состояние не снимаем
#define EV_SETTLE_MAX_MS 90000ULL      // ...и не ждём Wi-Fi дольше этого
#define EV_DEBOUNCE_MS  30000ULL       // датчики, дождь, Wi-Fi: изменение должно продержаться
// Створка «остановилась» — стоит на месте дольше паузы между ступенями авто-
// режима (STEP_DELAY_MIN = 50 с): цикл «0 → 20 → 40 → 60 %» пишется одним
// событием «0 → 60 %», а не тремя.
#define EV_VENT_STILL_MS 90000ULL
#define EV_AUTH_WIN_MS  60000ULL

// Два кольца: важные события (запуски, тревоги, отказы, обновления) не
// вытесняются рутиной (реле, ступени форточек, режимы).
// Блоб NVS: 12 + 80*10 = 812 байт. Запись не чаще раза в 10 минут —
// ~52 тыс. записей в год в худшем случае, что для NVS с кольцевым
// износом страниц — запас на десятилетия.
struct __attribute__((packed)) EvBlob {
    uint32_t magic;
    uint8_t  version;
    uint8_t  wImp, cImp, wRtn, cRtn;
    uint16_t seqNext;
    uint8_t  reserved;
    EvRecord imp[EVLOG_IMP];
    EvRecord rtn[EVLOG_RTN];
};

struct Ring {
    EvRecord* rec;
    uint8_t   size;
    uint8_t   w;          // куда писать следующую
    uint8_t   count;
    uint8_t   thisBoot;   // сколько последних записей — от этого запуска
};
static EvRecord s_impRec[EVLOG_IMP];
static EvRecord s_rtnRec[EVLOG_RTN];
static Ring     s_imp = { s_impRec, EVLOG_IMP, 0, 0, 0 };
static Ring     s_rtn = { s_rtnRec, EVLOG_RTN, 0, 0, 0 };
static uint16_t s_seq = 0;
static SemaphoreHandle_t s_mutex = nullptr;
static SemaphoreHandle_t s_saveMutex = nullptr;
static std::atomic<bool> s_dirty{false};
static uint64_t          s_lastSaveMs = 0;
static std::atomic<uint16_t> s_authFails{0};

static uint32_t stampNow() {
    if (NetMgr::isTimeSet()) {
        time_t now; time(&now);
        return (uint32_t)now;
    }
    return EVLOG_TS_UPTIME | (uint32_t)(steadyMs() / 1000ULL);
}

static bool isImportant(EvType t, uint8_t arg) {
    switch (t) {
        case EV_BOOT: case EV_PUMP: case EV_SENSOR: case EV_WIFI:
        case EV_AUTH_FAIL: case EV_OTA: case EV_REBOOT:
            return true;
        case EV_SETTINGS:
            return arg == EVS_FACTORY;
        default:
            return false;   // реле, форточки, режимы, дождь, обычные настройки
    }
}

static EvRecord& ringAt(Ring& r, uint8_t back) {   // back = 0 — последняя запись
    return r.rec[(r.w + r.size - 1 - back) % r.size];
}

void EventLog::add(EvType type, uint8_t arg, int16_t val) {
    if (!s_mutex || xSemaphoreTake(s_mutex, pdMS_TO_TICKS(20)) != pdTRUE) return;
    Ring& ring = isImportant(type, arg) ? s_imp : s_rtn;
    const uint32_t ts = stampNow();
    // Одно нажатие «Применить» шлёт несколько команд (три таймера полива) —
    // одинаковые записи о смене настроек в пределах 5 с склеиваем.
    if (type == EV_SETTINGS && ring.count > 0) {
        const EvRecord& last = ringAt(ring, 0);
        if (last.type == type && last.arg == arg
            && (last.ts & EVLOG_TS_UPTIME) == (ts & EVLOG_TS_UPTIME)
            && ts - last.ts <= 5) {
            xSemaphoreGive(s_mutex);
            return;
        }
    }
    EvRecord& r = ring.rec[ring.w];
    r.ts = ts;
    r.type = type;
    r.arg = arg;
    r.val = val;
    r.seq = s_seq++;
    ring.w = (ring.w + 1) % ring.size;
    if (ring.count < ring.size) ring.count++;
    if (ring.thisBoot < ring.size) ring.thisBoot++;
    xSemaphoreGive(s_mutex);
    s_dirty.store(true);
    Serial.printf("[EVT] type=%u arg=%u val=%d\n", (unsigned)type, (unsigned)arg, (int)val);
}

void EventLog::authFail() {
    s_authFails.fetch_add(1);
}

static bool loadFromNvs() {
    static EvBlob blob;
    Preferences prefs;
    if (!prefs.begin(EV_NVS_NS, true)) return false;   // namespace ещё не создан
    const bool ok = prefs.getBytesLength(EV_NVS_KEY) == sizeof(blob)
                 && prefs.getBytes(EV_NVS_KEY, &blob, sizeof(blob)) == sizeof(blob);
    prefs.end();
    if (!ok || blob.magic != EV_MAGIC || blob.version != EV_VERSION
        || blob.wImp >= EVLOG_IMP || blob.cImp > EVLOG_IMP
        || blob.wRtn >= EVLOG_RTN || blob.cRtn > EVLOG_RTN) return false;
    memcpy(s_impRec, blob.imp, sizeof(s_impRec));
    memcpy(s_rtnRec, blob.rtn, sizeof(s_rtnRec));
    s_imp.w = blob.wImp; s_imp.count = blob.cImp;
    s_rtn.w = blob.wRtn; s_rtn.count = blob.cRtn;
    s_seq = blob.seqNext;
    return true;
}

void EventLog::flush() {
    if (!s_mutex || !s_saveMutex) return;
    if (xSemaphoreTake(s_saveMutex, pdMS_TO_TICKS(1000)) != pdTRUE) return;
    static EvBlob blob;      // 812 байт — не на стеке; под s_saveMutex
    bool snap = false;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        blob.magic = EV_MAGIC;
        blob.version = EV_VERSION;
        blob.wImp = s_imp.w; blob.cImp = s_imp.count;
        blob.wRtn = s_rtn.w; blob.cRtn = s_rtn.count;
        blob.seqNext = s_seq;
        blob.reserved = 0;
        memcpy(blob.imp, s_impRec, sizeof(blob.imp));
        memcpy(blob.rtn, s_rtnRec, sizeof(blob.rtn));
        s_dirty.store(false);       // новое событие после снимка снова взведёт флаг
        xSemaphoreGive(s_mutex);
        snap = true;
    }
    if (snap) {
        Preferences prefs;
        if (prefs.begin(EV_NVS_NS, false)) {
            if (prefs.putBytes(EV_NVS_KEY, &blob, sizeof(blob)) != sizeof(blob)) {
                Serial.println(F("[EVT] NVS write FAILED"));
                s_dirty.store(true);
            }
            prefs.end();
        } else {
            s_dirty.store(true);
        }
        s_lastSaveMs = steadyMs();
    }
    xSemaphoreGive(s_saveMutex);
}

void EventLog::init() {
    if (!s_mutex) s_mutex = xSemaphoreCreateMutex();
    if (!s_saveMutex) s_saveMutex = xSemaphoreCreateMutex();
    configASSERT(s_mutex && s_saveMutex);
    if (loadFromNvs()) {
        Serial.printf("[EVT] Loaded %u important + %u routine events from NVS\n",
            (unsigned)s_imp.count, (unsigned)s_rtn.count);
    } else {
        memset(s_impRec, 0, sizeof(s_impRec));
        memset(s_rtnRec, 0, sizeof(s_rtnRec));
        s_imp.w = s_imp.count = s_rtn.w = s_rtn.count = 0;
        s_seq = 0;
    }
    s_imp.thisBoot = s_rtn.thisBoot = 0;
    add(EV_BOOT, (uint8_t)esp_reset_reason());
    // Запуск — то, что нужно увидеть даже после внезапного обесточивания
    // через минуту: сохраняем сразу.
    flush();
}

void EventLog::onTimeSync() {
    if (!s_mutex || xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
    time_t now; time(&now);
    const uint32_t upNow = (uint32_t)(steadyMs() / 1000ULL);
    bool changed = false;
    Ring* rings[2] = { &s_imp, &s_rtn };
    for (Ring* ring : rings) {
        for (uint8_t i = 0; i < ring->thisBoot; i++) {
            EvRecord& r = ringAt(*ring, i);
            if (r.ts & EVLOG_TS_UPTIME) {
                const uint32_t up = r.ts & ~EVLOG_TS_UPTIME;
                r.ts = (uint32_t)now - (upNow >= up ? upNow - up : 0);
                changed = true;
            }
        }
    }
    xSemaphoreGive(s_mutex);
    if (changed) s_dirty.store(true);
}

// Лента в хронологическом порядке: два кольца сливаются по порядковому
// номеру (сравнение с учётом переполнения 16 бит).
uint8_t EventLog::getAll(EvRecord* out, uint8_t maxCount) {
    if (!out || !s_mutex || xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return 0;
    uint8_t i = 0, j = 0, n = 0;
    const uint8_t ci = s_imp.count, cj = s_rtn.count;
    auto at = [](Ring& r, uint8_t k) -> const EvRecord& {   // k-я от самой старой
        const uint8_t start = (r.count < r.size) ? 0 : r.w;
        return r.rec[(start + k) % r.size];
    };
    while ((i < ci || j < cj) && n < maxCount) {
        const bool takeImp = (j >= cj) ||
            (i < ci && (int16_t)(at(s_imp, i).seq - at(s_rtn, j).seq) <= 0);
        out[n++] = takeImp ? at(s_imp, i++) : at(s_rtn, j++);
    }
    xSemaphoreGive(s_mutex);
    return n;
}

// ═══════════════════════════════════════════════════════════════════
//  НАБЛЮДАТЕЛЬ
// ═══════════════════════════════════════════════════════════════════
// Сравнивает текущее состояние с прошлым и пишет разницу. Первый проход
// (через EV_SETTLE_MS после запуска) только запоминает исходное состояние:
// иначе каждый запуск начинался бы десятком «реле выключено».

// Флаг с антидребезгом: событие — только если новое значение продержалось
// EV_DEBOUNCE_MS (мигающий датчик или Wi-Fi не засыпают журнал).
struct Debounced {
    bool     logged;
    bool     pending;
    uint64_t sinceMs;
};
static bool debounce(Debounced& d, bool now, uint64_t nowMs) {
    if (now == d.logged) { d.pending = now; return false; }
    if (now != d.pending) { d.pending = now; d.sinceMs = nowMs; return false; }
    if (nowMs - d.sinceMs < EV_DEBOUNCE_MS) return false;
    d.logged = now;
    return true;
}

static const uint8_t OBS_RELAYS[] = { RELAY_IDX_IRR1, RELAY_IDX_IRR2, RELAY_IDX_IRR3,
                                      RELAY_IDX_HUMID, RELAY_IDX_WATER_PUMP };

static bool      s_obsReady = false;
static uint64_t  s_lastObsMs = 0;
static bool      s_relay[sizeof(OBS_RELAYS)];
static uint8_t   s_mode[4];
static bool      s_pumpFault = false;
static Debounced s_sensor[4];
static Debounced s_rain;
static Debounced s_wifi;
static uint8_t   s_ventLogged[NUM_VENTS];
static uint8_t   s_ventSeen[NUM_VENTS];
static uint64_t  s_ventStillMs[NUM_VENTS];
static uint64_t  s_authWinMs = 0;

static void readModes(uint8_t out[4]) {
    // Однобайтовые поля читаются атомарно; лок настроек тут не нужен —
    // промах на одну секунду не страшен, а блокировать управление незачем.
    out[0] = g_settings.modeVents;
    out[1] = g_settings.modeTimers;
    out[2] = g_settings.modeHumid;
    out[3] = g_settings.modeWaterPump;
}

static void observe(uint64_t nowMs) {
    const SensorSnapshot s = SensorRdr::getData();
    const bool sens[4] = { s.bmeOk, s.dhtOk, s.luxOk, s.waterOk };
    uint8_t modes[4];
    readModes(modes);

    if (!s_obsReady) {
        for (uint8_t i = 0; i < sizeof(OBS_RELAYS); i++) s_relay[i] = RelayMgr::getState(OBS_RELAYS[i]);
        memcpy(s_mode, modes, sizeof(s_mode));
        s_pumpFault = WaterPumpCtrl::isInFault();
        for (uint8_t i = 0; i < 4; i++) s_sensor[i] = { sens[i], sens[i], nowMs };
        s_rain = { s.rain, s.rain, nowMs };
        const bool w = NetMgr::isConnected();
        s_wifi = { w, w, nowMs };
        VentStatusSnapshot vs0;
        VentCtrl::getStatus(vs0);
        for (uint8_t v = 0; v < NUM_VENTS; v++) {
            s_ventLogged[v] = s_ventSeen[v] = (v == 0 ? vs0.topPct : vs0.bottomPct);
            s_ventStillMs[v] = nowMs;
        }
        s_authWinMs = nowMs;
        s_obsReady = true;
        return;
    }

    for (uint8_t i = 0; i < sizeof(OBS_RELAYS); i++) {
        const bool st = RelayMgr::getState(OBS_RELAYS[i]);
        if (st != s_relay[i]) { s_relay[i] = st; EventLog::add(EV_RELAY, OBS_RELAYS[i], st ? 1 : 0); }
    }
    for (uint8_t i = 0; i < 4; i++) {
        if (modes[i] != s_mode[i]) { s_mode[i] = modes[i]; EventLog::add(EV_MODE, i, modes[i] ? 1 : 0); }
    }
    const bool pf = WaterPumpCtrl::isInFault();
    if (pf != s_pumpFault) { s_pumpFault = pf; EventLog::add(EV_PUMP, 0, pf ? 1 : 0); }
    for (uint8_t i = 0; i < 4; i++) {
        if (debounce(s_sensor[i], sens[i], nowMs)) EventLog::add(EV_SENSOR, i, sens[i] ? 1 : 0);
    }
    if (debounce(s_rain, s.rain, nowMs)) EventLog::add(EV_RAIN, 0, s.rain ? 1 : 0);
    const bool w = NetMgr::isConnected();
    if (debounce(s_wifi, w, nowMs)) EventLog::add(EV_WIFI, 0, w ? 1 : 0);

    // Створка: событие, когда она остановилась в новом положении
    // («верхняя: 0 → 40 %»), а не на каждый процент движения.
    VentStatusSnapshot vs;
    VentCtrl::getStatus(vs);
    for (uint8_t v = 0; v < NUM_VENTS; v++) {
        const uint8_t pos = v == 0 ? vs.topPct : vs.bottomPct;
        if (pos != s_ventSeen[v]) { s_ventSeen[v] = pos; s_ventStillMs[v] = nowMs; continue; }
        if (pos != s_ventLogged[v] && nowMs - s_ventStillMs[v] >= EV_VENT_STILL_MS) {
            EventLog::add(EV_VENT, v, (int16_t)(s_ventLogged[v] * 256 + pos));
            s_ventLogged[v] = pos;
        }
    }

    if (nowMs - s_authWinMs >= EV_AUTH_WIN_MS) {
        s_authWinMs = nowMs;
        const uint16_t n = s_authFails.exchange(0);
        if (n) EventLog::add(EV_AUTH_FAIL, 0, (int16_t)(n > 32767 ? 32767 : n));
    }
}

void EventLog::loop(uint64_t nowMs) {
    if (!s_mutex) return;
    // Исходное состояние снимаем, когда Wi-Fi уже поднялся (подключение при старте
    // ждёт до 30 с, медленный DHT22 тоже), но не позже EV_SETTLE_MAX_MS: иначе
    // каждый запуск начинался бы ложными «Wi-Fi: подключено» и «датчик снова работает».
    const bool settled = nowMs >= EV_SETTLE_MS && (NetMgr::isConnected() || nowMs >= EV_SETTLE_MAX_MS);
    if ((s_obsReady || settled) && nowMs - s_lastObsMs >= EV_OBSERVE_MS) {
        s_lastObsMs = nowMs;
        observe(nowMs);
    }
    if (s_dirty.load() && nowMs - s_lastSaveMs >= EV_SAVE_MS) flush();
}
