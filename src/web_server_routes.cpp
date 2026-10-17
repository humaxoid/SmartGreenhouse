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
#include "event_log.h"
#include "telemetry.h"
#include "network_manager.h"
#include "mqtt_manager.h"
#include "ws_handler.h"
#include "ota_manager.h"
#include "auth_session.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>   // v7.x
#include <atomic>

static AsyncWebServer s_server(80);
// Пишет AsyncTCP, читает loop() в loopTask. Срок записывается ДО флага:
// иначе loop() мог увидеть поднятый флаг со старым сроком (0) и
// перезагрузить контроллер раньше, чем ответ ушёл панели.
static std::atomic<bool> s_rebootPending{false};
static uint64_t          s_rebootAtMs = 0;

static void scheduleReboot(uint64_t delayMs) {
    s_rebootAtMs = steadyMs() + delayMs;
    s_rebootPending.store(true);
}

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
// В журнал «неудачные попытки входа» идёт только отказ Basic, то есть неверный
// пароль. Просроченный токен сессии — не попытка входа: сессии живут в RAM, и
// после каждой перезагрузки контроллера открытая панель шлёт несколько запросов
// со старым токеном — журнал наполнялся бы ложными тревогами.
static void countAuthFail(AsyncWebServerRequest* req) {
    if (req->hasHeader("Authorization") && !AuthSess::bearerOf(req)) EventLog::authFail();
}

static bool requireAuth(AsyncWebServerRequest* req) {
    if (AuthSess::authorized(req)) return true;   // Bearer-сессия или Basic

    const bool hasHdr = req->hasHeader("Authorization");
    countAuthFail(req);
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

// Пока действуют заводские admin/admin, изменяющие запросы отклоняются — кроме
// смены пароля и темы оформления. Иначе любой в сети (или у аварийной точки
// доступа) заходил бы с паролем из инструкции и заливал свою прошивку.
static bool denyIfDefaultCreds(AsyncWebServerRequest* req) {
    if (!NetMgr::credsAreDefault()) return false;
    req->send(403, "application/json", "{\"ok\":false,\"error\":\"default_credentials\"}");
    return true;
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

// ── Страница восстановления ───────────────────────────────────────
// v6.3: если файловая система пуста или переформатирована (оборвалась
// заливка контейнера на этапе FS, чистый флеш), корень раньше отвечал
// «500 LittleFS not uploaded», и вернуть панель можно было только по USB.
// Теперь отдаётся эта встроенная страница: она грузит контейнер
// greenhouse_vX.Y.bin на тот же /update с тем же паролем. Сама по себе
// страница ничего не раскрывает и ничего не делает без авторизации.
static const char RECOVERY_HTML[] PROGMEM = R"RECOVERY(<!DOCTYPE html>
<html lang="ru"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Greenhouse</title>
<style>body{font-family:sans-serif;max-width:420px;margin:24px auto;padding:0 16px}
input,button{width:100%;box-sizing:border-box;margin:6px 0;padding:10px;font-size:16px}
#m{margin-top:12px;font-weight:bold;word-break:break-word}</style></head><body>
<h2>Файловая система не загружена</h2>
<p>Веб-панели нет, автоматика теплицы при этом работает. Загрузите файл
<b>greenhouse_vX.Y.bin</b> — в нём и прошивка, и панель.</p>
<p lang="en">Web panel files are missing; the greenhouse automation keeps running.
Upload <b>greenhouse_vX.Y.bin</b> (firmware + panel).</p>
<input id="u" type="text" placeholder="Login" autocomplete="username">
<input id="p" type="password" placeholder="Password" autocomplete="current-password">
<input id="f" type="file" accept=".bin">
<button id="b" type="button">Загрузить / Upload</button>
<div id="m"></div>
<script>
document.getElementById('b').onclick=function(){
var f=document.getElementById('f').files[0],m=document.getElementById('m');
if(!f){m.textContent='Выберите файл / Choose a file';return;}
var fd=new FormData();fd.append('firmware',f,f.name);
var x=new XMLHttpRequest();x.open('POST','/update');
x.setRequestHeader('Authorization','Basic '+btoa(unescape(encodeURIComponent(
document.getElementById('u').value+':'+document.getElementById('p').value))));
x.upload.onprogress=function(e){if(e.lengthComputable)m.textContent=Math.round(e.loaded/e.total*100)+'%';};
x.onload=function(){if(x.status===200){m.textContent='OK. Перезагрузка / Rebooting…';
setTimeout(function(){location.reload();},12000);}
else{var t=x.responseText;try{t=JSON.parse(t).error||t;}catch(e){}m.textContent='Ошибка / Error '+x.status+': '+t;}};
x.onerror=function(){m.textContent='Ошибка соединения / Connection error';};
x.send(fd);};
</script></body></html>)RECOVERY";

// ── Статика ───────────────────────────────────────────────────────
static void handleRoot(AsyncWebServerRequest* req) {
    if (!LittleFS.exists("/index.html")) {
        AsyncWebServerResponse* rec = req->beginResponse_P(200, "text/html", RECOVERY_HTML);
        rec->addHeader("Cache-Control", "no-store");
        req->send(rec);
        return;
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
    // Заводские admin/admin: панель показывает обязательную смену пароля,
    // а всё изменяющее до неё отклоняется (см. denyIfDefaultCreds).
    doc["credsDefault"] = NetMgr::credsAreDefault();
    doc["otaPassSet"]   = NetMgr::cache().otaPass[0] != '\0';   // сам пароль не отдаём
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
        if (p[0] && strlen(p) < 8) { sendAck(req, false, "Wi-Fi password must be 8-63 characters", 400); return; }
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
    if (!NetMgr::saveNetSettings(nc, savePass)) {
        sendAck(req, false, "settings not saved (NVS)", 500);
        return;
    }
    EventLog::add(EV_SETTINGS, EVS_NETWORK);
    sendAck(req, true);
    Serial.println(F("[HTTP] Network settings changed — reboot in 2s"));
    scheduleReboot(2000ULL);
}

// ── GET/POST /api/telemetry ───────────────────────────────────────
// v6.3: анонимная статистика. GET отдаёт состояние и ТОЧНЫЙ текст сообщения,
// чтобы владелец видел, что уходит. POST {"enabled":true|false}.
static void handleGetTelemetry(AsyncWebServerRequest* req) {
    if (!requireAuth(req)) return;
    JsonDocument doc;
    doc["available"] = Telemetry::isAvailable();
    doc["enabled"]   = Telemetry::isEnabled();
    doc["id"]        = Telemetry::deviceId();
    doc["url"]       = Telemetry::url();
    const Telemetry::Status st = Telemetry::status();
    doc["lastOk"]    = st.lastOkTs;
    doc["lastCode"]  = st.lastCode;
    doc["sent"]      = st.sent;
    static char sample[320];                 // AsyncTCP — единственный вызывающий
    if (Telemetry::buildPayload(sample, sizeof(sample), false)) doc["payload"] = sample;
    sendJson(req, doc);
}

static void handlePostTelemetry(AsyncWebServerRequest* req, JsonVariant& json) {
    if (!requireAuth(req)) return;
    JsonObject body = json.as<JsonObject>();
    if (!body["enabled"].is<bool>()) { sendAck(req, false, "enabled (bool) required", 400); return; }
    if (!Telemetry::isAvailable()) { sendAck(req, false, "not configured in this build", 409); return; }
    Telemetry::setEnabled(body["enabled"].as<bool>());
    sendAck(req, true);
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

    // Сначала проверяем ВСЁ, и только потом пишем: раньше поля писались по
    // одному, и ошибка в порте возвращала 400, оставив новый сервер в работе
    // (тот же класс ошибки, что исправлен в 6.3 для set_thresholds).
    const char* server = body["server"].is<const char*>() ? body["server"].as<const char*>() : nullptr;
    const char* user   = body["user"].is<const char*>()   ? body["user"].as<const char*>()   : nullptr;
    const char* pass   = body["pass"].is<const char*>()   ? body["pass"].as<const char*>()   : nullptr;
    int port = -1;
    if (server && strlen(server) > 63) { sendAck(req, false, "server too long", 400); return; }
    if (body["port"].is<int>()) {
        port = body["port"];
        if (port < 1 || port > 65535) { sendAck(req, false, "port out of range", 400); return; }
    }
    if (user && strlen(user) > 31) { sendAck(req, false, "user too long", 400); return; }
    if (pass && strlen(pass) > 31) { sendAck(req, false, "pass too long", 400); return; }

    {
        MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(50));
        if (!lock.locked()) { sendAck(req, false, "mutex timeout", 503); return; }
        if (body["enabled"].is<bool>()) g_settings.mqttEnabled = body["enabled"];
        if (server)       strlcpy(g_settings.mqttServer, server, sizeof(g_settings.mqttServer));
        if (port > 0)     g_settings.mqttPort = (uint16_t)port;
        if (user)         strlcpy(g_settings.mqttUser, user, sizeof(g_settings.mqttUser));
        if (pass && pass[0]) strlcpy(g_settings.mqttPass, pass, sizeof(g_settings.mqttPass));
    }
    settingsSaveDeferred();
    MqttMgr::markDiscoveryDirty();
    MqttMgr::markReconnect();   // иначе новый адрес вступил бы в силу только после обрыва
    EventLog::add(EV_SETTINGS, EVS_MQTT);
    sendAck(req, true);
    Serial.println(F("[HTTP] MQTT settings saved"));
}

// ── POST /api/login ───────────────────────────────────────────────
// Единственное место, где панель отдаёт пароль: Basic → токен сессии.
// Bearer здесь не принимается — сессию из сессии не выписываем.
static void handleLogin(AsyncWebServerRequest* req) {
    const NetCache& nc = NetMgr::cache();
    if (!req->authenticate(nc.webUser, nc.webPass)) {
        if (req->hasHeader("Authorization")) EventLog::authFail();
        Serial.printf("[AUTH] 401 login — %s\n",
            req->hasHeader("Authorization") ? "credentials rejected" : "no Authorization header");
        req->send(401, "application/json", "{\"error\":\"unauthorized\"}");
        return;
    }
    char tok[AuthSess::TOKEN_BUF];
    if (!AuthSess::create(tok, sizeof(tok))) { sendAck(req, false, "session", 500); return; }
    JsonDocument doc;
    doc["ok"]           = true;
    doc["token"]        = tok;
    doc["credsDefault"] = NetMgr::credsAreDefault();
    sendJson(req, doc);
}

// ── POST /api/logout ──────────────────────────────────────────────
static void handleLogout(AsyncWebServerRequest* req) {
    const char* t = AuthSess::bearerOf(req);
    if (t) AuthSess::revoke(t);
    sendAck(req, true);
}

// ── POST /api/webcreds ────────────────────────────────────────────
// Новый пароль — от 8 символов и не заводской admin/admin (пароли от 4
// символов, заданные раньше, продолжают работать до смены). Смена сбрасывает
// все сессии: украденный токен после неё бесполезен. Тот, кто сменил пароль,
// получает в ответе новый токен и остаётся в панели.
static void handleWebCreds(AsyncWebServerRequest* req, JsonVariant& json) {
    if (!requireAuth(req)) return;
    JsonObject body = json.as<JsonObject>();
    const char* user = body["user"] | "";
    const char* pass = body["pass"] | "";
    // Текущий пароль обязателен: иначе украденный токен сессии позволял бы
    // сменить пароль и запереть владельца снаружи (вернуть доступ — только
    // стиранием NVS по USB). Неверный — считается попыткой входа.
    const char* cur  = body["curPass"] | "";
    const char* real = NetMgr::cache().webPass;
    const size_t curLen = strlen(cur), realLen = strlen(real);
    uint8_t diff = (uint8_t)(curLen != realLen);
    for (size_t i = 0; i < curLen && i < realLen; i++) diff |= (uint8_t)(cur[i] ^ real[i]);
    if (diff) {
        EventLog::authFail();
        sendAck(req, false, "current password is wrong", 403);
        return;
    }
    if (strlen(user) < 1 || strlen(user) > 31) { sendAck(req, false, "user length 1-31", 400); return; }
    if (strlen(pass) < 8 || strlen(pass) > 31) { sendAck(req, false, "pass length 8-31", 400); return; }
    if (strcmp(user, WEB_DEFAULT_USER) == 0 && strcmp(pass, WEB_DEFAULT_PASS) == 0) {
        sendAck(req, false, "default credentials not allowed", 400); return;
    }
    if (!NetMgr::saveWebCreds(user, pass)) {
        sendAck(req, false, "credentials not saved (NVS)", 500);
        return;
    }
    AuthSess::revokeAll();
    char tok[AuthSess::TOKEN_BUF];
    const bool haveTok = AuthSess::create(tok, sizeof(tok));
    Serial.printf("[HTTP] Web credentials changed: %s (all sessions revoked)\n", user);
    EventLog::add(EV_SETTINGS, EVS_CREDS);
    JsonDocument doc;
    doc["ok"] = true;
    if (haveTok) doc["token"] = tok;
    sendJson(req, doc);
}

// ── POST /api/otapass ─────────────────────────────────────────────
// Body: { "pass": "..." } — пароль ArduinoOTA (прошивка из PlatformIO по сети).
// Пустой — канал выключен. Действует после перезагрузки.
static void handleOtaPass(AsyncWebServerRequest* req, JsonVariant& json) {
    if (!requireAuth(req)) return;
    JsonObject body = json.as<JsonObject>();
    if (!body["pass"].is<const char*>()) { sendAck(req, false, "pass (string) required", 400); return; }
    const char* p = body["pass"];
    const size_t n = strlen(p);
    if (n != 0 && (n < 8 || n > 31)) { sendAck(req, false, "pass length 8-31 or empty", 400); return; }
    if (n != 0 && strcmp(p, NetMgr::cache().webPass) == 0) {
        sendAck(req, false, "must differ from the panel password", 400); return;
    }
    if (!NetMgr::saveOtaPass(p)) { sendAck(req, false, "not saved (NVS)", 500); return; }
    EventLog::add(EV_SETTINGS, EVS_CREDS);
    sendAck(req, true);
}

// ── POST /api/ntp ─────────────────────────────────────────────────
// Body: { "server": "pool.ntp.org", "tz": "MSK-3", "updateHrs": 4 }
static void handlePostNtp(AsyncWebServerRequest* req, JsonVariant& json) {
    if (!requireAuth(req)) return;
    JsonObject body = json.as<JsonObject>();
    bool tzChanged = false;
    {   // лок — только на правку структуры; запись якорей во флеш ниже, вне его
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
            tzChanged = (strcmp(g_settings.ntpTzString, tz) != 0);
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
    }
    // Часовой пояс сдвигает смысл «время старта» — опорные точки расписания
    // полива (интервалы не в сутки) пересчитаются от нового местного времени.
    if (tzChanged) timerAnchorsClear();
    // Сервер и интервал вступают в силу сразу: раньше reinitNtp() не звал
    // никто, и новый сервер начинал работать только после перезагрузки.
    NetMgr::reinitNtp();
    EventLog::add(EV_SETTINGS, EVS_NTP);
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
// Скан асинхронный: синхронный держал задачу AsyncTCP до ~4 с (13 каналов по
// 300 мс) — замирали веб-сокет и биения зажатой кнопки форточек, и привод мог
// остановиться по истечении аренды. Теперь первый запрос запускает скан и
// отвечает {"scanning":true}, панель повторяет запрос, пока не придёт список.
static void handleWifiScan(AsyncWebServerRequest* req) {
    if (!requireAuth(req)) return;
    const int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING || n < 0) {
        if (n != WIFI_SCAN_RUNNING) WiFi.scanNetworks(true, false, false, 300);
        req->send(200, "application/json", "{\"scanning\":true}");
        return;
    }
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
    EventLog::add(EV_SETTINGS, EVS_FACTORY);
    sendAck(req, true);
    scheduleReboot(2000ULL);
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
    EventLog::add(EV_SETTINGS, EVS_NETWORK);
    sendAck(req, true);
    scheduleReboot(2000ULL);
}

// ── POST /api/reboot ──────────────────────────────────────────────
static void handleReboot(AsyncWebServerRequest* req) {
    if (!requireAuth(req)) return;
    Serial.println(F("[HTTP] Manual reboot requested"));
    EventLog::add(EV_REBOOT, 0);
    sendAck(req, true);
    scheduleReboot(1000ULL);
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
    return s_rebootPending.load() && steadyMs() >= s_rebootAtMs;
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
    s_server.on("/api/telemetry",HTTP_GET, handleGetTelemetry);
    s_server.on("/api/wifi_scan",HTTP_GET, handleWifiScan);

    // POST API с JSON body
    // v6.3: JSON-обработчики принимали тело до 16 КБ (так по умолчанию в
    // библиотеке) и выделяли под него буфер ДО проверки пароля, потому что
    // авторизация стоит внутри самого обработчика. Несколько параллельных
    // POST с чужого узла сети съедали кучу и роняли контроллер. Теперь:
    //   • фильтр пропускает к обработчику только запросы с верным паролем —
    //     остальные до выделения буфера не доходят (см. onNotFound ниже);
    //   • тело ограничено 2 КБ (самый крупный запрос — /api/network ~400 Б).
    // requireAuth() внутри обработчиков оставлена как вторая линия.
    //
    // Запрет при заводских admin/admin (denyIfDefaultCreds) стоит здесь, при
    // регистрации, а не в каждом обработчике: новый POST-маршрут закрыт по
    // умолчанию. Исключения перечислены явно — ALLOW_DEFAULT.
    constexpr bool ALLOW_DEFAULT = true;
    auto addJsonPost = [](const char* uri, void (*fn)(AsyncWebServerRequest*, JsonVariant&),
                          bool allowDefault = false) {
        auto* h = new AsyncCallbackJsonWebHandler(uri,
            [fn, allowDefault](AsyncWebServerRequest* r, JsonVariant& j){
                if (!allowDefault && denyIfDefaultCreds(r)) return;   // фильтр уже проверил пароль
                fn(r, j);
            });
        h->setMethod(HTTP_POST);
        h->setMaxContentLength(2048);
        h->setFilter([](AsyncWebServerRequest* r) { return AuthSess::authorized(r); });
        s_server.addHandler(h);
    };
    auto addPost = [](const char* uri, void (*fn)(AsyncWebServerRequest*), bool allowDefault = false) {
        s_server.on(uri, HTTP_POST, [fn, allowDefault](AsyncWebServerRequest* r) {
            // Без пароля — пусть ответит сам обработчик (401 и учёт попытки).
            if (!allowDefault && AuthSess::authorized(r) && denyIfDefaultCreds(r)) return;
            fn(r);
        });
    };
    addJsonPost("/api/network", handlePostNetwork);
    addJsonPost("/api/mqtt",    handlePostMqtt);
    addJsonPost("/api/webcreds",handleWebCreds, ALLOW_DEFAULT);   // смена заводского пароля
    addJsonPost("/api/otapass", handleOtaPass);
    addJsonPost("/api/ntp",     handlePostNtp);
    addJsonPost("/api/ui",      handlePostUi,   ALLOW_DEFAULT);   // тема и язык
    addJsonPost("/api/telemetry", handlePostTelemetry);

    // POST без body
    addPost("/api/login",         handleLogin,  ALLOW_DEFAULT);
    addPost("/api/logout",        handleLogout, ALLOW_DEFAULT);
    addPost("/api/factory_reset", handleFactoryReset);
    addPost("/api/reset_wifi",    handleResetWifi);
    addPost("/api/reboot",        handleReboot);

    // WebSocket и OTA
    WsHandler::attachTo(s_server);
    OtaMgr::attachTo(s_server);

    // 404
    s_server.onNotFound([](AsyncWebServerRequest* req) {
        if (req->url().startsWith("/api/")) {
            // Запрос без верного пароля, отсеянный фильтром JSON-обработчиков,
            // тоже попадает сюда: отвечаем 401, а не 404, чтобы панель
            // отличала «нет доступа» от «нет такого маршрута».
            if (!AuthSess::authorized(req)) {
                countAuthFail(req);
                req->send(401, "application/json", "{\"error\":\"unauthorized\"}");
                return;
            }
            req->send(404, "application/json", "{\"error\":\"not found\"}");
        } else if (NetMgr::isRescueApActive()) {
            // v6.3: аварийная точка доступа перехватывает DNS, и проверки
            // «есть ли интернет» (Android /generate_204, iOS
            // /hotspot-detect.html, Windows /connecttest.txt) приходят сюда.
            // Редирект на панель — ОС видит «портал» и сама открывает браузер.
            AsyncWebServerResponse* resp = req->beginResponse(302);
            resp->addHeader("Location", "http://192.168.4.1/");
            resp->addHeader("Cache-Control", "no-store");
            req->send(resp);
        } else
            req->send(404, "text/plain", "Not found");
    });

    s_server.begin();
    Serial.println(F("[HTTP] Web server started on port 80"));
}
