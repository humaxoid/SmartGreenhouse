// ╔══════════════════════════════════════════════════════════════════╗
// ║  relay_manager.cpp — Реализация управления реле v5.0            ║
// ║                                                                  ║
// ║  Ключевые механизмы:                                             ║
// ║   1. Антидребезг: реле не переключается чаще 1 раза/сек         ║
// ║   2. H-bridge interlock: OPEN+CLOSE одновременно запрещены      ║
// ║   3. Break-before-make: при смене направления неблокирующая     ║
// ║      пауза 100 мс через конечный автомат                        ║
// ║   4. Эффективный doGpio: пропускает запись если состояние       ║
// ║      уже такое (экономия tick'ов и возможных glitch'ей)         ║
// ╚══════════════════════════════════════════════════════════════════╝

#include "relay_manager.h"
#include "settings.h"
#include "mqtt_manager.h"
#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════
//  СТАТИЧЕСКИЕ ТАБЛИЦЫ
// ═══════════════════════════════════════════════════════════════════

// Массив GPIO-пинов всех реле (порядок = RELAY_IDX_*)
static const uint8_t RELAY_PINS[NUM_RELAYS] = {
    RELAY_PIN_IRR1,              // 0: Полив 1
    RELAY_PIN_IRR2,              // 1: Полив 2
    RELAY_PIN_IRR3,              // 2: Полив 3
    RELAY_PIN_VENT_UPPER_OPEN,   // 3: Верх. OPEN
    RELAY_PIN_VENT_UPPER_CLOSE,  // 4: Верх. CLOSE
    RELAY_PIN_VENT_LOWER_OPEN,   // 5: Нижн. OPEN
    RELAY_PIN_VENT_LOWER_CLOSE,  // 6: Нижн. CLOSE
    RELAY_PIN_HUMID,             // 7: Увлажнитель
    RELAY_PIN_WATER_PUMP         // 8: Насос
};

// Человекочитаемые имена для логов
static const char* RELAY_NAMES[NUM_RELAYS] = {
    "Полив 1",
    "Полив 2",
    "Полив 3",
    "Верх. форт. OPEN",
    "Верх. форт. CLOSE",
    "Нижн. форт. OPEN",
    "Нижн. форт. CLOSE",
    "Увлажнитель",
    "Насос скважины"
};

// ═══════════════════════════════════════════════════════════════════
//  ВНУТРЕННЕЕ СОСТОЯНИЕ
// ═══════════════════════════════════════════════════════════════════

// Время последнего переключения каждого реле (для антидребезга).
// Инициализируется в init() в 0 — чтобы первое переключение прошло
// без ограничений.
static uint64_t s_lastToggleMs[NUM_RELAYS] = {0};

// ═══════════════════════════════════════════════════════════════════
//  PENDING OPERATIONS (конечный автомат H-bridge)
// ═══════════════════════════════════════════════════════════════════
// При смене направления форточки происходит:
//   1. t=0:   выключаем текущее реле (OPEN или CLOSE)
//   2. t=100: включаем противоположное реле
//
// Чтобы НЕ БЛОКИРОВАТЬ задачу на 100 мс (это недопустимо в
// многозадачной архитектуре), мы используем отложенное исполнение:
// сохраняем "какое реле включить и когда" в массиве pending[],
// updatePending() проверяет в каждом цикле и включает когда пора.
//
// Для каждой форточки (2 штуки) может быть ОДНА pending операция.

struct PendingOp {
    bool      active;         // Есть ли отложенная операция?
    uint8_t   relayIdx;       // Какое реле включить
    uint64_t  fireAtMs;       // Когда включить (абсолютное время)
};

static PendingOp s_pending[NUM_VENTS] = {};

// Heartbeat LED state
static uint64_t s_lastHeartbeatMs = 0;
static bool     s_heartbeatState  = false;

// ═══════════════════════════════════════════════════════════════════
//  НИЗКОУРОВНЕВОЕ УПРАВЛЕНИЕ GPIO
// ═══════════════════════════════════════════════════════════════════

// Прямая запись в GPIO без всяких проверок. Обновляет g_settings.relayState
// и уведомляет MQTT. Мьютекс g_settingsMutex ДОЛЖЕН быть захвачен вызывающим!
//
// Возвращает true, если состояние фактически изменилось.
static bool doGpio(uint8_t idx, bool on) {
    if (g_settings.relayState[idx] == on) {
        return false;  // уже в нужном состоянии — no-op
    }
    digitalWrite(RELAY_PINS[idx], on ? HIGH : LOW);
    g_settings.relayState[idx] = on;
    s_lastToggleMs[idx] = steadyMs();
    Serial.printf("[RELAY] %s -> %s\n", RELAY_NAMES[idx], on ? "ON" : "OFF");
    settingsSaveDeferred();
    MqttMgr::publishRelayState(idx);
    return true;
}

// ═══════════════════════════════════════════════════════════════════
//  ПУБЛИЧНОЕ API
// ═══════════════════════════════════════════════════════════════════

void RelayMgr::init() {
    // Все реле в OFF. Это критически важно — если не вызвать pinMode+LOW
    // до начала основной работы, реле могут случайно включиться.
    for (uint8_t i = 0; i < NUM_RELAYS; i++) {
        pinMode(RELAY_PINS[i], OUTPUT);
        digitalWrite(RELAY_PINS[i], LOW);
        s_lastToggleMs[i] = 0;
    }

    // Heartbeat LED
    pinMode(LED_HEARTBEAT_PIN, OUTPUT);
    digitalWrite(LED_HEARTBEAT_PIN, LOW);

    // Pending очистить
    for (uint8_t v = 0; v < NUM_VENTS; v++) {
        s_pending[v] = {false, 0, 0};
    }

    Serial.printf("[RELAY] Initialized: %u relays, all OFF\n", (unsigned)NUM_RELAYS);
}

void RelayMgr::restoreFromSettings() {
    // Восстанавливаем только ОДИНОЧНЫЕ реле (полив, увлажнитель, насос).
    // Реле форточек (H-bridge) всегда стартуют в OFF — позиция
    // восстанавливается через g_ventRuntime, а движение (если нужно)
    // задаст контроллер по температуре.
    MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(200));
    if (!lock.locked()) {
        Serial.println(F("[RELAY] restoreFromSettings: mutex timeout"));
        return;
    }

    for (uint8_t i = 0; i < NUM_RELAYS; i++) {
        if (isVentRelay(i)) {
            // Форточные реле НЕ восстанавливаем. Принудительно OFF:
            g_settings.relayState[i] = false;
            digitalWrite(RELAY_PINS[i], LOW);
            continue;
        }
        bool state = g_settings.relayState[i];
        digitalWrite(RELAY_PINS[i], state ? HIGH : LOW);
        Serial.printf("[RELAY] Restored %s -> %s\n",
            RELAY_NAMES[i], state ? "ON" : "OFF");
    }
}

// ═══════════════════════════════════════════════════════════════════
//  ВСПОМОГАТЕЛЬНЫЕ
// ═══════════════════════════════════════════════════════════════════

bool RelayMgr::isVentRelay(uint8_t idx) {
    return idx >= RELAY_IDX_VENT_UPPER_OPEN
        && idx <= RELAY_IDX_VENT_LOWER_CLOSE;
}

uint8_t RelayMgr::ventOpenRelayIdx(uint8_t ventIdx) {
    return (ventIdx == VENT_IDX_UPPER)
        ? RELAY_IDX_VENT_UPPER_OPEN
        : RELAY_IDX_VENT_LOWER_OPEN;
}

uint8_t RelayMgr::ventCloseRelayIdx(uint8_t ventIdx) {
    return (ventIdx == VENT_IDX_UPPER)
        ? RELAY_IDX_VENT_UPPER_CLOSE
        : RELAY_IDX_VENT_LOWER_CLOSE;
}

// ═══════════════════════════════════════════════════════════════════
//  ОДИНОЧНЫЕ РЕЛЕ
// ═══════════════════════════════════════════════════════════════════

bool RelayMgr::setRelay(uint8_t idx, bool on) {
    if (idx >= NUM_RELAYS) return false;

    // Защита: реле форточек НЕЛЬЗЯ дёргать напрямую, только через moveVent
    if (isVentRelay(idx)) {
        Serial.printf("[RELAY] WARN: setRelay on vent relay %u ignored — use moveVent()\n",
            (unsigned)idx);
        return false;
    }

    // Антидребезг
    const uint64_t now = steadyMs();
    if (s_lastToggleMs[idx] != 0 &&
        (now - s_lastToggleMs[idx]) < RELAY_MIN_TOGGLE_INTERVAL_MS) {
        Serial.printf("[RELAY] %s debounce: %llu ms since last toggle\n",
            RELAY_NAMES[idx],
            (unsigned long long)(now - s_lastToggleMs[idx]));
        return false;
    }

    MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(50));
    if (!lock.locked()) return false;

    return doGpio(idx, on);
}

bool RelayMgr::getState(uint8_t idx) {
    if (idx >= NUM_RELAYS) return false;
    // v6.1: добавлен mutex для гарантии последовательности с записями
    // в g_settings (избегаем torn reads если рядом меняется AppSettings).
    MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(10));
    if (!lock.locked()) {
        // Если mutex недоступен — fallback на прямое чтение (1 байт всё-таки атомарен).
        return g_settings.relayState[idx];
    }
    return g_settings.relayState[idx];
}

const char* RelayMgr::getName(uint8_t idx) {
    if (idx >= NUM_RELAYS) return "?";
    return RELAY_NAMES[idx];
}

// ═══════════════════════════════════════════════════════════════════
//  H-BRIDGE API
// ═══════════════════════════════════════════════════════════════════

bool RelayMgr::moveVent(uint8_t ventIdx, VentDirection dir) {
    if (ventIdx >= NUM_VENTS) return false;

    const uint8_t openIdx  = ventOpenRelayIdx(ventIdx);
    const uint8_t closeIdx = ventCloseRelayIdx(ventIdx);
    const uint64_t now = steadyMs();

    // ── Захватываем мьютекс на всю операцию ──────────────────────
    // Это КРИТИЧНО — иначе две задачи могли бы одновременно включить
    // OPEN и CLOSE (КЗ). Мьютекс обеспечивает атомарность всей
    // последовательности "выключить противоположное → запланировать включение".
    MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(100));
    if (!lock.locked()) {
        Serial.printf("[RELAY] moveVent #%u: mutex timeout\n", (unsigned)ventIdx);
        return false;
    }

    // ── Какое реле должно быть ВКЛ, какое ВЫКЛ? ──────────────────
    uint8_t targetOn  = 0xFF;  // 0xFF = ни одно (полный стоп)
    uint8_t targetOff = 0xFF;

    switch (dir) {
        case VENT_DIR_OPENING:
            targetOn  = openIdx;
            targetOff = closeIdx;
            break;
        case VENT_DIR_CLOSING:
            targetOn  = closeIdx;
            targetOff = openIdx;
            break;
        case VENT_DIR_STOPPED:
            // Оба выключены — обрабатывается stopVent()
            // Здесь тоже работает, но stopVent() проще и быстрее.
            break;
    }

    // ── Шаг 1: выключить противоположное реле, если оно активно ──
    if (targetOff != 0xFF && g_settings.relayState[targetOff]) {
        doGpio(targetOff, false);

        // Запланировать включение через break-before-make паузу
        s_pending[ventIdx] = {
            true,
            targetOn,
            now + HBRIDGE_BREAK_BEFORE_MAKE_MS
        };
        Serial.printf("[HBRIDGE] Vent #%u: %s OFF, scheduled %s ON in %u ms\n",
            (unsigned)ventIdx,
            RELAY_NAMES[targetOff],
            (targetOn != 0xFF) ? RELAY_NAMES[targetOn] : "(nothing)",
            (unsigned)HBRIDGE_BREAK_BEFORE_MAKE_MS);
        return true;
    }

    // ── Шаг 2 (немедленно): включаем нужное реле ─────────────────
    // Противоположное уже выключено, так что interlock выполнен.
    if (targetOn != 0xFF && !g_settings.relayState[targetOn]) {
        doGpio(targetOn, true);
    }

    // Отменяем ожидающую операцию, если была
    s_pending[ventIdx] = {false, 0, 0};

    return true;
}

bool RelayMgr::stopVent(uint8_t ventIdx) {
    if (ventIdx >= NUM_VENTS) return false;

    const uint8_t openIdx  = ventOpenRelayIdx(ventIdx);
    const uint8_t closeIdx = ventCloseRelayIdx(ventIdx);

    MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(100));
    if (!lock.locked()) {
        Serial.printf("[RELAY] stopVent #%u: mutex timeout\n", (unsigned)ventIdx);
        return false;
    }

    // Оба реле в OFF. Для стопа break-before-make НЕ НУЖЕН
    // (нет перекрытия — это безопасное состояние).
    bool changed = false;
    if (g_settings.relayState[openIdx])  { doGpio(openIdx, false);  changed = true; }
    if (g_settings.relayState[closeIdx]) { doGpio(closeIdx, false); changed = true; }

    // Отменяем pending операцию (если была)
    s_pending[ventIdx] = {false, 0, 0};

    return changed;
}

VentDirection RelayMgr::getVentDirection(uint8_t ventIdx) {
    if (ventIdx >= NUM_VENTS) return VENT_DIR_STOPPED;

    // Читаем состояния реле напрямую из g_settings.relayState[].
    // Строго говоря, нужен мьютекс, но чтение одного bool — атомарно,
    // а race condition возможен только на границе переключения
    // (~микросекунды), что некритично для получения "примерного" статуса.
    const uint8_t openIdx  = ventOpenRelayIdx(ventIdx);
    const uint8_t closeIdx = ventCloseRelayIdx(ventIdx);
    const bool openOn  = g_settings.relayState[openIdx];
    const bool closeOn = g_settings.relayState[closeIdx];

    // Защитная проверка: если оба ВКЛ — это баг, но возвращаем STOP
    // чтобы контроллер остановил форточку и разобрался.
    if (openOn && closeOn) return VENT_DIR_STOPPED;
    if (openOn)  return VENT_DIR_OPENING;
    if (closeOn) return VENT_DIR_CLOSING;
    return VENT_DIR_STOPPED;
}

// ═══════════════════════════════════════════════════════════════════
//  ПЕРИОДИЧЕСКИЙ UPDATE
// ═══════════════════════════════════════════════════════════════════

void RelayMgr::updatePending(uint64_t nowMs) {
    for (uint8_t v = 0; v < NUM_VENTS; v++) {
        if (!s_pending[v].active) continue;
        if (nowMs < s_pending[v].fireAtMs) continue;

        // Время пришло — включаем реле
        const uint8_t relayIdx = s_pending[v].relayIdx;

        MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(50));
        if (!lock.locked()) continue;  // попробуем в следующий раз

        // ВАЖНАЯ ПРОВЕРКА: противоположное реле действительно выключено?
        // Если за это время кто-то успел переключить (например, вторая
        // команда moveVent с другим направлением) — не включаем.
        const uint8_t openIdx  = ventOpenRelayIdx(v);
        const uint8_t closeIdx = ventCloseRelayIdx(v);
        const uint8_t otherIdx = (relayIdx == openIdx) ? closeIdx : openIdx;

        if (g_settings.relayState[otherIdx]) {
            // Противоположное включено — interlock! Отменяем.
            Serial.printf("[HBRIDGE] Vent #%u: pending cancelled (interlock)\n",
                (unsigned)v);
            s_pending[v] = {false, 0, 0};
            continue;
        }

        // Всё ОК, включаем
        doGpio(relayIdx, true);
        s_pending[v] = {false, 0, 0};
    }
}

// ═══════════════════════════════════════════════════════════════════
//  HEARTBEAT LED
// ═══════════════════════════════════════════════════════════════════

void RelayMgr::updateHeartbeat(uint64_t nowMs) {
    if (nowMs - s_lastHeartbeatMs < LED_HEARTBEAT_PERIOD_MS) return;
    s_lastHeartbeatMs = nowMs;
    s_heartbeatState = !s_heartbeatState;
    digitalWrite(LED_HEARTBEAT_PIN, s_heartbeatState ? HIGH : LOW);
}

// ═══════════════════════════════════════════════════════════════════
//  v6.0: ПРЯМОЕ управление реле (для VentCtrl)
//  Минует isVentRelay() и антидребезг.
// ═══════════════════════════════════════════════════════════════════
bool RelayMgr::setRelayDirect(uint8_t idx, bool on) {
    if (idx >= NUM_RELAYS) return false;

    digitalWrite(RELAY_PINS[idx], on ? HIGH : LOW);
    s_lastToggleMs[idx] = steadyMs();

    // Сохраняем состояние в g_settings для отображения в snapshot
    {
        MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(20));
        if (lock.locked()) {
            g_settings.relayState[idx] = on;
            // Реле форточек в NVS НЕ сохраняем — они стартуют в STOP
            if (!isVentRelay(idx)) settingsSaveDeferred();
        }
    }
    return true;
}
