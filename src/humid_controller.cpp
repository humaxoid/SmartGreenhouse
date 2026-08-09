// ╔══════════════════════════════════════════════════════════════════╗
// ║  humid_controller.cpp — Реализация контроллера увлажнителя v5.0║
// ║                                                                  ║
// ║  Изменения v5.0:                                                 ║
// ║   • Усиленная проверка NaN (безопасное выключение при отказе)   ║
// ║   • Детектирование fail-to-on переходов с логированием          ║
// ║   • Работа через новый RelayMgr API                             ║
// ╚══════════════════════════════════════════════════════════════════╝

#include "humid_controller.h"
#include "settings.h"
#include "relay_manager.h"
#include "ws_handler.h"
#include <Arduino.h>
#include <math.h>

// ═══════════════════════════════════════════════════════════════════
//  ВНУТРЕННЕЕ СОСТОЯНИЕ
// ═══════════════════════════════════════════════════════════════════

// Отладочный таймер — чтобы не засорять Serial каждым циклом
static uint64_t s_lastDbgMs = 0;

// Флаг "датчик сейчас в отказе" — для log-а только при переходе
static bool s_sensorFailed = false;

// ═══════════════════════════════════════════════════════════════════
//  ОСНОВНОЙ ЦИКЛ
// ═══════════════════════════════════════════════════════════════════
void HumidCtrl::update(float humidity) {
    // ── Читаем режим и пороги под коротким мьютексом ─────────────
    uint8_t mode;
    float   humidLow, humidHigh;
    {
        MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(20));
        if (!lock.locked()) return;
        mode       = g_settings.modeHumid;
        humidLow   = g_settings.humidLow;
        humidHigh  = g_settings.humidHigh;
    }

    // ── Ручной режим — автоматика выключена ──────────────────────
    if (mode == 0) {
        // Но если датчик вернулся после отказа — сбрасываем флаг
        if (s_sensorFailed && !isnan(humidity)) {
            s_sensorFailed = false;
        }
        return;
    }

    // ── Датчик отказал — безопасное выключение ───────────────────
    if (isnan(humidity)) {
        if (!s_sensorFailed) {
            s_sensorFailed = true;
            Serial.println(F("[HUMID] Sensor NaN — forcing OFF"));
        }
        // Принудительное выключение реле, если было включено
        if (RelayMgr::getState(RELAY_IDX_HUMID)) {
            if (RelayMgr::setRelay(RELAY_IDX_HUMID, false)) {
                WsHandler::notifyRelayChange(RELAY_IDX_HUMID);
            }
        }
        return;
    }

    // ── Датчик вернулся после отказа ─────────────────────────────
    if (s_sensorFailed) {
        s_sensorFailed = false;
        Serial.printf("[HUMID] Sensor recovered: H=%.1f%%\n", humidity);
    }

    // ── Проверка корректности порогов ────────────────────────────
    // humidLow должен быть меньше humidHigh, иначе настройки битые
    if (humidLow >= humidHigh) {
        // Безопасное поведение: выключить реле
        if (RelayMgr::getState(RELAY_IDX_HUMID)) {
            Serial.printf("[HUMID] Invalid thresholds (low=%.1f >= high=%.1f)\n",
                humidLow, humidHigh);
            RelayMgr::setRelay(RELAY_IDX_HUMID, false);
        }
        return;
    }

    // ── Гистерезис ───────────────────────────────────────────────
    bool currentState = RelayMgr::getState(RELAY_IDX_HUMID);
    bool newState     = currentState;

    if (humidity <= humidLow) {
        newState = true;   // включить увлажнитель
    } else if (humidity >= humidHigh) {
        newState = false;  // выключить, цель достигнута
    }
    // В промежуточной зоне — сохраняем текущее состояние

    // ── Применяем если изменилось ────────────────────────────────
    if (newState != currentState) {
        if (RelayMgr::setRelay(RELAY_IDX_HUMID, newState)) {
            Serial.printf("[HUMID] Auto: H=%.1f%% [%.1f..%.1f] -> %s\n",
                humidity, humidLow, humidHigh,
                newState ? "ON" : "OFF");
            WsHandler::notifyRelayChange(RELAY_IDX_HUMID);
        }
        // Если setRelay вернул false — сработал антидребезг. Повторим
        // попытку на следующем цикле, ничего страшного не произойдёт.
    }

    // ── Отладочный вывод раз в 30 сек ────────────────────────────
    uint64_t now = steadyMs();
    if (now - s_lastDbgMs >= 30000ULL) {
        s_lastDbgMs = now;
        Serial.printf("[HUMID] H=%.1f%% target=[%.1f..%.1f] relay=%s\n",
            humidity, humidLow, humidHigh, currentState ? "ON" : "OFF");
    }
}
