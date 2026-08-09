// ╔══════════════════════════════════════════════════════════════════╗
// ║  ws_handler.cpp — Реализация WebSocket v5.0                     ║
// ║                                                                  ║
// ║  Поток сообщений:                                                ║
// ║   Клиент ──[auth token]──▶ ESP32                                 ║
// ║   ESP32  ──[state JSON]──▶ Клиент (1 раз/сек)                   ║
// ║   Клиент ──[command JSON]──▶ ESP32                               ║
// ║   ESP32  ──[event JSON]──▶ Клиент (при изменениях)              ║
// ║                                                                  ║
// ║  JSON формат сообщений — в комментариях к handleXxx() функциям  ║
// ╚══════════════════════════════════════════════════════════════════╝

#include "ws_handler.h"
#include "settings.h"
#include "relay_manager.h"
#include "sensor_reader.h"
#include "vent_controller.h"
#include "water_pump_controller.h"
#include "timer_scheduler.h"
#include "history_logger.h"
#include "network_manager.h"
#include "mqtt_manager.h"
#include <ArduinoJson.h>      // v7.x — только JsonDocument
#include <ESPAsyncWebServer.h>
#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════
//  ГЛОБАЛЬНЫЕ ОБЪЕКТЫ
// ═══════════════════════════════════════════════════════════════════
static AsyncWebSocket s_ws("/ws");

// Размер документа для broadcast — ArduinoJson v7 использует динамический
// аллокатор, но мы всё равно задаём буфер сериализации нужного размера.
// Типичный broadcast v5: ~800-1200 байт.
#define WS_TX_BUFFER_SIZE     1600

// v6.1: Static-буферы для serializeJson — снимают нагрузку со стека taskCore1
// (8192 байт стека / 3000 байт буфер = опасный запас при глубоких вложениях).
// Защищены mutex: WS broadcast может вызываться параллельно с history-ответами.
#include <freertos/semphr.h>
static char s_wsTxBuf[WS_TX_BUFFER_SIZE];        // снапшот state — основной
static char s_wsHistBuf[3200];                   // история (24 точки)
static SemaphoreHandle_t s_wsBufMutex = nullptr;

// ═══════════════════════════════════════════════════════════════════
//  АУТЕНТИФИКАЦИЯ WEBSOCKET (v6.1)
// ═══════════════════════════════════════════════════════════════════
// До этой версии обработчик /ws регистрировался без какой-либо проверки:
// сокет был доступен любому в локальной сети, и командой set_relay можно
// было щёлкать реле без пароля. Веб-панель передавала логин и пароль
// в URL сокета (ws://user:pass@host/ws), но браузеры такие данные
// не отправляют — защиты не было вообще.
//
// Встроенной авторизации у AsyncWebSocket нет, а навешивать её на
// рукопожатие через middleware — путь, зависящий от версии библиотеки.
// Поэтому авторизация СВОЯ, на уровне протокола:
//
//   1. Клиент подключается. Состояние не отдаётся, команды не принимаются.
//   2. Первым сообщением клиент шлёт {"cmd":"auth","token":"<base64 user:pass>"}.
//   3. Токен сверяется с NetMgr::wsToken() — той же строкой, что
//      используется для HTTP Basic Auth. Отдельных учёток не заводим.
//   4. При совпадении клиент помечается авторизованным и получает снапшот.
//      При несовпадении — ошибка и разрыв соединения.
//
// Неавторизованные соединения, которые ничего не шлют, закрываются при
// первой же активности любого другого клиента (см. sweepUnauthorized).
// Отдельного таймера нет намеренно: периодического хука в контексте
// AsyncTCP не существует, а трогать сокет из чужой задачи нельзя.

#define WS_AUTH_SLOTS        8
#define WS_AUTH_TIMEOUT_MS   10000ULL

struct WsClientAuth {
    uint32_t id;          // 0 = слот свободен
    bool     authed;
    uint64_t connectedMs;
};
static WsClientAuth s_auth[WS_AUTH_SLOTS] = {};

static WsClientAuth* authSlot(uint32_t id, bool createIfMissing) {
    WsClientAuth* freeSlot = nullptr;
    for (uint8_t i = 0; i < WS_AUTH_SLOTS; i++) {
        if (s_auth[i].id == id && id != 0) return &s_auth[i];
        if (s_auth[i].id == 0 && !freeSlot) freeSlot = &s_auth[i];
    }
    if (!createIfMissing || !freeSlot) return nullptr;
    freeSlot->id          = id;
    freeSlot->authed      = false;
    freeSlot->connectedMs = steadyMs();
    return freeSlot;
}

static void authRelease(uint32_t id) {
    for (uint8_t i = 0; i < WS_AUTH_SLOTS; i++) {
        if (s_auth[i].id == id) { s_auth[i] = {0, false, 0}; return; }
    }
}

static bool isAuthed(uint32_t id) {
    WsClientAuth* s = authSlot(id, false);
    return s && s->authed;
}

// Закрыть соединения, которые подключились и не авторизовались вовремя.
// Вызывается из контекста AsyncTCP при обработке входящего сообщения.
static void sweepUnauthorized() {
    uint64_t now = steadyMs();
    for (uint8_t i = 0; i < WS_AUTH_SLOTS; i++) {
        if (s_auth[i].id == 0 || s_auth[i].authed) continue;
        if (now - s_auth[i].connectedMs < WS_AUTH_TIMEOUT_MS) continue;
        Serial.printf("[WS] Client #%u dropped — no auth within %u s\n",
            (unsigned)s_auth[i].id, (unsigned)(WS_AUTH_TIMEOUT_MS / 1000));
        s_ws.close(s_auth[i].id);
        s_auth[i] = {0, false, 0};
    }
}

// ═══════════════════════════════════════════════════════════════════
//  СЕРИАЛИЗАЦИЯ СОСТОЯНИЯ
// ═══════════════════════════════════════════════════════════════════

// Формат broadcast:
// {
//   "type": "state",
//   "uptime": 12345,
//   "time": "14:32:01",
//   "sensors": {
//     "temp": 24.5, "humidity": 67.2, "pressure": 758.0,
//     "lux": 12500, "rain": false, "waterLevel": 72.5,
//     "bmeOk": true, "dhtOk": true, "luxOk": true, "waterOk": true
//   },
//   "relays": [false, false, false, false, false, false, false, false, false],
//   "modes": {"vents": 1, "timers": 0, "humid": 1, "pump": 1},
//   "vents": {
//     "upper": {"current": 45, "target": 50, "direction": 1, "calibrating": false},
//     "lower": {"current": 0,  "target": 0,  "direction": 0, "calibrating": false}
//   },
//   "timers": [
//     {"active": false, "remaining": 0,   "nextStart": 21600},
//     {"active": true,  "remaining": 480, "nextStart": -1},
//     {"active": false, "remaining": 0,   "nextStart": -1}
//   ],
//   "pump": {"fault": false},
//   "wifi": {"rssi": -55, "ip": "192.168.1.100"},
//   "heap": 145000
// }
static void buildStateJson(JsonDocument& doc) {
    doc["type"]   = "state";
        {
        char vbuf[16];
        snprintf(vbuf, sizeof(vbuf), "%u.%u", (unsigned)FW_VERSION_MAJOR, (unsigned)FW_VERSION_MINOR);
        doc["fwVersion"] = vbuf;
    }
    doc["uptime"] = (uint32_t)(steadyMs() / 1000ULL);
    doc["time"]   = NetMgr::getFormattedTime();

    // ── Датчики ──────────────────────────────────────────────────
    SensorSnapshot s = SensorRdr::getData();
    JsonObject sensors = doc["sensors"].to<JsonObject>();

    float temp = !isnan(s.bmeTemp) ? s.bmeTemp : s.dhtTemp;
    float hum  = !isnan(s.bmeHum)  ? s.bmeHum  : s.dhtHum;
    if (!isnan(temp)) sensors["temp"]     = round(temp * 10) / 10.0;
    if (!isnan(hum))  sensors["humidity"] = round(hum * 10) / 10.0;

    // v6.1: раздельные показания DHT22 — то, что снаружи теплицы.
    // temp/humidity выше объединённые, с приоритетом BME280, поэтому
    // веб-панель подставляла в блок «DHT22» те же цифры, что и в приборы.
    // Для графиков раздельные ряды (T2/H2) уже были, для живого экрана — нет.
    if (s.dhtOk && !isnan(s.dhtTemp)) sensors["tempOut"]     = round(s.dhtTemp * 10) / 10.0;
    if (s.dhtOk && !isnan(s.dhtHum))  sensors["humidityOut"] = round(s.dhtHum  * 10) / 10.0;
    if (s.bmeOk && !isnan(s.bmePress))
        sensors["pressure"] = round(s.bmePress * 10) / 10.0;
    if (s.luxOk)      sensors["lux"]        = (uint32_t)s.lux;
    sensors["rain"] = s.rain;
    if (s.waterOk && !isnan(s.waterLevelPct))
        sensors["waterLevel"] = round(s.waterLevelPct * 10) / 10.0;

    // bmeTempOk = BME280 подключён и T/H валидны (даже если давление вне диапазона)
    // bmeOk = полный успех включая давление
    sensors["bmeOk"]     = !isnan(s.bmeTemp);   // датчик отвечает → чип зелёный
    sensors["bmePressOk"]= s.bmeOk;             // давление в норме → давление видно
    sensors["dhtOk"]   = s.dhtOk;
    sensors["luxOk"]   = s.luxOk;
    sensors["waterOk"] = s.waterOk;

    // ── Реле (массив из NUM_RELAYS булевых) ──────────────────────
    JsonArray relays = doc["relays"].to<JsonArray>();
    for (uint8_t i = 0; i < NUM_RELAYS; i++) {
        relays.add(RelayMgr::getState(i));
    }

    // ── Режимы ───────────────────────────────────────────────────
    JsonObject modes = doc["modes"].to<JsonObject>();
    {
        MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(10));
        if (lock.locked()) {
            modes["vents"]  = g_settings.modeVents;
            modes["timers"] = g_settings.modeTimers;
            modes["humid"]  = g_settings.modeHumid;
            modes["pump"]   = g_settings.modeWaterPump;
        }
    }

    // ── Форточки v6.0 ────────────────────────────────────────────
    {
        VentStatusSnapshot vs;
        VentCtrl::getStatus(vs);

        JsonObject vents = doc["vents"].to<JsonObject>();
        JsonObject upper = vents["upper"].to<JsonObject>();
        upper["current"]     = vs.topPct;
        upper["target"]      = vs.topPct;            // совместимость с UI
        upper["direction"]   = (int)vs.topDir;       // -1/0/+1
        upper["moving"]      = vs.topMoving;
        upper["calibrating"] = vs.topHoming;          // v6.1: калибровочный прогон
        upper["calibSec"]    = vs.topCalibSec;

        JsonObject lower = vents["lower"].to<JsonObject>();
        lower["current"]     = vs.bottomPct;
        lower["target"]      = vs.bottomPct;
        lower["direction"]   = (int)vs.bottomDir;
        lower["moving"]      = vs.bottomMoving;
        lower["calibrating"] = vs.bottomHoming;
        lower["calibSec"]    = vs.bottomCalibSec;

        vents["autoState"]   = vs.autoState;
        vents["rainBlocked"] = vs.rainBlocked;
    }

    // ── Таймеры ──────────────────────────────────────────────────
    JsonArray timers = doc["timers"].to<JsonArray>();
    for (uint8_t i = 0; i < NUM_IRRIGATION; i++) {
        JsonObject t = timers.add<JsonObject>();
        t["active"]    = TimerSched::isActive(i);
        t["remaining"] = TimerSched::getRemainingSec(i);
        t["nextStart"] = TimerSched::getNextStartSec(i);
    }

    // ── Насос ────────────────────────────────────────────────────
    JsonObject pump = doc["pump"].to<JsonObject>();
    pump["fault"] = WaterPumpCtrl::isInFault();

    // ── WiFi ─────────────────────────────────────────────────────
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["rssi"] = WiFi.RSSI();
    wifi["ip"]   = WiFi.localIP().toString();

    // ── Heap (для диагностики) ───────────────────────────────────
    doc["heap"] = ESP.getFreeHeap();
}

// ═══════════════════════════════════════════════════════════════════
//  v6.1: ПОЧЕМУ ЗДЕСЬ БОЛЬШЕ НЕТ BROADCAST'А ИЗ taskCore1
// ═══════════════════════════════════════════════════════════════════
// AsyncWebSocket НЕ потокобезопасен. В ESPAsyncWebServer 3.x список
// клиентов объявлен как
//     std::list<AsyncWebSocketClient> _clients;     // AsyncWebSocket.h
// и НЕ защищён мьютексом (для сравнения — у AsyncEventSource такой
// мьютекс есть, там в заголовке прямо написано "protect mutations of
// _clients list"). Клиенты хранятся ПО ЗНАЧЕНИЮ, поэтому erase()
// физически разрушает объект.
//
// Старая схема ломалась так:
//   1. Браузер закрывает вкладку. Задача AsyncTCP (Core 0) вызывает
//      AsyncWebSocketClient::_onDisconnect() → _client = nullptr,
//      затем наш обработчик WS_EVT_DISCONNECT, где мы читаем client->id().
//   2. В этот же момент taskCore1 (Core 1) внутри maybeBroadcast()
//      вызывал cleanupClients(). Тот видит shouldBeDeleted()==true
//      (это ровно "!_client") и делает _clients.erase() — объект клиента
//      уничтожается вместе с памятью.
//   3. Задача AsyncTCP продолжает работать с освобождённым объектом →
//      LoadProhibited → перезагрузка контроллера.
//
// Плюс textAll() итерировал тот же список без блокировки, параллельно
// с _clients.emplace_back() из AsyncTCP при подключении нового клиента.
//
// Окно узкое, поэтому отказ ловился не сразу, а через дни непрерывной
// работы — ровно при очередном закрытии вкладки.
//
// РЕШЕНИЕ: контроллер больше ничего не рассылает сам. Клиент раз в
// секунду шлёт {"cmd":"get_state"}, и ответ формируется в обработчике
// сообщения, то есть В ТОЙ ЖЕ задаче AsyncTCP. Список _clients при этом
// не обходится вообще — пишем строго в того клиента, чей запрос
// обрабатываем. Гонка устранена по построению, а не сужена.

// Отправить снапшот состояния КОНКРЕТНОМУ клиенту.
// Вызывать только из контекста AsyncTCP (обработчики onWsEvent).
static void sendStateTo(AsyncWebSocketClient* client) {
    if (!client) return;

    JsonDocument doc;
    buildStateJson(doc);

    if (s_wsBufMutex && xSemaphoreTake(s_wsBufMutex, pdMS_TO_TICKS(50)) != pdTRUE) return;
    size_t n = serializeJson(doc, s_wsTxBuf, sizeof(s_wsTxBuf));
    if (n == 0 || n >= sizeof(s_wsTxBuf)) {
        Serial.println(F("[WS] Serialize overflow — state skipped"));
        if (s_wsBufMutex) xSemaphoreGive(s_wsBufMutex);
        return;
    }
    client->text(s_wsTxBuf, n);
    if (s_wsBufMutex) xSemaphoreGive(s_wsBufMutex);
}

// ═══════════════════════════════════════════════════════════════════
//  УВЕДОМЛЕНИЯ (вызываются из других модулей)
// ═══════════════════════════════════════════════════════════════════
// Эти функции шлют полный snapshot (как обычный broadcast), потому что:
//   • отдельные event-сообщения усложнили бы клиентский код (нужен merge)
//   • полный snapshot всего 1-1.5 КБ, WebSocket быстро доставит
//   • клиент всегда в "правильном" состоянии
//
// Вызываются редко — только при реальных изменениях (реле toggle,
// кнопка нажата, форточка остановилась). Для частых изменений —
// штатный maybeBroadcast() раз в секунду.

// v6.1: эти функции вызываются из taskCore1 (кнопки, таймеры полива,
// гистерезис увлажнителя и насоса). Трогать отсюда AsyncWebSocket
// НЕЛЬЗЯ — см. большой комментарий выше. Изменение подхватится
// ближайшим опросом get_state от клиента, то есть не позже чем через
// секунду — ровно та же задержка, что давал прежний broadcast 1 Гц.
void WsHandler::notifyRelayChange(uint8_t /*relayIdx*/) {}
void WsHandler::notifyVentPosition(uint8_t /*ventIdx*/)  {}
void WsHandler::notifyModes()                             {}

// ═══════════════════════════════════════════════════════════════════
//  ОТВЕТ НА КОМАНДУ (успех / ошибка)
// ═══════════════════════════════════════════════════════════════════
static void sendAck(AsyncWebSocketClient* client, const char* cmd, bool ok,
                    const char* errorMsg = nullptr) {
    if (!client) return;
    JsonDocument doc;
    doc["type"]    = "ack";
    doc["cmd"]     = cmd;
    doc["ok"]      = ok;
    if (!ok && errorMsg) doc["error"] = errorMsg;
    char buf[256];
    size_t n = serializeJson(doc, buf, sizeof(buf));
    if (n > 0 && n < sizeof(buf)) client->text(buf, n);
    // mutex отдаётся внутри broadcastJson, повторно не нужно
}

// ═══════════════════════════════════════════════════════════════════
//  ОБРАБОТЧИКИ КОМАНД
// ═══════════════════════════════════════════════════════════════════
// Каждый ниже — функция, вызываемая из onMessage() по типу команды.
// Возвращают true при успехе.

// { "cmd": "set_relay", "idx": 0, "state": true }
// Переключает одиночное реле. Для H-bridge форточек НЕ работает
// (там используется vent_position).
static bool handleSetRelay(JsonDocument& doc, const char*& err) {
    if (!doc["idx"].is<int>() || !doc["state"].is<bool>()) {
        err = "missing idx or state"; return false;
    }
    int  idx   = doc["idx"];
    bool state = doc["state"];
    if (idx < 0 || idx >= NUM_RELAYS) { err = "bad idx"; return false; }
    if (RelayMgr::isVentRelay((uint8_t)idx)) {
        err = "vent relays are controlled by vent_hold"; return false;
    }

    // Переключаем в ручной режим соответствующей подсистемы
    {
        MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(50));
        if (lock.locked()) {
            if (idx >= RELAY_IDX_IRR1 && idx <= RELAY_IDX_IRR3) {
                g_settings.modeTimers = 0;
            } else if (idx == RELAY_IDX_HUMID) {
                g_settings.modeHumid = 0;
            } else if (idx == RELAY_IDX_WATER_PUMP) {
                g_settings.modeWaterPump = 0;
            }
            settingsSaveDeferred();
        }
    }

    // Для полива: если таймер был активен, форсируем его OFF
    if (idx >= RELAY_IDX_IRR1 && idx <= RELAY_IDX_IRR3) {
        uint8_t tIdx = idx - RELAY_IDX_IRR1;
        if (TimerSched::isActive(tIdx)) {
            TimerSched::forceOff(tIdx);
            return true;  // forceOff сам обновит состояние
        }
    }

    RelayMgr::setRelay((uint8_t)idx, state);
    return true;
}

// v6.1: удалены четыре функции — vent_position и vent_calibrate вместе с
// их «старыми реализациями» (_OLD). Первые две были заглушками, которые
// только возвращали текст «deprecated in v6.0», вторые не вызывались вообще.
// Соответствующие команды убраны и из диспетчера: панель ими не пользуется,
// управление форточками идёт через vent_hold и vent_set_calib.

// v6.0: { "cmd": "vent_hold", "cmd_type": "open"|"close"|"stop" }
// Веб-аналог удержания физической кнопки.
// open  → начать эстафету верх→низ;
// close → начать эстафету низ→верх;
// stop  → отпустить (мгновенный стоп активного мотора).
static bool handleVentHold(JsonDocument& doc, const char*& err) {
    const char* t = doc["cmd_type"];
    if (!t) { err = "missing cmd_type"; return false; }

    // Сначала валидируем команду
    VentManualCmd cmd;
    if      (strcmp(t, "open")  == 0) cmd = VMC_OPEN;
    else if (strcmp(t, "close") == 0) cmd = VMC_CLOSE;
    else if (strcmp(t, "stop")  == 0) cmd = VMC_NONE;
    else { err = "bad cmd_type"; return false; }

    // v6.0 FIX: переключаем в ручной режим ТОЛЬКО для активных команд
    // (open/close), но НЕ для stop. Иначе pointerup/leave события
    // в браузере самопроизвольно сбрасывают режим Auto.
    if (cmd != VMC_NONE) {
        // v6.1: onModeChanged() вынесен из-под лока (см. handleSetMode)
        bool switched = false;
        {
            MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(20));
            if (lock.locked() && g_settings.modeVents != 0) {
                g_settings.modeVents = 0;
                settingsSaveDeferred();
                switched = true;
            }
        }
        if (switched) VentCtrl::onModeChanged(false);
    } else {
        // stop в Auto-режиме игнорируем (нечего останавливать руками)
        MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(20));
        if (lock.locked() && g_settings.modeVents != 0) {
            return true;  // Auto активен — ручные команды игнорируются
        }
    }

    VentCtrl::manualSetCmd(cmd);
    return true;
}

// v6.1: { "cmd": "vent_home", "side": "top"|"bottom" }
// Калибровочный прогон: гонит створку на закрытие полное время хода
// с запасом, до упора в концевик, затем обнуляет расчётную позицию.
// Единственный способ убрать накопленный дрейф, не сбрасывая настройки.
static bool handleVentHome(JsonDocument& doc, const char*& err) {
    const char* side = doc["side"];
    if (!side) { err = "missing side"; return false; }

    uint8_t idx;
    if      (strcmp(side, "top")    == 0) idx = 0;
    else if (strcmp(side, "bottom") == 0) idx = 1;
    else { err = "bad side"; return false; }

    if (!VentCtrl::startHoming(idx)) {
        err = "homing already running";
        return false;
    }
    return true;
}

// v6.1: { "cmd": "vent_home_cancel" } — прервать прогон досрочно.
static bool handleVentHomeCancel() {
    VentCtrl::cancelHoming();
    return true;
}

// v6.0: { "cmd": "vent_set_calib", "side": "top"|"bottom", "sec": 42 }
static bool handleVentSetCalib(JsonDocument& doc, const char*& err) {
    const char* side = doc["side"];
    if (!side) { err = "missing side"; return false; }
    if (!doc["sec"].is<int>()) { err = "missing sec"; return false; }
    int sec = doc["sec"];
    bool ok = false;
    if      (strcmp(side, "top")    == 0) ok = VentCtrl::setTopCalibSec((uint16_t)sec);
    else if (strcmp(side, "bottom") == 0) ok = VentCtrl::setBottomCalibSec((uint16_t)sec);
    else { err = "bad side"; return false; }
    if (!ok) { err = "sec must be 1..120"; return false; }
    return true;
}

// { "cmd": "set_mode", "key": "vents", "value": 1 }
static bool handleSetMode(JsonDocument& doc, const char*& err) {
    const char* key = doc["key"];
    if (!key || !doc["value"].is<int>()) {
        err = "missing key or value"; return false;
    }
    int val = doc["value"];
    if (val < 0 || val > 1) { err = "bad value"; return false; }

    bool ventsChanged = false, timersChanged = false;

    // v6.1: лок держим ТОЛЬКО на время правки структуры настроек.
    // onModeChanged() у контроллеров останавливает моторы, дёргает реле и
    // пишет NVS — это долгие операции, держать на них мьютекс незачем
    // (рекурсивный лок это переживёт, но критическая секция раздувается
    // до сотен миллисекунд и тормозит broadcast и опрос датчиков).
    {
        MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(50));
        if (!lock.locked()) { err = "mutex timeout"; return false; }

        if      (strcmp(key, "vents")  == 0) { ventsChanged = (g_settings.modeVents != val); g_settings.modeVents = val; }
        else if (strcmp(key, "timers") == 0) { timersChanged = (g_settings.modeTimers != val); g_settings.modeTimers = val; }
        else if (strcmp(key, "humid")  == 0) g_settings.modeHumid     = val;
        else if (strcmp(key, "pump")   == 0) g_settings.modeWaterPump = val;
        else { err = "unknown key"; return false; }

        settingsSaveDeferred();
    }

    if (ventsChanged)  VentCtrl::onModeChanged(val == 1);
    if (timersChanged) TimerSched::onModeChanged(val == 1);
    return true;
}

// { "cmd": "set_thresholds", "ventUpperHigh": 31.0, ... }
// Все поля опциональны, обновляем только переданные.
// v6.0: используем settingsSave() (немедленно) вместо Deferred для порогов
static bool handleSetThresholds(JsonDocument& doc, const char*& err) {
    MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(50));
    if (!lock.locked()) { err = "mutex timeout"; return false; }

    if (doc["ventUpperHigh"].is<float>()) g_settings.ventUpper.high = doc["ventUpperHigh"];
    if (doc["ventUpperLow"].is<float>())  g_settings.ventUpper.low  = doc["ventUpperLow"];
    if (doc["ventLowerHigh"].is<float>()) g_settings.ventLower.high = doc["ventLowerHigh"];
    if (doc["ventLowerLow"].is<float>())  g_settings.ventLower.low  = doc["ventLowerLow"];
    if (doc["humidHigh"].is<float>())     g_settings.humidHigh      = doc["humidHigh"];
    if (doc["humidLow"].is<float>())      g_settings.humidLow       = doc["humidLow"];

    // Валидация: high > low
    if (g_settings.ventUpper.high <= g_settings.ventUpper.low) {
        err = "ventUpper: high must be > low"; return false;
    }
    if (g_settings.ventLower.high <= g_settings.ventLower.low) {
        err = "ventLower: high must be > low"; return false;
    }
    if (g_settings.humidHigh <= g_settings.humidLow) {
        err = "humid: high must be > low"; return false;
    }

    settingsSave();  // v6.0: немедленно — защита от потери данных при отключении питания
    MqttMgr::markDiscoveryDirty();  // пороги изменились → HA должен обновить
    return true;
}

// { "cmd": "set_timer", "idx": 0, "startMin": 360, "durationMin": 15, "intervalHours": 12 }
static bool handleSetTimer(JsonDocument& doc, const char*& err) {
    if (!doc["idx"].is<int>()) { err = "missing idx"; return false; }
    int idx = doc["idx"];
    if (idx < 0 || idx >= NUM_IRRIGATION) { err = "bad idx"; return false; }

    uint16_t startMin      = doc["startMin"]      | 0;
    uint16_t durationMin   = doc["durationMin"]   | 0;
    uint16_t intervalHours = doc["intervalHours"] | 0;

    if (startMin > 1439)       { err = "startMin > 1439"; return false; }
    if (durationMin > 1440)    { err = "durationMin > 1440"; return false; }
    if (intervalHours > 168)   { err = "intervalHours > 168 (1 week)"; return false; }

    MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(50));
    if (!lock.locked()) { err = "mutex timeout"; return false; }

    g_settings.timer[idx].startMin      = startMin;
    g_settings.timer[idx].durationMin   = durationMin;
    g_settings.timer[idx].intervalHours = intervalHours;
    settingsSaveDeferred();
    return true;
}

// { "cmd": "set_water_tank", "lowPct": 20, "highPct": 90, "depthCm": 100, "pumpTimeoutMin": 30 }
static bool handleSetWaterTank(JsonDocument& doc, const char*& err) {
    MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(50));
    if (!lock.locked()) { err = "mutex timeout"; return false; }

    if (doc["lowPct"].is<int>())         g_settings.waterTank.lowPct         = doc["lowPct"];
    if (doc["highPct"].is<int>())        g_settings.waterTank.highPct        = doc["highPct"];
    if (doc["depthCm"].is<int>())        g_settings.waterTank.depthCm        = doc["depthCm"];
    if (doc["pumpTimeoutMin"].is<int>()) g_settings.waterTank.pumpTimeoutMin = doc["pumpTimeoutMin"];

    if (g_settings.waterTank.lowPct >= g_settings.waterTank.highPct) {
        err = "lowPct must be < highPct"; return false;
    }
    if (g_settings.waterTank.depthCm == 0 || g_settings.waterTank.depthCm > 500) {
        err = "depthCm out of range (1-500)"; return false;
    }

    settingsSave();  // v6.0: немедленно
    MqttMgr::markDiscoveryDirty();
    return true;
}

// v6.1: удалена команда set_vent_cal.
//
// Она дублировала vent_set_calib (которой и пользуется панель), но делала
// это НЕПРАВИЛЬНО: писала travelMs прямо в g_settings, минуя VentCtrl.
// Живое значение s_top.calibMs при этом не обновлялось, поэтому новая
// калибровка не вступала в силу до перезагрузки — тихо и незаметно.
// Панель эту команду не вызывала, так что баг не проявлялся, но оставлять
// в API второй, расходящийся путь к тем же данным нельзя.

// { "cmd": "get_history" } → шлём клиенту массив точек
static bool handleGetHistory(AsyncWebSocketClient* client) {
    HistoryPoint pts[HISTORY_POINTS];
    uint8_t n = HistoryLogger::getAll(pts, HISTORY_POINTS);

    JsonDocument doc;
    doc["type"] = "history";
    JsonArray arr = doc["points"].to<JsonArray>();
    for (uint8_t i = 0; i < n; i++) {
        JsonObject p = arr.add<JsonObject>();
        p["t"] = (uint32_t)pts[i].timestamp;
        if (!isnan(pts[i].temp))        p["T"]  = round(pts[i].temp * 10) / 10.0;
        if (!isnan(pts[i].humidity))    p["H"]  = round(pts[i].humidity * 10) / 10.0;
        if (!isnan(pts[i].pressure))    p["P"]  = round(pts[i].pressure * 10) / 10.0;
        if (!isnan(pts[i].lux))         p["L"]  = (uint32_t)pts[i].lux;
        if (!isnan(pts[i].waterLevel))  p["W"]  = round(pts[i].waterLevel * 10) / 10.0;
        // v6.0: DHT22 (снаружи)
        if (!isnan(pts[i].dhtTemp))     p["T2"] = round(pts[i].dhtTemp * 10) / 10.0;
        if (!isnan(pts[i].dhtHumidity)) p["H2"] = round(pts[i].dhtHumidity * 10) / 10.0;
    }

    // v6.1: используем static buffer (не на стеке)
    if (s_wsBufMutex && xSemaphoreTake(s_wsBufMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }
    size_t sz = serializeJson(doc, s_wsHistBuf, sizeof(s_wsHistBuf));
    if (sz == 0 || sz >= sizeof(s_wsHistBuf)) {
        Serial.println(F("[WS] History serialize overflow"));
        if (s_wsBufMutex) xSemaphoreGive(s_wsBufMutex);
        return false;
    }
    client->text(s_wsHistBuf, sz);
    if (s_wsBufMutex) xSemaphoreGive(s_wsBufMutex);
    return true;
}

// { "cmd": "force_pump_reset" }
static bool handlePumpReset() {
    WaterPumpCtrl::resetFault();
    return true;
}

// { "cmd": "mqtt_resend_discovery" }
static bool handleMqttResend() {
    MqttMgr::markDiscoveryDirty();
    return true;
}

// { "cmd": "clear_history" }
static bool handleClearHistory() {
    HistoryLogger::clear();
    return true;
}

// ═══════════════════════════════════════════════════════════════════
//  ДИСПЕТЧЕР КОМАНД
// ═══════════════════════════════════════════════════════════════════
static void handleMessage(AsyncWebSocketClient* client,
                          const uint8_t* data, size_t len) {
    // v6.1: единственное место, где чистим отвалившихся клиентов.
    // Мы здесь в задаче AsyncTCP — той же, что создаёт и разрушает
    // соединения, поэтому erase() из cleanupClients() ни с чем не гонится.
    // Раньше это делал taskCore1, что и приводило к use-after-free.
    s_ws.cleanupClients();
    sweepUnauthorized();

    // Parse JSON
    JsonDocument doc;
    DeserializationError e = deserializeJson(doc, data, len);
    if (e) {
        Serial.printf("[WS] JSON parse error: %s\n", e.c_str());
        sendAck(client, "?", false, "invalid JSON");
        return;
    }

    const char* cmd = doc["cmd"];
    if (!cmd) { sendAck(client, "?", false, "missing cmd"); return; }

    // ── v6.1: авторизация ────────────────────────────────────────
    // Токен тот же, что у HTTP Basic Auth: base64("логин:пароль").
    // Отдельной учётки для сокета нет намеренно — иначе появилось бы
    // второе место, где пароль надо менять и легко забыть.
    if (strcmp(cmd, "auth") == 0) {
        const char* token = doc["token"];
        if (token && strcmp(token, NetMgr::wsToken()) == 0) {
            WsClientAuth* slot = authSlot(client->id(), true);
            if (slot) slot->authed = true;
            Serial.printf("[WS] Client #%u authorized\n", client->id());
            sendAck(client, "auth", true);
            sendStateTo(client);
        } else {
            Serial.printf("[WS] Client #%u auth FAILED — closing\n", client->id());
            sendAck(client, "auth", false, "bad token");
            authRelease(client->id());
            client->close();
        }
        return;
    }

    // Всё остальное — только после авторизации. Состояние тоже:
    // до неё сокет не отдаёт ни данных датчиков, ни настроек.
    if (!isAuthed(client->id())) {
        Serial.printf("[WS] Client #%u: '%s' before auth — rejected\n",
            client->id(), cmd);
        sendAck(client, cmd, false, "unauthorized");
        return;
    }

    Serial.printf("[WS] CMD from #%u: %s\n", client->id(), cmd);

    const char* err = nullptr;
    bool ok = false;

    if      (strcmp(cmd, "set_relay")            == 0) ok = handleSetRelay(doc, err);
    else if (strcmp(cmd, "vent_hold")            == 0) ok = handleVentHold(doc, err);
    else if (strcmp(cmd, "vent_set_calib")       == 0) ok = handleVentSetCalib(doc, err);
    else if (strcmp(cmd, "vent_home")            == 0) ok = handleVentHome(doc, err);
    else if (strcmp(cmd, "vent_home_cancel")     == 0) ok = handleVentHomeCancel();
    else if (strcmp(cmd, "set_mode")             == 0) ok = handleSetMode(doc, err);
    else if (strcmp(cmd, "set_thresholds")       == 0) ok = handleSetThresholds(doc, err);
    else if (strcmp(cmd, "set_timer")            == 0) ok = handleSetTimer(doc, err);
    else if (strcmp(cmd, "set_water_tank")       == 0) ok = handleSetWaterTank(doc, err);
    else if (strcmp(cmd, "get_state")            == 0) { sendStateTo(client); return; }
    else if (strcmp(cmd, "get_history")          == 0) { ok = handleGetHistory(client); return; }
    else if (strcmp(cmd, "force_pump_reset")     == 0) ok = handlePumpReset();
    else if (strcmp(cmd, "mqtt_resend_discovery")== 0) ok = handleMqttResend();
    else if (strcmp(cmd, "clear_history")        == 0) ok = handleClearHistory();
    else { err = "unknown cmd"; }

    sendAck(client, cmd, ok, err);

    // После успешной команды — сразу отдаём новое состояние тому,
    // кто команду прислал (мгновенная обратная связь в UI).
    // Остальные клиенты увидят изменение своим ближайшим get_state.
    if (ok) sendStateTo(client);
}

// ═══════════════════════════════════════════════════════════════════
//  WEBSOCKET EVENT HANDLER
// ═══════════════════════════════════════════════════════════════════
static void onWsEvent(AsyncWebSocket* /*server*/, AsyncWebSocketClient* client,
                      AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("[WS] Client #%u connected from %s — awaiting auth\n",
                client->id(), client->remoteIP().toString().c_str());
            // v6.1: снапшот здесь БОЛЬШЕ НЕ ОТПРАВЛЯЕТСЯ. До авторизации
            // соединение не получает ничего: ни показаний датчиков,
            // ни состояния реле, ни настроек.
            authSlot(client->id(), true);
            break;

        case WS_EVT_DISCONNECT:
            Serial.printf("[WS] Client #%u disconnected\n", client->id());
            authRelease(client->id());
            break;

        case WS_EVT_DATA: {
            AwsFrameInfo* info = (AwsFrameInfo*)arg;
            // Обрабатываем только полностью полученные текстовые сообщения.
            // AsyncWebSocket умеет фрагментацию, но наши команды мелкие (<1КБ),
            // фрагментированные не приходят в нормальной работе.
            if (info->final && info->index == 0 && info->len == len
                && info->opcode == WS_TEXT) {
                handleMessage(client, data, len);
            }
            break;
        }

        case WS_EVT_PONG:
        case WS_EVT_ERROR:
        default:
            break;
    }
}

// ═══════════════════════════════════════════════════════════════════
//  ПУБЛИЧНОЕ API
// ═══════════════════════════════════════════════════════════════════
void WsHandler::init() {
    if (!s_wsBufMutex) s_wsBufMutex = xSemaphoreCreateMutex();
    s_ws.onEvent(onWsEvent);
    Serial.println(F("[WS] Handler initialized"));
}

void WsHandler::attachTo(AsyncWebServer& server) {
    server.addHandler(&s_ws);
    Serial.println(F("[WS] Attached to web server at /ws"));
}
