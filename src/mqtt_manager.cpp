// ╔══════════════════════════════════════════════════════════════════╗
// ║  mqtt_manager.cpp — MQTT клиент v5.0                             ║
// ║                                                                  ║
// ║  Топики (basetopic = g_settings.mqttServer по умолч. "greenhouse"):
// ║                                                                  ║
// ║  Датчики:                                                        ║
// ║   greenhouse/sensor/temp              (°C)                      ║
// ║   greenhouse/sensor/humidity          (%)                       ║
// ║   greenhouse/sensor/pressure          (mmHg)                    ║
// ║   greenhouse/sensor/lux               (лк)                      ║
// ║   greenhouse/sensor/rain              (ON/OFF)                  ║
// ║   greenhouse/sensor/water_level       (%)                       ║
// ║                                                                  ║
// ║  Реле:                                                           ║
// ║   greenhouse/relay/irrigation1        (ON/OFF)                  ║
// ║   greenhouse/relay/irrigation2        (ON/OFF)                  ║
// ║   greenhouse/relay/irrigation3        (ON/OFF)                  ║
// ║   greenhouse/relay/humidifier         (ON/OFF)                  ║
// ║   greenhouse/relay/water_pump         (ON/OFF)                  ║
// ║   greenhouse/pump/fault               (ON/OFF)                  ║
// ║                                                                  ║
// ║  Форточки (как cover в HA):                                      ║
// ║   greenhouse/vent/upper/position      (0..100)                  ║
// ║   greenhouse/vent/upper/set           (команда 0..100)          ║
// ║   greenhouse/vent/lower/position                                 ║
// ║   greenhouse/vent/lower/set                                      ║
// ║                                                                  ║
// ║  Системные:                                                      ║
// ║   greenhouse/availability             (online/offline, LWT)     ║
// ║   greenhouse/ip                       (IP адрес устройства)     ║
// ║   greenhouse/uptime                   (секунды с загрузки)      ║
// ╚══════════════════════════════════════════════════════════════════╝

#include "mqtt_manager.h"
#include "settings.h"
#include "relay_manager.h"
#include "vent_controller.h"
#include "water_pump_controller.h"
#include "sensor_reader.h"
#include "network_manager.h"
#include "config.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include <atomic>

// ═══════════════════════════════════════════════════════════════════
//  ВНУТРЕННЕЕ СОСТОЯНИЕ
// ═══════════════════════════════════════════════════════════════════
static WiFiClient    s_wifiClient;
static PubSubClient  s_mqtt(s_wifiClient);

static uint64_t s_lastReconnectMs = 0;
static uint64_t s_lastPublishMs   = 0;
static uint64_t s_lastDiscoveryMs = 0;   // Когда последний раз отправляли discovery

// Флаги discovery
static std::atomic<bool> s_discoveryDirty{true};   // true → нужно отправить
static bool              s_firstConnect   = true;

// Client ID (уникальный по MAC)
static char s_clientId[32];

// Base topic из настроек (кэшируется)
static char s_baseTopic[32];

// ═══════════════════════════════════════════════════════════════════
//  ВСПОМОГАТЕЛЬНЫЕ
// ═══════════════════════════════════════════════════════════════════

// Формирование уникального client ID: "greenhouse-XXYYZZ"
static void buildClientId() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(s_clientId, sizeof(s_clientId),
        "greenhouse-%02X%02X%02X", mac[3], mac[4], mac[5]);
}

// Формирование базового топика
static void buildBaseTopic() {
    MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(20));
    const char* src = (lock.locked() && g_settings.mqttServer[0])
        ? MQTT_BASE_TOPIC
        : MQTT_BASE_TOPIC;
    strlcpy(s_baseTopic, src, sizeof(s_baseTopic));
}

// Формирование полного топика (результат во внешнем буфере)
static void buildTopic(char* dst, size_t dstSize, const char* suffix) {
    snprintf(dst, dstSize, "%s/%s", s_baseTopic, suffix);
}

// ═══════════════════════════════════════════════════════════════════
//  ОБРАБОТКА ВХОДЯЩИХ СООБЩЕНИЙ (команды из HA)
// ═══════════════════════════════════════════════════════════════════
// Команды, которые мы принимаем от HA:
//   greenhouse/relay/+/set     → ON/OFF для одиночных реле
//   greenhouse/vent/upper/set  → число 0..100 (позиция форточки)
//   greenhouse/vent/lower/set
//   greenhouse/mode/vents/set  → 0/1 (auto/manual)
//   greenhouse/mode/humid/set  → 0/1
//   greenhouse/mode/timers/set → 0/1
//   greenhouse/mode/pump/set   → 0/1
static void onMqttMessage(char* topic, uint8_t* payload, unsigned int len) {
    // Для безопасного чтения payload как строки
    char buf[16];
    size_t copyLen = (len < sizeof(buf) - 1) ? len : sizeof(buf) - 1;
    memcpy(buf, payload, copyLen);
    buf[copyLen] = '\0';

    Serial.printf("[MQTT] IN: %s = %s\n", topic, buf);

    // Парсим топик: skip basetopic + /
    size_t baseLen = strlen(s_baseTopic);
    if (strncmp(topic, s_baseTopic, baseLen) != 0) return;
    if (topic[baseLen] != '/') return;
    const char* sub = topic + baseLen + 1;

    // ── Реле: relay/<name>/set ──────────────────────────────────
    if (strncmp(sub, "relay/", 6) == 0) {
        const char* name = sub + 6;
        // Обрезаем "/set" в конце
        // Ищем реле по имени
        bool on = (strcasecmp(buf, "ON") == 0 || strcmp(buf, "1") == 0);
        uint8_t idx = 0xFF;
        if      (strncmp(name, "irrigation1", 11) == 0) idx = RELAY_IDX_IRR1;
        else if (strncmp(name, "irrigation2", 11) == 0) idx = RELAY_IDX_IRR2;
        else if (strncmp(name, "irrigation3", 11) == 0) idx = RELAY_IDX_IRR3;
        else if (strncmp(name, "humidifier",  10) == 0) idx = RELAY_IDX_HUMID;
        else if (strncmp(name, "water_pump",  10) == 0) idx = RELAY_IDX_WATER_PUMP;

        if (idx != 0xFF) {
            // Переводим в ручной режим соответствующей подсистемы
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
            RelayMgr::setRelay(idx, on);
        }
        return;
    }

    // ── Форточки: vent/cmd = OPEN | CLOSE | STOP ─────────────────
    // v6.1: управление восстановлено, но по-другому.
    //
    // Раньше здесь принималась позиция 0..100 и вызывалась заглушка
    // setTargetPosition(), не делавшая ничего с версии 6.0 — HA показывал
    // рабочий на вид ползунок, команды которого молча терялись.
    //
    // Механизма «поехать в позицию N» в v6 не существует: створки
    // управляются удержанием, с эстафетой верх→низ при открытии и
    // низ→верх при закрытии. Поэтому в HA отдаём ровно то, что прошивка
    // действительно умеет: три команды, те же, что у кнопок на корпусе.
    // Пара створок ведёт себя как одна штора — как и при ручном управлении.
    if (strncmp(sub, "vent/cmd", 8) == 0) {
        VentManualCmd cmd = VMC_NONE;
        if      (strcasecmp(buf, "OPEN")  == 0) cmd = VMC_OPEN;
        else if (strcasecmp(buf, "CLOSE") == 0) cmd = VMC_CLOSE;
        else if (strcasecmp(buf, "STOP")  == 0) cmd = VMC_NONE;
        else return;

        // Ручные команды имеют смысл только в ручном режиме: иначе
        // авто-логика перехватит мотор на следующем же тике.
        if (cmd != VMC_NONE) {
            bool switched = false;
            {
                MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(50));
                if (lock.locked() && g_settings.modeVents != 0) {
                    g_settings.modeVents = 0;
                    settingsSaveDeferred();
                    switched = true;
                }
            }
            if (switched) VentCtrl::onModeChanged(false);
        }
        VentCtrl::manualSetCmd(cmd);
        return;
    }

    // ── Режимы: mode/<name>/set ──────────────────────────────────
    if (strncmp(sub, "mode/", 5) == 0) {
        uint8_t m = (uint8_t)atoi(buf);
        if (m > 1) m = 1;
        MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(50));
        if (!lock.locked()) return;
        if      (strncmp(sub + 5, "vents/set",  9) == 0) g_settings.modeVents = m;
        else if (strncmp(sub + 5, "humid/set",  9) == 0) g_settings.modeHumid = m;
        else if (strncmp(sub + 5, "timers/set", 10) == 0) g_settings.modeTimers = m;
        else if (strncmp(sub + 5, "pump/set",   8) == 0)  g_settings.modeWaterPump = m;
        settingsSaveDeferred();
        return;
    }
}

// ═══════════════════════════════════════════════════════════════════
//  ПОДКЛЮЧЕНИЕ
// ═══════════════════════════════════════════════════════════════════
// Возвращает true при успешном подключении.
static bool tryConnect() {
    if (!NetMgr::isConnected()) return false;

    // Читаем настройки под мьютексом
    char server[64], user[32], pass[32];
    uint16_t port;
    bool enabled;
    {
        MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(50));
        if (!lock.locked()) return false;
        enabled = g_settings.mqttEnabled;
        if (!enabled) return false;
        strlcpy(server, g_settings.mqttServer, sizeof(server));
        port = g_settings.mqttPort;
        strlcpy(user,   g_settings.mqttUser,   sizeof(user));
        strlcpy(pass,   g_settings.mqttPass,   sizeof(pass));
    }

    if (!server[0]) return false;

    s_mqtt.setServer(server, port);
    s_mqtt.setCallback(onMqttMessage);
    s_mqtt.setKeepAlive(MQTT_KEEPALIVE_SEC);
    s_mqtt.setBufferSize(MQTT_BUFFER_SIZE);

    // LWT (Last Will and Testament): при отключении ESP32 брокер
    // автоматически публикует "offline" в availability топик.
    // Retained=true — новые клиенты сразу знают статус.
    char lwtTopic[48];
    buildTopic(lwtTopic, sizeof(lwtTopic), "availability");

    Serial.printf("[MQTT] Connecting to %s:%u as %s...\n",
        server, (unsigned)port, s_clientId);

    bool ok;
    if (user[0]) {
        ok = s_mqtt.connect(s_clientId, user, pass,
            lwtTopic, 1 /*qos*/, true /*retained*/, "offline");
    } else {
        ok = s_mqtt.connect(s_clientId, nullptr, nullptr,
            lwtTopic, 1, true, "offline");
    }

    if (!ok) {
        Serial.printf("[MQTT] Connect failed, state=%d\n", s_mqtt.state());
        return false;
    }

    // Публикуем "online" сразу после успешного подключения
    s_mqtt.publish(lwtTopic, "online", true);

    // Подписываемся на команды
    char subTopic[48];
    buildTopic(subTopic, sizeof(subTopic), "relay/+/set");
    s_mqtt.subscribe(subTopic);
    // v6.1: одна команда на пару створок — OPEN / CLOSE / STOP.
    buildTopic(subTopic, sizeof(subTopic), "vent/cmd");
    s_mqtt.subscribe(subTopic);
    buildTopic(subTopic, sizeof(subTopic), "mode/+/set");
    s_mqtt.subscribe(subTopic);

    Serial.println(F("[MQTT] Connected & subscribed"));

    // ── Discovery при первом подключении ─────────────────────────
    if (s_firstConnect) {
        s_firstConnect = false;
        s_discoveryDirty.store(true);  // форсируем в этом цикле
    }

    return true;
}

// ═══════════════════════════════════════════════════════════════════
//  HOME ASSISTANT DISCOVERY
// ═══════════════════════════════════════════════════════════════════
// Формат: https://www.home-assistant.io/integrations/mqtt/#discovery
// Retained=true — HA запомнит устройство даже если мы отключимся.

// Общий блок "device" — HA сгруппирует все entity под одним устройством.
// Принимаем JsonVariant вместо JsonObject& чтобы работало с временными
// объектами из doc["device"].to<JsonObject>() в ArduinoJson v7.
static void writeDeviceBlock(JsonVariant dev) {
    dev["identifiers"]       = s_clientId;
    dev["name"]              = "Greenhouse";
    dev["model"]             = "ESP32 v5";
    dev["manufacturer"]      = "Greenhouse Project";
    dev["sw_version"]        = FW_VERSION;
    // URL на веб-панель ESP32
    IPAddress ip = WiFi.localIP();
    char url[32];
    snprintf(url, sizeof(url), "http://%u.%u.%u.%u",
        ip[0], ip[1], ip[2], ip[3]);
    dev["configuration_url"] = url;
}

// Общий блок "availability" — HA знает, когда устройство offline.
// Параметр — JsonVariant (работает и с JsonDocument, и с JsonObject).
static void writeAvailability(JsonVariant doc) {
    char topic[48];
    buildTopic(topic, sizeof(topic), "availability");
    doc["availability_topic"]    = topic;
    doc["payload_available"]     = "online";
    doc["payload_not_available"] = "offline";
}

// Discovery sensor: один физический датчик → одна entity в HA
static void publishSensorDiscovery(
    const char* id, const char* name,
    const char* stateTopic, const char* unit,
    const char* deviceClass, const char* icon = nullptr
) {
    char discTopic[96];
    snprintf(discTopic, sizeof(discTopic),
        "homeassistant/sensor/%s_%s/config", s_clientId, id);

    JsonDocument doc;
    doc["unique_id"]    = String(s_clientId) + "_" + id;
    doc["name"]         = name;
    doc["state_topic"]  = stateTopic;
    if (unit)         doc["unit_of_measurement"] = unit;
    if (deviceClass)  doc["device_class"]        = deviceClass;
    if (icon)         doc["icon"]                = icon;
    doc["object_id"]    = String("greenhouse_") + id;
    writeAvailability(doc);

    JsonObject dev = doc["device"].to<JsonObject>();
    writeDeviceBlock(dev);

    char payload[512];
    size_t n = serializeJson(doc, payload, sizeof(payload));
    s_mqtt.publish(discTopic, (const uint8_t*)payload, n, true /*retained*/);
}

// Discovery binary_sensor (дождь)
static void publishBinarySensorDiscovery(
    const char* id, const char* name,
    const char* stateTopic, const char* deviceClass
) {
    char discTopic[96];
    snprintf(discTopic, sizeof(discTopic),
        "homeassistant/binary_sensor/%s_%s/config", s_clientId, id);

    JsonDocument doc;
    doc["unique_id"]    = String(s_clientId) + "_" + id;
    doc["name"]         = name;
    doc["state_topic"]  = stateTopic;
    doc["payload_on"]   = "ON";
    doc["payload_off"]  = "OFF";
    if (deviceClass) doc["device_class"] = deviceClass;
    doc["object_id"]    = String("greenhouse_") + id;
    writeAvailability(doc);

    JsonObject dev = doc["device"].to<JsonObject>();
    writeDeviceBlock(dev);

    char payload[512];
    size_t n = serializeJson(doc, payload, sizeof(payload));
    s_mqtt.publish(discTopic, (const uint8_t*)payload, n, true);
}

// Discovery switch (реле) — HA переключатель в интерфейсе
static void publishRelayDiscovery(
    const char* id, const char* name, const char* relayName
) {
    char discTopic[96];
    snprintf(discTopic, sizeof(discTopic),
        "homeassistant/switch/%s_%s/config", s_clientId, id);

    char stateTopic[64], commandTopic[64];
    char stateSuffix[32], cmdSuffix[32];
    snprintf(stateSuffix, sizeof(stateSuffix), "relay/%s",     relayName);
    snprintf(cmdSuffix,   sizeof(cmdSuffix),   "relay/%s/set", relayName);
    buildTopic(stateTopic,   sizeof(stateTopic),   stateSuffix);
    buildTopic(commandTopic, sizeof(commandTopic), cmdSuffix);

    JsonDocument doc;
    doc["unique_id"]     = String(s_clientId) + "_" + id;
    doc["name"]          = name;
    doc["state_topic"]   = stateTopic;
    doc["command_topic"] = commandTopic;
    doc["payload_on"]    = "ON";
    doc["payload_off"]   = "OFF";
    doc["object_id"]     = String("greenhouse_") + id;
    writeAvailability(doc);

    JsonObject dev = doc["device"].to<JsonObject>();
    writeDeviceBlock(dev);

    char payload[512];
    size_t n = serializeJson(doc, payload, sizeof(payload));
    s_mqtt.publish(discTopic, (const uint8_t*)payload, n, true);
}

// Discovery cover (форточка как "жалюзи" с позицией 0-100%)
static void publishVentDiscovery(uint8_t ventIdx) {
    const char* id        = (ventIdx == VENT_IDX_UPPER) ? "vent_upper" : "vent_lower";
    const char* name      = (ventIdx == VENT_IDX_UPPER)
        ? "Форточка верхняя" : "Форточка нижняя";
    const char* topicName = (ventIdx == VENT_IDX_UPPER) ? "upper" : "lower";

    char discTopic[96];
    snprintf(discTopic, sizeof(discTopic),
        "homeassistant/cover/%s_%s/config", s_clientId, id);

    char posTopic[64];
    char posSuffix[32];
    snprintf(posSuffix, sizeof(posSuffix), "vent/%s/position", topicName);
    buildTopic(posTopic, sizeof(posTopic), posSuffix);

    JsonDocument doc;
    doc["unique_id"]              = String(s_clientId) + "_" + id;
    doc["name"]                   = name;
    doc["device_class"]           = "window";

    // position_topic публикует текущую позицию этой створки (0 = закрыто).
    //
    // v6.1: вместо set_position_topic — command_topic с тремя командами.
    // «Поехать в позицию N» прошивка не умеет: створки управляются
    // удержанием, с эстафетой верх→низ при открытии и низ→верх при закрытии.
    // Раньше здесь объявлялся ползунок позиции, команды которого молча
    // терялись в заглушке. Теперь в HA ровно то, что реально работает.
    //
    // ВАЖНО: команда действует на ПАРУ створок, как и кнопки на корпусе.
    // Обе сущности cover шлют в один топик; позиция у каждой своя.
    char cmdTopic[64];
    buildTopic(cmdTopic, sizeof(cmdTopic), "vent/cmd");

    doc["position_topic"]         = posTopic;
    doc["command_topic"]          = cmdTopic;
    doc["payload_open"]           = "OPEN";
    doc["payload_close"]          = "CLOSE";
    doc["payload_stop"]           = "STOP";
    doc["position_open"]          = 100;
    doc["position_closed"]        = 0;
    doc["optimistic"]             = false;
    doc["object_id"]              = String("greenhouse_") + id;
    writeAvailability(doc);

    JsonObject dev = doc["device"].to<JsonObject>();
    writeDeviceBlock(dev);

    char payload[600];
    size_t n = serializeJson(doc, payload, sizeof(payload));
    s_mqtt.publish(discTopic, (const uint8_t*)payload, n, true);
}

// ═══════════════════════════════════════════════════════════════════
//  ПУБЛИКАЦИЯ ВСЕГО DISCOVERY ПАКЕТА
// ═══════════════════════════════════════════════════════════════════
static void publishAllDiscovery() {
    Serial.println(F("[MQTT] Publishing HA discovery..."));

    // Датчики
    char t[64];
    buildTopic(t, sizeof(t), "sensor/temp");
    publishSensorDiscovery("temp",     "Температура", t, "°C",    "temperature");
    buildTopic(t, sizeof(t), "sensor/humidity");
    publishSensorDiscovery("humidity", "Влажность",   t, "%",     "humidity");
    buildTopic(t, sizeof(t), "sensor/pressure");
    publishSensorDiscovery("pressure", "Давление",    t, "mmHg",  "pressure");
    buildTopic(t, sizeof(t), "sensor/lux");
    publishSensorDiscovery("lux",      "Освещённость", t, "lx",    "illuminance");
    buildTopic(t, sizeof(t), "sensor/water_level");
    publishSensorDiscovery("water_level", "Уровень воды", t, "%", nullptr, "mdi:water-percent");

    // Дождь (binary sensor)
    buildTopic(t, sizeof(t), "sensor/rain");
    publishBinarySensorDiscovery("rain", "Дождь", t, "moisture");

    // Реле (только одиночные — для форточек используются cover)
    publishRelayDiscovery("irrigation1", "Полив 1",      "irrigation1");
    publishRelayDiscovery("irrigation2", "Полив 2",      "irrigation2");
    publishRelayDiscovery("irrigation3", "Полив 3",      "irrigation3");
    publishRelayDiscovery("humidifier",  "Увлажнитель",  "humidifier");
    publishRelayDiscovery("water_pump",  "Насос",         "water_pump");

    // Форточки (cover)
    publishVentDiscovery(VENT_IDX_UPPER);
    publishVentDiscovery(VENT_IDX_LOWER);

    // FAULT насоса (binary sensor, problem class)
    buildTopic(t, sizeof(t), "pump/fault");
    publishBinarySensorDiscovery("pump_fault", "Авария насоса", t, "problem");

    Serial.println(F("[MQTT] Discovery published"));
}

// ═══════════════════════════════════════════════════════════════════
//  ПЕРИОДИЧЕСКИЕ ПУБЛИКАЦИИ (состояния датчиков)
// ═══════════════════════════════════════════════════════════════════
static void publishSensors() {
    if (!s_mqtt.connected()) return;

    SensorSnapshot s = SensorRdr::getData();
    char topic[64], value[16];

    if (s.bmeOk || s.dhtOk) {
        float t = !isnan(s.bmeTemp) ? s.bmeTemp : s.dhtTemp;
        float h = !isnan(s.bmeHum)  ? s.bmeHum  : s.dhtHum;
        if (!isnan(t)) {
            buildTopic(topic, sizeof(topic), "sensor/temp");
            snprintf(value, sizeof(value), "%.1f", t);
            s_mqtt.publish(topic, value);
        }
        if (!isnan(h)) {
            buildTopic(topic, sizeof(topic), "sensor/humidity");
            snprintf(value, sizeof(value), "%.1f", h);
            s_mqtt.publish(topic, value);
        }
    }
    if (s.bmeOk && !isnan(s.bmePress)) {
        buildTopic(topic, sizeof(topic), "sensor/pressure");
        snprintf(value, sizeof(value), "%.1f", s.bmePress);
        s_mqtt.publish(topic, value);
    }
    if (s.luxOk) {
        buildTopic(topic, sizeof(topic), "sensor/lux");
        snprintf(value, sizeof(value), "%u", (unsigned)s.lux);
        s_mqtt.publish(topic, value);
    }
    {
        buildTopic(topic, sizeof(topic), "sensor/rain");
        s_mqtt.publish(topic, s.rain ? "ON" : "OFF");
    }
    if (s.waterOk && !isnan(s.waterLevelPct)) {
        buildTopic(topic, sizeof(topic), "sensor/water_level");
        snprintf(value, sizeof(value), "%.1f", s.waterLevelPct);
        s_mqtt.publish(topic, value);
    }

    // Системные
    IPAddress ip = WiFi.localIP();
    buildTopic(topic, sizeof(topic), "ip");
    snprintf(value, sizeof(value), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    s_mqtt.publish(topic, value);

    buildTopic(topic, sizeof(topic), "uptime");
    uint64_t upSec = steadyMs() / 1000ULL;
    snprintf(value, sizeof(value), "%llu", (unsigned long long)upSec);
    s_mqtt.publish(topic, value);
}

// ═══════════════════════════════════════════════════════════════════
//  ПУБЛИКАЦИЯ ОТДЕЛЬНЫХ СОСТОЯНИЙ (по событиям)
// ═══════════════════════════════════════════════════════════════════

// Сопоставление индекса реле → суффикс топика. Для форточек nullptr
// (они публикуются как позиция cover, не как relay).
static const char* relayTopicSuffix(uint8_t idx) {
    switch (idx) {
        case RELAY_IDX_IRR1:       return "relay/irrigation1";
        case RELAY_IDX_IRR2:       return "relay/irrigation2";
        case RELAY_IDX_IRR3:       return "relay/irrigation3";
        case RELAY_IDX_HUMID:      return "relay/humidifier";
        case RELAY_IDX_WATER_PUMP: return "relay/water_pump";
        default: return nullptr;
    }
}

// ═══════════════════════════════════════════════════════════════════
//  v6.1: ОТЛОЖЕННАЯ ПУБЛИКАЦИЯ — ЗАЩИТА ОТ МЕЖЗАДАЧНОЙ ГОНКИ
// ═══════════════════════════════════════════════════════════════════
// PubSubClient не потокобезопасен: у него один общий буфер и одно
// TCP-соединение, без какой-либо блокировки внутри.
//
// А publishRelayState() вызывается из RelayMgr::doGpio(), куда попадают
// сразу три задачи:
//   • taskCore1  (Core 1) — гистерезис увлажнителя, насос, таймеры, кнопки
//   • AsyncTCP   (Core 0) — команда set_relay из веб-панели
//   • loop       (Core 0) — команда из MQTT (onMqttMessage внутри s_mqtt.loop())
//
// Одновременный publish из двух задач перемешивает байты в TCP-потоке
// (брокер рвёт соединение) и портит общий буфер клиента.
//
// Мьютексом это чинить нельзя: doGpio() уже держит g_settingsMutex и
// взял бы мьютекс MQTT, а onMqttMessage() держит MQTT и берёт
// g_settingsMutex — классическая инверсия порядка блокировок, то есть
// гарантированный взаимный клин.
//
// Поэтому публикация именно ОТКЛАДЫВАЕТСЯ: внешние вызовы лишь ставят
// атомарный бит, а реальный publish() делает только задача loop.
// Один писатель — гонки нет, лишних блокировок тоже.

static std::atomic<uint16_t> s_relayDirty{0};   // битовая маска по NUM_RELAYS
static std::atomic<uint8_t>  s_ventDirty{0};    // биты по форточкам
static std::atomic<bool>     s_pumpDirty{false};

// ─── Реальные публикаторы: только из задачи loop! ────────────────
static void doPublishRelayState(uint8_t idx) {
    if (!s_mqtt.connected()) return;
    const char* suffix = relayTopicSuffix(idx);
    if (!suffix) return;

    char topic[64];
    buildTopic(topic, sizeof(topic), suffix);
    bool state = RelayMgr::getState(idx);
    s_mqtt.publish(topic, state ? "ON" : "OFF", true /*retained*/);
}

static void doPublishVentState(uint8_t ventIdx) {
    if (!s_mqtt.connected()) return;
    if (ventIdx >= NUM_VENTS) return;

    char topic[64];
    char suffix[32];
    snprintf(suffix, sizeof(suffix), "vent/%s/position",
        (ventIdx == VENT_IDX_UPPER) ? "upper" : "lower");
    buildTopic(topic, sizeof(topic), suffix);

    char value[8];
    uint8_t pct = VentCtrl::getCurrentPosition(ventIdx);
    snprintf(value, sizeof(value), "%u", (unsigned)pct);
    s_mqtt.publish(topic, value, true);
}

static void doPublishPumpState() {
    if (!s_mqtt.connected()) return;
    char topic[64];
    buildTopic(topic, sizeof(topic), "pump/fault");
    s_mqtt.publish(topic, WaterPumpCtrl::isInFault() ? "ON" : "OFF", true);
}

// Слить накопленные пометки. Вызывается только из MqttMgr::loop().
static void flushDirtyStates() {
    uint16_t relays = s_relayDirty.exchange(0, std::memory_order_acq_rel);
    for (uint8_t i = 0; i < NUM_RELAYS && relays; i++) {
        if (relays & (1u << i)) {
            doPublishRelayState(i);
            relays &= ~(uint16_t)(1u << i);
        }
    }
    uint8_t vents = s_ventDirty.exchange(0, std::memory_order_acq_rel);
    for (uint8_t v = 0; v < NUM_VENTS && vents; v++) {
        if (vents & (1u << v)) {
            doPublishVentState(v);
            vents &= ~(uint8_t)(1u << v);
        }
    }
    if (s_pumpDirty.exchange(false, std::memory_order_acq_rel)) {
        doPublishPumpState();
    }
}

// ─── Публичный API: только помечаем, не публикуем ────────────────
void MqttMgr::publishRelayState(uint8_t idx) {
    if (idx >= NUM_RELAYS) return;
    s_relayDirty.fetch_or((uint16_t)(1u << idx), std::memory_order_relaxed);
}

void MqttMgr::publishVentState(uint8_t ventIdx) {
    if (ventIdx >= NUM_VENTS) return;
    s_ventDirty.fetch_or((uint8_t)(1u << ventIdx), std::memory_order_relaxed);
}

void MqttMgr::publishPumpState() {
    s_pumpDirty.store(true, std::memory_order_relaxed);
}

// ═══════════════════════════════════════════════════════════════════
//  ПУБЛИЧНЫЙ API
// ═══════════════════════════════════════════════════════════════════
void MqttMgr::init() {
    buildClientId();
    buildBaseTopic();
    s_discoveryDirty.store(true);
    s_firstConnect = true;
    Serial.printf("[MQTT] Init: client_id=%s, base_topic=%s\n",
        s_clientId, s_baseTopic);
}

void MqttMgr::loop() {
    // Быстрый выход если MQTT выключен в настройках
    bool enabled;
    {
        MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(10));
        if (!lock.locked()) return;
        enabled = g_settings.mqttEnabled;
    }
    if (!enabled) {
        // Если были подключены — отключаемся
        if (s_mqtt.connected()) s_mqtt.disconnect();
        return;
    }

    uint64_t now = steadyMs();

    if (!s_mqtt.connected()) {
        if (now - s_lastReconnectMs >= MQTT_RECONNECT_MS) {
            s_lastReconnectMs = now;
            tryConnect();
        }
        return;
    }

    // Обработка входящих сообщений + keepalive
    s_mqtt.loop();

    // ── Discovery (только при реальных изменениях) ───────────────
    bool needDiscovery = s_discoveryDirty.load();
    // Страховка: раз в 24 часа переотправляем (на случай потери retained)
    if (!needDiscovery && s_lastDiscoveryMs > 0
        && (now - s_lastDiscoveryMs) >= MQTT_DISCOVERY_RESEND_INTERVAL_MS) {
        needDiscovery = true;
        Serial.println(F("[MQTT] 24-hour discovery refresh"));
    }
    if (needDiscovery) {
        publishAllDiscovery();
        s_discoveryDirty.store(false);
        s_lastDiscoveryMs = now;

        // После discovery — публикуем текущие состояния реле и форточек,
        // чтобы HA сразу увидел корректные значения (а не ждал следующего изменения).
        // Мы уже в задаче loop, поэтому зовём внутренние публикаторы напрямую.
        for (uint8_t i = 0; i < NUM_RELAYS; i++) {
            if (relayTopicSuffix(i)) doPublishRelayState(i);
        }
        doPublishVentState(VENT_IDX_UPPER);
        doPublishVentState(VENT_IDX_LOWER);
        doPublishPumpState();
    }

    // ── Отложенные публикации, накопленные другими задачами ──────
    flushDirtyStates();

    // ── Периодическая публикация датчиков ────────────────────────
    if (now - s_lastPublishMs >= MQTT_PUBLISH_MS) {
        s_lastPublishMs = now;
        publishSensors();
    }
}

void MqttMgr::markDiscoveryDirty() {
    s_discoveryDirty.store(true);
    Serial.println(F("[MQTT] Discovery marked dirty"));
}

bool MqttMgr::isConnected() {
    return s_mqtt.connected();
}
