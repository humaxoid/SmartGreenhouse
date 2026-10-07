// ╔══════════════════════════════════════════════════════════════════╗
// ║  wifi_provisioning.cpp — AP + captive portal v5.0               ║
// ║                                                                  ║
// ║  Технические детали:                                             ║
// ║   • AP на IP 192.168.4.1 (стандарт Arduino ESP32)               ║
// ║   • DNSServer на порту 53, отвечает 192.168.4.1 на ЛЮБОЙ запрос ║
// ║   • AsyncWebServer на порту 80, /portal — форма, / — редирект  ║
// ║   • Обработка особых URL для captive detection:                  ║
// ║     - /generate_204, /hotspot-detect.html, /connecttest.txt     ║
// ║   • Сканирование сетей через WiFi.scanNetworks()                 ║
// ╚══════════════════════════════════════════════════════════════════╝

#include "wifi_provisioning.h"
#include "network_manager.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

// ═══════════════════════════════════════════════════════════════════
//  ВНУТРЕННЕЕ СОСТОЯНИЕ
// ═══════════════════════════════════════════════════════════════════
static DNSServer          s_dns;
static AsyncWebServer*    s_http = nullptr;
static bool               s_active = false;

// Флаги успешного сохранения + перезагрузки (отложенная reboot)
static volatile bool      s_credentialsSaved = false;
static volatile uint64_t  s_rebootAtMs = 0;

// Статический IP для AP (стандартный для ESP32 Arduino)
static const IPAddress    AP_IP(192, 168, 4, 1);
static const IPAddress    AP_NETMASK(255, 255, 255, 0);

// ═══════════════════════════════════════════════════════════════════
//  HTML СТРАНИЦА ПОРТАЛА
// ═══════════════════════════════════════════════════════════════════
// Встроена в прошивку — не зависит от LittleFS (LittleFS может быть
// не смонтирован при provisioning). Минималистичный дизайн, работает
// в любом мобильном браузере.
//
// PROGMEM: строка хранится во flash, не в RAM (экономия ~4 КБ RAM).
static const char PORTAL_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Greenhouse Setup</title>
<style>
*{box-sizing:border-box;margin:0;padding:0;font-family:-apple-system,BlinkMacSystemFont,sans-serif}
body{background:#f0f4f8;color:#2d3748;padding:20px;min-height:100vh}
.card{max-width:420px;margin:0 auto;background:#fff;border-radius:12px;padding:24px;box-shadow:0 4px 16px rgba(0,0,0,0.08)}
h1{font-size:22px;margin-bottom:8px;color:#1a365d}
.sub{font-size:14px;color:#718096;margin-bottom:20px}
label{display:block;font-size:13px;color:#4a5568;margin:14px 0 4px}
input,select{width:100%;padding:10px 12px;font-size:16px;border:1px solid #cbd5e0;border-radius:6px;background:#fff}
input:focus,select:focus{outline:none;border-color:#3182ce}
button{width:100%;padding:12px;margin-top:20px;font-size:16px;font-weight:500;background:#3182ce;color:#fff;border:0;border-radius:6px;cursor:pointer}
button:hover{background:#2c5282}
button:disabled{background:#a0aec0;cursor:wait}
.msg{margin-top:16px;padding:10px;border-radius:6px;font-size:14px;display:none}
.msg.ok{display:block;background:#c6f6d5;color:#22543d}
.msg.err{display:block;background:#fed7d7;color:#742a2a}
.scanbtn{background:none;color:#3182ce;padding:6px 0;margin:0;text-align:left;font-size:13px}
.scanbtn:hover{background:none;text-decoration:underline}
.signal{color:#718096;font-size:12px;margin-left:6px}
</style>
</head>
<body>
<div class="card">
  <h1>Wi-Fi Setup</h1>
  <div class="sub">Smart Greenhouse — initial network setup</div>

  <label for="ssid">Wi-Fi Network</label>
  <select id="ssid"><option value="">— loading... —</option></select>
  <button type="button" class="scanbtn" onclick="scan()">Refresh list</button>

  <label for="pass">Password</label>
  <input type="password" id="pass" autocomplete="off" placeholder="Wi-Fi password">

  <label style="margin-top:16px;display:flex;align-items:center;gap:8px">
    <input type="checkbox" id="show" onchange="togglePass()" style="width:auto">
    <span style="font-size:13px;color:#4a5568">Show password</span>
  </label>

  <button type="button" id="submit" onclick="save()">Save & reboot</button>

  <div id="msg" class="msg"></div>
</div>

<script>
function togglePass(){
  var p=document.getElementById('pass');
  p.type=document.getElementById('show').checked?'text':'password';
}

function scan(){
  var sel=document.getElementById('ssid');
  sel.innerHTML='<option value="">— scanning... —</option>';
  fetch('/scan').then(function(r){return r.json();}).then(function(d){
    sel.innerHTML='<option value="">— select network —</option>';
    for(var i=0;i<d.networks.length;i++){
      var n=d.networks[i];
      var opt=document.createElement('option');
      opt.value=n.ssid;
      opt.textContent=n.ssid+' ('+n.rssi+' dBm'+(n.enc?', 🔒':'')+')';
      sel.appendChild(opt);
    }
    if(d.networks.length===0){
      sel.innerHTML='<option value="">— no networks found —</option>';
    }
  }).catch(function(){
    sel.innerHTML='<option value="">— scan error —</option>';
  });
}

function save(){
  var ssid=document.getElementById('ssid').value;
  var pass=document.getElementById('pass').value;
  var msg=document.getElementById('msg');
  var btn=document.getElementById('submit');
  if(!ssid){ showMsg('err','Select a Wi-Fi network'); return; }
  btn.disabled=true;
  btn.textContent='Saving...';
  var fd=new FormData();
  fd.append('ssid',ssid);
  fd.append('pass',pass);
  fetch('/save',{method:'POST',body:fd}).then(function(r){
    if(r.ok){
      showMsg('ok','Settings saved! Device is rebooting. Please connect your phone to your home Wi-Fi network.');
    }else{
      showMsg('err','Save error. Please try again.');
      btn.disabled=false;
      btn.textContent='Save & reboot';
    }
  }).catch(function(){
    showMsg('err','No connection to device');
    btn.disabled=false;
    btn.textContent='Save & reboot';
  });
}

function showMsg(type,text){
  var m=document.getElementById('msg');
  m.className='msg '+type;
  m.textContent=text;
}

window.onload=scan;
</script>
</body>
</html>)HTML";

// ═══════════════════════════════════════════════════════════════════
//  КОНСТРУИРОВАНИЕ ИМЕНИ AP
// ═══════════════════════════════════════════════════════════════════
// AP name = "Greenhouse-XXXXXX" где XXXXXX = последние 6 hex символов MAC.
// Это даёт уникальное имя для каждого устройства — если рядом 2 теплицы,
// пользователь их различит.
//
// ВАЖНО: используем WiFi.macAddress() (базовый MAC), а НЕ
// esp_wifi_get_mac(WIFI_IF_AP). Причина: на момент вызова buildApSsid
// AP интерфейс ещё не активирован (WiFi.softAP() будет вызван после),
// поэтому esp_wifi_get_mac(WIFI_IF_AP) может вернуть нули или ошибку.
// Базовый MAC (STA) всегда доступен, он прописан в efuse при
// производстве. AP MAC отличается от базового на +1, но для
// идентификации устройства это не критично.
static void buildApSsid(const char* prefix, char* dst, size_t dstSize) {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(dst, dstSize, "%s%02X%02X%02X",
        prefix, mac[3], mac[4], mac[5]);
}

// ═══════════════════════════════════════════════════════════════════
//  HTTP HANDLERS
// ═══════════════════════════════════════════════════════════════════

// Главная: отдаём страницу портала
static void handleRoot(AsyncWebServerRequest* req) {
    req->send_P(200, "text/html", PORTAL_HTML);
}

// Captive portal "детектор" — перенаправление на портал.
// Разные ОС используют разные URL для проверки интернета:
//   • Android: /generate_204
//   • iOS:     /hotspot-detect.html, /library/test/success.html
//   • Windows: /connecttest.txt, /ncsi.txt
//   • Mac:     /hotspot-detect.html
// На все отвечаем редиректом 302 → портал.
static void handleCaptiveRedirect(AsyncWebServerRequest* req) {
    AsyncWebServerResponse* resp = req->beginResponse(302);
    resp->addHeader("Location", "http://192.168.4.1/portal");
    resp->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    req->send(resp);
}

// Сканирование Wi-Fi сетей
static void handleScan(AsyncWebServerRequest* req) {
    // v6.0 FIX: в режиме AP_STA скан безопасен — AP не отваливается.
    // Но всё равно делаем синхронный скан с увеличенным таймаутом, чтобы
    // на загруженных каналах успевало получить полный список.
    int n = WiFi.scanNetworks(false /*async*/, false /*show_hidden*/, false /*passive*/, 500 /*max_ms_per_chan*/);

    String json = "{\"networks\":[";
    // Сортировка: сильнейший сигнал сверху (scanNetworks возвращает в порядке найденного)
    // Для простоты вывода — не сортируем, пользователь разберётся.
    // Ограничиваем 20 сетями, чтобы JSON не раздулся.
    int count = n > 20 ? 20 : n;
    for (int i = 0; i < count; i++) {
        if (i > 0) json += ",";
        String ssid = WiFi.SSID(i);
        // Экранирование кавычек и слешей в JSON
        ssid.replace("\\", "\\\\");
        ssid.replace("\"", "\\\"");
        json += "{\"ssid\":\"" + ssid + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
        json += "\"enc\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? 1 : 0) + "}";
    }
    json += "]}";

    WiFi.scanDelete();
    req->send(200, "application/json", json);
}

// Сохранение настроек
static void handleSave(AsyncWebServerRequest* req) {
    if (!req->hasParam("ssid", true) || !req->hasParam("pass", true)) {
        req->send(400, "text/plain", "Missing ssid or pass");
        return;
    }
    String ssid = req->getParam("ssid", true)->value();
    String pass = req->getParam("pass", true)->value();

    if (ssid.length() == 0 || ssid.length() > 32) {
        req->send(400, "text/plain", "Invalid SSID length");
        return;
    }
    if (pass.length() > 63) {
        req->send(400, "text/plain", "Password too long");
        return;
    }

    // Формируем NetCache для сохранения.
    // Начинаем с текущего кэша (чтобы сохранить IP, DNS и т.д.),
    // только SSID и пароль обновляем.
    NetCache nc = NetMgr::cache();
    strlcpy(nc.ssid, ssid.c_str(), sizeof(nc.ssid));
    strlcpy(nc.pass, pass.c_str(), sizeof(nc.pass));
    // DHCP по умолчанию при первой настройке — пользователь легко
    // поменяет потом через веб-панель.
    nc.isDHCP = true;

    NetMgr::saveNetSettings(nc, true);   // true = сохранить и пароль

    Serial.printf("[PROV] Saved: ssid=\"%s\", pass=%u chars. Reboot in 3s.\n",
        ssid.c_str(), pass.length());

    req->send(200, "text/plain", "OK");

    // Отложенная перезагрузка: даём браузеру получить ответ
    s_credentialsSaved = true;
    s_rebootAtMs = (uint64_t)esp_timer_get_time() / 1000ULL + 3000ULL;
}

// Универсальный catch-all: всё что не /portal, /scan, /save → редирект на портал
static void handleNotFound(AsyncWebServerRequest* req) {
    // Для некоторых captive detection URL браузер/ОС ожидает 200+пустое тело
    // и интерпретирует любой редирект как "captive portal обнаружен".
    // Редирект работает надёжно для всех ОС.
    handleCaptiveRedirect(req);
}

// ═══════════════════════════════════════════════════════════════════
//  ПУБЛИЧНЫЙ API
// ═══════════════════════════════════════════════════════════════════

void WifiProv::start(const char* ssidPrefix, const char* password) {
    s_active = true;

    // ── 1. Поднимаем AP ──────────────────────────────────────────
    char apSsid[32];
    buildApSsid(ssidPrefix, apSsid, sizeof(apSsid));

    // Сначала mode(OFF) — очищаем предыдущее состояние Wi-Fi
    // (если был зарегистрирован STA event handler в NetMgr::init,
    // он не мешает, но mode может быть в странном состоянии).
    WiFi.mode(WIFI_OFF);
    vTaskDelay(pdMS_TO_TICKS(100));

    // v6.0 FIX: используем WIFI_AP_STA (двойной режим), а не WIFI_AP.
    // Причина: при чистом WIFI_AP вызов WiFi.scanNetworks() заставляет
    // радио переключиться на скан → AP отваливается на 2-4 сек → клиенты
    // теряют связь, пинги пропадают. В режиме AP_STA скан идёт через STA
    // интерфейс, а AP остаётся на своём канале.
    WiFi.mode(WIFI_AP_STA);
    vTaskDelay(pdMS_TO_TICKS(100));

    // STA нужно явно отключить от любых сохранённых сетей чтобы он не
    // пытался подключаться и не дёргал канал.
    //
    // v6.1: первый аргумент был true, а это `wifioff` — WiFi.disconnect()
    // при нём вызывает enableSTA(false) и режим тут же падает с AP_STA
    // обратно в чистый AP. То есть строка выше, поставленная в v6.0 ровно
    // ради того, чтобы скан сетей в портале не ронял точку доступа на
    // 2–4 секунды, отменялась следующей же строкой. Отключаемся, не гася
    // интерфейс.
    WiFi.disconnect(false, true);
    vTaskDelay(pdMS_TO_TICKS(100));

    // softAPConfig ДО softAP — задаёт IP адреса AP
    if (!WiFi.softAPConfig(AP_IP, AP_IP, AP_NETMASK)) {
        Serial.println(F("[PROV] softAPConfig FAILED"));
    }

    // v6.0 FIX: фиксированный канал 6 (середина диапазона 2.4 ГГц).
    // Раньше был 1, но в шумной среде канал 6 стабильнее.
    // hidden=0, max_connection=4
    bool apOk = WiFi.softAP(apSsid, password, 6, 0, 4);
    vTaskDelay(pdMS_TO_TICKS(500));  // AP нужно время на полный старт

    // v6.0 FIX: повышаем мощность передатчика до максимума (19.5 dBm)
    // — улучшает дальность и стабильность связи в режиме AP
    WiFi.setTxPower(WIFI_POWER_19_5dBm);

    Serial.println(F("═══════════════════════════════════════════"));
    if (apOk) {
        Serial.printf("[PROV] AP started: \"%s\"\n", apSsid);
        Serial.printf("[PROV] Password: %s\n", password);
        Serial.printf("[PROV] AP IP: %s\n", WiFi.softAPIP().toString().c_str());
        Serial.printf("[PROV] AP MAC: %s\n", WiFi.softAPmacAddress().c_str());
        Serial.println(F("[PROV] Connect your phone, open browser → http://192.168.4.1"));
    } else {
        Serial.printf("[PROV] !!! AP START FAILED for \"%s\" !!!\n", apSsid);
        Serial.println(F("[PROV] Check WiFi radio hardware. Rebooting in 10 sec..."));
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP.restart();
    }
    Serial.println(F("═══════════════════════════════════════════"));

    // ── 2. DNS hijack (captive portal) ───────────────────────────
    // "*" — любой запрос → 192.168.4.1
    s_dns.setErrorReplyCode(DNSReplyCode::NoError);
    s_dns.start(53, "*", AP_IP);

    // ── 3. HTTP-сервер ───────────────────────────────────────────
    if (!s_http) {
        s_http = new AsyncWebServer(80);
    }

    // Главная страница
    s_http->on("/",        HTTP_GET,  handleRoot);
    s_http->on("/portal",  HTTP_GET,  handleRoot);

    // API
    s_http->on("/scan",    HTTP_GET,  handleScan);
    s_http->on("/save",    HTTP_POST, handleSave);

    // Captive detection URL (Android, iOS, Windows, macOS)
    s_http->on("/generate_204",           HTTP_GET, handleCaptiveRedirect);
    s_http->on("/gen_204",                HTTP_GET, handleCaptiveRedirect);
    s_http->on("/hotspot-detect.html",    HTTP_GET, handleCaptiveRedirect);
    s_http->on("/library/test/success.html", HTTP_GET, handleCaptiveRedirect);
    s_http->on("/connecttest.txt",        HTTP_GET, handleCaptiveRedirect);
    s_http->on("/ncsi.txt",               HTTP_GET, handleCaptiveRedirect);
    s_http->on("/success.txt",            HTTP_GET, handleCaptiveRedirect);

    // Всё остальное → редирект на портал
    s_http->onNotFound(handleNotFound);

    s_http->begin();
    Serial.println(F("[PROV] HTTP server started on port 80"));
}

void WifiProv::loop() {
    if (!s_active) return;

    // DNS-запросы: это ЛЁГКАЯ операция, безопасно вызывать часто.
    s_dns.processNextRequest();

    // Отложенная перезагрузка после успешного сохранения
    if (s_credentialsSaved && s_rebootAtMs > 0) {
        uint64_t now = (uint64_t)esp_timer_get_time() / 1000ULL;
        if (now >= s_rebootAtMs) {
            Serial.println(F("[PROV] Rebooting now..."));
            vTaskDelay(pdMS_TO_TICKS(100));  // flush serial
            ESP.restart();
        }
    }
}

bool WifiProv::isActive() {
    return s_active;
}
