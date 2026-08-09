#pragma once
// ╔══════════════════════════════════════════════════════════════════╗
// ║  sensor_reader.h — Опрос всех датчиков теплицы v5.0             ║
// ║                                                                  ║
// ║  Датчики:                                                        ║
// ║   • BME280 (I²C 0x76)  — T внутри, H внутри, P                  ║
// ║   • BH1750 (I²C 0x23)  — освещённость                           ║
// ║   • DHT22  (GPIO13)    — T наружу, H наружу                     ║
// ║   • YL-83  (GPIO39)    — дождь (цифровой DO)                    ║
// ║   • AJ-SR04M (TRIG=0, ECHO=34)  — уровень воды (RMT RX)        ║
// ║                                                                  ║
// ║  Ключевые изменения v5.0:                                        ║
// ║   • RMT RX вместо pulseIn() для ультразвука (не блокирует!)     ║
// ║   • DHT22 переехал на GPIO13 (уход со strapping GPIO0)          ║
// ║   • TRIG переехал на GPIO0 (только OUTPUT — безопасно)          ║
// ║   • BME280 pressure bug fix: bmeOk=false при невалидном давлении║
// ╚══════════════════════════════════════════════════════════════════╝

#include "config.h"
#include <stdint.h>
#include <stdbool.h>

// ─── Текущий снимок всех датчиков ────────────────────────────────
// Возвращается через SensorRdr::getData(). Все значения актуальны
// на момент last update() — могут быть устаревшими если датчик отказал.
struct SensorSnapshot {
    // BME280 (внутри теплицы)
    float    bmeTemp;    // °C. NaN при отказе.
    float    bmeHum;     // %.  NaN при отказе.
    float    bmePress;   // мм рт. ст. NaN при отказе или вне диапазона.
    bool     bmeOk;      // Последнее чтение успешное?

    // DHT22 (снаружи)
    float    dhtTemp;    // °C. NaN при отказе.
    float    dhtHum;     // %.  NaN при отказе.
    bool     dhtOk;

    // BH1750 (освещённость)
    uint16_t lux;        // 0..65535. 0 при отказе.
    bool     luxOk;

    // YL-83 (дождь)
    bool     rain;       // true = дождь, false = сухо

    // AJ-SR04M (уровень воды)
    float    waterLevelPct;  // %. NaN при отказе.
    float    waterDistCm;    // см до поверхности воды. NaN при отказе.
    bool     waterOk;

    // Метка времени снимка (для диагностики устаревших данных)
    uint32_t timestamp;      // младшие 32 бита steadyMs()
};

namespace SensorRdr {
    bool isI2cDead();   // v6.1: I²C переведён в DEAD после N неудачных recovery

    // Инициализация I²C, датчиков, RMT RX для ультразвука.
    // Вызывать в setup() после LittleFS.begin() (не критично, но
    // логи Serial будут читаемы).
    void init();

    // Периодический опрос. Вызывать из taskCore1 каждый цикл.
    // Каждый датчик опрашивается по своему таймеру (BME280_POLL_MS и т.д.).
    void update(uint64_t nowMs);

    // Снимок всех данных (thread-safe, через мьютекс).
    SensorSnapshot getData();

    // Основная T — BME280 с fallback на DHT22.
    // NaN если оба датчика отказали.
    float primaryTemp();

    // Основная H — BME280 с fallback на DHT22.
    float primaryHum();

    // Идёт ли дождь (прошедший антидребезг).
    bool isRaining();

    // Уровень воды, %. NaN если датчик отказал.
    float waterLevelPct();

}  // namespace SensorRdr
