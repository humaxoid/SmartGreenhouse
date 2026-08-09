// ╔══════════════════════════════════════════════════════════════════╗
// ║  settings.cpp — Реализация NVS загрузки/сохранения (v5.0)       ║
// ║                                                                  ║
// ║  Хранит настройки в NVS под namespace "greenhouse".              ║
// ║  При несовпадении APP_SETTINGS_VERSION — применяет defaults      ║
// ║  (защита от мусорных данных после обновления прошивки).         ║
// ╚══════════════════════════════════════════════════════════════════╝

#include "settings.h"
#include <Preferences.h>
#include <Arduino.h>
#include <string.h>

// ─── Глобальные объекты ──────────────────────────────────────────
AppSettings       g_settings;
SemaphoreHandle_t g_settingsMutex = nullptr;
std::atomic<bool> g_settingsDirty{false};

VentRuntime       g_ventRuntime[NUM_VENTS];
SemaphoreHandle_t g_ventRuntimeMutex = nullptr;

// NVS namespace — изменился в v5.0 (старый "gh4" не совместим по структуре)
static const char* NVS_NAMESPACE = "gh5";
static const char* NVS_KEY_BLOB  = "settings";

// ═══════════════════════════════════════════════════════════════════
//  ПРИМЕНЕНИЕ ЗНАЧЕНИЙ ПО УМОЛЧАНИЮ
// ═══════════════════════════════════════════════════════════════════
// Вызывается при первом запуске или при несовпадении версии NVS.
// Все поля должны быть явно установлены — memset(0) НЕ использовать,
// т.к. float-поля требуют определённых дефолтов, а строки — NULL-term.
static void applyDefaults() {
    g_settings.settingsVersion = APP_SETTINGS_VERSION;

    // ── Режимы: по умолчанию всё ручное (безопасное состояние) ───
    // v6.0: по умолчанию все режимы в AUTO (1), раньше было MANUAL (0)
    g_settings.modeVents     = 1;
    g_settings.modeTimers    = 1;
    g_settings.modeHumid     = 1;
    g_settings.modeWaterPump = 1;

    // ── Реле: все выключены ──────────────────────────────────────
    for (int i = 0; i < NUM_RELAYS; i++) {
        g_settings.relayState[i] = false;
    }

    // ── Таймеры полива: пустые, не активны ──────────────────────
    for (int i = 0; i < NUM_IRRIGATION; i++) {
        g_settings.timer[i] = {0, 0, 0};
        g_settings.timerActiveStartSec[i] = -1;
    }

    // ── Форточки: пороги по умолчанию ───────────────────────────
    g_settings.ventUpper = { DEF_VENT_UPPER_HIGH, DEF_VENT_UPPER_LOW };
    g_settings.ventLower = { DEF_VENT_LOWER_HIGH, DEF_VENT_LOWER_LOW };

    // ── Форточки: калибровка хода (из config.h) ─────────────────
    g_settings.ventUpperCal = {
        (uint32_t)VENT_UPPER_TRAVEL_MS,
        (uint16_t)VENT_TRAVEL_OVERRUN_PCT
    };
    g_settings.ventLowerCal = {
        (uint32_t)VENT_LOWER_TRAVEL_MS,
        (uint16_t)VENT_TRAVEL_OVERRUN_PCT
    };

    // ── Форточки: позиция после reset — требует калибровки ──────
    // При первом запуске мы не знаем, в каком положении актуаторы.
    // needsCalibration=1 приведёт к принудительному закрытию до концевика.
    g_settings.ventUpperPos = { 0, 0 };  // pct=0, valid=0 (требует калибровки)
    g_settings.ventLowerPos = { 0, 0 };

    // ── Увлажнитель ──────────────────────────────────────────────
    g_settings.humidHigh = DEF_HUMID_HIGH;
    g_settings.humidLow  = DEF_HUMID_LOW;

    // ── Ёмкость/насос ───────────────────────────────────────────
    g_settings.waterTank = {
        DEF_WATER_LOW_PCT,
        DEF_WATER_HIGH_PCT,
        DEF_WATER_DEPTH_CM,
        DEF_WATER_PUMP_TIMEOUT
    };

    // ── NTP (POSIX TZ) ──────────────────────────────────────────
    strlcpy(g_settings.ntpServer,   NTP_SERVER_PRIMARY, sizeof(g_settings.ntpServer));
    strlcpy(g_settings.ntpTzString, NTP_TZ_STRING,      sizeof(g_settings.ntpTzString));
    g_settings.ntpUpdateHrs = 1;

    // ── MQTT — пусто, пользователь настраивает через веб ────────
    strlcpy(g_settings.mqttServer, MQTT_DEFAULT_SERVER, sizeof(g_settings.mqttServer));
    strlcpy(g_settings.mqttUser,   MQTT_DEFAULT_USER,   sizeof(g_settings.mqttUser));
    strlcpy(g_settings.mqttPass,   MQTT_DEFAULT_PASS,   sizeof(g_settings.mqttPass));
    g_settings.mqttPort    = MQTT_DEFAULT_PORT;
    g_settings.mqttEnabled = false;

    // ── UI настройки (v6.0) ─────────────────────────────────────
    strlcpy(g_settings.uiTheme, "",   sizeof(g_settings.uiTheme));
    strlcpy(g_settings.uiLang,  "en", sizeof(g_settings.uiLang));

    Serial.println(F("[SETTINGS] Applied factory defaults"));
}

// ═══════════════════════════════════════════════════════════════════
//  ЗАГРУЗКА ИЗ NVS
// ═══════════════════════════════════════════════════════════════════
// Читаем целую структуру одним blob'ом — это быстрее и атомарнее,
// чем десятки отдельных полей (как было в v4).
//
// Проверка целостности:
//   1. Размер данных в NVS == sizeof(AppSettings)? (защита от старого формата)
//   2. settingsVersion == APP_SETTINGS_VERSION? (защита от миграции)
// Если что-то не так → applyDefaults() + settingsSave().
void settingsLoad() {
    // Инициализируем мьютексы если ещё не созданы (защита от повторного вызова)
    // v6.1: РЕКУРСИВНЫЙ мьютекс — см. комментарий к MutexGuard в settings.h.
    // Обязан быть рекурсивным: MutexGuard использует xSemaphoreTakeRecursive.
    if (!g_settingsMutex) {
        g_settingsMutex = xSemaphoreCreateRecursiveMutex();
        configASSERT(g_settingsMutex);
    }
    if (!g_ventRuntimeMutex) {
        g_ventRuntimeMutex = xSemaphoreCreateRecursiveMutex();
        configASSERT(g_ventRuntimeMutex);
    }

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) {  // read-only mode
        Serial.println(F("[SETTINGS] NVS namespace missing, using defaults"));
        applyDefaults();
        prefs.end();
        settingsSave();  // сразу сохраним дефолты
        return;
    }

    size_t expectedSize = sizeof(AppSettings);
    size_t actualSize   = prefs.getBytesLength(NVS_KEY_BLOB);

    if (actualSize != expectedSize) {
        Serial.printf("[SETTINGS] NVS size mismatch (got %u, expected %u) — reset to defaults\n",
            (unsigned)actualSize, (unsigned)expectedSize);
        prefs.end();
        applyDefaults();
        settingsSave();
        return;
    }

    size_t readBytes = prefs.getBytes(NVS_KEY_BLOB, &g_settings, expectedSize);
    prefs.end();

    if (readBytes != expectedSize) {
        Serial.println(F("[SETTINGS] NVS read failed — reset to defaults"));
        applyDefaults();
        settingsSave();
        return;
    }

    if (g_settings.settingsVersion != APP_SETTINGS_VERSION) {
        Serial.printf("[SETTINGS] Version mismatch (NVS=%u, FW=%u) — reset to defaults\n",
            (unsigned)g_settings.settingsVersion,
            (unsigned)APP_SETTINGS_VERSION);
        applyDefaults();
        settingsSave();
        return;
    }

    Serial.printf("[SETTINGS] Loaded OK (version %u, %u bytes)\n",
        (unsigned)g_settings.settingsVersion, (unsigned)readBytes);
    Serial.printf("[SETTINGS] Vent upper: pos=%u%% valid=%u cal=%ums\n",
        g_settings.ventUpperPos.lastKnownPct,
        g_settings.ventUpperPos.positionValid,
        g_settings.ventUpperCal.travelMs);
    Serial.printf("[SETTINGS] Vent lower: pos=%u%% valid=%u cal=%ums\n",
        g_settings.ventLowerPos.lastKnownPct,
        g_settings.ventLowerPos.positionValid,
        g_settings.ventLowerCal.travelMs);
}

// ═══════════════════════════════════════════════════════════════════
//  СОХРАНЕНИЕ В NVS
// ═══════════════════════════════════════════════════════════════════
// Пишем целую структуру blob'ом. Важно: putBytes — атомарная для
// NVS (либо записалось всё, либо ничего — при сбое питания).
// v6.1: контрольная сумма последнего ЗАПИСАННОГО блоба и счётчик реальных
// записей. Нужны, чтобы не жечь флеш впустую — см. подробности в settingsSave().
static uint32_t s_lastWrittenCrc = 0;
static bool     s_lastCrcValid   = false;
static uint32_t s_nvsWriteCount  = 0;

// CRC32 (полином 0xEDB88320) без таблицы: блоб небольшой, считается
// за десятки микросекунд, а таблица заняла бы килобайт RAM.
static uint32_t crc32Buf(const void* data, size_t len) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFFu;
    while (len--) {
        crc ^= *p++;
        for (uint8_t i = 0; i < 8; i++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1)));
        }
    }
    return ~crc;
}

uint32_t settingsNvsWriteCount() { return s_nvsWriteCount; }

void settingsSave() {
    // Под мьютексом делаем копию — минимизируем время блокировки,
    // т.к. запись в flash может занимать до 50 мс.
    AppSettings copy;
    {
        MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(100));
        if (!lock.locked()) {
            Serial.println(F("[SETTINGS] Save skipped — mutex timeout"));
            return;
        }
        copy = g_settings;  // bitwise copy POD-структуры
    }

    // ── v6.1: не пишем то, что уже записано ──────────────────────
    // settingsSaveDeferred() дёргается из многих мест: с каждым щелчком
    // реле, при каждой остановке мотора форточки, при любой правке настроек.
    // Значительная часть этих вызовов не меняет блоб вообще — например,
    // повторная запись той же позиции створки или того же состояния реле.
    // Флеш при этом переписывался целиком.
    //
    // Сравниваем контрольную сумму: совпала — записи нет, флеш не тронут.
    // Флаг dirty снимаем в любом случае, иначе цикл сохранения будет
    // возвращаться к этим же данным каждые NVS_SAVE_DELAY_MS.
    uint32_t crc = crc32Buf(&copy, sizeof(copy));
    if (s_lastCrcValid && crc == s_lastWrittenCrc) {
        g_settingsDirty.store(false);
        return;
    }

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {  // read-write mode
        Serial.println(F("[SETTINGS] NVS begin failed for save!"));
        return;
    }

    size_t written = prefs.putBytes(NVS_KEY_BLOB, &copy, sizeof(copy));
    prefs.end();

    if (written == sizeof(copy)) {
        g_settingsDirty.store(false);
        s_lastWrittenCrc = crc;
        s_lastCrcValid   = true;
        s_nvsWriteCount++;
        Serial.printf("[SETTINGS] Saved to NVS (%u bytes, write #%u)\n",
            (unsigned)written, (unsigned)s_nvsWriteCount);
    } else {
        Serial.printf("[SETTINGS] Save FAILED (wrote %u, expected %u)\n",
            (unsigned)written, (unsigned)sizeof(copy));
        // Dirty флаг НЕ сбрасываем — попробуем снова в следующий цикл
    }
}

// ═══════════════════════════════════════════════════════════════════
//  ОТЛОЖЕННОЕ СОХРАНЕНИЕ
// ═══════════════════════════════════════════════════════════════════
// Просто помечаем флаг. Фактическое сохранение в taskCore1 через
// NVS_SAVE_DELAY_MS (см. main.cpp). Это защищает flash от износа
// при частых изменениях (например, слайдер форточки).
void settingsSaveDeferred() {
    g_settingsDirty.store(true);
}

// ═══════════════════════════════════════════════════════════════════
//  FACTORY RESET
// ═══════════════════════════════════════════════════════════════════
// Применяет defaults и сохраняет. Не трогает сетевые настройки (они
// хранятся в отдельном namespace "net3" — см. network_manager.cpp).
void settingsReset() {
    {
        MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(200));
        if (!lock.locked()) {
            Serial.println(F("[SETTINGS] Reset skipped — mutex timeout"));
            return;
        }
        applyDefaults();
    }
    settingsSave();
    Serial.println(F("[SETTINGS] Reset to factory defaults completed"));
}

// ═══════════════════════════════════════════════════════════════════
//  ИНИЦИАЛИЗАЦИЯ RUNTIME СОСТОЯНИЯ ФОРТОЧЕК
// ═══════════════════════════════════════════════════════════════════
// Вызывается после settingsLoad(). Инициализирует эфемерное состояние
// g_ventRuntime на основе сохранённой позиции из g_settings.
//
// Логика:
//   • positionValid=1 → верим сохранённой позиции, ставим её в targetPct
//     (чтобы при старте в auto-режиме форточка не двигалась без причины).
//   • positionValid=0 → needsCalibration=true, контроллер выполнит
//     home-последовательность (закрыть до концевика + запас 10%).
//
// Мьютекс при вызове не нужен — задачи ещё не запущены.
void ventRuntimeInit() {
    for (uint8_t v = 0; v < NUM_VENTS; v++) {
        VentRuntime& rt = g_ventRuntime[v];
        const VentPersistent& ps = (v == VENT_IDX_UPPER)
            ? g_settings.ventUpperPos
            : g_settings.ventLowerPos;

        // v6.0: автокалибровка убрана. Если позиция невалидна (первый старт) —
        // считаем что форточка ЗАКРЫТА (0%) — пользователь должен убедиться
        // в этом перед первой подачей питания.
        if (ps.positionValid == 0) {
            rt.currentPct = 0.0f;
        } else {
            rt.currentPct = (float)ps.lastKnownPct;
        }
        rt.targetPct        = rt.currentPct;
        rt.direction        = VENT_DIR_STOPPED;
        rt.moveStartMs      = 0;
        rt.lastDirChangeMs  = 0;
        rt.needsCalibration = false;   // v6.0: автокалибровка отключена
        rt.calibrating      = false;

        // Если позиция была невалидной — сохраняем "0% valid" в NVS,
        // чтобы при следующих запусках не выводить лишний лог
        if (ps.positionValid == 0) {
            VentPersistent& pps = (v == VENT_IDX_UPPER)
                ? g_settings.ventUpperPos
                : g_settings.ventLowerPos;
            pps.lastKnownPct  = 0;
            pps.positionValid = 1;
            settingsSaveDeferred();
        }

        Serial.printf("[VENT] Runtime init #%u: pos=%.1f%% valid=%u (autocal disabled)\n",
            (unsigned)v, rt.currentPct, ps.positionValid);
    }
}
