// ╔══════════════════════════════════════════════════════════════════╗
// ║  ota_manager.cpp — OTA обновления v6.1                           ║
// ║                                                                  ║
// ║  v6.1 ИЗМЕНЕНИЯ:                                                ║
// ║   • Принимаем единый файл greenhouse_vX.X.bin (контейнер).      ║
// ║   • Контейнер содержит firmware.bin + littlefs.bin с заголовком.║
// ║   • Заголовок: magic "GHFW" + версия + размеры + CRC32 обоих.   ║
// ║   • Прошивка сначала шьёт LittleFS (менее критичный), потом     ║
// ║     firmware. Если на каком-то этапе ошибка → откатываемся.     ║
// ║   • Старая поддержка раздельных firmware.bin/littlefs.bin       ║
// ║     удалена — формат только новый.                               ║
// ╚══════════════════════════════════════════════════════════════════╝

#include "ota_manager.h"
#include "settings.h"
#include "vent_controller.h"
#include "relay_manager.h"
#include "config.h"
#include "network_manager.h"
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>
#include <atomic>

namespace {

std::atomic<bool> s_isUpdating{false};
std::atomic<bool> s_rebootPending{false};
uint64_t          s_rebootAtMs = 0;

// ═══════════════════════════════════════════════════════════════════
//  ФОРМАТ КОНТЕЙНЕРА (заголовок 32 байта)
// ═══════════════════════════════════════════════════════════════════
struct __attribute__((packed)) GhfwHeader {
    char     magic[4];      // "GHFW"
    uint8_t  formatVer;     // 1
    uint8_t  fwMajor;       // версия прошивки
    uint8_t  fwMinor;
    uint8_t  reserved1;     // = 0
    uint32_t fwSize;        // размер firmware.bin, байт
    uint32_t fsSize;        // размер littlefs.bin, байт
    uint32_t fwCrc;         // CRC32 IEEE 802.3 firmware
    uint32_t fsCrc;         // CRC32 IEEE 802.3 littlefs
    uint8_t  reserved2[8];  // = zeros
};
static_assert(sizeof(GhfwHeader) == 32, "GhfwHeader must be exactly 32 bytes");

// ═══════════════════════════════════════════════════════════════════
//  СОСТОЯНИЕ ПРИЁМА КОНТЕЙНЕРА
// ═══════════════════════════════════════════════════════════════════
// AsyncWebServer передаёт файл частями. Нам нужно собрать заголовок,
// потом передать firmware в Update (cmd=U_FLASH), затем littlefs (U_SPIFFS).
enum UploadPhase : uint8_t {
    UP_IDLE   = 0,
    UP_HEADER = 1,    // собираем 32 байта заголовка
    UP_FW     = 2,    // пишем firmware
    UP_FS     = 3,    // пишем littlefs
    UP_DONE   = 4,
    UP_ERROR  = 5
};

struct UploadState {
    UploadPhase phase;
    GhfwHeader  hdr;
    uint8_t     hdrBuf[sizeof(GhfwHeader)];
    uint32_t    hdrFilled;       // сколько байт заголовка собрано
    uint32_t    fwWritten;       // сколько firmware байт принято
    uint32_t    fsWritten;       // сколько littlefs байт принято
    uint32_t    fwCalcCrc;       // вычисленный CRC firmware
    uint32_t    fsCalcCrc;       // вычисленный CRC littlefs
    char        errMsg[80];
};
static UploadState s_up = {};

// ═══════════════════════════════════════════════════════════════════
//  CRC32 (IEEE 802.3)
// ═══════════════════════════════════════════════════════════════════
static uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t len) {
    crc = ~crc;
    while (len--) {
        crc ^= *data++;
        for (int k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(int32_t)(crc & 1));
        }
    }
    return ~crc;
}

// ═══════════════════════════════════════════════════════════════════
//  ПРЕДВАРИТЕЛЬНОЕ ОТКЛЮЧЕНИЕ ПЕРЕД OTA
// ═══════════════════════════════════════════════════════════════════
// Реле OFF, форточки в STOP, LittleFS unmount (только перед FS upd).
static void gracefulShutdown(bool isLittleFs) {
    Serial.println(F("[OTA] Graceful shutdown..."));

    // 1. Моторы форточек — СТОП.
    //
    // v6.1 FIX: здесь стоял RelayMgr::setRelay(), который реле форточек
    // переключать ОТКАЗЫВАЕТСЯ:
    //     if (isVentRelay(idx)) { ...WARN... return false; }
    // То есть все четыре вызова были no-op с руганью в лог, и моторы
    // продолжали крутиться всю запись флеша и перезагрузку — секунд
    // двадцать-тридцать. Створка успевала дойти до концевика и стоять
    // под током.
    //
    // v6.2: через forceOff(), а не setRelayDirect(). Разница в том, что
    // forceOff ещё и снимает отложенное включение H-моста: без этого
    // updatePending() мог через 100 мс добросовестно включить реле,
    // которое мы только что обесточили перед записью флеша.
    RelayMgr::forceOff(RELAY_IDX_VENT_UPPER_OPEN);
    RelayMgr::forceOff(RELAY_IDX_VENT_UPPER_CLOSE);
    RelayMgr::forceOff(RELAY_IDX_VENT_LOWER_OPEN);
    RelayMgr::forceOff(RELAY_IDX_VENT_LOWER_CLOSE);

    {
        VentStatusSnapshot vs;
        VentCtrl::getStatus(vs);
        MutexGuard sLock(g_settingsMutex, pdMS_TO_TICKS(100));
        if (sLock.locked()) {
            g_settings.ventUpperPos.lastKnownPct  = vs.topPct;
            g_settings.ventUpperPos.positionValid = 1;
            g_settings.ventLowerPos.lastKnownPct  = vs.bottomPct;
            g_settings.ventLowerPos.positionValid = 1;
            settingsSave();   // немедленно
        }
    }

    if (isLittleFs) {
        Serial.println(F("[OTA] Unmounting LittleFS..."));
        LittleFS.end();
    }
}

// ═══════════════════════════════════════════════════════════════════
//  ОБРАБОТКА БАЙТОВ КОНТЕЙНЕРА
// ═══════════════════════════════════════════════════════════════════
static void setError(const char* msg) {
    s_up.phase = UP_ERROR;
    snprintf(s_up.errMsg, sizeof(s_up.errMsg), "%s", msg);
    Serial.printf("[OTA] ERROR: %s\n", msg);
    if (Update.isRunning()) Update.abort();
}

// Вызывается на каждый chunk данных файла.
static void processBytes(const uint8_t* data, size_t len) {
    if (s_up.phase == UP_ERROR) return;

    size_t pos = 0;

    // ── Phase 1: собираем заголовок 32 байта ─────────────────────
    if (s_up.phase == UP_HEADER) {
        size_t need = sizeof(GhfwHeader) - s_up.hdrFilled;
        size_t take = (len < need) ? len : need;
        memcpy(&s_up.hdrBuf[s_up.hdrFilled], data, take);
        s_up.hdrFilled += take;
        pos += take;

        if (s_up.hdrFilled < sizeof(GhfwHeader)) return;   // ещё не весь

        // Заголовок собран — проверяем
        memcpy(&s_up.hdr, s_up.hdrBuf, sizeof(GhfwHeader));

        if (memcmp(s_up.hdr.magic, "GHFW", 4) != 0) {
            setError("Invalid magic (not a greenhouse_vX.X.bin)");
            return;
        }
        if (s_up.hdr.formatVer != 1) {
            setError("Unsupported format version");
            return;
        }
        // Sanity на размеры
        if (s_up.hdr.fwSize < 100000 || s_up.hdr.fwSize > 1769472) {  // app0=1728KB
            setError("Firmware size out of range");
            return;
        }
        if (s_up.hdr.fsSize < 1000 || s_up.hdr.fsSize > 589824) {     // spiffs=576KB
            setError("LittleFS size out of range");
            return;
        }

        Serial.printf("[OTA] Container: v%u.%u, FW=%u bytes, FS=%u bytes\n",
            (unsigned)s_up.hdr.fwMajor, (unsigned)s_up.hdr.fwMinor,
            (unsigned)s_up.hdr.fwSize,  (unsigned)s_up.hdr.fsSize);

        // ── Начинаем с LittleFS (менее критично — если не получится,
        //    firmware ещё не тронули) ─ ВАЖНО: на самом деле порядок
        //    в файле: firmware → littlefs. Поэтому сначала идёт FW.
        gracefulShutdown(false);   // пока не FS-обновление
        esp_task_wdt_delete(NULL);

        if (!Update.begin(s_up.hdr.fwSize, U_FLASH)) {
            char buf[80];
            snprintf(buf, sizeof(buf), "Update.begin(FW) failed: %s",
                Update.errorString());
            setError(buf);
            return;
        }
        s_up.phase     = UP_FW;
        s_up.fwWritten = 0;
        s_up.fwCalcCrc = 0;
        Serial.println(F("[OTA] Phase: writing FIRMWARE"));
    }

    // ── Phase 2: пишем firmware ─────────────────────────────────
    if (s_up.phase == UP_FW && pos < len) {
        size_t remaining = s_up.hdr.fwSize - s_up.fwWritten;
        size_t take = (len - pos < remaining) ? (len - pos) : remaining;

        if (take > 0) {
            if (Update.write((uint8_t*)(data + pos), take) != take) {
                char buf[80];
                snprintf(buf, sizeof(buf), "FW write error: %s",
                    Update.errorString());
                setError(buf);
                return;
            }
            s_up.fwCalcCrc = crc32Update(s_up.fwCalcCrc, data + pos, take);
            s_up.fwWritten += take;
            pos += take;
        }

        // Firmware закончилось
        if (s_up.fwWritten == s_up.hdr.fwSize) {
            // Проверка CRC firmware
            if (s_up.fwCalcCrc != s_up.hdr.fwCrc) {
                char buf[80];
                snprintf(buf, sizeof(buf), "FW CRC mismatch: got %08X, expected %08X",
                    (unsigned)s_up.fwCalcCrc, (unsigned)s_up.hdr.fwCrc);
                setError(buf);
                return;
            }
            // Завершаем firmware update (без перезагрузки)
            if (!Update.end(true)) {
                char buf[80];
                snprintf(buf, sizeof(buf), "FW Update.end failed: %s",
                    Update.errorString());
                setError(buf);
                return;
            }
            Serial.printf("[OTA] Firmware OK (%u bytes, CRC valid)\n",
                (unsigned)s_up.fwWritten);

            // Переходим к LittleFS
            gracefulShutdown(true);    // unmount LittleFS
            if (!Update.begin(s_up.hdr.fsSize, U_SPIFFS)) {
                char buf[80];
                snprintf(buf, sizeof(buf), "Update.begin(FS) failed: %s",
                    Update.errorString());
                setError(buf);
                return;
            }
            s_up.phase     = UP_FS;
            s_up.fsWritten = 0;
            s_up.fsCalcCrc = 0;
            Serial.println(F("[OTA] Phase: writing LITTLEFS"));
        }
    }

    // ── Phase 3: пишем littlefs ─────────────────────────────────
    if (s_up.phase == UP_FS && pos < len) {
        size_t remaining = s_up.hdr.fsSize - s_up.fsWritten;
        size_t take = (len - pos < remaining) ? (len - pos) : remaining;

        if (take > 0) {
            if (Update.write((uint8_t*)(data + pos), take) != take) {
                char buf[80];
                snprintf(buf, sizeof(buf), "FS write error: %s",
                    Update.errorString());
                setError(buf);
                return;
            }
            s_up.fsCalcCrc = crc32Update(s_up.fsCalcCrc, data + pos, take);
            s_up.fsWritten += take;
            pos += take;
        }

        if (s_up.fsWritten == s_up.hdr.fsSize) {
            if (s_up.fsCalcCrc != s_up.hdr.fsCrc) {
                char buf[80];
                snprintf(buf, sizeof(buf), "FS CRC mismatch: got %08X, expected %08X",
                    (unsigned)s_up.fsCalcCrc, (unsigned)s_up.hdr.fsCrc);
                setError(buf);
                return;
            }
            if (!Update.end(true)) {
                char buf[80];
                snprintf(buf, sizeof(buf), "FS Update.end failed: %s",
                    Update.errorString());
                setError(buf);
                return;
            }
            Serial.printf("[OTA] LittleFS OK (%u bytes, CRC valid)\n",
                (unsigned)s_up.fsWritten);
            s_up.phase = UP_DONE;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
//  WEB-СЕРВЕР: ОБРАБОТКА UPLOAD
// ═══════════════════════════════════════════════════════════════════
static bool otaCheckAuth(AsyncWebServerRequest* req) {
    const NetCache& nc = NetMgr::cache();
    if (nc.webUser[0] == 0 && nc.webPass[0] == 0) return true;
    if (req->authenticate(nc.webUser, nc.webPass)) return true;
    // v6.1: 401 без WWW-Authenticate — как и в requireAuth().
    // Загрузку прошивки инициирует панель через XHR и сама подписывает
    // запрос, поэтому системное окно ввода пароля только мешало бы
    // посреди процесса обновления.
    Serial.printf("[AUTH] 401 %s %s (OTA)\n",
        req->methodToString(), req->url().c_str());
    req->send(401, "application/json", "{\"error\":\"unauthorized\"}");
    return false;
}

static void handleUploadChunk(AsyncWebServerRequest* req,
                              String filename, size_t index,
                              uint8_t* data, size_t len, bool final) {
    if (!otaCheckAuth(req)) return;

    if (index == 0) {
        // Валидация имени файла: greenhouse_vX.Y.bin (X.Y = любые цифры)
        bool nameOk = false;
        if (filename.startsWith("greenhouse_v") && filename.endsWith(".bin")) {
            // Проверяем что между _v и .bin — формат "число.число"
            int vPos = 12;  // длина "greenhouse_v"
            int dotBin = filename.length() - 4;  // позиция ".bin"
            int innerDot = -1;
            bool allDigitsOk = true;
            for (int i = vPos; i < dotBin; i++) {
                char c = filename.charAt(i);
                if (c == '.') {
                    if (innerDot >= 0) { allDigitsOk = false; break; }
                    innerDot = i;
                } else if (c < '0' || c > '9') {
                    allDigitsOk = false; break;
                }
            }
            nameOk = allDigitsOk && innerDot > vPos && innerDot < dotBin - 1;
        }
        if (!nameOk) {
            Serial.printf("[OTA] Rejected filename: %s (must be greenhouse_vX.Y.bin)\n",
                filename.c_str());
            setError("Invalid filename");
            return;
        }

        Serial.printf("[OTA] Web upload: %s\n", filename.c_str());
        s_isUpdating.store(true);

        // Сброс состояния
        s_up.phase     = UP_HEADER;
        s_up.hdrFilled = 0;
        s_up.errMsg[0] = 0;
        memset(&s_up.hdr, 0, sizeof(s_up.hdr));
    }

    if (s_up.phase == UP_ERROR) {
        // Уже в ошибке — продолжать нечего, просто принимаем оставшиеся
        // байты (AsyncWebServer не даёт способ прервать поток)
        return;
    }

    if (len > 0) {
        processBytes(data, len);
    }

    if (final) {
        if (s_up.phase == UP_DONE) {
            Serial.println(F("[OTA] Container fully processed — OK"));
        } else if (s_up.phase != UP_ERROR) {
            setError("Container truncated (file ended too early)");
        }
    }
}

static void handleUpdateFinish(AsyncWebServerRequest* req) {
    if (!otaCheckAuth(req)) return;

    bool ok = (s_up.phase == UP_DONE);
    JsonDocument doc;
    doc["ok"] = ok;
    if (!ok) {
        doc["error"] = s_up.errMsg[0] ? s_up.errMsg : "Unknown error";
    } else {
        char msg[80];
        snprintf(msg, sizeof(msg), "Update v%u.%u successful, rebooting...",
            (unsigned)s_up.hdr.fwMajor, (unsigned)s_up.hdr.fwMinor);
        doc["message"] = msg;
    }

    char buf[256];
    size_t n = serializeJson(doc, buf, sizeof(buf));
    AsyncWebServerResponse* resp = req->beginResponse(ok ? 200 : 400,
        "application/json", buf);
    req->send(resp);

    if (ok) {
        s_rebootPending.store(true);
        s_rebootAtMs = steadyMs() + 1500;
    } else {
        s_isUpdating.store(false);
        esp_task_wdt_add(NULL);
    }
}

// ═══════════════════════════════════════════════════════════════════
//  ArduinoOTA (для разработки через USB/IDE)
// ═══════════════════════════════════════════════════════════════════
void setupArduinoOTA() {
    // v6.1: уникальный hostname на основе последних 3 байт MAC,
    // чтобы две теплицы в одной сети не конфликтовали в mDNS.
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char hostname[32];
    snprintf(hostname, sizeof(hostname), "greenhouse-%02X%02X%02X",
        mac[3], mac[4], mac[5]);
    ArduinoOTA.setHostname(hostname);

    // v6.1: ПАРОЛЬ НА СЕТЕВУЮ ПРОШИВКУ.
    // Раньше setPassword() не вызывался вообще — но канал ArduinoOTA был
    // сломан (handle() никогда не выполнялся), и дыра не проявлялась.
    // После починки канала он заработал, и без пароля любой в локальной
    // сети смог бы залить в теплицу произвольную прошивку.
    //
    // Пароль берём тот же, что у веб-панели: одна учётка вместо двух,
    // и секрет не хранится ни в исходниках, ни в platformio.ini.
    // В окружении [env:ota] его надо передать как --auth=<пароль>.
    const NetCache& nc = NetMgr::cache();
    if (nc.webPass[0] != '\0') {
        ArduinoOTA.setPassword(nc.webPass);
        Serial.println(F("[OTA] ArduinoOTA: password set (same as web panel password)"));
    } else {
        Serial.println(F("[OTA] !!! WARNING: web panel password is empty -"));
        Serial.println(F("[OTA] !!! network flashing is OPEN to everyone on the LAN."));
        Serial.println(F("[OTA] !!! Set a password under Settings -> Access."));
    }

    ArduinoOTA.onStart([]() {
        bool isFs = (ArduinoOTA.getCommand() == U_SPIFFS);
        Serial.printf("[OTA] ArduinoOTA start (%s)\n", isFs ? "FS" : "FW");
        s_isUpdating.store(true);
        gracefulShutdown(isFs);
        esp_task_wdt_delete(NULL);
    });
    ArduinoOTA.onEnd([]() {
        Serial.println(F("[OTA] ArduinoOTA end"));
    });
    ArduinoOTA.onError([](ota_error_t err) {
        Serial.printf("[OTA] ArduinoOTA error: %u\n", (unsigned)err);
        s_isUpdating.store(false);
        esp_task_wdt_add(NULL);
    });
}

}  // anonymous namespace

// ═══════════════════════════════════════════════════════════════════
//  ПУБЛИЧНЫЙ API
// ═══════════════════════════════════════════════════════════════════
void OtaMgr::init() {
    setupArduinoOTA();
    Serial.println(F("[OTA] ArduinoOTA configured (not started yet)"));
}

void OtaMgr::attachTo(AsyncWebServer& server) {
    // POST /update — загрузка контейнера greenhouse_vX.Y.bin
    server.on(
        "/update", HTTP_POST,
        handleUpdateFinish,             // ответ после загрузки
        handleUploadChunk,              // chunk файла
        nullptr                         // body (не используется)
    );
    Serial.println(F("[OTA] /update endpoint registered (greenhouse_vX.Y.bin format)"));

    // Запуск ArduinoOTA здесь
    ArduinoOTA.begin();
    Serial.printf("[OTA] ArduinoOTA started (mDNS: %s.local)\n", ArduinoOTA.getHostname().c_str());
}

void OtaMgr::loop() {
    // v6.1 FIX: раньше handle() вызывался ТОЛЬКО при s_isUpdating==true, а этот
    // флаг выставляется из ArduinoOTA.onStart, который срабатывает изнутри самого
    // handle(). Замкнутый круг — ArduinoOTA никогда не стартовал, обновление из
    // PlatformIO по сети было нерабочим. handle() обязан вызываться всегда:
    // он слушает mDNS/UDP и сам решает, началась ли сессия.
    ArduinoOTA.handle();
    // Перезагрузка после успешного обновления
    if (s_rebootPending.load() && steadyMs() >= s_rebootAtMs) {
        s_rebootPending.store(false);
        Serial.println(F("[OTA] Rebooting now..."));
        Serial.flush();
        delay(100);
        ESP.restart();
    }
}

bool OtaMgr::isUpdating() {
    return s_isUpdating.load();
}
