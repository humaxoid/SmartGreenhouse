// ╔══════════════════════════════════════════════════════════════════╗
// ║  telemetry.cpp — анонимная статистика разработчику (см. .h)      ║
// ╚══════════════════════════════════════════════════════════════════╝

#include "telemetry.h"
#include "config.h"
#include "settings.h"
#include "network_manager.h"
#include "ota_manager.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <mbedtls/sha256.h>
#include <atomic>
#include <string.h>
#include <time.h>

// Корневые сертификаты, встроенные в ESP-IDF (пакет Mozilla): приёмник может
// стоять за любым публичным центром сертификации, в том числе Let's Encrypt.
// Проверка включена всегда; setInsecure() не используется — иначе любой
// в сети мог бы подменить приёмник.
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");

#define TLM_NVS_NS        "tlm"
#define TLM_PERIOD_MS     3600000ULL     // раз в час
#define TLM_FIRST_MS      90000ULL       // первое сообщение — не раньше чем через 90 с после запуска
#define TLM_RETRY_MS      600000ULL      // после неудачи — через 10 минут
#define TLM_TIMEOUT_MS    6000
#define TLM_MIN_BLOCK     45000          // нужен непрерывный блок под TLS-буферы (2×16 КБ + служебные)

static std::atomic<bool> s_enabled{false};   // пишет AsyncTCP (setEnabled), читает loopTask
static std::atomic<bool> s_kickReq{false};   // «включили — отправить в ближайшую минуту»
static uint32_t     s_install = 0;       // Unix-время первого запуска с часами; 0 — ещё не знаем
static char         s_id[17] = "";
static bool         s_bootSent = false;  // первое сообщение после запуска ещё не ушло
static uint64_t     s_nextMs = 0;
static uint64_t     s_lastTry = 0;
static uint64_t     s_retryMs = 600000ULL;   // растёт при подряд неудачах
static std::atomic<uint32_t> s_lastOkTs{0};
static std::atomic<int16_t>  s_lastCode{0};
static std::atomic<uint32_t> s_sent{0};

static const char* resetName(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON:   return "POWERON";
        case ESP_RST_EXT:       return "EXT";
        case ESP_RST_SW:        return "SW";
        case ESP_RST_PANIC:     return "PANIC";
        case ESP_RST_INT_WDT:   return "INT_WDT";
        case ESP_RST_TASK_WDT:  return "TASK_WDT";
        case ESP_RST_WDT:       return "WDT";
        case ESP_RST_BROWNOUT:  return "BROWNOUT";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
        default:                return "UNKNOWN";
    }
}

static void buildId() {
    uint8_t mac[6] = {0};
    esp_efuse_mac_get_default(mac);
    uint8_t digest[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts_ret(&ctx, 0);
    mbedtls_sha256_update_ret(&ctx, (const uint8_t*)TELEMETRY_SALT, strlen(TELEMETRY_SALT));
    mbedtls_sha256_update_ret(&ctx, mac, sizeof(mac));
    mbedtls_sha256_finish_ret(&ctx, digest);
    mbedtls_sha256_free(&ctx);
    for (int i = 0; i < 8; i++) snprintf(s_id + i * 2, 3, "%02x", digest[i]);
}

void Telemetry::init() {
    buildId();
    Preferences p;
    if (p.begin(TLM_NVS_NS, true)) {
        s_enabled = p.getBool("en", false);
        s_install = p.getUInt("inst", 0);
        p.end();
    }
    // Разброс первой отправки по устройствам, чтобы после общего сбоя питания
    // сотни контроллеров не постучались на приёмник в одну секунду.
    uint32_t jitter = ((uint32_t)strtoul(String(s_id).substring(0, 4).c_str(), nullptr, 16)) % 300000UL;
    s_nextMs = steadyMs() + TLM_FIRST_MS + jitter;
}

bool Telemetry::isAvailable() { return TELEMETRY_URL[0] != '\0'; }
bool Telemetry::isEnabled()   { return s_enabled && isAvailable(); }
const char* Telemetry::deviceId() { return s_id; }
const char* Telemetry::url()  { return TELEMETRY_URL; }

void Telemetry::setEnabled(bool on) {
    if (!isAvailable()) return;
    s_enabled = on;
    Preferences p;
    if (p.begin(TLM_NVS_NS, false)) {
        p.putBool("en", on);
        p.end();
    }
    // Включили — первое сообщение в ближайшую минуту. Расписание (64-битное s_nextMs)
    // правит только loop() в своей задаче: отсюда, из AsyncTCP, только флаг.
    if (on) s_kickReq.store(true);
    Serial.printf("[TLM] %s\n", on ? "enabled" : "disabled");
}

Telemetry::Status Telemetry::status() {
    return { s_lastOkTs.load(), s_lastCode.load(), s_sent.load() };
}

size_t Telemetry::buildPayload(char* out, size_t outSize, bool boot) {
    char tz[48] = "";
    bool mqtt = false;
    {
        MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(50));
        if (lock.locked()) {
            strlcpy(tz, g_settings.ntpTzString, sizeof(tz));
            mqtt = g_settings.mqttEnabled;
        }
    }
    // Часовой пояс — POSIX-строка из настроек. Кавычки и обратные слэши в ней
    // недопустимы (проверяет /api/ntp), но на всякий случай заменяем.
    for (char* c = tz; *c; c++) if (*c == '"' || *c == '\\' || (uint8_t)*c < 0x20) *c = '_';

    char fw[12];
    snprintf(fw, sizeof(fw), "%d.%d", FW_VERSION_MAJOR, FW_VERSION_MINOR);
    const IPAddress ip = WiFi.localIP();

    int n = snprintf(out, outSize,
        "{\"id\":\"%s\",\"fw\":\"%s\",\"inst\":%lu,\"up\":%lu,\"tz\":\"%s\",\"rr\":\"%s\","
        "\"hm\":%lu,\"mq\":%s,\"lan\":\"%u.%u.%u.%u\",\"boot\":%s}",
        s_id, fw, (unsigned long)s_install,
        (unsigned long)(steadyMs() / 1000ULL), tz, resetName(esp_reset_reason()),
        (unsigned long)ESP.getMinFreeHeap(), mqtt ? "true" : "false",
        ip[0], ip[1], ip[2], ip[3], boot ? "true" : "false");
    return (n > 0 && (size_t)n < outSize) ? (size_t)n : 0;
}

static void rememberInstallTime() {
    if (s_install != 0 || !NetMgr::isTimeSet()) return;
    time_t now; time(&now);
    if (now < 1700000000) return;           // часы ещё не настоящие
    s_install = (uint32_t)now;
    Preferences p;
    if (p.begin(TLM_NVS_NS, false)) {       // один раз за всю жизнь платы
        p.putUInt("inst", s_install);
        p.end();
    }
    Serial.printf("[TLM] Install date recorded: %lu\n", (unsigned long)s_install);
}

static bool sendOnce() {
    static char body[320];                  // под loopTask, не на стеке
    if (!Telemetry::buildPayload(body, sizeof(body), !s_bootSent)) return false;

    if (heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < TLM_MIN_BLOCK) {
        Serial.println(F("[TLM] skipped: not enough contiguous heap for TLS"));
        s_lastCode.store(-100);
        return false;
    }

    WiFiClientSecure client;
    client.setCACertBundle(rootca_crt_bundle_start);
    client.setTimeout(TLM_TIMEOUT_MS / 1000);
    HTTPClient http;
    http.setConnectTimeout(TLM_TIMEOUT_MS);
    http.setTimeout(TLM_TIMEOUT_MS);
    http.setReuse(false);
    if (!http.begin(client, TELEMETRY_URL)) { s_lastCode.store(-101); return false; }
    http.addHeader("Content-Type", "application/json");
    http.setUserAgent("greenhouse-esp32");
    const int code = http.POST((uint8_t*)body, strlen(body));
    http.end();
    s_lastCode.store((int16_t)code);
    if (code >= 200 && code < 300) {
        time_t now; time(&now);
        s_lastOkTs.store((uint32_t)now);
        s_sent.fetch_add(1);
        return true;
    }
    Serial.printf("[TLM] send failed: %d\n", code);
    return false;
}

void Telemetry::loop(uint64_t nowMs) {
    rememberInstallTime();
    if (s_kickReq.exchange(false)) {
        s_bootSent = false;
        s_nextMs = nowMs + 20000ULL;
    }
    if (!isEnabled()) return;
    if (nowMs < s_nextMs) return;
    if (!NetMgr::isConnected() || OtaMgr::isUpdating()) { s_nextMs = nowMs + 30000ULL; return; }
    // Своих попыток не чаще раза в минуту, что бы ни случилось с расписанием.
    if (s_lastTry && nowMs - s_lastTry < 60000ULL) return;
    s_lastTry = nowMs;

    if (sendOnce()) {
        s_bootSent = true;
        s_retryMs = TLM_RETRY_MS;
        s_nextMs = nowMs + TLM_PERIOD_MS;
    } else {
        // Приёмник недоступен: 10 → 20 → 40 → 60 мин, дальше раз в час.
        s_nextMs = nowMs + s_retryMs;
        s_retryMs = s_retryMs * 2 > TLM_PERIOD_MS ? TLM_PERIOD_MS : s_retryMs * 2;
    }
}
