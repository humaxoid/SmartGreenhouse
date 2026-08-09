// ╔══════════════════════════════════════════════════════════════════╗
// ║  sensor_reader.cpp — Реализация опроса датчиков v5.0            ║
// ║                                                                  ║
// ║  Главные изменения относительно v4:                              ║
// ║   1. pulseIn() → RMT RX (неблокирующий, аппаратный)              ║
// ║   2. DHT22 pin: GPIO0 → GPIO13                                   ║
// ║   3. TRIG pin: GPIO13 → GPIO0                                    ║
// ║   4. BME280 bmeOk=false при давлении вне диапазона (bug fix)    ║
// ╚══════════════════════════════════════════════════════════════════╝

#include "sensor_reader.h"
#include "settings.h"
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <DHT.h>
#include <BH1750.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <driver/rmt.h>       // Старый API RMT (IDF 4.4, Arduino core 2.x)
#include <soc/rmt_reg.h>

// ═══════════════════════════════════════════════════════════════════
//  ОБЪЕКТЫ ДАТЧИКОВ
// ═══════════════════════════════════════════════════════════════════
static Adafruit_BME280  s_bme;
static DHT              s_dht(DHT22_PIN, DHT22);
static BH1750           s_bh1750;
static bool             s_bmeReady  = false;
static bool             s_luxReady  = false;

// ═══════════════════════════════════════════════════════════════════
//  СНИМОК + МЬЮТЕКС
// ═══════════════════════════════════════════════════════════════════
static SensorSnapshot      s_snap;
static SemaphoreHandle_t   s_mutex = nullptr;

// Таймеры последнего опроса каждого датчика
static uint64_t s_lastBme    = 0;
static uint64_t s_lastDht    = 0;
static uint64_t s_lastLux    = 0;
static uint64_t s_lastWater  = 0;

// Счётчики отказов (после N ошибок подряд — переинициализация)
static uint8_t  s_dhtFails   = 0;
static uint8_t  s_bmeFails   = 0;
static uint8_t  s_luxFails   = 0;

#define I2C_FAIL_THRESHOLD        10    // ошибок подряд до recovery
#define I2C_RECOVERY_COOLDOWN_MS  30000ULL
static uint64_t s_lastRecoveryMs   = 0;
// v6.1: после MAX_I2C_RECOVERY_ATTEMPTS неудач — переводим I²C-датчики в DEAD,
// больше не пытаемся recovery. В snapshot чипы покажутся красными.
// Сбрасывается только перезагрузкой ESP32.
#define MAX_I2C_RECOVERY_ATTEMPTS  10
static uint8_t  s_i2cFailCount     = 0;
static bool     s_i2cDead          = false;

// ═══════════════════════════════════════════════════════════════════
//  I²C BUS RECOVERY
// ═══════════════════════════════════════════════════════════════════
// Полная процедура восстановления шины I²C по NXP UM10204 §3.1.16.
// Используется когда slave-устройство зависло и держит SDA в LOW.
//
// Безопасно вызывать из любого контекста (~200 мкс).
static void i2cBusRecovery() {
    Serial.println(F("[I2C] Bus recovery — sending 9 clock pulses..."));

    Wire.end();

    pinMode(BME280_SCL, OUTPUT);
    pinMode(BME280_SDA, INPUT_PULLUP);

    // 9 тактов: slave обязан отпустить SDA после 8 бит + 1 бит ACK.
    for (int i = 0; i < 9; i++) {
        digitalWrite(BME280_SCL, LOW);
        delayMicroseconds(5);
        digitalWrite(BME280_SCL, HIGH);
        delayMicroseconds(5);
        if (digitalRead(BME280_SDA) == HIGH) {
            Serial.printf("[I2C] SDA released after %d clocks\n", i + 1);
            break;
        }
    }

    // STOP condition: SDA LOW→HIGH при SCL=HIGH
    pinMode(BME280_SDA, OUTPUT);
    digitalWrite(BME280_SDA, LOW);
    delayMicroseconds(5);
    digitalWrite(BME280_SCL, HIGH);
    delayMicroseconds(5);
    digitalWrite(BME280_SDA, HIGH);
    delayMicroseconds(5);

    Wire.begin(BME280_SDA, BME280_SCL);
    Serial.println(F("[I2C] Bus recovery complete"));
}

// Переинициализация датчиков после recovery
static void reinitI2cSensors() {
    Serial.println(F("[I2C] Reinitializing sensors..."));

    if (s_bme.begin(BME280_I2C_ADDRESS)) {
        s_bme.setSampling(
            Adafruit_BME280::MODE_NORMAL,
            Adafruit_BME280::SAMPLING_X1,
            Adafruit_BME280::SAMPLING_X1,
            Adafruit_BME280::SAMPLING_X1,
            Adafruit_BME280::FILTER_OFF,
            Adafruit_BME280::STANDBY_MS_1000
        );
        s_bmeReady = true;
        Serial.println(F("[I2C] BME280 reinit OK"));
    } else {
        s_bmeReady = false;
        Serial.println(F("[I2C] BME280 reinit FAILED"));
    }

    if (s_bh1750.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, BH1750_I2C_ADDRESS)) {
        s_luxReady = true;
        Serial.println(F("[I2C] BH1750 reinit OK"));
    } else {
        s_luxReady = false;
        Serial.println(F("[I2C] BH1750 reinit FAILED"));
    }

    s_bmeFails = 0;
    s_luxFails = 0;
}

static void tryI2cRecovery(uint64_t nowMs) {
    if (s_i2cDead) return;   // v6.1: не пытаемся реанимировать мёртвые датчики
    if (nowMs - s_lastRecoveryMs < I2C_RECOVERY_COOLDOWN_MS) return;
    s_lastRecoveryMs = nowMs;

    s_i2cFailCount++;
    Serial.printf("[I2C] Recovery attempt %u/%u\n",
        (unsigned)s_i2cFailCount, (unsigned)MAX_I2C_RECOVERY_ATTEMPTS);

    if (s_i2cFailCount >= MAX_I2C_RECOVERY_ATTEMPTS) {
        s_i2cDead = true;
        Serial.println(F("[I2C] DEAD: max recovery attempts exceeded — sensors disabled until reboot"));
        return;
    }

    i2cBusRecovery();
    vTaskDelay(pdMS_TO_TICKS(100));
    reinitI2cSensors();
}

// Вызывается, когда I²C-датчик удачно отдал данные: счётчик неудачных
// recovery обнуляется.
//
// v6.1 FIX: функция существовала, но не вызывалась НИОТКУДА. Из-за этого
// s_i2cFailCount только рос и никогда не сбрасывался: десять разрозненных
// сбоев за любой срок — хоть по одному в неделю — накапливались и навсегда
// переводили BME280 и BH1750 в DEAD до перезагрузки. Для устройства,
// рассчитанного на месяцы непрерывной работы, это гарантированная деградация.
static void onI2cSuccess() {
    if (s_i2cFailCount > 0) {
        Serial.printf("[I2C] Success — resetting fail counter (was %u)\n",
            (unsigned)s_i2cFailCount);
        s_i2cFailCount = 0;
    }
}

// Публичный геттер для snapshot
namespace SensorRdr { bool isI2cDead() { return s_i2cDead; } }

// ═══════════════════════════════════════════════════════════════════
//  АНТИДРЕБЕЗГ ДОЖДЯ
// ═══════════════════════════════════════════════════════════════════
// Цифровой DO YL-83 может дребезжать на границе срабатывания.
// Считаем стабильным если 5 чтений подряд дали одно значение.
static bool    s_rainState    = false;
static uint8_t s_rainStable   = 0;
static bool    s_rainLastRead = false;
#define RAIN_STABLE_COUNT 5

// ═══════════════════════════════════════════════════════════════════
//  RMT RX для ультразвука AJ-SR04M
// ═══════════════════════════════════════════════════════════════════
// ПРИНЦИП РАБОТЫ:
//   1. Настраиваем RMT канал как RX на ECHO pin.
//     RMT делитель: 80 МГц APB / 80 = 1 МГц → тик RMT = 1 мкс.
//   2. idle_threshold = 30000 мкс — это ТАЙМАУТ, за который должен
//     вернуться эхо. Если эха нет → RMT завершает приём.
//   3. Функция measureEchoUs():
//     • Запускаем RMT RX
//     • Генерируем TRIG импульс 10 мкс
//     • Читаем очередь RMT с таймаутом 40 мс
//     • Возвращаем длительность первого HIGH-уровня в мкс
//   4. Вся операция блокирует задачу на <40 мс, НО фактически
//     вызывается 1 раз в 2 сек — WDT не сработает.
//     В отличие от pulseIn (которая крутит процессор), здесь
//     задача находится в BLOCKED состоянии и позволяет другим
//     задачам работать.

#define RMT_RX_CHANNEL       ((rmt_channel_t)RMT_CHANNEL_0)
#define RMT_CLK_DIV          80        // 1 тик = 1 мкс
#define RMT_IDLE_THRESHOLD   30000     // 30 мс макс. длительность HIGH

// Очередь результатов RMT (создаётся в initRmtRx())
static RingbufHandle_t s_rmtRingbuf = nullptr;

// Инициализация RMT RX канала на ECHO pin
static bool initRmtRx() {
    rmt_config_t cfg = {};
    cfg.rmt_mode                  = RMT_MODE_RX;
    cfg.channel                   = RMT_RX_CHANNEL;
    cfg.clk_div                   = RMT_CLK_DIV;
    cfg.gpio_num                  = (gpio_num_t)WATER_ECHO_PIN;
    cfg.mem_block_num             = 1;   // 64 элемента — с запасом
    cfg.rx_config.filter_en       = true;
    cfg.rx_config.filter_ticks_thresh = 100;  // фильтр шумов <100 нс (но с clk_div=80 это минимум)
    cfg.rx_config.idle_threshold  = RMT_IDLE_THRESHOLD;

    esp_err_t r = rmt_config(&cfg);
    if (r != ESP_OK) {
        Serial.printf("[RMT] Config failed: %d\n", (int)r);
        return false;
    }

    // buffer_size=1000: запас под несколько неотловленных результатов
    r = rmt_driver_install(RMT_RX_CHANNEL, 1000, 0);
    if (r != ESP_OK) {
        Serial.printf("[RMT] Driver install failed: %d\n", (int)r);
        return false;
    }

    r = rmt_get_ringbuf_handle(RMT_RX_CHANNEL, &s_rmtRingbuf);
    if (r != ESP_OK || !s_rmtRingbuf) {
        Serial.printf("[RMT] Ringbuf handle failed: %d\n", (int)r);
        return false;
    }

    Serial.println(F("[RMT] RX channel configured for AJ-SR04M ECHO"));
    return true;
}

// Сгенерировать TRIG импульс (10 мкс HIGH) — это ОК, микросекунды не блокируют
static void sendTrigPulse() {
    digitalWrite(WATER_TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(WATER_TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(WATER_TRIG_PIN, LOW);
}

// Измерение длительности ECHO через RMT RX.
// Возвращает длительность HIGH-уровня в мкс, или 0 при таймауте.
//
// Алгоритм:
//   1. Очищаем ringbuf от предыдущих результатов (если остались)
//   2. Запускаем приём
//   3. Шлём TRIG
//   4. Ждём результат из ringbuf с таймаутом 40 мс
//   5. Останавливаем приём
//   6. Парсим rmt_item32_t: первая запись должна быть HIGH level
static uint32_t measureEchoUs() {
    if (!s_rmtRingbuf) return 0;

    // ── 1. Очистка буфера от старых результатов ──────────────────
    // v6.1: вычерпываем ВСЁ, а не один элемент. Раньше при накоплении
    // нескольких просроченных результатов буфер постепенно забивался.
    size_t rxSize = 0;
    for (;;) {
        void* oldItems = xRingbufferReceive(s_rmtRingbuf, &rxSize, 0);
        if (!oldItems) break;
        vRingbufferReturnItem(s_rmtRingbuf, oldItems);
    }

    // ── 2. Запускаем приём ───────────────────────────────────────
    rmt_rx_start(RMT_RX_CHANNEL, true);  // true = очистить RAM канала

    // ── 3. Шлём TRIG ─────────────────────────────────────────────
    sendTrigPulse();

    // ── 4. Ждём результат. 40 мс таймаут (30 мс эха + запас) ─────
    rmt_item32_t* items = (rmt_item32_t*)xRingbufferReceive(
        s_rmtRingbuf, &rxSize, pdMS_TO_TICKS(40));

    // ── 5. Останавливаем приём ───────────────────────────────────
    rmt_rx_stop(RMT_RX_CHANNEL);

    if (!items) {
        return 0;  // таймаут / нет эха
    }

    // ── 6. Парсим первый элемент ─────────────────────────────────
    // rmt_item32_t содержит 2 "символа" в 32 битах:
    //   level0, duration0 — первый уровень и его длительность
    //   level1, duration1 — второй уровень и его длительность
    // Нас интересует длительность HIGH (level0 == 1).
    //
    // v6.1 FIX: раньше при items != NULL и rxSize == 0 функция выходила
    // через `return 0` НЕ вернув элемент в кольцевой буфер. Каждый такой
    // случай съедал кусок буфера (всего 1000 байт) — через несколько
    // дней он забивался, xRingbufferReceive переставал отдавать данные,
    // и датчик уровня воды отказывал до перезагрузки.
    uint32_t echoUs = 0;
    if (rxSize >= sizeof(rmt_item32_t)) {
        if (items[0].level0 == 1 && items[0].duration0 > 0) {
            echoUs = items[0].duration0;  // в единицах clk_div, т.е. мкс
        } else if (items[0].level1 == 1 && items[0].duration1 > 0) {
            echoUs = items[0].duration1;
        }
    }

    vRingbufferReturnItem(s_rmtRingbuf, items);   // возвращаем ВСЕГДА
    return echoUs;
}

// Расчёт расстояния из длительности эха (см).
// Скорость звука ≈ 343 м/с = 0.0343 см/мкс. Туда-обратно → делим на 2.
// distanceCm = echoUs * 0.0343 / 2 = echoUs * 0.01715
//
// Возвращает -1.0f при таймауте или некорректном значении.
static float measureDistanceCm() {
    uint32_t us = measureEchoUs();
    if (us == 0) return -1.0f;

    float distCm = (float)us * 0.01715f;

    // Фильтр заведомо невалидных значений
    if (distCm < 1.0f || distCm > 500.0f) return -1.0f;

    return distCm;
}

// ═══════════════════════════════════════════════════════════════════
//  МЕДИАННЫЙ ФИЛЬТР УРОВНЯ ВОДЫ
// ═══════════════════════════════════════════════════════════════════
// Ультразвук склонен к выбросам (отражение от ряби, потолка и т.д.).
// Берём 3 замера, медиана — устойчива к одному выбросу.
static float s_waterDistBuf[3] = {0, 0, 0};
static uint8_t s_waterBufIdx = 0;
static bool    s_waterFirstFill = true;
static uint8_t s_waterFails = 0;

static float median3(float a, float b, float c) {
    if (a > b) { float t = a; a = b; b = t; }
    if (b > c) { float t = b; b = c; c = t; }
    if (a > b) { float t = a; a = b; b = t; }
    return b;
}

// ═══════════════════════════════════════════════════════════════════
//  ИНИЦИАЛИЗАЦИЯ
// ═══════════════════════════════════════════════════════════════════
void SensorRdr::init() {
    s_mutex = xSemaphoreCreateMutex();
    configASSERT(s_mutex);

    memset(&s_snap, 0, sizeof(s_snap));
    s_snap.bmeTemp       = NAN;
    s_snap.bmeHum        = NAN;
    s_snap.bmePress      = NAN;
    s_snap.dhtTemp       = NAN;
    s_snap.dhtHum        = NAN;
    s_snap.waterLevelPct = NAN;
    s_snap.waterDistCm   = NAN;

    // ── I²C шина ─────────────────────────────────────────────────
    Wire.begin(BME280_SDA, BME280_SCL);

    // ── BME280 ───────────────────────────────────────────────────
    if (s_bme.begin(BME280_I2C_ADDRESS)) {
        s_bme.setSampling(
            Adafruit_BME280::MODE_NORMAL,
            Adafruit_BME280::SAMPLING_X1,
            Adafruit_BME280::SAMPLING_X1,
            Adafruit_BME280::SAMPLING_X1,
            Adafruit_BME280::FILTER_OFF,
            Adafruit_BME280::STANDBY_MS_1000
        );
        vTaskDelay(pdMS_TO_TICKS(1100));
        s_bmeReady = true;
        Serial.println(F("[SENSOR] BME280 OK"));
    } else {
        Serial.println(F("[SENSOR] BME280 NOT FOUND"));
    }

    // ── BH1750 ───────────────────────────────────────────────────
    if (s_bh1750.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, BH1750_I2C_ADDRESS)) {
        s_luxReady = true;
        Serial.println(F("[SENSOR] BH1750 OK"));
    } else {
        Serial.println(F("[SENSOR] BH1750 NOT FOUND"));
    }

    // ── DHT22 на GPIO13 (больше не strapping!) ──────────────────
    s_dht.begin();
    Serial.printf("[SENSOR] DHT22 on GPIO%d\n", DHT22_PIN);

    // ── YL-83 (дождь) ─────────────────────────────────────────────
    pinMode(RAIN_SENSOR_PIN, INPUT);
    s_rainState    = (digitalRead(RAIN_SENSOR_PIN) == RAIN_ACTIVE_LEVEL);
    s_rainLastRead = s_rainState;
    s_rainStable   = RAIN_STABLE_COUNT;
    Serial.printf("[SENSOR] Rain sensor on GPIO%d, initial=%s\n",
        RAIN_SENSOR_PIN, s_rainState ? "RAIN" : "DRY");

    // ── AJ-SR04M: TRIG как OUTPUT, ECHO настраивается RMT ────────
    pinMode(WATER_TRIG_PIN, OUTPUT);
    digitalWrite(WATER_TRIG_PIN, LOW);
    // ECHO pin будет настроен RMT драйвером — pinMode не нужен!

    if (initRmtRx()) {
        Serial.printf("[SENSOR] AJ-SR04M: TRIG=GPIO%d, ECHO=GPIO%d (RMT RX)\n",
            WATER_TRIG_PIN, WATER_ECHO_PIN);
    } else {
        Serial.println(F("[SENSOR] AJ-SR04M: RMT init FAILED — water sensor disabled"));
    }
}

// ═══════════════════════════════════════════════════════════════════
//  ЦИКЛ ОПРОСА
// ═══════════════════════════════════════════════════════════════════
void SensorRdr::update(uint64_t nowMs) {
    SensorSnapshot tmp;

    // Читаем текущий снимок под мьютексом в локальную копию
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        tmp = s_snap;
        xSemaphoreGive(s_mutex);
    } else {
        return;
    }

    // ── BME280 ──────────────────────────────────────────────────
    if (s_bmeReady && (nowMs - s_lastBme >= BME280_POLL_MS)) {
        s_lastBme = nowMs;
        float t = s_bme.readTemperature();
        float h = s_bme.readHumidity();
        float p = s_bme.readPressure();  // Па

        if (!isnan(t) && !isnan(h) && p > 0) {
            tmp.bmeTemp = t;
            tmp.bmeHum  = h;
            // Перевод Па → мм рт. ст.
            float press_mm = p / 133.322f;
            // ─── BUG FIX v5.0 ────────────────────────────────────
            // Если давление вне реального диапазона (600..810 мм рт. ст.),
            // это ОШИБКА измерения (датчик даёт мусор). Раньше bmeOk=true
            // оставался — теперь корректно ставим bmeOk=false.
            if (press_mm >= 600.0f && press_mm <= 810.0f) {
                tmp.bmePress = press_mm;
                tmp.bmeOk    = true;
            } else {
                tmp.bmePress = NAN;
                tmp.bmeOk    = false;   // ← исправлено (было true)
                Serial.printf("[BME280] Pressure out of range: %.1f mmHg — data rejected\n",
                    press_mm);
            }
            if (tmp.bmeOk) { s_bmeFails = 0; onI2cSuccess(); }
        } else {
            tmp.bmeOk = false;
            if (s_bmeFails < 255) s_bmeFails++;
            if (s_bmeFails == I2C_FAIL_THRESHOLD) {
                Serial.printf("[SENSOR] BME280: %u errors in a row — I2C recovery\n",
                    s_bmeFails);
                tryI2cRecovery(nowMs);
            }
        }
    }

    // ── DHT22 ──────────────────────────────────────────────────
    if (nowMs - s_lastDht >= DHT22_POLL_MS) {
        s_lastDht = nowMs;
        float t = s_dht.readTemperature();
        float h = s_dht.readHumidity();

        if (!isnan(t) && !isnan(h) && t > -40.0f && t < 80.0f) {
            tmp.dhtTemp = t;
            tmp.dhtHum  = h;
            tmp.dhtOk   = true;
            s_dhtFails  = 0;
        } else {
            s_dhtFails++;
            if (s_dhtFails >= 3) {
                tmp.dhtTemp = NAN;
                tmp.dhtHum  = NAN;
                tmp.dhtOk   = false;
                s_dhtFails  = 3;
            }
        }
    }

    // ── BH1750 ──────────────────────────────────────────────────
    if (s_luxReady && (nowMs - s_lastLux >= BH1750_POLL_MS)) {
        s_lastLux = nowMs;
        float lux = s_bh1750.readLightLevel();
        if (lux >= 0) {
            tmp.lux   = (uint16_t)constrain(lux, 0, 65535);
            tmp.luxOk = true;
            s_luxFails = 0;
            onI2cSuccess();
        } else {
            tmp.luxOk = false;
            if (s_luxFails < 255) s_luxFails++;
            if (s_luxFails == I2C_FAIL_THRESHOLD) {
                Serial.printf("[SENSOR] BH1750: %u errors — I2C recovery\n", s_luxFails);
                tryI2cRecovery(nowMs);
            }
        }
    }

    // ── Дождь (опрос каждый цикл, быстро) ────────────────────────
    {
        bool curRead = (digitalRead(RAIN_SENSOR_PIN) == RAIN_ACTIVE_LEVEL);
        if (curRead == s_rainLastRead) {
            if (s_rainStable < RAIN_STABLE_COUNT) s_rainStable++;
        } else {
            s_rainStable   = 1;
            s_rainLastRead = curRead;
        }
        if (s_rainStable >= RAIN_STABLE_COUNT && s_rainState != curRead) {
            s_rainState = curRead;
            Serial.printf("[RAIN] %s\n", s_rainState ? "RAIN detected" : "DRY");
        }
        tmp.rain = s_rainState;
    }

    // ── AJ-SR04M через RMT RX ────────────────────────────────────
    if (nowMs - s_lastWater >= WATER_LEVEL_POLL_MS) {
        s_lastWater = nowMs;

        float distCm = measureDistanceCm();

        if (distCm > 0) {
            s_waterDistBuf[s_waterBufIdx] = distCm;
            s_waterBufIdx = (s_waterBufIdx + 1) % 3;
            if (s_waterFirstFill && s_waterBufIdx == 0) s_waterFirstFill = false;

            float filtered = s_waterFirstFill
                ? distCm
                : median3(s_waterDistBuf[0], s_waterDistBuf[1], s_waterDistBuf[2]);

            // Глубина ёмкости из настроек
            uint16_t depthCm;
            {
                MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(10));
                depthCm = lock.locked()
                    ? g_settings.waterTank.depthCm
                    : DEF_WATER_DEPTH_CM;
            }

            // Расчёт уровня в %:
            // distance = расстояние от датчика до воды (сверху вниз)
            // waterHeight = depthCm - distance (высота столба воды)
            // pct = waterHeight / (depthCm - minDist) * 100
            float effectiveDepth = (float)depthCm - (float)WATER_SENSOR_MIN_DIST_CM;
            if (effectiveDepth < 1.0f) effectiveDepth = 1.0f;

            float waterHeight = (float)depthCm - filtered;
            float pct = (waterHeight / effectiveDepth) * 100.0f;
            if (pct < 0.0f)   pct = 0.0f;
            if (pct > 100.0f) pct = 100.0f;

            tmp.waterLevelPct = pct;
            tmp.waterDistCm   = filtered;
            tmp.waterOk       = true;
            s_waterFails      = 0;
        } else {
            s_waterFails++;
            if (s_waterFails >= 5) {
                tmp.waterOk       = false;
                tmp.waterLevelPct = NAN;
                tmp.waterDistCm   = NAN;
                s_waterFails      = 5;
            }
        }
    }

    tmp.timestamp = (uint32_t)(nowMs & 0xFFFFFFFFUL);

    // Атомарное обновление снимка
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        s_snap = tmp;
        xSemaphoreGive(s_mutex);
    }
}

// ═══════════════════════════════════════════════════════════════════
//  ГЕТТЕРЫ
// ═══════════════════════════════════════════════════════════════════
SensorSnapshot SensorRdr::getData() {
    SensorSnapshot out;
    memset(&out, 0, sizeof(out));
    out.bmeTemp = NAN;
    out.bmeHum  = NAN;
    out.bmePress = NAN;
    out.dhtTemp = NAN;
    out.dhtHum  = NAN;
    out.waterLevelPct = NAN;
    out.waterDistCm   = NAN;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        out = s_snap;
        xSemaphoreGive(s_mutex);
    }
    return out;
}

float SensorRdr::primaryTemp() {
    SensorSnapshot s = getData();
    if (s.bmeOk && !isnan(s.bmeTemp)) return s.bmeTemp;
    if (s.dhtOk && !isnan(s.dhtTemp)) return s.dhtTemp;
    return NAN;
}

float SensorRdr::primaryHum() {
    SensorSnapshot s = getData();
    if (s.bmeOk && !isnan(s.bmeHum)) return s.bmeHum;
    if (s.dhtOk && !isnan(s.dhtHum)) return s.dhtHum;
    return NAN;
}

bool SensorRdr::isRaining() {
    SensorSnapshot s = getData();
    return s.rain;
}

float SensorRdr::waterLevelPct() {
    SensorSnapshot s = getData();
    return s.waterLevelPct;
}
