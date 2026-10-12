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
#include "mqtt_manager.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <Preferences.h>
#include <nvs.h>
#include <math.h>
#include <string.h>

// ═══════════════════════════════════════════════════════════════════
//  ФОРМАТ ФАЙЛА
// ═══════════════════════════════════════════════════════════════════
#define HIST_MAGIC     0x48495354UL   // "HIST"
#define HIST_VERSION   3  // v6.0: +DHT22 (2); v6.3: неделя, 168 точек (3)
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
    //
    // v6.2: перед rename стоял LittleFS.remove(HIST_FILE) — и он-то
    // атомарность и ломал. lfs_rename заменяет существующий файл сам,
    // одной транзакцией; предварительное удаление создавало окно, в
    // котором обрыв питания оставлял систему вообще без истории.
    if (!LittleFS.rename(tmpFile, HIST_FILE)) {
        Serial.println(F("[HIST] Rename failed"));
        LittleFS.remove(tmpFile);   // не копим мусор при отказе
    }
}

// ═══════════════════════════════════════════════════════════════════
//  КОПИЯ В NVS (v6.3)
// ═══════════════════════════════════════════════════════════════════
// История живёт не только в файле LittleFS: образ файловой системы при
// целиком, а форматирование при обрыве заливки или сбое монтирования
// NVS при этом не трогается ни прошивкой, ни образом FS.
//
// v6.3: неделя (168 точек) в формате HistoryPoint заняла бы 5,4 КБ — четверть
// всего NVS, а при перезаписи блоба NVS временно держит обе копии. Поэтому
// в NVS точки лежат упакованными: 18 байт вместо 32 (десятые доли в int16,
// уровень воды с шагом 0,5 %), блоб 8 + 168*18 = 3032 байта. Точность та же,
// что показывает панель (одна цифра после запятой).
//
// Запись раз в час даёт ~8,8 тыс. записей в год; NVS изнашивает страницы
// по кругу, стираний — сотни в год против ресурса порядка 100 тыс.
#define HIST_NVS_NS   "ghist"
#define HIST_NVS_KEY  "ring"

struct __attribute__((packed)) PackedPoint {
    uint32_t ts;        // 0 = слот пуст
    int16_t  t, h, p;   // ×10; INT16_MIN = нет данных
    uint16_t lux;       // 0xFFFF = нет данных
    uint8_t  w;         // ×2 (шаг 0,5 %); 0xFF = нет данных
    int16_t  t2, h2;    // ×10; INT16_MIN = нет данных
    uint8_t  pad;
};
static_assert(sizeof(PackedPoint) == 18, "PackedPoint layout");

struct __attribute__((packed)) HistoryNvsBlob {
    uint32_t     magic;
    uint8_t      version;
    uint8_t      writeIdx;
    uint8_t      count;
    uint8_t      reserved;
    PackedPoint  points[HISTORY_POINTS];
};

// Формат 6.2 (24 точки HistoryPoint без упаковки) — только для переноса
#define HIST_V2_POINTS 24
struct HistoryNvsBlobV2 {
    uint32_t     magic;
    uint8_t      version;
    uint8_t      writeIdx;
    uint8_t      count;
    uint8_t      reserved;
    HistoryPoint points[HIST_V2_POINTS];
};

static int16_t packX10(float v) {
    if (isnan(v)) return INT16_MIN;
    float r = roundf(v * 10.0f);
    if (r < -32767.0f) r = -32767.0f;
    if (r >  32767.0f) r =  32767.0f;
    return (int16_t)r;
}
static float unpackX10(int16_t v) { return v == INT16_MIN ? NAN : v / 10.0f; }

static void packPoint(const HistoryPoint& in, PackedPoint& out) {
    out.ts  = (uint32_t)in.timestamp;
    out.t   = packX10(in.temp);
    out.h   = packX10(in.humidity);
    out.p   = packX10(in.pressure);
    out.lux = isnan(in.lux) ? 0xFFFF : (uint16_t)constrain(lroundf(in.lux), 0L, 65534L);
    out.w   = isnan(in.waterLevel) ? 0xFF : (uint8_t)constrain(lroundf(in.waterLevel * 2.0f), 0L, 254L);
    out.t2  = packX10(in.dhtTemp);
    out.h2  = packX10(in.dhtHumidity);
    out.pad = 0;
}
static void unpackPoint(const PackedPoint& in, HistoryPoint& out) {
    out.timestamp   = (time_t)in.ts;
    out.temp        = unpackX10(in.t);
    out.humidity    = unpackX10(in.h);
    out.pressure    = unpackX10(in.p);
    out.lux         = in.lux == 0xFFFF ? NAN : (float)in.lux;
    out.waterLevel  = in.w == 0xFF ? NAN : in.w / 2.0f;
    out.dhtTemp     = unpackX10(in.t2);
    out.dhtHumidity = unpackX10(in.h2);
}

// Снимок буфера под мьютексом: файл и NVS пишутся без лока (это долго),
// а читать буфер при этом должен кто-то один за раз.
static bool snapshotBlob(HistoryNvsBlob& out) {
    if (!s_mutex || xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return false;
    out.magic    = HIST_MAGIC;
    out.version  = HIST_VERSION;
    out.writeIdx = s_writeIdx;
    out.count    = s_count;
    out.reserved = 0;
    for (uint16_t i = 0; i < HISTORY_POINTS; i++) packPoint(s_buffer[i], out.points[i]);
    xSemaphoreGive(s_mutex);
    return true;
}

static void saveToNvs() {
    static HistoryNvsBlob blob;      // 3 КБ: не на стеке задачи; вызывается из одной задачи за раз
    if (!snapshotBlob(blob)) {
        Serial.println(F("[HIST] NVS copy skipped: mutex timeout"));
        return;
    }
    Preferences prefs;
    if (!prefs.begin(HIST_NVS_NS, false)) {
        Serial.println(F("[HIST] NVS begin failed - backup copy NOT stored"));
        return;
    }
    if (prefs.putBytes(HIST_NVS_KEY, &blob, sizeof(blob)) != sizeof(blob)) {
        Serial.println(F("[HIST] NVS write FAILED - backup copy not updated"));
    }
    prefs.end();
}

// v6.3: копия формата 6.2 (сутки) — переносим её точки в недельный буфер
static bool migrateFromV2(Preferences& prefs) {
    static HistoryNvsBlobV2 old;
    if (prefs.getBytes(HIST_NVS_KEY, &old, sizeof(old)) != sizeof(old)) return false;
    if (old.magic != HIST_MAGIC || old.version != 2
        || old.writeIdx >= HIST_V2_POINTS || old.count > HIST_V2_POINTS) return false;
    const uint8_t start = (old.count < HIST_V2_POINTS) ? 0 : old.writeIdx;
    for (uint8_t i = 0; i < old.count; i++) {
        const HistoryPoint& p = old.points[(start + i) % HIST_V2_POINTS];
        if (p.timestamp == 0) continue;
        s_buffer[s_writeIdx] = p;
        s_writeIdx = (s_writeIdx + 1) % HISTORY_POINTS;
        if (s_count < HISTORY_POINTS) s_count++;
    }
    Serial.printf("[HIST] Migrated %u points from 6.2 NVS format\n", (unsigned)s_count);
    return true;
}

// true — буфер заполнен из NVS. Невалидное содержимое не трогает буфер.
static bool loadFromNvs() {
    static HistoryNvsBlob blob;
    Preferences prefs;
    if (!prefs.begin(HIST_NVS_NS, true)) return false;   // namespace ещё не создан
    const size_t len = prefs.getBytesLength(HIST_NVS_KEY);
    if (len == sizeof(HistoryNvsBlobV2)) {
        const bool ok = migrateFromV2(prefs);
        prefs.end();
        return ok;
    }
    if (len != sizeof(blob) || prefs.getBytes(HIST_NVS_KEY, &blob, sizeof(blob)) != sizeof(blob)) {
        prefs.end();
        return false;
    }
    prefs.end();

    if (blob.magic != HIST_MAGIC || blob.version != HIST_VERSION
        || blob.writeIdx >= HISTORY_POINTS || blob.count > HISTORY_POINTS) {
        Serial.println(F("[HIST] NVS backup invalid - ignored"));
        return false;
    }
    for (uint16_t i = 0; i < HISTORY_POINTS; i++) unpackPoint(blob.points[i], s_buffer[i]);
    s_writeIdx = blob.writeIdx;
    s_count    = blob.count;
    Serial.printf("[HIST] Restored %u points from NVS backup (writeIdx=%u)\n",
        (unsigned)s_count, (unsigned)s_writeIdx);
    return true;
}

// Запись истории во все места хранения. Сначала NVS — оно переживает
// всё, что может случиться с файловой системой.
// Записи из разных задач (почасовая — taskCore1, «очистить» — AsyncTCP)
// выстраиваются в очередь: иначе обе открывали бы один и тот же
// /history.tmp.
static SemaphoreHandle_t s_saveMutex = nullptr;

static void persistHistory() {
    if (!s_saveMutex) s_saveMutex = xSemaphoreCreateMutex();
    if (!s_saveMutex || xSemaphoreTake(s_saveMutex, pdMS_TO_TICKS(3000)) != pdTRUE) {
        Serial.println(F("[HIST] persist skipped: save mutex timeout"));
        return;
    }
    saveToNvs();
    saveToFile();
    xSemaphoreGive(s_saveMutex);
}

// Пустой буфер: NaN вместо нулей, чтобы пустые слоты не рисовались как 0.
static void resetBuffer() {
    memset(s_buffer, 0, sizeof(s_buffer));
    for (uint8_t i = 0; i < HISTORY_POINTS; i++) {
        s_buffer[i].temp        = NAN;
        s_buffer[i].humidity    = NAN;
        s_buffer[i].pressure    = NAN;
        s_buffer[i].lux         = NAN;
        s_buffer[i].waterLevel  = NAN;
        s_buffer[i].dhtTemp     = NAN;
        s_buffer[i].dhtHumidity = NAN;
    }
    s_writeIdx = 0;
    s_count    = 0;
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

    persistHistory();

    // v6.3: ту же точку — в MQTT (топик history) для внешнего архива.
    // Здесь только атомарный флаг: публикует loopTask (PubSubClient).
    MqttMgr::markHistoryPoint();
}

// ═══════════════════════════════════════════════════════════════════
//  ПУБЛИЧНЫЙ API
// ═══════════════════════════════════════════════════════════════════

void HistoryLogger::init() {
    if (!s_mutex) {
        s_mutex = xSemaphoreCreateMutex();
        configASSERT(s_mutex);
    }
    if (!s_saveMutex) {
        s_saveMutex = xSemaphoreCreateMutex();
        configASSERT(s_saveMutex);
    }

    // Загрузка: NVS → файл → пустая история. NVS главнее: оно пишется
    // первым (см. persistHistory), поэтому не бывает старше файла, а файл
    // Лок — на время правки буфера. init() вызывается из setup() до запуска
    // задач и веб-сервера (их обращения к буферу требуют уже созданного
    // мьютекса), лок оставлен на случай повторного вызова.
    enum { SRC_NONE, SRC_NVS, SRC_FILE } src = SRC_NONE;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        resetBuffer();
        if (loadFromNvs()) {
            src = SRC_NVS;
        } else {
            resetBuffer();           // на случай частично заполненного буфера
            if (loadFromFile()) src = SRC_FILE;
            else                resetBuffer();   // loadFromFile мог заполнить буфер частично
        }
        xSemaphoreGive(s_mutex);
    }

    if (src == SRC_NVS) {
        saveToFile();                // вернуть файл на место, если его не было
    } else if (src == SRC_FILE) {
        saveToNvs();                 // «засеять» копию: первый запуск после
    } else {
        // Истории нет нигде: создаём пустой файл, чтобы последующие записи
        // были быстрее. NVS пустым блобом не затираем.
        saveToFile();
    }

    // v6.3: неделя истории занимает ~3 КБ NVS; при перезаписи блоба NVS
    // временно держит обе копии. Печатаем запас — по нему видно, не тесно ли.
    nvs_stats_t st;
    if (nvs_get_stats(NULL, &st) == ESP_OK) {
        Serial.printf("[HIST] NVS entries: used %u, free %u of %u (32 B each)\n",
            (unsigned)st.used_entries, (unsigned)st.free_entries, (unsigned)st.total_entries);
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

bool HistoryLogger::getLast(HistoryPoint& out) {
    if (!s_mutex || xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return false;
    bool ok = false;
    if (s_count > 0) {
        const uint8_t idx = (s_writeIdx + HISTORY_POINTS - 1) % HISTORY_POINTS;
        out = s_buffer[idx];
        ok = out.timestamp > 0;
    }
    xSemaphoreGive(s_mutex);
    return ok;
}

void HistoryLogger::clear() {
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

    resetBuffer();

    xSemaphoreGive(s_mutex);

    // Очистка — явное действие пользователя: стираем и копию в NVS, иначе
    persistHistory();
    Serial.println(F("[HIST] History cleared"));
}
