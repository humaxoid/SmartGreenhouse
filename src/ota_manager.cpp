// ╔══════════════════════════════════════════════════════════════════╗
// ║                                                                  ║
// ║  v6.1 ИЗМЕНЕНИЯ:                                                ║
// ║   • Контейнер содержит firmware.bin + littlefs.bin с заголовком.║
// ║   • Заголовок: magic "GHFW" + версия + размеры + CRC32 обоих.   ║
// ║   • Прошивка сначала шьёт LittleFS (менее критичный), потом     ║
// ║     firmware. Если на каком-то этапе ошибка → откатываемся.     ║
// ║   • Старая поддержка раздельных firmware.bin/littlefs.bin       ║
// ║     удалена — формат только новый.                               ║
// ╚══════════════════════════════════════════════════════════════════╝

#include "ota_manager.h"
#include "settings.h"
#include "event_log.h"
#include "vent_controller.h"
#include "relay_manager.h"
#include "config.h"
#include "network_manager.h"
#include "auth_session.h"
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>
#include <mbedtls/sha256.h>
#include <mbedtls/pk.h>
#include <atomic>
#include "ota_pubkey.h"

namespace {

std::atomic<bool> s_isUpdating{false};
std::atomic<bool> s_rebootPending{false};
uint64_t          s_rebootAtMs = 0;

// v6.3: состояние незавершённой загрузки.
//  s_fsUnmounted  — раздел LittleFS размонтирован под запись образа; если
//                   загрузка сорвалась, он частично затёрт, и работать с ним
//                   нельзя до перезагрузки (на старте он переформатируется).
//  s_uploadActive — веб-загрузка принята и ещё не завершена; нужен, чтобы
//                   отличить обрыв соединения от обычного завершения запроса.
std::atomic<bool> s_fsUnmounted{false};
std::atomic<bool> s_uploadActive{false};
bool              s_arduinoOtaIsFs = false;
bool              s_arduinoOtaOn   = false;

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

// v6.3: формат 2 — подписанный. За 32-байтной частью (formatVer = 2) идут
// SHA-256 прошивки и образа FS, длина подписи и подпись ECDSA P-256 (DER)
// байтов 0..95, всего 176 байт (см. tools/pack_firmware.py). Подпись
// проверяется сразу после заголовка, до первой записи во флеш; хэши —
// перед Update.end() каждой части, то есть до переключения загрузки.
#define GHFW_HDR_V1       32
#define GHFW_HDR_V2       176
#define GHFW_SIGNED_PART  96
#define GHFW_SIG_OFF      100
#define GHFW_SIG_MAX      72

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
    uint8_t     hdrBuf[GHFW_HDR_V2];
    uint32_t    hdrNeed;         // сколько байт заголовка ждём: 32, для формата 2 — 176
    uint32_t    hdrFilled;       // сколько байт заголовка собрано
    bool        hashed;          // формат 2: сверять SHA-256 частей
    uint32_t    fwWritten;       // сколько firmware байт принято
    uint32_t    fsWritten;       // сколько littlefs байт принято
    uint32_t    fwCalcCrc;       // вычисленный CRC firmware
    uint32_t    fsCalcCrc;       // вычисленный CRC littlefs
    char        errMsg[80];
};
static UploadState s_up = {};
static mbedtls_sha256_context s_sha;   // хэш текущей части (прошивка, затем FS), формат 2

// Хэш части: начать / дописать / сверить с заголовком (и освободить контекст).
static void shaBegin() {
    mbedtls_sha256_init(&s_sha);
    mbedtls_sha256_starts_ret(&s_sha, 0);
}
static bool shaMatches(const uint8_t* want) {
    uint8_t got[32];
    const bool ok = mbedtls_sha256_finish_ret(&s_sha, got) == 0 && memcmp(got, want, 32) == 0;
    mbedtls_sha256_free(&s_sha);
    return ok;
}

#ifdef OTA_PUBKEY_PEM
// Подпись ECDSA P-256 / SHA-256 первых GHFW_SIGNED_PART байт заголовка.
static bool signatureOk(const uint8_t* hdr) {
    const uint16_t sigLen = (uint16_t)(hdr[96] | (hdr[97] << 8));
    if (sigLen == 0 || sigLen > GHFW_SIG_MAX) return false;
    uint8_t h[32];
    if (mbedtls_sha256_ret(hdr, GHFW_SIGNED_PART, h, 0) != 0) return false;
    static const char PEM[] = OTA_PUBKEY_PEM;
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    const bool ok =
           mbedtls_pk_parse_public_key(&pk, (const unsigned char*)PEM, sizeof(PEM)) == 0
        && mbedtls_pk_can_do(&pk, MBEDTLS_PK_ECKEY)
        && mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, h, sizeof(h),
                             hdr + GHFW_SIG_OFF, sigLen) == 0;
    mbedtls_pk_free(&pk);
    return ok;
}
#endif

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
    VentCtrl::forceStopAll();   // согласовать учёт VentCtrl с обесточенными реле

    {
        VentStatusSnapshot vs;
        VentCtrl::getStatus(vs);
        bool changed = false;
        {
            MutexGuard sLock(g_settingsMutex, pdMS_TO_TICKS(100));
            if (sLock.locked()) {
                g_settings.ventUpperPos.lastKnownPct  = vs.topPct;
                g_settings.ventUpperPos.positionValid = 1;
                g_settings.ventLowerPos.lastKnownPct  = vs.bottomPct;
                g_settings.ventLowerPos.positionValid = 1;
                changed = true;
            }
        }
        // Немедленно, но вне лока настроек: settingsSave() берёт свой мьютекс
        // сохранения, а под ним — g_settingsMutex; обратный порядок здесь
        // упирался бы в таймауты при встречном отложенном сохранении.
        if (changed) settingsSave();
    }

    if (isLittleFs) {
        Serial.println(F("[OTA] Unmounting LittleFS..."));
        // Флаг — ДО размонтирования и записи: если связь или питание пропадут
        // посреди образа, на старте раздел будет отформатирован (main.cpp), а
        // не смонтирован как есть — из смеси старых и новых блоков.
        fsDirtySet(true);
        LittleFS.end();
        s_fsUnmounted.store(true);
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

// снимался только в ответе на запрос (handleUpdateFinish), а при ОБРЫВЕ
// соединения посреди загрузки этот обработчик не вызывается вовсе: флаг
// оставался поднятым, и taskCore1 пропускал всю автоматику до перезагрузки —
// не закрывались форточки по дождю, не останавливались поливы и насос.
//
// Если файловая система уже размонтирована, она могла быть записана
// частично; работать с ней нельзя, поэтому перезагружаемся: на старте
// раздел переформатируется, а панель-восстановление (см.
// Флаг s_isUpdating до перезагрузки остаётся поднятым намеренно: моторы
// и реле уже обесточены, а HistoryLogger не должен писать в мёртвый раздел.
static void abortFailedUpdate() {
    EventLog::add(EV_OTA, 0, 0);
    if (Update.isRunning()) Update.abort();
    if (s_fsUnmounted.load()) {
        Serial.println(F("[OTA] Update failed after LittleFS was unmounted — rebooting"));
        s_rebootAtMs = steadyMs() + 1500;
        s_rebootPending.store(true);
    } else {
        s_isUpdating.store(false);
        esp_task_wdt_add(NULL);
        Serial.println(F("[OTA] Update failed — automation resumed"));
    }
}

// Вызывается на каждый chunk данных файла.
static void processBytes(const uint8_t* data, size_t len) {
    if (s_up.phase == UP_ERROR) return;

    size_t pos = 0;

    // ── Phase 1: собираем заголовок (32 байта, формат 2 — 176) ────
    if (s_up.phase == UP_HEADER) {
        for (;;) {
            size_t need = s_up.hdrNeed - s_up.hdrFilled;
            size_t take = (len - pos < need) ? (len - pos) : need;
            memcpy(&s_up.hdrBuf[s_up.hdrFilled], data + pos, take);
            s_up.hdrFilled += take;
            pos += take;

            if (s_up.hdrFilled < s_up.hdrNeed) return;   // ещё не весь

            if (s_up.hdrNeed == GHFW_HDR_V1) {
                memcpy(&s_up.hdr, s_up.hdrBuf, sizeof(GhfwHeader));
                if (memcmp(s_up.hdr.magic, "GHFW", 4) != 0) {
                    setError("Invalid magic (not a greenhouse_vX.X.bin)");
                    return;
                }
                if (s_up.hdr.formatVer == 2) {      // дочитать хэши и подпись
                    s_up.hdrNeed = GHFW_HDR_V2;
                    continue;
                }
                if (s_up.hdr.formatVer != 1) {
                    setError("Unsupported format version");
                    return;
                }
            }
            break;
        }

#ifdef OTA_PUBKEY_PEM
        if (s_up.hdr.formatVer != 2) {
            setError("Unsigned container rejected: signed updates only");
            return;
        }
        if (!signatureOk(s_up.hdrBuf)) {
            setError("Container signature is invalid");
            return;
        }
        Serial.println(F("[OTA] Container signature OK"));
#endif
        s_up.hashed = (s_up.hdr.formatVer == 2);
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
        gracefulShutdown(false);
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
        if (s_up.hashed) shaBegin();
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
            if (s_up.hashed) mbedtls_sha256_update_ret(&s_sha, data + pos, take);
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
            // Формат 2: прошивка обязана совпасть с подписанным хэшем — до
            // Update.end(), который переключает загрузку на новый раздел.
            if (s_up.hashed && !shaMatches(s_up.hdrBuf + GHFW_HDR_V1)) {
                setError("FW hash does not match the signed header");
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
            if (s_up.hashed) shaBegin();
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
            if (s_up.hashed) mbedtls_sha256_update_ret(&s_sha, data + pos, take);
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
            if (s_up.hashed && !shaMatches(s_up.hdrBuf + GHFW_HDR_V1 + 32)) {
                setError("FS hash does not match the signed header");
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
            fsDirtySet(false);   // образ записан целиком и проверен
            s_up.phase = UP_DONE;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
//  WEB-СЕРВЕР: ОБРАБОТКА UPLOAD
// ═══════════════════════════════════════════════════════════════════
// Сессия (Bearer) или Basic, как у остального API. Раньше пустые логин и
// не принимается: иначе любой в сети заливал бы свою прошивку по паролю
// из инструкции.
static bool otaCheckAuth(AsyncWebServerRequest* req) {
    if (AuthSess::authorized(req)) {
        if (!NetMgr::credsAreDefault()) return true;
        // Исключение — страница восстановления (панели на флеше нет): сменить
        // Пустую файловую систему снаружи не устроить.
        if (!LittleFS.exists("/index.html")) return true;
        Serial.println(F("[OTA] Rejected: default credentials still in use"));
        req->send(403, "application/json", "{\"ok\":false,\"error\":\"default_credentials\"}");
        return false;
    }
    // v6.1: 401 без WWW-Authenticate — как и в requireAuth().
    // Загрузку прошивки инициирует панель через XHR и сама подписывает
    // запрос, поэтому системное окно ввода пароля только мешало бы
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
        s_uploadActive.store(true);
        // Обрыв соединения: обработчик завершения запроса в этом случае не
        // вызывается, зато вызывается onDisconnect. Если к этому моменту
        // загрузка не доведена до конца — это сбой (см. abortFailedUpdate).
        req->onDisconnect([]() {
            if (s_uploadActive.exchange(false)) {
                setError("Connection lost during upload");
                abortFailedUpdate();
            }
        });

        // Сброс состояния
        s_up.phase     = UP_HEADER;
        s_up.hdrNeed   = GHFW_HDR_V1;
        s_up.hdrFilled = 0;
        s_up.hashed    = false;
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

    // Загрузка дошла до конца запроса — дальше отвечаем за исход мы, а
    // не обработчик обрыва соединения.
    s_uploadActive.store(false);

    bool ok = (s_up.phase == UP_DONE);
    if (ok) EventLog::add(EV_OTA, 0, 1);   // отказ записывает abortFailedUpdate()
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
        abortFailedUpdate();
    }
}

// ═══════════════════════════════════════════════════════════════════
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

    // Раньше это был пароль панели: кража сессии панели (XSS) или подсмотренный
    // Basic-заголовок означали право залить произвольную прошивку мимо проверки
    // [env:ota] с --auth=<пароль>), поэтому по умолчанию выключен; пароль
    // задаётся в «Настройки → Доступ» и действует после перезагрузки.
    const NetCache& nc = NetMgr::cache();
    if (nc.otaPass[0] == '\0') {
        s_arduinoOtaOn = false;
        Serial.println(F("[OTA] ArduinoOTA disabled (no OTA password set)"));
        return;
    }
    s_arduinoOtaOn = true;
    ArduinoOTA.setPassword(nc.otaPass);
    Serial.println(F("[OTA] ArduinoOTA: enabled with its own password"));

    ArduinoOTA.onStart([]() {
        bool isFs = (ArduinoOTA.getCommand() == U_SPIFFS);
        s_arduinoOtaIsFs = isFs;
        Serial.printf("[OTA] ArduinoOTA start (%s)\n", isFs ? "FS" : "FW");
        s_isUpdating.store(true);
        gracefulShutdown(isFs);
        esp_task_wdt_delete(NULL);
    });
    ArduinoOTA.onEnd([]() {
        if (s_arduinoOtaIsFs) fsDirtySet(false);   // образ записан целиком
        Serial.println(F("[OTA] ArduinoOTA end"));
        // журнал сохраняем здесь, до OtaMgr::loop дело не дойдёт.
        EventLog::add(EV_OTA, 0, 1);
        EventLog::flush();
    });
    ArduinoOTA.onError([](ota_error_t err) {
        Serial.printf("[OTA] ArduinoOTA error: %u\n", (unsigned)err);
        abortFailedUpdate();
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
    server.on(
        "/update", HTTP_POST,
        handleUpdateFinish,             // ответ после загрузки
        handleUploadChunk,              // chunk файла
        nullptr                         // body (не используется)
    );
    Serial.println(F("[OTA] /update endpoint registered (greenhouse_vX.Y.bin format)"));

    if (s_arduinoOtaOn) {
        ArduinoOTA.begin();
        Serial.printf("[OTA] ArduinoOTA started (mDNS: %s.local)\n", ArduinoOTA.getHostname().c_str());
    }
}

void OtaMgr::loop() {
    // v6.1 FIX: раньше handle() вызывался ТОЛЬКО при s_isUpdating==true, а этот
    // PlatformIO по сети было нерабочим. handle() обязан вызываться всегда:
    // он слушает mDNS/UDP и сам решает, началась ли сессия.
    if (s_arduinoOtaOn) ArduinoOTA.handle();
    if (s_rebootPending.load() && steadyMs() >= s_rebootAtMs) {
        s_rebootPending.store(false);
        Serial.println(F("[OTA] Rebooting now..."));
        EventLog::flush();
        Serial.flush();
        delay(100);
        ESP.restart();
    }
}

bool OtaMgr::isUpdating() {
    return s_isUpdating.load();
}
