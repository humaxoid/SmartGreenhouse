// ╔══════════════════════════════════════════════════════════════════╗
// ║  web_server_routes.cpp — HTTP эндпоинты v6.0                    ║
// ║  Изменения v6.0:                                                 ║
// ║   • Маршруты для banner.jpg, seven-segment.otf, *.gif            ║
// ║   • POST /api/ntp  — сервер NTP, TZ строка, интервал            ║
// ║   • POST /api/ui   — тема и язык в NVS                          ║
// ║   • GET /api/settings отдаёт ntp{tz,updateHrs} + ui{theme,lang} ║
// ╚══════════════════════════════════════════════════════════════════╝

#include "web_server_routes.h"
#include "settings.h"
#include "network_manager.h"
#include "mqtt_manager.h"
#include "ws_handler.h"
#include "ota_manager.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>   // v7.x

static AsyncWebServer s_server(80);
static volatile bool     s_rebootPending = false;
static volatile uint64_t s_rebootAtMs    = 0;

// ── Auth ─────────────────────────────────────────────────────────
// v6.1: второй аргумент requestAuthentication — это isDigest, а не realm-флаг:
//     void requestAuthentication(const char* realm = nullptr, bool isDigest = true)
// Здесь стояло true, то есть браузеру уходил вызов Digest-аутентификации,
// хотя веб-панель во всех своих запросах шлёт Basic (btoa в app.js).
// Digest требует согласования nonce, и при рассинхроне браузер показывает
// системное окно ввода пароля ещё раз. Приводим вызов к тому же типу,
// которым панель и пользуется.
//
// Плюс диагностика: при отказе печатаем, какой именно URL был отклонён и
// был ли вообще заголовок Authorization. Без этого невозможно понять,
// какой запрос вызывает системное окно — в логе браузера видно только
// его результат, а не причину на стороне контроллера.
static bool requireAuth(AsyncWebServerRequest* req) {
    const NetCache& nc = NetMgr::cache();
    if (req->authenticate(nc.webUser, nc.webPass)) return true;

    const bool hasHdr = req->hasHeader("Authorization");
    Serial.printf("[AUTH] 401 %s %s — %s\n",
        req->methodToString(),
        req->url().c_str(),
        hasHdr ? "Authorization header present but credentials rejected"
               : "no Authorization header");

    // v6.1: отвечаем 401 БЕЗ заголовка WWW-Authenticate.
    //
    // Раньше здесь вызывался requestAuthentication(), который этот заголовок
    // добавляет. Из-за него браузер поверх собственной формы входа панели
    // показывал ещё и системное окно ввода логина и пароля: получалось два
    // механизма аутентификации на одну учётку, и пользователь вводил пароль
    // дважды.
    //
    // Панель — одностраничное приложение со своей формой входа, все запросы
    // она подписывает заголовком Authorization сама (см. authHeaders в app.js).
    // Системный диалог здесь не нужен и только мешает: без WWW-Authenticate
    // браузеру нечего показывать, а fetch() получает честный 401, по которому
    // doLogin() выводит «Неверный логин или пароль» в интерфейсе панели.
    //
    // Побочно снимается ещё одна проблема: у requestAuthentication() второй
    // параметр — isDigest, и по умолчанию он true. То есть браузеру уходил
    // вызов Digest-аутентификации, хотя панель подписывает запросы Basic.
    req->send(401, "application/json", "{\"error\":\"unauthorized\"}");
    return false;
}

// ── Helpers ──────────────────────────────────────────────────────
static void sendJson(AsyncWebServerRequest* req, JsonDocument& doc, int code = 200) {
    char buf[2048];
    size_t n = serializeJson(doc, buf, sizeof(buf));
    if (n == 0 || n >= sizeof(buf)) { req->send(500, "text/plain", "JSON overflow"); return; }
    AsyncWebServerResponse* resp = req->beginResponse(code, "application/json", buf);
    resp->addHeader("Cache-Control", "no-store");
    req->send(resp);
}
static void sendAck(AsyncWebServerRequest* req, bool ok, const char* error = nullptr, int code = 200) {
    JsonDocument doc; doc["ok"] = ok;
    if (!ok && error) doc["error"] = error;
    sendJson(req, doc, code);
}

// ── Статика ───────────────────────────────────────────────────────
static void handleRoot(AsyncWebServerRequest* req) {
    if (!LittleFS.exists("/index.html")) {
        req->send(500, "text/plain", "LittleFS not uploaded."); return;
    }
    AsyncWebServerResponse* resp = req->beginResponse(LittleFS, "/index.html", "text/html");
    resp->addHeader("Cache-Control", "no-store");
    req->send(resp);
}

static void serveStatic(const char* url, const char* path, const char* ct, bool cache) {
    s_server.on(url, HTTP_GET, [path, ct, cache](AsyncWebServerRequest* req) {
        if (!LittleFS.exists(path)) { req->send(404, "text/plain", "Not found"); return; }
        AsyncWebServerResponse* resp = req->beginResponse(LittleFS, path, ct);
        resp->addHeader("Cache-Control", cache ? "public, max-age=86400" : "no-store");
        req->send(resp);
    });
}

// ── GET /api/settings ─────────────────────────────────────────────
static void handleGetSettings(AsyncWebServerRequest* req) {
    if (!requireAuth(req)) return;
    JsonDocument doc;
    MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(50));
    if (!lock.locked()) { req->send(503, "text/plain", "mutex busy"); return; }

    JsonObject modes = doc["modes"].to<JsonObject>();
    modes["vents"]  = g_settings.modeVents;
    modes["timers"] = g_settings.modeTimers;
    modes["humid"]  = g_settings.modeHumid;
    modes["pump"]   = g_settings.modeWaterPump;

    JsonObject vu = doc["ventUpper"].to<JsonObject>();
    vu["high"] = g_settings.ventUpper.high;  vu["low"] = g_settings.ventUpper.low;
    vu["travelMs"] = g_settings.ventUpperCal.travelMs;
    vu["overrunPct"] = g_settings.ventUpperCal.overrunPct;

    JsonObject vl = doc["ventLower"].to<JsonObject>();
    vl["high"] = g_settings.ventLower.high;  vl["low"] = g_settings.ventLower.low;
    vl["travelMs"] = g_settings.ventLowerCal.travelMs;
    vl["overrunPct"] = g_settings.ventLowerCal.overrunPct;

    JsonObject h = doc["humid"].to<JsonObject>();
    h["high"] = g_settings.humidHigh;  h["low"] = g_settings.humidLow;

    JsonArray timers = doc["timers"].to<JsonArray>();
    for (uint8_t i = 0; i < NUM_IRRIGATION; i++) {
        JsonObject t = timers.add<JsonObject>();
        t["startMin"]      = g_settings.timer[i].startMin;
        t["durationMin"]   = g_settings.timer[i].durationMin;
        t["intervalHours"] = g_settings.timer[i].intervalHours;
    }

    JsonObject w = doc["waterTank"].to<JsonObject>();
    w["lowPct"] = g_settings.waterTank.lowPct;  w["highPct"] = g_settings.waterTank.highPct;
    w["depthCm"] = g_settings.waterTank.depthCm;
    w["pumpTimeoutMin"] = g_settings.waterTank.pumpTimeoutMin;

    // NTP: v6.0 добавлены tz и updateHrs
    JsonObject ntp = doc["ntp"].to<JsonObject>();
    ntp["server"]    = g_settings.ntpServer;
    ntp["tz"]        = g_settings.ntpTzString;
    ntp["updateHrs"] = g_settings.ntpUpdateHrs;

    // UI: v6.0 тема и язык из NVS
    JsonObject ui = doc["ui"].to<JsonObject>();
    ui["theme"] = g_settings.uiTheme;
    ui["lang"]  = g_settings.uiLang;

    doc["fwVersion"] = FW_VERSION;
    sendJson(req, doc);
}

// ── GET /api/network ─────────────────────────────────────────────
static void handleGetNetwork(AsyncWebServerRequest* req) {
    if (!requireAuth(req)) return;
    const NetCache& nc = NetMgr::cache();
    JsonDocument doc;
    doc["isDHCP"] = nc.isDHCP;  doc["ssid"] = nc.ssid;
    doc["ip"] = nc.ip;  doc["subnet"] = nc.subnet;
    doc["gateway"] = nc.gateway;  doc["dns1"] = nc.dns1;  doc["dns2"] = nc.dns2;
    doc["connected"] = NetMgr::isConnected();
    doc["rssi"]      = WiFi.RSSI();
    doc["currentIp"] = WiFi.localIP().toString();
    doc["mac"]       = WiFi.macAddress();
    sendJson(req, doc);
}

// ── POST /api/network ─────────────────────────────────────────────
static void handlePostNetwork(AsyncWebServerRequest* req, JsonVariant& json) {
    if (!requireAuth(req)) return;
    JsonObject body = json.as<JsonObject>();
    NetCache nc = NetMgr::cache();
    bool savePass = false;

    if (body["isDHCP"].is<bool>()) nc.isDHCP = body["isDHCP"];
    if (body["ssid"].is<const char*>()) {
        const char* s = body["ssid"];
        if (strlen(s) == 0 || strlen(s) > 32) { sendAck(req, false, "invalid SSID", 400); return; }
        strlcpy(nc.ssid, s, sizeof(nc.ssid));
    }
    if (body["pass"].is<const char*>()) {
        const char* p = body["pass"];
        if (strlen(p) > 63) { sendAck(req, false, "password too long", 400); return; }
        if (p[0]) { strlcpy(nc.pass, p, sizeof(nc.pass)); savePass = true; }
    }
    if (body["ip"].is<const char*>())      strlcpy(nc.ip,      body["ip"],      sizeof(nc.ip));
    if (body["subnet"].is<const char*>())  strlcpy(nc.subnet,  body["subnet"],  sizeof(nc.subnet));
    if (body["gateway"].is<const char*>()) strlcpy(nc.gateway, body["gateway"], sizeof(nc.gateway));
    if (body["dns1"].is<const char*>())    strlcpy(nc.dns1,    body["dns1"],    sizeof(nc.dns1));
    if (body["dns2"].is<const char*>())    strlcpy(nc.dns2,    body["dns2"],    sizeof(nc.dns2));

    if (!nc.isDHCP) {
        IPAddress check;
        if (!check.fromString(nc.ip) || !check.fromString(nc.subnet)
                || !check.fromString(nc.gateway)) {
            sendAck(req, false, "invalid IP format", 400); return;
        }
    }
    NetMgr::saveNetSettings(nc, savePass);
    sendAck(req, true);
    Serial.println(F("[HTTP] Network settings changed — reboot in 2s"));
    s_rebootPending = true;
    s_rebootAtMs = steadyMs() + 2000ULL;
}

// ── GET /api/mqtt ─────────────────────────────────────────────────
static void handleGetMqtt(AsyncWebServerRequest* req) {
    if (!requireAuth(req)) return;
    JsonDocument doc;
    MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(50));
    if (!lock.locked()) { req->send(503, "text/plain", "mutex busy"); return; }
    doc["enabled"]   = g_settings.mqttEnabled;
    doc["server"]    = g_settings.mqttServer;
    doc["port"]      = g_settings.mqttPort;
    doc["user"]      = g_settings.mqttUser;
    doc["connected"] = MqttMgr::isConnected();
    sendJson(req, doc);
}

// ── POST /api/mqtt ────────────────────────────────────────────────
static void handlePostMqtt(AsyncWebServerRequest* req, JsonVariant& json) {
    if (!requireAuth(req)) return;
    JsonObject body = json.as<JsonObject>();
    MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(50));
    if (!lock.locked()) { sendAck(req, false, "mutex timeout", 503); return; }

    if (body["enabled"].is<bool>()) g_settings.mqttEnabled = body["enabled"];
    if (body["server"].is<const char*>()) {
        const char* s = body["server"];
        if (strlen(s) > 63) { sendAck(req, false, "server too long", 400); return; }
        strlcpy(g_settings.mqttServer, s, sizeof(g_settings.mqttServer));
    }
    if (body["port"].is<int>()) {
        int p = body["port"];
        if (p < 1 || p > 65535) { sendAck(req, false, "port out of range", 400); return; }
        g_settings.mqttPort = (uint16_t)p;
    }
    if (body["user"].is<const char*>()) {
        const char* u = body["user"];
        if (strlen(u) > 31) { sendAck(req, false, "user too long", 400); return; }
        strlcpy(g_settings.mqttUser, u, sizeof(g_settings.mqttUser));
    }
    if (body["pass"].is<const char*>()) {
        const char* p = body["pass"];
        if (strlen(p) > 31) { sendAck(req, false, "pass too long", 400); return; }
        if (p[0]) strlcpy(g_settings.mqttPass, p, sizeof(g_settings.mqttPass));
    }
    settingsSaveDeferred();
    MqttMgr::markDiscoveryDirty();
    sendAck(req, true);
    Serial.println(F("[HTTP] MQTT settings saved"));
}

// ── POST /api/webcreds ────────────────────────────────────────────
static void handleWebCreds(AsyncWebServerRequest* req, JsonVariant& json) {
    if (!requireAuth(req)) return;
    JsonObject body = json.as<JsonObject>();
    const char* user = body["user"] | "";
    const char* pass = body["pass"] | "";
    if (strlen(user) < 1 || strlen(user) > 31) { sendAck(req, false, "user length 1-31", 400); return; }
    if (strlen(pass) < 4 || strlen(pass) > 31) { sendAck(req, false, "pass length 4-31", 400); return; }
    NetMgr::saveWebCreds(user, pass);
    Serial.printf("[HTTP] Web credentials changed: %s\n", user);
    sendAck(req, true);
}

// ── POST /api/ntp ─────────────────────────────────────────────────
// Body: { "server": "pool.ntp.org", "tz": "MSK-3", "updateHrs": 4 }
static void handlePostNtp(AsyncWebServerRequest* req, JsonVariant& json) {
    if (!requireAuth(req)) return;
    JsonObject body = json.as<JsonObject>();
    MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(100));
    if (!lock.locked()) { sendAck(req, false, "mutex timeout", 503); return; }

    if (body["server"].is<const char*>()) {
        const char* s = body["server"];
        if (strlen(s) > 0 && strlen(s) < 64)
            strlcpy(g_settings.ntpServer, s, sizeof(g_settings.ntpServer));
    }
    if (body["tz"].is<const char*>()) {
        const char* tz = body["tz"];
        if (strlen(tz) > 0 && strlen(tz) < 48) {
            strlcpy(g_settings.ntpTzString, tz, sizeof(g_settings.ntpTzString));
            setenv("TZ", g_settings.ntpTzString, 1);
            tzset();
        }
    }
    if (body["updateHrs"].is<int>()) {
        int hrs = body["updateHrs"];
        const int allowed[] = {1,2,3,4,5,6,8,12,24};
        for (int v : allowed) {
            if (hrs == v) { g_settings.ntpUpdateHrs = (uint8_t)hrs; break; }
        }
    }
    settingsSaveDeferred();
    sendAck(req, true);
    Serial.println(F("[HTTP] NTP settings saved"));
}

// ── POST /api/ui ──────────────────────────────────────────────────
// Body: { "theme": "night", "lang": "ru" }
static void handlePostUi(AsyncWebServerRequest* req, JsonVariant& json) {
    if (!requireAuth(req)) return;
    JsonObject body = json.as<JsonObject>();
    MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(50));
    if (!lock.locked()) { sendAck(req, false, "mutex timeout", 503); return; }

    if (body["theme"].is<const char*>()) {
        const char* theme = body["theme"];
        const char* valid[] = {"", "beige", "night", "win98", "skyblue", "brown"};
        for (const char* v : valid) {
            if (strcmp(theme, v) == 0) {
                strlcpy(g_settings.uiTheme, theme, sizeof(g_settings.uiTheme)); break;
            }
        }
    }
    if (body["lang"].is<const char*>()) {
        const char* lang = body["lang"];
        if (strcmp(lang, "en") == 0 || strcmp(lang, "ru") == 0)
            strlcpy(g_settings.uiLang, lang, sizeof(g_settings.uiLang));
    }
    settingsSaveDeferred();
    sendAck(req, true);
}

// ── GET /api/wifi_scan ────────────────────────────────────────────
static void handleWifiScan(AsyncWebServerRequest* req) {
    if (!requireAuth(req)) return;
    int n = WiFi.scanNetworks(false, false, false, 300);
    JsonDocument doc;
    JsonArray arr = doc["networks"].to<JsonArray>();
    int maxN = (n > 20) ? 20 : n;
    for (int i = 0; i < maxN; i++) {
        JsonObject net = arr.add<JsonObject>();
        net["ssid"] = WiFi.SSID(i);
        net["rssi"] = WiFi.RSSI(i);
        net["enc"]  = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
    WiFi.scanDelete();
    sendJson(req, doc);
}

// ── POST /api/factory_reset ───────────────────────────────────────
static void handleFactoryReset(AsyncWebServerRequest* req) {
    if (!requireAuth(req)) return;
    Serial.println(F("[HTTP] FACTORY RESET requested"));
    settingsReset();
    sendAck(req, true);
    s_rebootPending = true;
    s_rebootAtMs = steadyMs() + 2000ULL;
}

// ── POST /api/reset_wifi ──────────────────────────────────────────
static void handleResetWifi(AsyncWebServerRequest* req) {
    if (!requireAuth(req)) return;
    Serial.println(F("[HTTP] WiFi credentials reset requested"));
    // v6.1: проверять результат по NetMgr::hasCredentials() нельзя — она
    // смотрит в кэш RAM, а он обновляется независимо от того, дошла ли
    // запись до флеша. Стирание и проверку делает clearWifiCredentials(),
    // которая читает NVS обратно.
    if (!NetMgr::clearWifiCredentials()) {
        Serial.println(F("[HTTP] !!! WiFi reset FAILED — credentials still in NVS !!!"));
        req->send(500, "application/json",
                  "{\"ok\":false,\"error\":\"wifi credentials not cleared\"}");
        return;
    }
    Serial.println(F("[HTTP] WiFi credentials cleared — AP provisioning after reboot"));
    sendAck(req, true);
    s_rebootPending = true;
    s_rebootAtMs = steadyMs() + 2000ULL;
}

// ── POST /api/reboot ──────────────────────────────────────────────
static void handleReboot(AsyncWebServerRequest* req) {
    if (!requireAuth(req)) return;
    Serial.println(F("[HTTP] Manual reboot requested"));
    sendAck(req, true);
    s_rebootPending = true;
    s_rebootAtMs = steadyMs() + 1000ULL;
}

// ── Отложенная перезагрузка ──────────────────────────────────────
// v6.1: была отдельная задача FreeRTOS «rebootWatch» — 2 КБ стека и
// вечный цикл с опросом раз в 500 мс ради события, случающегося пару раз
// за всю жизнь устройства. Задача к тому же не была закреплена за ядром
// и не стояла под watchdog.
//
// Теперь проверку делает loop(), который и так крутится каждые 100 мс.
// Минус одна задача, минус 2 КБ RAM, реакция даже быстрее.
bool WebRoutes::rebootDue() {
    return s_rebootPending && steadyMs() >= s_rebootAtMs;
}

// ═══════════════════════════════════════════════════════════════════
//  WebRoutes::init()
// ═══════════════════════════════════════════════════════════════════
void WebRoutes::init() {

    // Корень
    s_server.on("/",           HTTP_GET, handleRoot);
    s_server.on("/index.html", HTTP_GET, handleRoot);

    // Статика JS/CSS без кеша
    serveStatic("/app.js",      "/app.js",      "application/javascript", false);
    serveStatic("/style.css",   "/style.css",   "text/css",               false);
    serveStatic("/favicon.ico", "/favicon.ico", "image/x-icon",           false);

    // Медиафайлы с кешем (ИСПРАВЛЕНИЕ v6.0: отсутствовали в v5.2 → banner давал 404)
    serveStatic("/banner.jpg",        "/banner.jpg",        "image/jpeg", true);
    serveStatic("/seven-segment.otf", "/seven-segment.otf", "font/otf",   true);
    serveStatic("/sun.gif",           "/sun.gif",           "image/gif",  true);
    serveStatic("/rain.gif",          "/rain.gif",          "image/gif",  true);
    serveStatic("/moon.gif",          "/moon.gif",          "image/gif",  true);
    serveStatic("/cloud.gif",         "/cloud.gif",         "image/gif",  true);

    // GET API
    s_server.on("/api/settings", HTTP_GET, handleGetSettings);
    s_server.on("/api/network",  HTTP_GET, handleGetNetwork);
    s_server.on("/api/mqtt",     HTTP_GET, handleGetMqtt);
    s_server.on("/api/wifi_scan",HTTP_GET, handleWifiScan);

    // POST API с JSON body
    auto* hNetwork = new AsyncCallbackJsonWebHandler("/api/network",
        [](AsyncWebServerRequest* r, JsonVariant& j){ handlePostNetwork(r, j); });
    hNetwork->setMethod(HTTP_POST);
    s_server.addHandler(hNetwork);

    auto* hMqtt = new AsyncCallbackJsonWebHandler("/api/mqtt",
        [](AsyncWebServerRequest* r, JsonVariant& j){ handlePostMqtt(r, j); });
    hMqtt->setMethod(HTTP_POST);
    s_server.addHandler(hMqtt);

    auto* hCreds = new AsyncCallbackJsonWebHandler("/api/webcreds",
        [](AsyncWebServerRequest* r, JsonVariant& j){ handleWebCreds(r, j); });
    hCreds->setMethod(HTTP_POST);
    s_server.addHandler(hCreds);

    auto* hNtp = new AsyncCallbackJsonWebHandler("/api/ntp",
        [](AsyncWebServerRequest* r, JsonVariant& j){ handlePostNtp(r, j); });
    hNtp->setMethod(HTTP_POST);
    s_server.addHandler(hNtp);

    auto* hUi = new AsyncCallbackJsonWebHandler("/api/ui",
        [](AsyncWebServerRequest* r, JsonVariant& j){ handlePostUi(r, j); });
    hUi->setMethod(HTTP_POST);
    s_server.addHandler(hUi);

    // POST без body
    s_server.on("/api/factory_reset", HTTP_POST, handleFactoryReset);
    s_server.on("/api/reset_wifi",    HTTP_POST, handleResetWifi);
    s_server.on("/api/reboot",        HTTP_POST, handleReboot);

    // WebSocket и OTA
    WsHandler::attachTo(s_server);
    OtaMgr::attachTo(s_server);

    // 404
    s_server.onNotFound([](AsyncWebServerRequest* req) {
        if (req->url().startsWith("/api/"))
            req->send(404, "application/json", "{\"error\":\"not found\"}");
        else
            req->send(404, "text/plain", "Not found");
    });

    s_server.begin();
    Serial.println(F("[HTTP] Web server started on port 80"));
}
