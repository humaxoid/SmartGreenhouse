// ╔══════════════════════════════════════════════════════════════════╗
// ║  button_manager.cpp — Реализация обработки кнопок v6.0          ║
// ║                                                                  ║
// ║  Изменения v6.0:                                                 ║
// ║   • Кнопки полива (3 шт) — toggle (как было).                    ║
// ║   • Кнопки форточек (2 шт): hold-режим вместо toggle.            ║
// ║       BTN_VENT_OPEN  (GPIO23) — пока зажата, эстафета верх→низ. ║
// ║       BTN_VENT_CLOSE (GPIO19) — пока зажата, эстафета низ→верх. ║
// ║   • Опрос кнопок hold-режима каждые 50 мс (а не на ISR FALLING). ║
// ║   • Дополнительно ISR на FALLING для мгновенного реагирования    ║
// ║     полива (там по-прежнему антидребезг 30 мс).                  ║
// ╚══════════════════════════════════════════════════════════════════╝

#include "button_manager.h"
#include "settings.h"
#include "relay_manager.h"
#include "timer_scheduler.h"
#include "vent_controller.h"
#include "water_pump_controller.h"
#include "ws_handler.h"
#include <Arduino.h>
#include <esp_timer.h>
#include <atomic>

// ═══════════════════════════════════════════════════════════════════
//  ТАБЛИЦЫ КНОПОК
// ═══════════════════════════════════════════════════════════════════

// GPIO-пины (порядок = BTN_IDX_*)
// v6.0: BTN_VENT_UPPER → BTN_VENT_OPEN, BTN_VENT_LOWER → BTN_VENT_CLOSE
static const uint8_t BTN_PINS[NUM_BUTTONS] = {
    BTN_IRR1_PIN,         // 0: Полив 1 → GPIO18
    BTN_IRR2_PIN,         // 1: Полив 2 → GPIO17
    BTN_IRR3_PIN,         // 2: Полив 3 → GPIO16
    BTN_VENT_OPEN_PIN,    // 3: Форточки ОТКРЫТЬ → GPIO23 (hold)
    BTN_VENT_CLOSE_PIN,   // 4: Форточки ЗАКРЫТЬ → GPIO19 (hold)
    BTN_HUMID_PIN,        // 5: Увлажнитель → GPIO35 (toggle, опрос) — v6.2
    BTN_PUMP_PIN          // 6: Насос → GPIO39 (toggle, опрос) — v6.2
};

#define BTN_IDX_IRR1        0
#define BTN_IDX_VENT_OPEN   3
#define BTN_IDX_VENT_CLOSE  4
#define BTN_IDX_HUMID       5
#define BTN_IDX_PUMP        6

// Какие кнопки работают в hold-режиме (опрос каждые 50 мс)
// Для них ISR не нужен — их состояние читаем напрямую.
static const bool BTN_IS_HOLD[NUM_BUTTONS] = {
    false, false, false,   // полив — toggle
    true,  true,           // форточки — hold
    false, false           // увлажнитель и насос — toggle (v6.2)
};

// ─── Почему часть toggle-кнопок опрашивается, а не висит на ISR ──
// v6.2: кнопки увлажнителя и насоса стоят на GPIO35 и GPIO39, то есть
// на входных-только выводах. Для GPIO39 (как и GPIO36) документация
// Espressif прямо запрещает прерывания в нашей конфигурации:
//
//   «Please do not use the interrupt of GPIO36 and GPIO39 when using
//    ADC or Wi-Fi with sleep mode enabled»
//   (ESP-IDF, GPIO & RTC GPIO; подробности — ECO_and_Workarounds_for_
//    Bugs_in_ESP32, раздел 3.11)
//
// У нас выполняются оба условия сразу: Wi-Fi работает всегда, а ADC1
// с v6.2 используется датчиком дождя на GPIO36. Обходной путь через
// adc_power_acquire() стоит ~1 мА постоянно и не нужен — механизм
// опроса в модуле уже есть и обкатан на кнопках форточек.
//
// GPIO35 этой ошибкой не затронут (она про SENSOR_VP/SENSOR_VN), но
// обе кнопки сделаны одинаково: меньше разных механизмов — меньше
// поводов для сюрпризов.
static bool btnIsPolledToggle(uint8_t idx) {
    return !BTN_IS_HOLD[idx] && BTN_PIN_IS_INPUT_ONLY(BTN_PINS[idx]);
}

// Состояние опрашиваемых toggle-кнопок: срабатываем по фронту «отпущена →
// нажата», удерживание ничего не повторяет.
#define BTN_POLL_STABLE_COUNT 3
static bool    s_toggleHeld[NUM_BUTTONS]   = {};
static uint8_t s_toggleStable[NUM_BUTTONS] = {};

// ═══════════════════════════════════════════════════════════════════
//  ISR: только для toggle-кнопок (полив)
// ═══════════════════════════════════════════════════════════════════
// v6.2: uint16_t вместо uint8_t. Кнопок стало 7, в байт они ещё влезают,
// но следующая добавленная молча потерялась бы при сдвиге.
static std::atomic<uint16_t> s_events{0};
static volatile uint64_t     s_lastUs[NUM_BUTTONS] = {};

static void IRAM_ATTR btnISR(void* arg) {
    int idx = (int)(uintptr_t)arg;
    if (idx < 0 || idx >= NUM_BUTTONS) return;

    uint64_t now = (uint64_t)esp_timer_get_time();
    if (now - s_lastUs[idx] > (uint64_t)BTN_DEBOUNCE_US) {
        s_lastUs[idx] = now;
        s_events.fetch_or((uint16_t)(1u << idx), std::memory_order_relaxed);
    }
}

// ═══════════════════════════════════════════════════════════════════
//  ИНИЦИАЛИЗАЦИЯ
// ═══════════════════════════════════════════════════════════════════
void BtnMgr::init() {
    for (int i = 0; i < NUM_BUTTONS; i++) {
        // v6.2: GPIO34-39 не имеют внутренней подтяжки вообще, INPUT_PULLUP
        // для них — тихий no-op. Подтяжка там внешняя, 10 кОм на 3.3 В
        // (см. схему в config.h). Указываем просто INPUT, чтобы в коде было
        // видно, что подтяжки здесь нет и она не подразумевается.
        pinMode(BTN_PINS[i],
                BTN_PIN_IS_INPUT_ONLY(BTN_PINS[i]) ? INPUT : INPUT_PULLUP);

        if (!BTN_IS_HOLD[i] && !btnIsPolledToggle(i)) {
            // Toggle-кнопки на обычных выводах: ISR на FALLING + антидребезг
            attachInterruptArg(
                digitalPinToInterrupt(BTN_PINS[i]),
                btnISR,
                (void*)(uintptr_t)i,
                FALLING
            );
        }
        // Hold-кнопки и toggle на входных-только выводах: ISR не вешаем,
        // опрашиваем в processEvents().
    }

    // Стартовое состояние опрашиваемых toggle-кнопок, иначе первый же
    // проход увидел бы «переход» из выдуманного отпущенного состояния.
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (!btnIsPolledToggle(i)) continue;
        s_toggleHeld[i]   = (digitalRead(BTN_PINS[i]) == LOW);
        s_toggleStable[i] = BTN_POLL_STABLE_COUNT;
    }

    Serial.printf("[BTN] %d buttons initialized: 3 toggle ISR (irrigation), "
                  "2 hold polled (vents), 2 toggle polled (humid GPIO%d, "
                  "pump GPIO%d)\n",
                  NUM_BUTTONS, BTN_HUMID_PIN, BTN_PUMP_PIN);
}

// ═══════════════════════════════════════════════════════════════════
//  ОБРАБОТКА TOGGLE-КНОПОК (полив)
// ═══════════════════════════════════════════════════════════════════
static bool switchIrrigationToManual() {
    MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(10));
    if (!lock.locked()) return false;
    if (g_settings.modeTimers == 0) return false;
    g_settings.modeTimers = 0;
    settingsSaveDeferred();
    return true;
}

static void handleIrrigationButton(uint8_t btnIdx, uint8_t relayIdx) {
    if (switchIrrigationToManual()) {
        WsHandler::notifyModes();
        Serial.printf("[BTN%u] Irrigation switched to MANUAL\n", btnIdx);
    }

    uint8_t timerIdx = relayIdx - RELAY_IDX_IRR1;
    if (TimerSched::isActive(timerIdx)) {
        TimerSched::forceOff(timerIdx);
        WsHandler::notifyRelayChange(relayIdx);
        Serial.printf("[BTN%u] Timer%u force-stopped\n", btnIdx, timerIdx);
        return;
    }

    bool newState = !RelayMgr::getState(relayIdx);
    if (RelayMgr::setRelay(relayIdx, newState)) {
        WsHandler::notifyRelayChange(relayIdx);
        Serial.printf("[BTN%u] %s -> %s\n", btnIdx,
            RelayMgr::getName(relayIdx), newState ? "ON" : "OFF");
    }
}

// ═══════════════════════════════════════════════════════════════════
//  КНОПКИ УВЛАЖНИТЕЛЯ И НАСОСА (v6.2)
// ═══════════════════════════════════════════════════════════════════
// Работают как кнопки полива: нажал — включилось, нажал ещё раз —
// выключилось. Нажатие уводит подсистему в ручной режим, иначе
// автоматика вернула бы реле в своё состояние на следующем же тике,
// и кнопка выглядела бы неработающей.
static bool switchHumidToManual() {
    MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(10));
    if (!lock.locked()) return false;
    if (g_settings.modeHumid == 0) return false;
    g_settings.modeHumid = 0;
    settingsSaveDeferred();
    return true;
}

static bool switchPumpToManual() {
    MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(10));
    if (!lock.locked()) return false;
    if (g_settings.modeWaterPump == 0) return false;
    g_settings.modeWaterPump = 0;
    settingsSaveDeferred();
    return true;
}

static void handleHumidButton() {
    if (switchHumidToManual()) {
        WsHandler::notifyModes();
        Serial.println(F("[BTN] Humidifier switched to MANUAL"));
    }
    const bool newState = !RelayMgr::getState(RELAY_IDX_HUMID);
    if (RelayMgr::setRelay(RELAY_IDX_HUMID, newState)) {
        WsHandler::notifyRelayChange(RELAY_IDX_HUMID);
        Serial.printf("[BTN] %s -> %s\n",
            RelayMgr::getName(RELAY_IDX_HUMID), newState ? "ON" : "OFF");
    }
}

static void handlePumpButton() {
    if (switchPumpToManual()) {
        WsHandler::notifyModes();
        Serial.println(F("[BTN] Pump switched to MANUAL"));
    }
    // Авария насоса сбрасывается той же кнопкой: иначе из FAULT можно было
    // выйти только через панель или переждав час.
    if (WaterPumpCtrl::isInFault()) {
        WaterPumpCtrl::resetFault();
        Serial.println(F("[BTN] Pump FAULT reset by button"));
        return;
    }
    const bool newState = !RelayMgr::getState(RELAY_IDX_WATER_PUMP);
    if (RelayMgr::setRelay(RELAY_IDX_WATER_PUMP, newState)) {
        WsHandler::notifyRelayChange(RELAY_IDX_WATER_PUMP);
        Serial.printf("[BTN] %s -> %s\n",
            RelayMgr::getName(RELAY_IDX_WATER_PUMP), newState ? "ON" : "OFF");
    }
}

// Опрос toggle-кнопок на входных-только выводах. Срабатывание — по фронту
// нажатия; отпускание только снимает флаг.
static void pollToggleButtons() {
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        if (!btnIsPolledToggle(i)) continue;

        const bool pressed = (digitalRead(BTN_PINS[i]) == LOW);
        if (pressed == s_toggleHeld[i]) {
            s_toggleStable[i] = 0;
            continue;
        }
        if (++s_toggleStable[i] < BTN_POLL_STABLE_COUNT) continue;

        s_toggleStable[i] = 0;
        s_toggleHeld[i]   = pressed;
        if (!pressed) continue;          // реагируем только на нажатие

        if (i == BTN_IDX_HUMID)     handleHumidButton();
        else if (i == BTN_IDX_PUMP) handlePumpButton();
    }
}

// ═══════════════════════════════════════════════════════════════════
//  ОБРАБОТКА HOLD-КНОПОК (форточки) — опрос каждые 50 мс
// ═══════════════════════════════════════════════════════════════════
// Принцип:
//   • Читаем GPIO напрямую: LOW = нажата, HIGH = отпущена.
//   • Антидребезг через "стабильность последних N показаний" (3 подряд).
//   • При смене состояния — отправляем команду в VentCtrl.
//
// Важно: ESP32 кнопки могут "звенеть" 1-3 мс при нажатии.
// При опросе раз в 50 мс — этот звон уже физически закончен.
static bool switchVentsToManual() {
    // v6.1: onModeChanged() вызываем ПОСЛЕ освобождения лока — он
    // останавливает моторы и пишет позицию в NVS, это долго.
    {
        MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(10));
        if (!lock.locked()) return false;
        if (g_settings.modeVents == 0) return false;
        g_settings.modeVents = 0;
        settingsSaveDeferred();
    }
    VentCtrl::onModeChanged(false);
    return true;
}

// Прошлое состояние hold-кнопок (для определения "только что отпустили")
static bool s_holdStateOpen  = false;   // true = была нажата
static bool s_holdStateClose = false;

// Счётчики стабильности (антидребезг)
static uint8_t s_stableOpen  = 0;
static uint8_t s_stableClose = 0;
static const uint8_t HOLD_STABLE_NEEDED = 2;   // 2*50мс = 100мс на дребезг

static void pollHoldButtons() {
    bool nowOpen  = (digitalRead(BTN_VENT_OPEN_PIN)  == LOW);
    bool nowClose = (digitalRead(BTN_VENT_CLOSE_PIN) == LOW);

    // Антидребезг для OPEN
    if (nowOpen != s_holdStateOpen) {
        s_stableOpen++;
        if (s_stableOpen >= HOLD_STABLE_NEEDED) {
            s_holdStateOpen = nowOpen;
            s_stableOpen    = 0;
            // Изменение состояния — отправляем команду
            if (nowOpen) {
                if (switchVentsToManual()) WsHandler::notifyModes();
                Serial.println(F("[BTN] OPEN button pressed"));
                VentCtrl::manualSetCmd(VMC_OPEN);
            } else {
                Serial.println(F("[BTN] OPEN button released"));
                VentCtrl::manualSetCmd(VMC_NONE);
            }
        }
    } else {
        s_stableOpen = 0;
    }

    // Антидребезг для CLOSE
    if (nowClose != s_holdStateClose) {
        s_stableClose++;
        if (s_stableClose >= HOLD_STABLE_NEEDED) {
            s_holdStateClose = nowClose;
            s_stableClose    = 0;
            if (nowClose) {
                if (switchVentsToManual()) WsHandler::notifyModes();
                Serial.println(F("[BTN] CLOSE button pressed"));
                VentCtrl::manualSetCmd(VMC_CLOSE);
            } else {
                Serial.println(F("[BTN] CLOSE button released"));
                VentCtrl::manualSetCmd(VMC_NONE);
            }
        }
    } else {
        s_stableClose = 0;
    }

    // Защита: если обе зажаты — игнорируем (NONE)
    if (s_holdStateOpen && s_holdStateClose) {
        VentCtrl::manualSetCmd(VMC_NONE);
    }
}

// ═══════════════════════════════════════════════════════════════════
//  ГЛАВНЫЙ ЦИКЛ
// ═══════════════════════════════════════════════════════════════════
// Вызывается из task Core1 примерно каждые 50 мс.
void BtnMgr::processEvents() {
    // 1) Toggle-кнопки (полив)
    uint8_t events = s_events.exchange(0, std::memory_order_acq_rel);
    if (events) {
        for (int i = 0; i < NUM_BUTTONS; i++) {
            if (!(events & (1u << i)) || BTN_IS_HOLD[i]) continue;

            // Повторная проверка: кнопка всё ещё нажата?
            if (digitalRead(BTN_PINS[i]) != LOW) {
                Serial.printf("[BTN%d] False trigger ignored\n", i);
                continue;
            }
            // На ISR сидят только кнопки полива (индексы 0..2) — остальные
            // toggle опрашиваются, см. btnIsPolledToggle().
            if (btnIsPolledToggle(i)) continue;
            uint8_t relayIdx = RELAY_IDX_IRR1 + i;
            handleIrrigationButton(i, relayIdx);
        }
    }

    // 2) Hold-кнопки (форточки) — опрос напрямую
    pollHoldButtons();

    // 3) Toggle-кнопки на входных-только выводах (увлажнитель, насос) — v6.2
    pollToggleButtons();
}
