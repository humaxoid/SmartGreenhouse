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
#include <atomic>

// ═══════════════════════════════════════════════════════════════════
//  ВНУТРЕННЕЕ СОСТОЯНИЕ
// ═══════════════════════════════════════════════════════════════════
static DNSServer          s_dns;
static AsyncWebServer*    s_http = nullptr;
static bool               s_active = false;

// Флаги успешного сохранения + перезагрузки (отложенная reboot)
static volatile bool      s_credentialsSaved = false;
static volatile uint64_t  s_rebootAtMs = 0;

// v6.3: проверка пароля ДО сохранения. Раньше портал сохранял SSID и
// пароль не глядя и перезагружался; с опечаткой в пароле контроллер
// уходил в аварийную точку доступа, и вернуть портал можно было только
// кнопкой «Забыть Wi-Fi», о которой владелец не знал. Теперь /save лишь
// принимает заявку, а пробное подключение ведёт loop() (loopTask):
// успех — сохранить и перезагрузиться, отказ — сказать об этом на той же
// странице. Обработчик HTTP работает в задаче AsyncTCP, ждать в нём
// 20 секунд нельзя.
enum ProvTest : uint8_t { PT_IDLE, PT_REQUESTED, PT_RUNNING, PT_OK, PT_FAIL };
static std::atomic<uint8_t>  s_test{PT_IDLE};
static char                  s_testSsid[33];
static char                  s_testPass[64];
static uint64_t              s_testStartMs = 0;
static std::atomic<bool>     s_testGotIp{false};
// Было событие STA_CONNECTED: точка приняла нас, рукопожатие WPA пройдено,
// то есть пароль верный. Если адреса после этого нет — дело в DHCP сети
// (выдача только известным MAC и т.п.), а не в пароле.
static std::atomic<bool>     s_testAssoc{false};
static std::atomic<uint8_t>  s_testDiscCount{0};
static std::atomic<uint16_t> s_testLastReason{0};
static char                  s_testResult[24] = "";   // причина отказа для /status
static uint16_t              s_testCode = 0;          // код причины, замороженный в момент отказа
static char                  s_testIp[16]     = "";   // адрес в домашней сети при успехе
#define PROV_TEST_TIMEOUT_MS   20000ULL
#define PROV_TEST_MAX_DISC     3       // столько отказов подряд — пароль неверный

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
  <h1 data-t="h1">Wi-Fi Setup</h1>
  <div class="sub" data-t="sub">Smart Greenhouse — initial network setup</div>

  <label for="ssid" data-t="lbl_net">Wi-Fi Network</label>
  <select id="ssid"><option value="">— loading... —</option></select>
  <button type="button" class="scanbtn" onclick="scan()" data-t="refresh">Refresh list</button>

  <label for="pass" data-t="lbl_pass">Password</label>
  <input type="password" id="pass" autocomplete="off" placeholder="Wi-Fi password">

  <label style="margin-top:16px;display:flex;align-items:center;gap:8px">
    <input type="checkbox" id="show" onchange="togglePass()" style="width:auto">
    <span style="font-size:13px;color:#4a5568" data-t="show">Show password</span>
  </label>

  <button type="button" id="submit" onclick="save()" data-t="connect">Connect</button>

  <div id="msg" class="msg"></div>
</div>

<script>
// Язык — по языку браузера: русский для ru*, иначе английский. Выбора на
// странице нет намеренно: портал открывается один раз, и телефон уже знает,
// на каком языке говорит его владелец.
var T={
en:{title:'Greenhouse Setup',h1:'Wi-Fi Setup',sub:'Smart Greenhouse — initial network setup',
  lbl_net:'Wi-Fi Network',refresh:'Refresh list',lbl_pass:'Password',ph_pass:'Wi-Fi password',
  show:'Show password',connect:'Connect',loading:'— loading... —',scanning:'— scanning... —',
  select:'— select network —',none:'— no networks found —',scan_err:'— scan error —',
  need_net:'Select a Wi-Fi network',
  pass_len:'A Wi-Fi password is 8 to 63 characters (leave it empty for an open network)',
  checking_btn:'Checking...',
  checking:'Checking the password, up to 20 seconds. Your phone may briefly lose this network — that is normal.',
  err:'Error: ',no_conn:'No connection to device',
  ok:'Connected! Settings saved, the device is rebooting. Connect your phone to your home Wi-Fi',
  ok_ip:' and open http://{ip}',
  wrong_password:'Wrong password. Nothing was saved — check the password and try again.',
  not_found:'Network not found. Move the device closer to the router or pick another network.',
  no_ip:'The password was accepted, but the network did not give the device an IP address (DHCP). '+
    'The router probably hands out addresses only to known devices — reserve an address for the '+
    'controller\'s MAC in its DHCP settings. Nothing was saved.',
  dropped:'The password was accepted, but the network then dropped the connection{code}. '+
    'Check the router\'s limits on clients or MAC addresses. Nothing was saved.',
  timeout:'The network did not respond within 20 seconds. Move the device closer to the router and try again. '+
    'Nothing was saved.',
  save_failed:'Connected, but the settings could not be written to memory. Try again.',
  failed:'Could not connect{code}. The network refused the connection for a reason other than the password '+
    '(for example WPA3-only or WPA2-Enterprise). Nothing was saved.',
  code:' (code {n})',
  lost:'Lost contact with the device. If the Greenhouse network is gone, the device has connected and rebooted. '+
    'If it is still there, reconnect to it and open http://192.168.4.1 again.'},
ru:{title:'Настройка теплицы',h1:'Настройка Wi-Fi',sub:'Умная теплица — первичная настройка сети',
  lbl_net:'Сеть Wi-Fi',refresh:'Обновить список',lbl_pass:'Пароль',ph_pass:'Пароль Wi-Fi',
  show:'Показать пароль',connect:'Подключить',loading:'— загрузка… —',scanning:'— поиск сетей… —',
  select:'— выберите сеть —',none:'— сети не найдены —',scan_err:'— ошибка поиска —',
  need_net:'Выберите сеть Wi-Fi',
  pass_len:'Пароль Wi-Fi — от 8 до 63 символов (для открытой сети оставьте поле пустым)',
  checking_btn:'Проверка…',
  checking:'Проверяю пароль, до 20 секунд. Телефон может ненадолго потерять эту сеть — это нормально.',
  err:'Ошибка: ',no_conn:'Нет связи с устройством',
  ok:'Подключено! Настройки сохранены, устройство перезагружается. Подключите телефон к домашней сети Wi-Fi',
  ok_ip:' и откройте http://{ip}',
  wrong_password:'Неверный пароль. Ничего не сохранено — проверьте пароль и попробуйте снова.',
  not_found:'Сеть не найдена. Поднесите устройство ближе к роутеру или выберите другую сеть.',
  no_ip:'Пароль принят, но сеть не выдала устройству IP-адрес (DHCP). '+
    'Скорее всего, роутер раздаёт адреса только известным устройствам — закрепите адрес '+
    'за MAC контроллера в настройках DHCP. Ничего не сохранено.',
  dropped:'Пароль принят, но сеть затем разорвала подключение{code}. '+
    'Проверьте ограничения роутера по числу клиентов или по MAC-адресам. Ничего не сохранено.',
  timeout:'Сеть не ответила за 20 секунд. Поднесите устройство ближе к роутеру и попробуйте снова. '+
    'Ничего не сохранено.',
  save_failed:'Подключено, но настройки не удалось записать в память. Попробуйте ещё раз.',
  failed:'Не удалось подключиться{code}. Сеть отклонила подключение по причине, не связанной с паролем '+
    '(например, только WPA3 или WPA2-Enterprise). Ничего не сохранено.',
  code:' (код {n})',
  lost:'Связь с устройством потеряна. Если сети Greenhouse больше нет — устройство подключилось и перезагрузилось. '+
    'Если она есть — подключитесь к ней снова и откройте http://192.168.4.1.'}
};
var L=/^ru\b/i.test(navigator.language||'')?'ru':'en';
function t(k,a){
  var v=T[L][k]||T.en[k]||k;
  if(a)for(var x in a)v=v.split('{'+x+'}').join(a[x]);
  return v;
}
function applyLang(){
  document.documentElement.lang=L;
  document.title=t('title');
  var els=document.querySelectorAll('[data-t]');
  for(var i=0;i<els.length;i++)els[i].textContent=t(els[i].getAttribute('data-t'));
  document.getElementById('pass').placeholder=t('ph_pass');
}
function opt1(sel,k){
  sel.textContent='';
  var o=document.createElement('option');
  o.value='';o.textContent=t(k);
  sel.appendChild(o);
}

function togglePass(){
  var p=document.getElementById('pass');
  p.type=document.getElementById('show').checked?'text':'password';
}

function scan(){
  var sel=document.getElementById('ssid');
  opt1(sel,'scanning');
  fetch('/scan').then(function(r){return r.json();}).then(function(d){
    opt1(sel,d.networks.length?'select':'none');
    for(var i=0;i<d.networks.length;i++){
      var n=d.networks[i];
      var opt=document.createElement('option');
      opt.value=n.ssid;
      opt.textContent=n.ssid+' ('+n.rssi+' dBm'+(n.enc?', 🔒':'')+')';
      sel.appendChild(opt);
    }
  }).catch(function(){
    opt1(sel,'scan_err');
  });
}

function save(){
  var ssid=document.getElementById('ssid').value;
  var pass=document.getElementById('pass').value;
  var btn=document.getElementById('submit');
  if(!ssid){ showMsg('err',t('need_net')); return; }
  // Длина — в байтах UTF-8, как её считает контроллер (и стандарт WPA2).
  var pb=unescape(encodeURIComponent(pass)).length;
  if(pb&&(pb<8||pb>63)){ showMsg('err',t('pass_len')); return; }
  btn.disabled=true;
  btn.textContent=t('checking_btn');
  showMsg('ok',t('checking'));
  var fd=new FormData();
  fd.append('ssid',ssid);
  fd.append('pass',pass);
  fetch('/save',{method:'POST',body:fd}).then(function(r){
    if(r.ok){ poll(Date.now()); }
    else{ r.text().then(function(x){ fail(t('err')+x); }); }
  }).catch(function(){ fail(t('no_conn')); });
}

// Опрос результата пробного подключения. Пока контроллер подключается к
// домашней сети, его точка доступа может переехать на её канал и телефон
// ненадолго отвалится — ошибки запросов в это время ожидаемы.
var FAIL_KEYS={wrong_password:1,not_found:1,no_ip:1,timeout:1,save_failed:1,dropped:1};
function poll(t0){
  fetch('/status',{cache:'no-store'}).then(function(r){return r.json();}).then(function(d){
    if(d.state==='ok'){
      showMsg('ok',t('ok')+(d.ip?t('ok_ip',{ip:d.ip}):'')+'.');
      return;
    }
    if(d.state==='fail'){
      var cd={code:d.code?t('code',{n:d.code}):''};
      fail(FAIL_KEYS[d.reason]?t(d.reason,cd):t('failed',cd));
      return;
    }
    next(t0);
  }).catch(function(){ next(t0); });
}
function next(t0){
  if(Date.now()-t0>60000){ fail(t('lost')); return; }
  setTimeout(function(){ poll(t0); },1000);
}
function fail(text){
  var btn=document.getElementById('submit');
  showMsg('err',text);
  btn.disabled=false;
  btn.textContent=t('connect');
}

function showMsg(type,text){
  var m=document.getElementById('msg');
  m.className='msg '+type;
  m.textContent=text;
}

window.onload=function(){ applyLang(); scan(); };
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
    // v6.3: WPA2 требует 8–63 символов; короче — WiFi.begin() не даёт ни одного
    // события отказа, и проверка ждала бы весь таймаут. Открытая сеть — пустой пароль.
    if (pass.length() > 0 && pass.length() < 8) {
        req->send(400, "text/plain", "Password must be 8-63 characters (or empty for an open network)");
        return;
    }

    // v6.3: не сохраняем сразу — ставим заявку на пробное подключение
    // (см. WifiProv::loop). Буферы пишем, только пока проверка не идёт:
    // loop() читает их после того, как увидит PT_REQUESTED.
    const uint8_t st = s_test.load();
    if (st == PT_REQUESTED || st == PT_RUNNING || st == PT_OK) {
        req->send(409, "text/plain", "Check already in progress");
        return;
    }
    strlcpy(s_testSsid, ssid.c_str(), sizeof(s_testSsid));
    strlcpy(s_testPass, pass.c_str(), sizeof(s_testPass));
    s_test.store(PT_REQUESTED);

    Serial.printf("[PROV] Test requested: ssid=\"%s\", pass=%u chars\n",
        ssid.c_str(), pass.length());
    req->send(202, "text/plain", "Checking");
}

// Состояние пробного подключения: {"state":"testing|ok|fail|idle", ...}
static void handleStatus(AsyncWebServerRequest* req) {
    const uint8_t st = s_test.load();
    char buf[128];
    const char* name = (st == PT_OK) ? "ok" : (st == PT_FAIL) ? "fail"
                     : (st == PT_IDLE) ? "idle" : "testing";
    // code — причина отказа от модуля Wi-Fi (WIFI_REASON_*): по ней разбирают
    // жалобу «не подключается», не подключая USB.
    snprintf(buf, sizeof(buf), "{\"state\":\"%s\",\"reason\":\"%s\",\"code\":%u,\"ip\":\"%s\"}",
        name, st == PT_FAIL ? s_testResult : "",
        st == PT_FAIL ? (unsigned)s_testCode : 0u,
        st == PT_OK ? s_testIp : "");
    AsyncWebServerResponse* resp = req->beginResponse(200, "application/json", buf);
    resp->addHeader("Cache-Control", "no-store");
    req->send(resp);
}

// События STA во время пробного подключения. Контекст event task —
// только атомарные флаги.
static void onProvWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (s_test.load() != PT_RUNNING) return;
    if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
        s_testGotIp.store(true);
    } else if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
        s_testAssoc.store(true);
    } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        // ASSOC_LEAVE — отключились мы сами: ядро Arduino при автопереподключении
        // зовёт WiFi.disconnect(), портал — по итогу проверки. Это не отказ
        // сети; посчитанный как отказ, он подменял бы настоящую причину.
        const uint16_t r = info.wifi_sta_disconnected.reason;
        if (r == WIFI_REASON_ASSOC_LEAVE) return;
        s_testLastReason.store(r);
        s_testDiscCount.fetch_add(1);
    }
}

// Причина отказа по коду ESP-IDF. Неверный пароль на практике приходит
// как AUTH_FAIL или как таймаут рукопожатия WPA — по-разному у разных роутеров.
static const char* testFailReason(uint16_t r) {
    switch (r) {
        case WIFI_REASON_NO_AP_FOUND:
            return "not_found";
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_AUTH_EXPIRE:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_MIC_FAILURE:
            return "wrong_password";
        default:
            return "failed";
    }
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
    s_http->on("/status",  HTTP_GET,  handleStatus);

    WiFi.onEvent(onProvWiFiEvent);

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

    // ── v6.3: пробное подключение к выбранной сети ───────────────
    const uint64_t nowMs = (uint64_t)esp_timer_get_time() / 1000ULL;
    if (s_test.load() == PT_REQUESTED) {
        s_testGotIp.store(false);
        s_testAssoc.store(false);
        s_testDiscCount.store(0);
        s_testLastReason.store(0);
        s_testResult[0] = '\0';
        s_testIp[0] = '\0';
        s_testStartMs = nowMs;
        s_test.store(PT_RUNNING);
        // Режим AP_STA уже стоит: точка доступа остаётся, телефон может
        // ненадолго отвалиться, пока радио переходит на канал домашней сети.
        WiFi.begin(s_testSsid, s_testPass);
        Serial.printf("[PROV] Testing \"%s\"...\n", s_testSsid);
    } else if (s_test.load() == PT_RUNNING) {
        if (s_testGotIp.load()) {
            strlcpy(s_testIp, WiFi.localIP().toString().c_str(), sizeof(s_testIp));
            // Формируем NetCache для сохранения.
            // Начинаем с текущего кэша (чтобы сохранить IP, DNS и т.д.),
            // только SSID и пароль обновляем.
            NetCache nc = NetMgr::cache();
            strlcpy(nc.ssid, s_testSsid, sizeof(nc.ssid));
            strlcpy(nc.pass, s_testPass, sizeof(nc.pass));
            // DHCP по умолчанию при первой настройке — пользователь легко
            // поменяет потом через веб-панель.
            nc.isDHCP = true;
            if (NetMgr::saveNetSettings(nc, true)) {
                Serial.printf("[PROV] Connected (IP %s), saved. Reboot in 8s.\n", s_testIp);
                s_test.store(PT_OK);
                // 8 с, а не 3: телефон мог отвалиться при смене канала и
                // должен успеть вернуться и забрать результат с адресом.
                s_credentialsSaved = true;
                s_rebootAtMs = nowMs + 8000ULL;
            } else {
                strlcpy(s_testResult, "save_failed", sizeof(s_testResult));
                WiFi.disconnect(false);
                s_test.store(PT_FAIL);
            }
        } else if (s_testDiscCount.load() >= PROV_TEST_MAX_DISC
                   || nowMs - s_testStartMs >= PROV_TEST_TIMEOUT_MS) {
            const uint16_t reason = s_testLastReason.load();
            const char* res = reason ? testFailReason(reason) : "timeout";
            // Точка нас уже принимала — пароль верный, что бы ни пришло потом:
            // таймаут при этом значит, что сеть не выдала адрес (DHCP).
            if (s_testAssoc.load()) {
                res = (strcmp(res, "timeout") == 0) ? "no_ip" : "dropped";
            }
            strlcpy(s_testResult, res, sizeof(s_testResult));
            s_testCode = reason;
            // disconnect(false): STA останавливаем, точку доступа — нет.
            WiFi.disconnect(false);
            Serial.printf("[PROV] Test FAILED: %s (reason %u, %u disconnects, %s)\n",
                s_testResult, (unsigned)reason, (unsigned)s_testDiscCount.load(),
                s_testAssoc.load() ? "was associated" : "never associated");
            s_test.store(PT_FAIL);
        }
    }

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
