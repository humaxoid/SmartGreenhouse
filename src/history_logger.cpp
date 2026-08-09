// ╔══════════════════════════════════════════════════════════════════╗
// ║  history_logger.cpp — Реализация истории данных v5.0            ║
// ║                                                                  ║
// ║  Формат файла /history.bin:                                      ║
// ║   • Заголовок: uint32_t magic = 0x48495354 ("HIST")             ║
// ║                uint8_t  version = 1                              ║
// ║                uint8_t  writeIdx                                 ║
// ║                uint8_t  count                                    ║
// ║                uint8_t  reserved                                 ║
// ║   • Данные:   HistoryPoint[HISTORY_POINTS]                      ║
// ║                                                                  ║
// ║  При несовпадении magic/version файл считается битым             ║
// ║  и перезаписывается пустым буфером.                              ║
// ╚══════════════════════════════════════════════════════════════════╝

#include "history_logger.h"
#include "sensor_reader.h"
#include "network_manager.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <math.h>
#include <string.h>

// ═══════════════════════════════════════════════════════════════════
//  ФОРМАТ ФАЙЛА
// ═══════════════════════════════════════════════════════════════════
#define HIST_MAGIC     0x48495354UL   // "HIST"
#define HIST_VERSION   2  // v6.0: формат изменён (добавлены DHT22 поля)
#define HIST_FILE      "/history.bin"

struct HistoryHeader {
    uint32_t magic;
    uint8_t  version;
    uint8_t  writeIdx;    // Индекс следующей записи (0..HISTORY_POINTS-1)
    uint8_t  count;       // Сколько валидных точек (0..HISTORY_POINTS)
    uint8_t  reserved;
};

// ═══════════════════════════════════════════════════════════════════
//  ВНУТРЕННЕЕ СОСТОЯНИЕ
// ═══════════════════════════════════════════════════════════════════
static HistoryPoint     s_buffer[HISTORY_POINTS];
static uint8_t          s_writeIdx      = 0;
static uint8_t          s_count         = 0;
static bool             s_timeAvailable = false;
static uint64_t         s_lastWriteMs   = 0;
static SemaphoreHandle_t s_mutex        = nullptr;

// ═══════════════════════════════════════════════════════════════════
//  ЗАГРУЗКА / СОХРАНЕНИЕ ФАЙЛА
// ═══════════════════════════════════════════════════════════════════

// Возвращает true если успешно загрузили валидный файл
static bool loadFromFile() {
    File f = LittleFS.open(HIST_FILE, "r");
    if (!f) {
        Serial.println(F("[HIST] File not found, starting empty"));
        return false;
    }

    HistoryHeader hdr;
    size_t read = f.read((uint8_t*)&hdr, sizeof(hdr));
    if (read != sizeof(hdr) || hdr.magic != HIST_MAGIC
        || hdr.version != HIST_VERSION) {
        Serial.printf("[HIST] Invalid header (magic=0x%08lX, ver=%u) — resetting\n",
            (unsigned long)hdr.magic, (unsigned)hdr.version);
        f.close();
        return false;
    }

    if (hdr.writeIdx >= HISTORY_POINTS || hdr.count > HISTORY_POINTS) {
        Serial.println(F("[HIST] Corrupted indices — resetting"));
        f.close();
        return false;
    }

    read = f.read((uint8_t*)s_buffer, sizeof(s_buffer));
    f.close();

    if (read != sizeof(s_buffer)) {
        Serial.printf("[HIST] Short read (%u/%u) — resetting\n",
            (unsigned)read, (unsigned)sizeof(s_buffer));
        return false;
    }

    s_writeIdx = hdr.writeIdx;
    s_count    = hdr.count;
    Serial.printf("[HIST] Loaded %u points (writeIdx=%u)\n",
        (unsigned)s_count, (unsigned)s_writeIdx);
    return true;
}

// Сохранение атомарное: пишем в временный файл, затем переименовываем.
// Это защищает от повреждения при отключении питания во время записи.
static void saveToFile() {
    const char* tmpFile = "/history.tmp";

    File f = LittleFS.open(tmpFile, "w");
    if (!f) {
        Serial.println(F("[HIST] Failed to open tmp file for write"));
        return;
    }

    HistoryHeader hdr = {
        HIST_MAGIC, HIST_VERSION, s_writeIdx, s_count, 0
    };

    size_t wrote = f.write((const uint8_t*)&hdr, sizeof(hdr));
    if (wrote == sizeof(hdr)) {
        wrote = f.write((const uint8_t*)s_buffer, sizeof(s_buffer));
    }
    f.close();

    if (wrote != sizeof(s_buffer)) {
        Serial.println(F("[HIST] Write failed — tmp file abandoned"));
        LittleFS.remove(tmpFile);
        return;
    }

    // Атомарное переименование: либо старый файл, либо новый полный.
    // Никогда "половина нового" — защита от обрыва питания.
    LittleFS.remove(HIST_FILE);
    if (!LittleFS.rename(tmpFile, HIST_FILE)) {
        Serial.println(F("[HIST] Rename failed"));
    }
}

// ═══════════════════════════════════════════════════════════════════
//  ДОБАВЛЕНИЕ ТОЧКИ
// ═══════════════════════════════════════════════════════════════════
// Записывает текущий снимок датчиков в слот s_writeIdx,
// инкрементирует writeIdx по кольцу, увеличивает count до HISTORY_POINTS.
static void addPoint() {
    time_t now;
    time(&now);

    SensorSnapshot s = SensorRdr::getData();

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        Serial.println(F("[HIST] addPoint: mutex timeout"));
        return;
    }

    HistoryPoint& p = s_buffer[s_writeIdx];
    p.timestamp  = now;
    // v6.0: пишем РАЗДЕЛЬНО BME280 и DHT22 — для двух графиков
    // (внутри теплицы и снаружи)
    p.temp        = s.bmeOk ? s.bmeTemp : NAN;     // внутри
    p.humidity    = s.bmeOk ? s.bmeHum  : NAN;     // внутри
    p.dhtTemp     = s.dhtOk ? s.dhtTemp : NAN;     // снаружи
    p.dhtHumidity = s.dhtOk ? s.dhtHum  : NAN;     // снаружи
    p.pressure    = s.bmeOk ? s.bmePress : NAN;
    p.lux         = s.luxOk ? (float)s.lux : NAN;
    p.waterLevel  = s.waterOk ? s.waterLevelPct : NAN;

    s_writeIdx = (s_writeIdx + 1) % HISTORY_POINTS;
    if (s_count < HISTORY_POINTS) s_count++;

    xSemaphoreGive(s_mutex);

    // Логирование
    struct tm tm;
    localtime_r(&now, &tm);
    Serial.printf("[HIST] Point added at %02d:%02d: T=%.1f H=%.1f P=%.0f lux=%.0f water=%.1f (count=%u)\n",
        tm.tm_hour, tm.tm_min,
        p.temp, p.humidity, p.pressure, p.lux, p.waterLevel,
        (unsigned)s_count);

    // Сохраняем в файл
    saveToFile();
}

// ═══════════════════════════════════════════════════════════════════
//  ПУБЛИЧНЫЙ API
// ═══════════════════════════════════════════════════════════════════

void HistoryLogger::init() {
    if (!s_mutex) {
        s_mutex = xSemaphoreCreateMutex();
        configASSERT(s_mutex);
    }

    // Инициализируем буфер пустыми точками
    memset(s_buffer, 0, sizeof(s_buffer));
    for (uint8_t i = 0; i < HISTORY_POINTS; i++) {
        s_buffer[i].temp       = NAN;
        s_buffer[i].humidity   = NAN;
        s_buffer[i].pressure   = NAN;
        s_buffer[i].lux        = NAN;
        s_buffer[i].waterLevel = NAN;
    }
    s_writeIdx = 0;
    s_count    = 0;

    // Пытаемся загрузить из файла
    if (!loadFromFile()) {
        // Создаём пустой файл чтобы последующие saveToFile были быстрее
        saveToFile();
    }
}

void HistoryLogger::onTimeAvailable() {
    if (s_timeAvailable) return;
    s_timeAvailable = true;
    Serial.println(F("[HIST] NTP synced — history logging active"));

    // Сразу делаем первую запись — чтобы пользователь не ждал час
    // перед первой точкой на графике.
    s_lastWriteMs = steadyMs();
    addPoint();
}

void HistoryLogger::update(uint64_t nowMs) {
    if (!s_timeAvailable) return;

    if (nowMs - s_lastWriteMs >= HISTORY_INTERVAL_MS) {
        s_lastWriteMs = nowMs;
        addPoint();
    }
}

uint8_t HistoryLogger::getAll(HistoryPoint* outPoints, uint8_t maxPoints) {
    if (!outPoints || maxPoints == 0) return 0;

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return 0;

    // Восстанавливаем хронологический порядок.
    // Кольцевой буфер: самая старая точка — следующая после writeIdx
    // (если count == HISTORY_POINTS). Иначе — просто точка 0.
    uint8_t startIdx = (s_count < HISTORY_POINTS)
        ? 0
        : s_writeIdx;

    uint8_t written = 0;
    for (uint8_t i = 0; i < s_count && written < maxPoints; i++) {
        uint8_t idx = (startIdx + i) % HISTORY_POINTS;
        // Пропускаем "нулевые" точки (timestamp==0 = не заполнено)
        if (s_buffer[idx].timestamp > 0) {
            outPoints[written++] = s_buffer[idx];
        }
    }

    xSemaphoreGive(s_mutex);
    return written;
}

void HistoryLogger::clear() {
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

    memset(s_buffer, 0, sizeof(s_buffer));
    for (uint8_t i = 0; i < HISTORY_POINTS; i++) {
        s_buffer[i].temp       = NAN;
        s_buffer[i].humidity   = NAN;
        s_buffer[i].pressure   = NAN;
        s_buffer[i].lux        = NAN;
        s_buffer[i].waterLevel = NAN;
    }
    s_writeIdx = 0;
    s_count    = 0;

    xSemaphoreGive(s_mutex);

    saveToFile();
    Serial.println(F("[HIST] History cleared"));
}
