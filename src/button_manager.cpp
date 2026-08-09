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
    BTN_VENT_CLOSE_PIN    // 4: Форточки ЗАКРЫТЬ → GPIO19 (hold)
};

// Какие кнопки работают в hold-режиме (опрос каждые 50 мс)
// Для них ISR не нужен — их состояние читаем напрямую.
static const bool BTN_IS_HOLD[NUM_BUTTONS] = {
    false, false, false,   // полив — toggle
    true,  true            // форточки — hold
};

// ═══════════════════════════════════════════════════════════════════
//  ISR: только для toggle-кнопок (полив)
// ═══════════════════════════════════════════════════════════════════
static std::atomic<uint8_t> s_events{0};
static volatile uint64_t    s_lastUs[NUM_BUTTONS] = {};

static void IRAM_ATTR btnISR(void* arg) {
    int idx = (int)(uintptr_t)arg;
    if (idx < 0 || idx >= NUM_BUTTONS) return;

    uint64_t now = (uint64_t)esp_timer_get_time();
    if (now - s_lastUs[idx] > (uint64_t)BTN_DEBOUNCE_US) {
        s_lastUs[idx] = now;
        s_events.fetch_or((uint8_t)(1u << idx), std::memory_order_relaxed);
    }
}

// ═══════════════════════════════════════════════════════════════════
//  ИНИЦИАЛИЗАЦИЯ
// ═══════════════════════════════════════════════════════════════════
void BtnMgr::init() {
    for (int i = 0; i < NUM_BUTTONS; i++) {
        pinMode(BTN_PINS[i], INPUT_PULLUP);

        if (!BTN_IS_HOLD[i]) {
            // Toggle-кнопки: ISR на FALLING + аппаратный антидребезг
            attachInterruptArg(
                digitalPinToInterrupt(BTN_PINS[i]),
                btnISR,
                (void*)(uintptr_t)i,
                FALLING
            );
        }
        // Hold-кнопки: ISR не вешаем, опрашиваем в processEvents()
    }
    Serial.printf("[BTN] %d buttons initialized: 3 toggle (irrigation), "
                  "2 hold (vents OPEN/CLOSE)\n", NUM_BUTTONS);
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
            // Все toggle-кнопки сейчас — это полив (индексы 0..2)
            uint8_t relayIdx = RELAY_IDX_IRR1 + i;
            handleIrrigationButton(i, relayIdx);
        }
    }

    // 2) Hold-кнопки (форточки) — опрос напрямую
    pollHoldButtons();
}
