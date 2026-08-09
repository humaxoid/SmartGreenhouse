// ╔══════════════════════════════════════════════════════════════════╗
// ║  water_pump_controller.cpp — Реализация контроллера насоса v5.0║
// ║                                                                  ║
// ║  Машина состояний:                                                ║
// ║   IDLE ──[level<=low, sensor OK]──▶ PUMPING                      ║
// ║   PUMPING ──[level>=high]──────────▶ IDLE                        ║
// ║   PUMPING ──[elapsed>timeoutMin]───▶ FAULT (авария)              ║
// ║   FAULT ──[1 час или ручной сброс]▶ IDLE                         ║
// ║                                                                  ║
// ║  Аварийные остановки:                                            ║
// ║   • Таймаут непрерывной работы (скорее всего нет воды в скважине)║
// ║   • NaN датчика уровня (не можем контролировать — выключаем)     ║
// ╚══════════════════════════════════════════════════════════════════╝

#include "water_pump_controller.h"
#include "settings.h"
#include "relay_manager.h"
#include "ws_handler.h"
#include "config.h"
#include <Arduino.h>
#include <math.h>

// ═══════════════════════════════════════════════════════════════════
//  ВНУТРЕННЕЕ СОСТОЯНИЕ
// ═══════════════════════════════════════════════════════════════════
enum PumpState : uint8_t {
    PUMP_IDLE    = 0,   // Ждём срабатывания по низкому уровню
    PUMP_PUMPING = 1,   // Насос работает
    PUMP_FAULT   = 2    // Авария (превышен таймаут)
};

static PumpState s_state           = PUMP_IDLE;
static uint64_t  s_pumpStartMs     = 0;   // Время включения насоса
static uint64_t  s_faultStartMs    = 0;   // Время входа в FAULT
static bool      s_sensorFailed    = false;
static uint64_t  s_lastDbgMs       = 0;

// Время авто-сброса аварии, мс (1 час)
#define FAULT_AUTO_RESET_MS   (60ULL * 60ULL * 1000ULL)

// ═══════════════════════════════════════════════════════════════════
//  ВНУТРЕННИЕ ФУНКЦИИ
// ═══════════════════════════════════════════════════════════════════

// Принудительное выключение реле насоса (защита, авария)
static void forceOff(const char* reason) {
    if (RelayMgr::getState(RELAY_IDX_WATER_PUMP)) {
        if (RelayMgr::setRelay(RELAY_IDX_WATER_PUMP, false)) {
            Serial.printf("[PUMP] Force OFF: %s\n", reason);
            WsHandler::notifyRelayChange(RELAY_IDX_WATER_PUMP);
        }
    }
}

// Переход в состояние FAULT
static void enterFault(uint64_t nowMs, const char* reason) {
    if (s_state == PUMP_FAULT) return;   // уже в аварии
    s_state        = PUMP_FAULT;
    s_faultStartMs = nowMs;
    forceOff(reason);
    Serial.printf("[PUMP] FAULT: %s (auto-reset in 1 hour)\n", reason);
}

// ═══════════════════════════════════════════════════════════════════
//  ОСНОВНОЙ ЦИКЛ
// ═══════════════════════════════════════════════════════════════════
void WaterPumpCtrl::update(float waterLevelPct) {
    const uint64_t nowMs = steadyMs();

    // ── Читаем настройки и режим под мьютексом ───────────────────
    uint8_t  mode;
    uint8_t  lowPct, highPct;
    uint16_t timeoutMin;
    {
        MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(20));
        if (!lock.locked()) return;
        mode       = g_settings.modeWaterPump;
        lowPct     = g_settings.waterTank.lowPct;
        highPct    = g_settings.waterTank.highPct;
        timeoutMin = g_settings.waterTank.pumpTimeoutMin;
    }

    // ── Ручной режим: авто-логика не работает, но FAULT обрабатываем
    if (mode == 0) {
        // В ручном режиме пользователь сам включает/выключает насос.
        // Но если он забыл — защиту таймаута всё равно применяем.
        if (RelayMgr::getState(RELAY_IDX_WATER_PUMP)) {
            // Если насос ВКЛ и мы ещё не начинали отсчёт — начинаем
            if (s_state != PUMP_PUMPING) {
                s_state        = PUMP_PUMPING;
                s_pumpStartMs  = nowMs;
            }
            // Проверяем таймаут
            uint64_t elapsed = nowMs - s_pumpStartMs;
            if (elapsed >= (uint64_t)timeoutMin * 60ULL * 1000ULL) {
                enterFault(nowMs, "manual mode timeout");
            }
        } else {
            // Насос ВЫКЛ — возврат в IDLE
            if (s_state == PUMP_PUMPING) {
                s_state = PUMP_IDLE;
            }
        }
        return;
    }

    // ── Датчик отказал — принудительное выключение ───────────────
    if (isnan(waterLevelPct)) {
        if (!s_sensorFailed) {
            s_sensorFailed = true;
            Serial.println(F("[PUMP] Water sensor NaN — forcing OFF"));
        }
        forceOff("water sensor NaN");
        // Остаёмся в текущем состоянии (не переходим в FAULT —
        // это временная недоступность датчика, а не реальная авария)
        return;
    }
    if (s_sensorFailed) {
        s_sensorFailed = false;
        Serial.printf("[PUMP] Water sensor recovered: %.1f%%\n", waterLevelPct);
    }

    // ── Проверка корректности порогов ────────────────────────────
    if (lowPct >= highPct) {
        forceOff("invalid thresholds");
        return;
    }

    // ── Машина состояний ─────────────────────────────────────────
    switch (s_state) {
        case PUMP_IDLE: {
            // Запустить насос при низком уровне
            if (waterLevelPct <= (float)lowPct) {
                if (RelayMgr::setRelay(RELAY_IDX_WATER_PUMP, true)) {
                    s_state       = PUMP_PUMPING;
                    s_pumpStartMs = nowMs;
                    Serial.printf("[PUMP] START: level=%.1f%% <= %u%%\n",
                        waterLevelPct, (unsigned)lowPct);
                    WsHandler::notifyRelayChange(RELAY_IDX_WATER_PUMP);
                }
            }
            break;
        }

        case PUMP_PUMPING: {
            // Наполнили до highPct — стоп
            if (waterLevelPct >= (float)highPct) {
                if (RelayMgr::setRelay(RELAY_IDX_WATER_PUMP, false)) {
                    s_state = PUMP_IDLE;
                    uint64_t elapsed = nowMs - s_pumpStartMs;
                    Serial.printf("[PUMP] DONE: level=%.1f%% >= %u%% (ran %llu sec)\n",
                        waterLevelPct, (unsigned)highPct,
                        (unsigned long long)(elapsed / 1000ULL));
                    WsHandler::notifyRelayChange(RELAY_IDX_WATER_PUMP);
                }
                break;
            }

            // Проверка таймаута
            uint64_t elapsed = nowMs - s_pumpStartMs;
            if (elapsed >= (uint64_t)timeoutMin * 60ULL * 1000ULL) {
                enterFault(nowMs, "pump timeout (no water or clog?)");
            }
            break;
        }

        case PUMP_FAULT: {
            // Выход из FAULT — либо ручной сброс, либо по таймауту 1 час
            if (nowMs - s_faultStartMs >= FAULT_AUTO_RESET_MS) {
                Serial.println(F("[PUMP] FAULT auto-reset after 1 hour"));
                s_state = PUMP_IDLE;
            }
            // В FAULT реле гарантированно OFF (forceOff было в enterFault)
            break;
        }
    }

    // ── Отладочный вывод раз в 30 сек ────────────────────────────
    if (nowMs - s_lastDbgMs >= 30000ULL) {
        s_lastDbgMs = nowMs;
        const char* stateStr = (s_state == PUMP_IDLE) ? "IDLE"
                             : (s_state == PUMP_PUMPING) ? "PUMPING" : "FAULT";
        Serial.printf("[PUMP] level=%.1f%% target=[%u..%u] state=%s\n",
            waterLevelPct, (unsigned)lowPct, (unsigned)highPct, stateStr);
    }
}

bool WaterPumpCtrl::isInFault() {
    return s_state == PUMP_FAULT;
}

void WaterPumpCtrl::resetFault() {
    if (s_state == PUMP_FAULT) {
        s_state = PUMP_IDLE;
        Serial.println(F("[PUMP] FAULT manually reset"));
    }
}
