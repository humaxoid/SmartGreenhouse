// ╔══════════════════════════════════════════════════════════════════╗
// ║  vent_controller.cpp — Управление форточками v6.0               ║
// ║                                                                  ║
// ║  Архитектура:                                                    ║
// ║   • Внутреннее состояние: топ/боттом позиция (0..100%),         ║
// ║     направление каждого мотора, время начала движения.           ║
// ║   • Ручной режим:                                                ║
// ║       VMC_OPEN  → крутит верх до 100%, потом эстафета на низ;   ║
// ║                   при отпускании — мгновенный стоп активного.   ║
// ║       VMC_CLOSE → зеркально (низ → верх).                       ║
// ║   • Авто режим:                                                  ║
// ║       КАА с 7 состояниями (см. vent_controller.h).               ║
// ║       Ступени по 20%, паузы STEP_DELAY_MIN.                     ║
// ║   • Дождь: абсолютный приоритет, прерывает любую логику.        ║
// ║   • NVS: сохраняем только 0% и 100% (полные положения).         ║
// ║                                                                  ║
// ║  ВАЖНО: вызывается из задачи Core1, никаких блокирующих delay.  ║
// ╚══════════════════════════════════════════════════════════════════╝
#include "vent_controller.h"
#include "relay_manager.h"
#include "settings.h"
#include "config.h"
#include <math.h>

// ─── Внутренние типы ─────────────────────────────────────────────
// Какая форточка задействована
enum VentSide : uint8_t { VS_TOP = 0, VS_BOTTOM = 1 };

// Направление одного мотора
enum MotorDir : int8_t { MD_CLOSE = -1, MD_STOP = 0, MD_OPEN = +1 };

// ─── Состояние одной форточки ────────────────────────────────────
struct VentLeaf {
    float    pct;             // Текущая позиция 0.0..100.0
    MotorDir dir;             // Направление мотора
    uint64_t moveStartMs;     // Когда начал движение (steadyMs)
    float    pctAtMoveStart;  // Позиция в момент старта движения
    uint32_t calibMs;         // Время полного хода 0→100, мс (v6.1: uint16 → uint32)
    bool     valid;           // Позиция достоверна (был хотя бы один upper/lower bound)
};

// ─── Глобальное состояние модуля ─────────────────────────────────
static VentLeaf s_top    = {0.0f, MD_STOP, 0, 0.0f, TOP_CALIB_SEC * 1000U, false};
static VentLeaf s_bottom = {0.0f, MD_STOP, 0, 0.0f, BOTTOM_CALIB_SEC * 1000U, false};

// Текущая команда ручного режима
static VentManualCmd s_manualCmd       = VMC_NONE;
static uint64_t      s_manualCmdSetMs  = 0;     // Когда нажали кнопку
static bool          s_manualHandoff   = false; // Прошёл break-before-make между моторами

// Автомат авто-режима
static VentAutoState s_autoState   = VAS_IDLE;
static uint64_t      s_autoStateMs = 0;       // Когда вошли в текущее состояние
static float         s_tempBeforePause = NAN; // Температура до паузы (для решения о ступени)
static VentSide      s_autoActiveSide = VS_TOP; // Какая форточка двигается в текущей ступени
static int8_t        s_autoStepDir    = +1;     // +1 открываем, -1 закрываем

// Защита от спама в Serial (логируем смену состояния)
static VentAutoState s_lastLoggedState = (VentAutoState)0xFF;

// Дождь
static bool s_rainActive = false;

// ─── Калибровочный прогон (v6.1) ─────────────────────────────────
// Активен для одной створки за раз. Пока идёт, обычная логика режима
// для этой створки не работает: мотор держится на закрытие до истечения
// deadline, затем позиция обнуляется.
static bool     s_homingActive = false;
static VentSide s_homingSide   = VS_TOP;
static uint64_t s_homingEndMs  = 0;

// ═══════════════════════════════════════════════════════════════════
//  УТИЛИТЫ
// ═══════════════════════════════════════════════════════════════════
static const char* stateName(VentAutoState s) {
    switch (s) {
        case VAS_IDLE:              return "IDLE";
        case VAS_OPEN_TOP_STEP:     return "OPEN_TOP_STEP";
        case VAS_OPEN_BOTTOM_STEP:  return "OPEN_BOTTOM_STEP";
        case VAS_CLOSE_TOP_STEP:    return "CLOSE_TOP_STEP";
        case VAS_CLOSE_BOTTOM_STEP: return "CLOSE_BOTTOM_STEP";
        case VAS_PAUSE_WAIT:        return "PAUSE_WAIT";
        case VAS_RAIN_CLOSING:      return "RAIN_CLOSING";
    }
    return "?";
}

static const char* cmdName(VentManualCmd c) {
    switch (c) {
        case VMC_NONE:  return "NONE";
        case VMC_OPEN:  return "OPEN";
        case VMC_CLOSE: return "CLOSE";
    }
    return "?";
}

// v6.1: Сохраняет ЛЮБУЮ позицию форточки в NVS.
// Вызывается ТОЛЬКО при фактической остановке мотора (setMotorDir → MD_STOP),
// что даёт ~10-20 записей в день максимум при активном использовании.
// Защита flash от износа: при удержании кнопки 30 сек — это 1 запись (на стопе),
// а не 6 (как было при сохранении в updateMovingPct каждые 5 сек).
static void persistVentPosition(VentSide side, float pct) {
    uint8_t pctU = (uint8_t)(pct + 0.5f);
    if (pctU > 100) pctU = 100;

    MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(50));
    if (!lock.locked()) return;

    VentPersistent& pp = (side == VS_TOP)
        ? g_settings.ventUpperPos
        : g_settings.ventLowerPos;
    if (pp.lastKnownPct == pctU && pp.positionValid) return;
    pp.lastKnownPct  = pctU;
    pp.positionValid = 1;
    settingsSaveDeferred();
    Serial.printf("[VENT] Saved %s position: %u%% to NVS\n",
        side == VS_TOP ? "top" : "bottom", (unsigned)pctU);
}

// Загрузить позиции из NVS
static void loadPositionsFromNVS() {
    MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(100));
    if (!lock.locked()) return;

    if (g_settings.ventUpperPos.positionValid) {
        s_top.pct   = (float)g_settings.ventUpperPos.lastKnownPct;
        s_top.valid = true;
    } else {
        s_top.pct   = 0.0f;
        s_top.valid = false;
    }
    if (g_settings.ventLowerPos.positionValid) {
        s_bottom.pct   = (float)g_settings.ventLowerPos.lastKnownPct;
        s_bottom.valid = true;
    } else {
        s_bottom.pct   = 0.0f;
        s_bottom.valid = false;
    }
    s_top.calibMs    = g_settings.ventUpperCal.travelMs;
    if (s_top.calibMs    == 0) s_top.calibMs    = TOP_CALIB_SEC * 1000U;
    s_bottom.calibMs = g_settings.ventLowerCal.travelMs;
    if (s_bottom.calibMs == 0) s_bottom.calibMs = BOTTOM_CALIB_SEC * 1000U;
}

// ═══════════════════════════════════════════════════════════════════
//  УПРАВЛЕНИЕ РЕЛЕ
// ═══════════════════════════════════════════════════════════════════
// Аккуратное переключение моторов с защитой H-bridge.
// Никогда не включаем OPEN+CLOSE одновременно для одной форточки.
static void setMotorDir(VentSide side, MotorDir newDir, uint64_t nowMs) {
    VentLeaf& leaf = (side == VS_TOP) ? s_top : s_bottom;
    if (leaf.dir == newDir) return;  // ничего не меняется

    uint8_t openIdx  = (side == VS_TOP) ? RELAY_IDX_VENT_UPPER_OPEN  : RELAY_IDX_VENT_LOWER_OPEN;
    uint8_t closeIdx = (side == VS_TOP) ? RELAY_IDX_VENT_UPPER_CLOSE : RELAY_IDX_VENT_LOWER_CLOSE;

    // Сначала ВЫКЛЮЧАЕМ всё (break-before-make)
    RelayMgr::setRelayDirect(openIdx,  false);
    RelayMgr::setRelayDirect(closeIdx, false);

    // Если был движущийся мотор — обновляем pct по фактическому ходу
    if (leaf.dir != MD_STOP && leaf.calibMs > 0) {
        uint64_t elapsed = nowMs - leaf.moveStartMs;
        float deltaPct = (float)elapsed * 100.0f / (float)leaf.calibMs;
        if (leaf.dir == MD_OPEN)  leaf.pct = leaf.pctAtMoveStart + deltaPct;
        if (leaf.dir == MD_CLOSE) leaf.pct = leaf.pctAtMoveStart - deltaPct;
        if (leaf.pct < 0.0f)   leaf.pct = 0.0f;
        if (leaf.pct > 100.0f) leaf.pct = 100.0f;
        persistVentPosition(side, leaf.pct);
    }

    // Включаем нужное направление
    if (newDir == MD_OPEN) {
        RelayMgr::setRelayDirect(openIdx, true);
    } else if (newDir == MD_CLOSE) {
        RelayMgr::setRelayDirect(closeIdx, true);
    }

    leaf.dir            = newDir;
    leaf.moveStartMs    = nowMs;
    leaf.pctAtMoveStart = leaf.pct;

    Serial.printf("[VENT] %s motor: %s (pct=%.1f)\n",
        side == VS_TOP ? "TOP" : "BOTTOM",
        newDir == MD_OPEN ? "OPEN" : (newDir == MD_CLOSE ? "CLOSE" : "STOP"),
        leaf.pct);
}

// Принудительный стоп обоих
static void stopAllMotors(uint64_t nowMs) {
    setMotorDir(VS_TOP,    MD_STOP, nowMs);
    setMotorDir(VS_BOTTOM, MD_STOP, nowMs);
}

// Обновить позицию движущегося мотора (без переключения реле)
static void updateMovingPct(VentSide side, uint64_t nowMs) {
    VentLeaf& leaf = (side == VS_TOP) ? s_top : s_bottom;
    if (leaf.dir == MD_STOP || leaf.calibMs == 0) return;

    uint64_t elapsed = nowMs - leaf.moveStartMs;
    float deltaPct = (float)elapsed * 100.0f / (float)leaf.calibMs;
    float newPct = leaf.pctAtMoveStart + (leaf.dir == MD_OPEN ? deltaPct : -deltaPct);
    if (newPct < 0.0f)   newPct = 0.0f;
    if (newPct > 100.0f) newPct = 100.0f;
    leaf.pct = newPct;
    // v6.1: НЕ сохраняем в NVS при движении (износ flash). Сохранение только
    // в setMotorDir() когда мотор фактически останавливается.
}

// ═══════════════════════════════════════════════════════════════════
//  РУЧНОЙ РЕЖИМ
// ═══════════════════════════════════════════════════════════════════
// Логика "Вариант Б": зажал — крутит, отпустил — стоп.
// При достижении 100% (открытие) или 0% (закрытие) активной форточки —
// эстафета: переключаемся на парную форточку через паузу
// MANUAL_RELAY_HANDOFF_MS.
//
// VMC_OPEN:
//   1. Если top.pct < 100 → крутим top OPEN
//   2. Если top.pct >= 100 → стоп top, пауза, потом bottom OPEN
//      (если bottom.pct < 100)
//
// VMC_CLOSE:
//   зеркально (низ→верх).
static void processManual(uint64_t nowMs) {
    // Защита от зависания: если кнопка "удержание" длится слишком долго
    if (s_manualCmd != VMC_NONE) {
        if (nowMs - s_manualCmdSetMs > MANUAL_HOLD_TIMEOUT_MS) {
            Serial.println(F("[VENT] Manual hold TIMEOUT — auto stop"));
            s_manualCmd = VMC_NONE;
            stopAllMotors(nowMs);
            return;
        }
    }

    if (s_manualCmd == VMC_NONE) {
        // Кнопка отпущена — стопим всё
        stopAllMotors(nowMs);
        s_manualHandoff = false;
        return;
    }

    // ── Дождь блокирует открытие ────────────────────────────────
    if (s_rainActive && s_manualCmd == VMC_OPEN) {
        stopAllMotors(nowMs);
        return;
    }

    // ── Обновим текущие позиции ─────────────────────────────────
    updateMovingPct(VS_TOP,    nowMs);
    updateMovingPct(VS_BOTTOM, nowMs);

    if (s_manualCmd == VMC_OPEN) {
        // Эстафета верх → низ
        if (s_top.pct < 99.5f) {
            // Крутим верх. Если работает нижний — стоп.
            if (s_bottom.dir != MD_STOP) setMotorDir(VS_BOTTOM, MD_STOP, nowMs);
            if (s_top.dir != MD_OPEN) {
                if (s_manualHandoff) { vTaskDelay(pdMS_TO_TICKS(MANUAL_RELAY_HANDOFF_MS)); s_manualHandoff = false; }
                setMotorDir(VS_TOP, MD_OPEN, nowMs);
            }
        } else {
            // Верх 100% — эстафета на низ
            if (s_top.dir != MD_STOP) {
                setMotorDir(VS_TOP, MD_STOP, nowMs);
                s_manualHandoff = true;
                return;  // на следующем тике запустим bottom
            }
            if (s_bottom.pct < 99.5f) {
                if (s_bottom.dir != MD_OPEN) {
                    if (s_manualHandoff) {
                        vTaskDelay(pdMS_TO_TICKS(MANUAL_RELAY_HANDOFF_MS));
                        s_manualHandoff = false;
                    }
                    setMotorDir(VS_BOTTOM, MD_OPEN, nowMs);
                }
            } else {
                // И низ 100% — больше открывать нечего
                if (s_bottom.dir != MD_STOP) setMotorDir(VS_BOTTOM, MD_STOP, nowMs);
            }
        }
    } else if (s_manualCmd == VMC_CLOSE) {
        // Эстафета низ → верх
        if (s_bottom.pct > 0.5f) {
            if (s_top.dir != MD_STOP) setMotorDir(VS_TOP, MD_STOP, nowMs);
            if (s_bottom.dir != MD_CLOSE) {
                if (s_manualHandoff) { vTaskDelay(pdMS_TO_TICKS(MANUAL_RELAY_HANDOFF_MS)); s_manualHandoff = false; }
                setMotorDir(VS_BOTTOM, MD_CLOSE, nowMs);
            }
        } else {
            if (s_bottom.dir != MD_STOP) {
                setMotorDir(VS_BOTTOM, MD_STOP, nowMs);
                s_manualHandoff = true;
                return;
            }
            if (s_top.pct > 0.5f) {
                if (s_top.dir != MD_CLOSE) {
                    if (s_manualHandoff) {
                        vTaskDelay(pdMS_TO_TICKS(MANUAL_RELAY_HANDOFF_MS));
                        s_manualHandoff = false;
                    }
                    setMotorDir(VS_TOP, MD_CLOSE, nowMs);
                }
            } else {
                if (s_top.dir != MD_STOP) setMotorDir(VS_TOP, MD_STOP, nowMs);
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
//  АВТО-РЕЖИМ
// ═══════════════════════════════════════════════════════════════════
// Автомат с 7 состояниями (см. enum VentAutoState).
//
//  Открытие:  IDLE → (T > T_OPEN) → OPEN_TOP_STEP → PAUSE_WAIT →
//             (если T выросла: следующая ступень верха или эстафета на низ;
//              иначе: IDLE)
//
//  Закрытие:  IDLE → (T < T_CLOSE) → CLOSE_BOTTOM_STEP → PAUSE_WAIT →
//             (то же самое в обратном порядке)
//
//  Дождь:     любое состояние → RAIN_CLOSING (моментально 0%, 0%) → IDLE
static void setAutoState(VentAutoState newSt, uint64_t nowMs) {
    if (s_autoState == newSt) return;
    s_autoState   = newSt;
    s_autoStateMs = nowMs;
    if (s_lastLoggedState != newSt) {
        Serial.printf("[VENT-AUTO] state -> %s (top=%u%% bot=%u%%)\n",
            stateName(newSt),
            (unsigned)(s_top.pct + 0.5f),
            (unsigned)(s_bottom.pct + 0.5f));
        s_lastLoggedState = newSt;
    }
}

// Запустить движение мотора на одну ступень (20%).
// Длительность: calibMs * 0.2.
static void startStep(VentSide side, int8_t dir, uint64_t nowMs) {
    setMotorDir(side, dir > 0 ? MD_OPEN : MD_CLOSE, nowMs);
    s_autoActiveSide = side;
    s_autoStepDir    = dir;
}

// Проверить — закончилась ли ступень (мотор отработал нужное время)
static bool stepCompleted(uint64_t nowMs) {
    VentLeaf& leaf = (s_autoActiveSide == VS_TOP) ? s_top : s_bottom;
    if (leaf.dir == MD_STOP) return true;
    uint64_t elapsed = nowMs - leaf.moveStartMs;
    uint32_t needMs  = (uint32_t)leaf.calibMs * VENT_AUTO_STEP_PCT / 100U;
    return elapsed >= needMs;
}

static void processAuto(uint64_t nowMs, float temp) {
    // Получаем актуальные пороги из настроек
    float openHigh = 29.0f, closeLow = 24.0f;
    {
        MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(20));
        if (lock.locked()) {
            // Берём пороги верхней форточки (теперь они общие для пары)
            openHigh = g_settings.ventUpper.high;
            closeLow = g_settings.ventUpper.low;
        }
    }

    // ── ДОЖДЬ — абсолютный приоритет ──────────────────────────────
    if (s_rainActive) {
        // Если форточки уже закрыты — IDLE, иначе — закрываем
        if (s_top.pct < 0.5f && s_bottom.pct < 0.5f) {
            stopAllMotors(nowMs);
            setAutoState(VAS_IDLE, nowMs);
        } else {
            setAutoState(VAS_RAIN_CLOSING, nowMs);
            // Закрываем сначала нижнюю, потом верхнюю
            if (s_bottom.pct > 0.5f) {
                setMotorDir(VS_TOP, MD_STOP, nowMs);
                setMotorDir(VS_BOTTOM, MD_CLOSE, nowMs);
            } else if (s_top.pct > 0.5f) {
                setMotorDir(VS_BOTTOM, MD_STOP, nowMs);
                setMotorDir(VS_TOP, MD_CLOSE, nowMs);
            }
            updateMovingPct(VS_TOP,    nowMs);
            updateMovingPct(VS_BOTTOM, nowMs);
        }
        return;
    }

    // Обновляем позиции движущихся моторов
    updateMovingPct(VS_TOP,    nowMs);
    updateMovingPct(VS_BOTTOM, nowMs);

    bool tempValid = !isnan(temp);

    // ── ПРИОРИТЕТ ЗАКРЫТИЯ: если T упала ниже порога — прервать всё ──
    if (tempValid && temp < closeLow
            && (s_top.pct > 0.5f || s_bottom.pct > 0.5f)
            && s_autoState != VAS_CLOSE_TOP_STEP
            && s_autoState != VAS_CLOSE_BOTTOM_STEP
            && s_autoState != VAS_PAUSE_WAIT) {
        // Прерываем что бы то ни было
        stopAllMotors(nowMs);
        Serial.printf("[VENT-AUTO] T=%.1f < %.1f — interrupt, start closing\n", temp, closeLow);
        // Идём через закрытие нижней (низ→верх)
        if (s_bottom.pct > 0.5f) {
            startStep(VS_BOTTOM, -1, nowMs);
            setAutoState(VAS_CLOSE_BOTTOM_STEP, nowMs);
        } else if (s_top.pct > 0.5f) {
            startStep(VS_TOP, -1, nowMs);
            setAutoState(VAS_CLOSE_TOP_STEP, nowMs);
        }
        return;
    }

    switch (s_autoState) {

      case VAS_IDLE: {
            stopAllMotors(nowMs);
            if (!tempValid) return;
            // Решаем — нужно открывать или закрывать
            if (temp > openHigh && (s_top.pct < 99.5f || s_bottom.pct < 99.5f)) {
                Serial.printf("[VENT-AUTO] T=%.1f > %.1f — start opening\n", temp, openHigh);
                s_tempBeforePause = temp;
                if (s_top.pct < 99.5f) {
                    startStep(VS_TOP, +1, nowMs);
                    setAutoState(VAS_OPEN_TOP_STEP, nowMs);
                } else {
                    startStep(VS_BOTTOM, +1, nowMs);
                    setAutoState(VAS_OPEN_BOTTOM_STEP, nowMs);
                }
            } else if (temp < closeLow && (s_top.pct > 0.5f || s_bottom.pct > 0.5f)) {
                Serial.printf("[VENT-AUTO] T=%.1f < %.1f — start closing\n", temp, closeLow);
                s_tempBeforePause = temp;
                if (s_bottom.pct > 0.5f) {
                    startStep(VS_BOTTOM, -1, nowMs);
                    setAutoState(VAS_CLOSE_BOTTOM_STEP, nowMs);
                } else {
                    startStep(VS_TOP, -1, nowMs);
                    setAutoState(VAS_CLOSE_TOP_STEP, nowMs);
                }
            }
            break;
        }

      case VAS_OPEN_TOP_STEP:
      case VAS_OPEN_BOTTOM_STEP:
      case VAS_CLOSE_TOP_STEP:
      case VAS_CLOSE_BOTTOM_STEP: {
            if (!stepCompleted(nowMs)) return;
            // Ступень завершена — стоп, переходим в паузу
            VentSide activeSide = s_autoActiveSide;
            setMotorDir(activeSide, MD_STOP, nowMs);
            s_tempBeforePause = tempValid ? temp : s_tempBeforePause;
            setAutoState(VAS_PAUSE_WAIT, nowMs);
            break;
        }

      case VAS_PAUSE_WAIT: {
            // Ждём STEP_DELAY_MIN секунд
            uint64_t needMs = (uint64_t)STEP_DELAY_MIN * 1000ULL;
            if (nowMs - s_autoStateMs < needMs) return;

            if (!tempValid) {
                setAutoState(VAS_IDLE, nowMs);
                return;
            }

            // Сравнение температуры — решаем что делать дальше
            // Контекст: какое было направление (открытие/закрытие)?
            bool wasOpening = (s_autoStepDir > 0);
            bool tempRose   = (temp > s_tempBeforePause + STEP_TEMP_RISE);
            bool tempFell   = (temp < s_tempBeforePause - STEP_TEMP_RISE);

            if (wasOpening) {
                // Продолжаем открывать только если T продолжает расти
                if (tempRose && temp > openHigh) {
                    s_tempBeforePause = temp;
                    if (s_top.pct < 99.5f) {
                        startStep(VS_TOP, +1, nowMs);
                        setAutoState(VAS_OPEN_TOP_STEP, nowMs);
                    } else if (s_bottom.pct < 99.5f) {
                        startStep(VS_BOTTOM, +1, nowMs);
                        setAutoState(VAS_OPEN_BOTTOM_STEP, nowMs);
                    } else {
                        // Обе на 100% — некуда дальше
                        Serial.println(F("[VENT-AUTO] Both vents at 100% — IDLE"));
                        setAutoState(VAS_IDLE, nowMs);
                    }
                } else {
                    // Не выросла — останавливаемся в IDLE
                    Serial.printf("[VENT-AUTO] T did not rise (%.1f -> %.1f) — IDLE\n",
                                  s_tempBeforePause, temp);
                    setAutoState(VAS_IDLE, nowMs);
                }
            } else {
                // Закрытие: продолжаем только если T продолжает падать
                if (tempFell && temp < closeLow) {
                    s_tempBeforePause = temp;
                    if (s_bottom.pct > 0.5f) {
                        startStep(VS_BOTTOM, -1, nowMs);
                        setAutoState(VAS_CLOSE_BOTTOM_STEP, nowMs);
                    } else if (s_top.pct > 0.5f) {
                        startStep(VS_TOP, -1, nowMs);
                        setAutoState(VAS_CLOSE_TOP_STEP, nowMs);
                    } else {
                        Serial.println(F("[VENT-AUTO] Both vents at 0% — IDLE"));
                        setAutoState(VAS_IDLE, nowMs);
                    }
                } else if (!tempFell) {
                    // Не упала — прерываем закрытие
                    Serial.printf("[VENT-AUTO] T did not fall (%.1f -> %.1f) — IDLE\n",
                                  s_tempBeforePause, temp);
                    setAutoState(VAS_IDLE, nowMs);
                }
            }
            break;
        }

      case VAS_RAIN_CLOSING: {
            // Этот state обрабатывается в начале функции (s_rainActive).
            // Если попали сюда — значит дождь снят.
            setAutoState(VAS_IDLE, nowMs);
            break;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
//  ПУБЛИЧНЫЙ API
// ═══════════════════════════════════════════════════════════════════
void VentCtrl::init() {
    loadPositionsFromNVS();

    // Инициализируем реле в STOP (на всякий случай)
    RelayMgr::setRelayDirect(RELAY_IDX_VENT_UPPER_OPEN,  false);
    RelayMgr::setRelayDirect(RELAY_IDX_VENT_UPPER_CLOSE, false);
    RelayMgr::setRelayDirect(RELAY_IDX_VENT_LOWER_OPEN,  false);
    RelayMgr::setRelayDirect(RELAY_IDX_VENT_LOWER_CLOSE, false);

    s_top.dir         = MD_STOP;
    s_bottom.dir      = MD_STOP;
    s_manualCmd       = VMC_NONE;
    s_autoState       = VAS_IDLE;

    Serial.printf("[VENT] Init v6.0: top=%.1f%% (calib=%us, valid=%d) "
                  "bottom=%.1f%% (calib=%us, valid=%d)\n",
        s_top.pct,    (unsigned)(s_top.calibMs / 1000),    (int)s_top.valid,
        s_bottom.pct, (unsigned)(s_bottom.calibMs / 1000), (int)s_bottom.valid);
}

// ═══════════════════════════════════════════════════════════════════
//  КАЛИБРОВОЧНЫЙ ПРОГОН
// ═══════════════════════════════════════════════════════════════════
bool VentCtrl::startHoming(uint8_t side) {
    if (side > 1) return false;
    if (s_homingActive) return false;

    VentSide s = (side == 0) ? VS_TOP : VS_BOTTOM;
    VentLeaf& leaf = (s == VS_TOP) ? s_top : s_bottom;
    if (leaf.calibMs == 0) return false;

    // Полное время хода плюс запас — чтобы гарантированно дойти до
    // концевика даже если расчётная позиция сильно занижена.
    uint32_t runMs = (uint32_t)((uint64_t)leaf.calibMs
                                * (100 + VENT_TRAVEL_OVERRUN_PCT) / 100);

    uint64_t now = steadyMs();
    s_homingActive = true;
    s_homingSide   = s;
    s_homingEndMs  = now + runMs;

    // Ручное управление и авто-логику на время прогона отключаем,
    // чтобы они не перехватили мотор.
    s_manualCmd = VMC_NONE;
    s_autoState = VAS_IDLE;

    stopAllMotors(now);
    setMotorDir(s, MD_CLOSE, now);

    Serial.printf("[VENT] Homing %s: closing for %u ms (travel %u + %u%% margin)\n",
        s == VS_TOP ? "TOP" : "BOTTOM",
        (unsigned)runMs, (unsigned)leaf.calibMs, (unsigned)VENT_TRAVEL_OVERRUN_PCT);
    return true;
}

bool VentCtrl::isHoming(uint8_t side) {
    if (!s_homingActive) return false;
    return (side == 0) ? (s_homingSide == VS_TOP) : (s_homingSide == VS_BOTTOM);
}

void VentCtrl::cancelHoming() {
    if (!s_homingActive) return;
    s_homingActive = false;
    stopAllMotors(steadyMs());
    Serial.println(F("[VENT] Homing cancelled"));
}

// Обработка активного прогона. Возвращает true, если прогон идёт и
// обычную логику режима выполнять не нужно.
static bool processHoming(uint64_t nowMs) {
    if (!s_homingActive) return false;

    VentLeaf& leaf = (s_homingSide == VS_TOP) ? s_top : s_bottom;

    // Мотор мог быть остановлен снаружи (авария, смена режима) — тогда
    // прогон недействителен: позицию обнулять нельзя, до упора не дошли.
    if (leaf.dir != MD_CLOSE) {
        s_homingActive = false;
        Serial.println(F("[VENT] Homing aborted — motor was stopped externally"));
        return false;
    }

    if (nowMs < s_homingEndMs) return true;   // ещё едем

    // Дошли: створка упёрлась в концевик, расчётная позиция теперь заведомо ноль.
    s_homingActive = false;
    setMotorDir(s_homingSide, MD_STOP, nowMs);
    leaf.pct   = 0.0f;
    leaf.valid = true;
    persistVentPosition(s_homingSide, 0.0f);

    Serial.printf("[VENT] Homing %s DONE — position reset to 0%%\n",
        s_homingSide == VS_TOP ? "TOP" : "BOTTOM");
    return false;
}

void VentCtrl::update(uint64_t nowMs, float temp, bool rain, bool autoMode) {
    // Edge: смена состояния дождя
    if (rain && !s_rainActive) {
        Serial.println(F("[VENT] RAIN detected — emergency close"));
    } else if (!rain && s_rainActive) {
        Serial.println(F("[VENT] RAIN cleared — back to normal"));
    }
    s_rainActive = rain;

    // v6.1: калибровочный прогон имеет приоритет над обоими режимами.
    // Дождь ему не мешает: прогон и так закрывает створку.
    if (processHoming(nowMs)) return;

    if (autoMode) {
        processAuto(nowMs, temp);
    } else {
        processManual(nowMs);
    }
}

void VentCtrl::manualSetCmd(VentManualCmd cmd) {
    // v6.1: любое ручное вмешательство отменяет калибровочный прогон —
    // иначе кнопка не отреагирует, а пользователь решит, что панель зависла.
    if (s_homingActive && cmd != VMC_NONE) VentCtrl::cancelHoming();

    if (cmd == s_manualCmd) return;  // нет изменения

    s_manualCmd      = cmd;
    s_manualCmdSetMs = steadyMs();
    s_manualHandoff  = false;

    Serial.printf("[VENT] Manual cmd: %s\n", cmdName(cmd));

    // Если кнопку отпустили — мгновенный стоп активного мотора
    if (cmd == VMC_NONE) {
        stopAllMotors(steadyMs());
    }
}

bool VentCtrl::setTopCalibSec(uint16_t sec) {
    if (sec < 1 || sec > 120) return false;
    s_top.calibMs = sec * 1000U;
    {
        MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(50));
        if (lock.locked()) {
            g_settings.ventUpperCal.travelMs = s_top.calibMs;
            settingsSaveDeferred();
        }
    }
    Serial.printf("[VENT] Top calibration set: %u sec\n", (unsigned)sec);
    return true;
}

bool VentCtrl::setBottomCalibSec(uint16_t sec) {
    if (sec < 1 || sec > 120) return false;
    s_bottom.calibMs = sec * 1000U;
    {
        MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(50));
        if (lock.locked()) {
            g_settings.ventLowerCal.travelMs = s_bottom.calibMs;
            settingsSaveDeferred();
        }
    }
    Serial.printf("[VENT] Bottom calibration set: %u sec\n", (unsigned)sec);
    return true;
}

uint16_t VentCtrl::getTopCalibSec()    { return s_top.calibMs    / 1000U; }
uint16_t VentCtrl::getBottomCalibSec() { return s_bottom.calibMs / 1000U; }

void VentCtrl::getStatus(VentStatusSnapshot& out) {
    out.topPct         = (uint8_t)(s_top.pct + 0.5f);
    out.bottomPct      = (uint8_t)(s_bottom.pct + 0.5f);
    out.topMoving      = (s_top.dir != MD_STOP);
    out.bottomMoving   = (s_bottom.dir != MD_STOP);
    out.topDir         = (int8_t)s_top.dir;
    out.bottomDir      = (int8_t)s_bottom.dir;
    out.autoState      = (uint8_t)s_autoState;
    out.rainBlocked    = s_rainActive;
    out.topCalibSec    = s_top.calibMs    / 1000U;
    out.bottomCalibSec = s_bottom.calibMs / 1000U;
    out.topHoming      = s_homingActive && s_homingSide == VS_TOP;
    out.bottomHoming   = s_homingActive && s_homingSide == VS_BOTTOM;
}

void VentCtrl::onModeChanged(bool autoMode) {
    Serial.printf("[VENT] Mode changed -> %s, stopping all motors\n",
                  autoMode ? "AUTO" : "MANUAL");
    s_homingActive = false;   // v6.1: смена режима прерывает прогон
    stopAllMotors(steadyMs());
    s_manualCmd = VMC_NONE;
    s_autoState = VAS_IDLE;
}
