// ╔══════════════════════════════════════════════════════════════════╗
// ║  main.cpp — Точка входа v5.0                                     ║
// ║                                                                  ║
// ║  Порядок инициализации:                                          ║
// ║   1. Serial, базовые модули                                      ║
// ║   2. RelayMgr::init() — все реле в OFF (критично для безопасности)
// ║   3. LittleFS, settings                                          ║
// ║   4. RelayMgr::restoreFromSettings()                             ║
// ║   5. Датчики, кнопки                                             ║
// ║   6. ★ Развилка: есть Wi-Fi креды? → STA : AP provisioning      ║
// ║   7. В STA: NTP, MQTT, веб-сервер, задачи FreeRTOS              ║
// ║   8. Task Watchdog и IWDT                                        ║
// ╚══════════════════════════════════════════════════════════════════╝

#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>
#include <esp_system.h>
#include <esp_heap_caps.h>

#include "config.h"
#include "settings.h"
#include "relay_manager.h"
#include "sensor_reader.h"
#include "button_manager.h"
#include "vent_controller.h"
#include "humid_controller.h"
#include "water_pump_controller.h"
#include "timer_scheduler.h"
#include "network_manager.h"
#include "wifi_provisioning.h"
#include "mqtt_manager.h"
#include "ws_handler.h"
#include "web_server_routes.h"
#include "history_logger.h"
#include "ota_manager.h"

// ═══════════════════════════════════════════════════════════════════
//  ВРЕМЯ СТАРТА И ФЛАГИ
// ═══════════════════════════════════════════════════════════════════
static uint64_t s_bootMs         = 0;
static uint64_t s_lastNvsSaveMs  = 0;
static uint64_t s_lastHealthMs   = 0;

// Флаг "таск Core1 уже обработал first NTP sync callback"
static bool s_firstNtpCbDone = false;

// ═══════════════════════════════════════════════════════════════════
//  ПРИЧИНА ПЕРЕЗАГРУЗКИ (v6.1)
// ═══════════════════════════════════════════════════════════════════
// Раньше в лог печаталось голое число, и понять постфактум, почему
// контроллер ушёл в перезагрузку, было невозможно. Теперь пишем
// расшифровку: по ней сразу видно, был ли это штатный ребут, паника
// (порча памяти / разыменование нуля), сторожевой таймер или просадка
// питания.
static const char* resetReasonStr(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON:   return "POWERON (power applied)";
        case ESP_RST_EXT:       return "EXT (external reset)";
        case ESP_RST_SW:        return "SW (normal ESP.restart)";
        case ESP_RST_PANIC:     return "PANIC (exception - memory corruption or null pointer!)";
        case ESP_RST_INT_WDT:   return "INT_WDT (interrupt watchdog!)";
        case ESP_RST_TASK_WDT:  return "TASK_WDT (task stalled over 30 s!)";
        case ESP_RST_WDT:       return "WDT (other watchdog!)";
        case ESP_RST_BROWNOUT:  return "BROWNOUT (supply voltage sag - check PSU!)";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
        case ESP_RST_SDIO:      return "SDIO";
        default:                return "UNKNOWN";
    }
}

// ═══════════════════════════════════════════════════════════════════
//  NTP FIRST SYNC CALLBACK
// ═══════════════════════════════════════════════════════════════════
// Вызывается через NetMgr::setOnFirstTimeSet() из taskCore1,
// когда NTP впервые синхронизирован. Здесь инициализируем всё,
// что требует реального времени: расписание таймеров полива,
// история данных, логирование с датой.
static void onFirstTimeSync() {
    Serial.println(F("[BOOT] NTP synced — initializing time-dependent modules"));

    // Таймеры полива могут правильно рассчитывать "следующий запуск"
    // только с реальным временем. До этого момента они ждут.
    TimerSched::onTimeAvailable();

    // История данных — начинаем писать с известным временем
    HistoryLogger::onTimeAvailable();

    s_firstNtpCbDone = true;
}

// ═══════════════════════════════════════════════════════════════════
//  TASK: CORE 1 (ОСНОВНАЯ БИЗНЕС-ЛОГИКА)
// ═══════════════════════════════════════════════════════════════════
// Цикл ~50 мс. Выполняет:
//   • Опрос датчиков (по индивидуальным таймерам)
//   • Управление форточками, увлажнителем, насосом
//   • Обработку событий кнопок
//   • Логирование истории
//   • Heartbeat LED
//   • Отложенное сохранение NVS
//   • Проверка NTP callback
#define TASK_CORE1_STACK        8192
#define TASK_CORE1_PRIORITY     2

static void taskCore1(void* /*param*/) {
    esp_task_wdt_add(NULL);

    // Регистрируем коллбэк NTP. Если время уже синхронизировано
    // (редкий случай), коллбэк вызовется сразу из setOnFirstTimeSet.
    NetMgr::setOnFirstTimeSet(onFirstTimeSync);

    for (;;) {
        esp_task_wdt_reset();

        const uint64_t nowMs = steadyMs();

        // ── Датчики ──────────────────────────────────────────────
        SensorRdr::update(nowMs);
        SensorSnapshot snap = SensorRdr::getData();

        // ═══════════════════════════════════════════════════════
        //  v6.1: НА ВРЕМЯ ОБНОВЛЕНИЯ ВСЯ АВТОМАТИКА ЗАМИРАЕТ
        // ═══════════════════════════════════════════════════════
        // OtaMgr::gracefulShutdown() обесточивает моторы форточек перед
        // записью флеша — но без этой проверки VentCtrl через 50 мс
        // запускал бы их заново, и мотор крутился бы всё обновление.
        // Отдельно: HistoryLogger пишет в LittleFS, а на этапе обновления
        // файловой системы раздел уже размонтирован — запись туда попала
        // бы прямо поверх заливаемого образа.
        //
        // Опрос датчиков и heartbeat оставляем: они ничего не двигают,
        // а мигающий светодиод показывает, что контроллер жив.
        //
        // До v6.1 флаг isUpdating() не проверялся вообще нигде.
        if (!OtaMgr::isUpdating()) {
            // ── Контроллеры (используют свежие данные датчиков) ──
            //
            // v6.2: управляем ТОЛЬКО по внутреннему BME280. Раньше при его
            // отказе подставлялся DHT22, а он висит СНАРУЖИ теплицы — это
            // разные величины, и подмена уводила автоматику в другую сторону:
            // в жару снаружи +17 при +31 внутри форточки начали бы
            // закрываться, а увлажнитель управлялся бы уличной влажностью.
            // Без внутреннего датчика правильнее замереть: VentCtrl при
            // NaN удерживает позицию, HumidCtrl выключает увлажнитель.
            // Дождь и уровень воды на BME280 не завязаны и работают всегда.
            const float temp = snap.bmeTemp;
            const float hum  = snap.bmeHum;

            static bool s_bmeLost = false;
            if (isnan(temp) && !s_bmeLost) {
                s_bmeLost = true;
                Serial.println(F("[BOOT] BME280 lost — vent/humidity control on hold "
                                 "(outdoor DHT22 is NOT a substitute)"));
            } else if (!isnan(temp) && s_bmeLost) {
                s_bmeLost = false;
                Serial.println(F("[BOOT] BME280 back — control resumed"));
            }

            // v6.0: VentCtrl сам читает modeVents через settings, передаём только данные
            bool ventsAuto = false;
            {
                MutexGuard lock(g_settingsMutex, pdMS_TO_TICKS(10));
                if (lock.locked()) ventsAuto = (g_settings.modeVents != 0);
            }
            VentCtrl::update(nowMs, temp, snap.rain, ventsAuto);
            HumidCtrl::update(hum);
            WaterPumpCtrl::update(snap.waterLevelPct);

            // ── H-bridge pending операции (break-before-make) ────
            RelayMgr::updatePending(nowMs);

            // ── Таймеры полива (если в auto) ─────────────────────
            TimerSched::update(nowMs);

            // ── Кнопки ───────────────────────────────────────────
            BtnMgr::processEvents();
        }

        // ── Heartbeat LED (1 Гц) ─────────────────────────────────
        RelayMgr::updateHeartbeat(nowMs);

        // v6.1: WebSocket broadcast отсюда УБРАН. AsyncWebSocket не
        // потокобезопасен, и обращение к нему из этой задачи ломало
        // память при отключении клиента (см. ws_handler.cpp).
        // Состояние теперь отдаётся по запросу get_state в задаче AsyncTCP.

        // ── История данных (раз в HISTORY_INTERVAL_MS) ──────────
        // Пишет в LittleFS — при обновлении файловой системы раздел
        // размонтирован, трогать нельзя (см. блок выше).
        if (!OtaMgr::isUpdating()) {
            HistoryLogger::update(nowMs);
        }

        // ── Отложенное сохранение NVS ────────────────────────────
        // Если есть dirty флаг и прошло NVS_SAVE_DELAY_MS с последнего save —
        // сохраняем. Это защищает flash от износа при частых изменениях.
        if (g_settingsDirty.load()
            && (nowMs - s_lastNvsSaveMs) >= NVS_SAVE_DELAY_MS) {
            s_lastNvsSaveMs = nowMs;
            settingsSave();  // сам сбросит dirty при успехе
        }

        // ── Health monitor (раз в 60 сек) ────────────────────────
        if (nowMs - s_lastHealthMs >= 60000ULL) {
            s_lastHealthMs = nowMs;
            uint32_t freeHeap   = ESP.getFreeHeap();
            uint32_t minHeap    = ESP.getMinFreeHeap();
            UBaseType_t stackWM = uxTaskGetStackHighWaterMark(NULL);
            // v6.1: largest — крупнейший непрерывный свободный блок.
            // Именно он показывает фрагментацию: heap может оставаться
            // большим, а largest падать — тогда аллокации начнут падать
            // при формально свободной памяти. Если за недели largest
            // ползёт вниз, а heap стоит — это утечка через фрагментацию.
            uint32_t largest    = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
            Serial.printf("[HEALTH] heap=%u min=%u largest=%u stackWM=%u nvsWr=%u uptime=%llus\n",
                (unsigned)freeHeap, (unsigned)minHeap, (unsigned)largest,
                (unsigned)stackWM, (unsigned)settingsNvsWriteCount(),
                (unsigned long long)((nowMs - s_bootMs) / 1000ULL));
        }

        // ── Цикл 50 мс ───────────────────────────────────────────
        // Это время — компромисс:
        //   • достаточно частое для отзывчивости кнопок (макс 80 мс задержки)
        //   • достаточно редкое чтобы не есть CPU (нагрузка ~5%)
        //   • позволяет FreeRTOS давать время другим задачам
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ═══════════════════════════════════════════════════════════════════
//  ПЛАНОВЫЙ ПЕРЕЗАПУСК (опциональный)
// ═══════════════════════════════════════════════════════════════════
// Прошивка перезагружает ESP32 раз в N дней для устранения накопленных
// утечек памяти (в драйверах ESP-IDF, не в нашем коде).
// Перезагрузка происходит в 4:00, когда нагрузка минимальна.
static void maybeScheduledReboot() {
#if PLANNED_REBOOT_DAYS > 0
    uint64_t uptimeSec = (steadyMs() - s_bootMs) / 1000ULL;
    uint64_t threshold = (uint64_t)PLANNED_REBOOT_DAYS * 24ULL * 3600ULL;
    if (uptimeSec < threshold) return;

    // Только в 4:00 и только если NTP знает время
    if (!NetMgr::isTimeSet()) return;
    if (NetMgr::getHour() != 4) return;

    Serial.printf("[REBOOT] Scheduled reboot after %llu days uptime\n",
        (unsigned long long)(uptimeSec / 86400ULL));
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP.restart();
#endif
}

// ═══════════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════════
void setup() {
    Serial.begin(115200);
    vTaskDelay(pdMS_TO_TICKS(100));
    Serial.println();
    Serial.println(F("═══════════════════════════════════════════"));
    Serial.printf("  Smart Greenhouse  v%s\n", FW_VERSION);
    {
        esp_reset_reason_t rr = esp_reset_reason();
        Serial.printf("  Reset reason: %s\n", resetReasonStr(rr));
        // Всё, кроме POWERON/EXT/SW — признак сбоя. Выделяем в логе,
        // чтобы не пропустить при разборе долгой работы.
        if (rr != ESP_RST_POWERON && rr != ESP_RST_EXT && rr != ESP_RST_SW) {
            Serial.println(F("  !!! PREVIOUS RUN ENDED ABNORMALLY !!!"));
        }
    }
    Serial.println(F("═══════════════════════════════════════════"));

    s_bootMs = steadyMs();

    // ── 1. РЕЛЕ В OFF: САМОЕ ПЕРВОЕ (безопасность!) ──────────────
    // До загрузки настроек все реле должны быть принудительно выключены.
    // Это предотвращает "случайное" включение при старте (например,
    // если GPIO15 strapping pin подтянулся в HIGH и включил реле форточки).
    RelayMgr::init();

    // ── 2. LittleFS ──────────────────────────────────────────────
    if (!LittleFS.begin(false)) {
        Serial.println(F("[BOOT] LittleFS mount failed — formatting..."));
        LittleFS.format();
        if (!LittleFS.begin(false)) {
            Serial.println(F("[BOOT] LittleFS format failed — critical"));
            // Всё равно продолжаем — провижн работает без LittleFS
        }
    } else {
        Serial.printf("[BOOT] LittleFS OK: total=%u used=%u bytes\n",
            (unsigned)LittleFS.totalBytes(), (unsigned)LittleFS.usedBytes());
    }

    // ── 3. Настройки + runtime форточек ──────────────────────────
    settingsLoad();

    // ── 4. Восстановление состояний одиночных реле ───────────────
    // (Реле форточек НЕ восстанавливаются — они всегда стартуют в OFF,
    // см. RelayMgr::restoreFromSettings.)
    RelayMgr::restoreFromSettings();

    // ── 5. Датчики и кнопки ──────────────────────────────────────
    SensorRdr::init();
    BtnMgr::init();

    // ── 6. Контроллеры ───────────────────────────────────────────
    VentCtrl::init();
    TimerSched::init();

    // ── 7. Network manager (только регистрация event handler) ────
    NetMgr::init();

    // ── 8. Watchdog и управляющая задача ─────────────────────────
    // v6.2: подняты СЮДА, выше развилки provisioning. Раньше taskCore1
    // создавалась предпоследним шагом, и это давало два провала:
    //   • в режиме первоначальной настройки setup() уходил в WifiProv и
    //     не возвращался — автоматика не работала вообще, форточки не
    //     закрывались по дождю всё время, пока владелец настраивает Wi-Fi;
    //   • при обычном старте подключение к сети занимает до 30 секунд,
    //     и всё это время теплица тоже была без управления.
    //
    // Всё, к чему обращается taskCore1 и что ещё не поднято, к вызовам
    // без инициализации готово: HistoryLogger и TimerSched выходят по
    // флагу «времени нет», MqttMgr только ставит атомарный бит,
    // WsHandler::notify* — пустышки, OtaMgr::isUpdating() читает статик.
    esp_task_wdt_init(WDT_TIMEOUT_SEC, true);
    esp_task_wdt_add(NULL);              // сюда же loopTask

    xTaskCreatePinnedToCore(
        taskCore1, "core1_main", TASK_CORE1_STACK, nullptr,
        TASK_CORE1_PRIORITY, nullptr, 1);
    Serial.printf("[BOOT] Core1 task started (stack=%u, prio=%u)\n",
        (unsigned)TASK_CORE1_STACK, (unsigned)TASK_CORE1_PRIORITY);

    // ── 9. ★ КЛЮЧЕВАЯ РАЗВИЛКА ★ ──────────────────────────────
    // Если нет Wi-Fi кредов в NVS — запускаем provisioning mode
    // и НЕ возвращаемся из setup() в обычный режим (прошивка крутится
    // в loop(), ждёт пока пользователь настроит Wi-Fi через AP).
    // Автоматика теплицы при этом уже работает — см. шаг 8.
    if (!NetMgr::hasCredentials()) {
        Serial.println(F("[BOOT] No WiFi credentials — starting AP provisioning"));
        WifiProv::start(WIFI_AP_SSID_PREFIX, WIFI_AP_PASSWORD);
        return;  // основной setup() здесь завершается!
    }

    // ── 9. Подключение к Wi-Fi (STA mode) ────────────────────────
    Serial.println(F("[BOOT] Connecting to WiFi..."));
    bool connected = NetMgr::connectSTA();
    if (connected) {
        Serial.printf("[BOOT] WiFi OK: IP=%s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println(F("[BOOT] WiFi connect failed — will retry in background"));
        // Не фатально: фоновая задача будет пытаться переподключиться.
        // MQTT и веб-сервер стартуем в любом случае — они начнут работать
        // как только появится IP.
#if WIFI_AP_FALLBACK
        // v6.1: учётные данные есть, но сеть недоступна — типичный случай
        // опечатки в пароле Wi-Fi. Поднимаем аварийную точку доступа, иначе
        // исправить пароль было бы негде: в сети контроллера нет, а сам он
        // раньше в режим настройки не возвращался. Автоматика теплицы при
        // этом работает как обычно — точка доступа лишь даёт доступ к панели.
        NetMgr::startRescueAp();
#endif
    }

    // ── 10. NTP (esp_sntp) ───────────────────────────────────────
    NetMgr::initNtp();
    NetMgr::startWifiTask();

    // ── 11. WebSocket + HTTP-сервер ──────────────────────────────
    WsHandler::init();
    WebRoutes::init();  // регистрирует роуты и запускает AsyncWebServer

    // ── 12. MQTT ─────────────────────────────────────────────────
    MqttMgr::init();

    // ── 13. OTA ──────────────────────────────────────────────────
    OtaMgr::init();

    // ── 14. История ──────────────────────────────────────────────
    HistoryLogger::init();

    // Watchdog и задача Core 1 подняты выше, на шаге 8.

    Serial.println(F("[BOOT] Setup complete"));
}

// ═══════════════════════════════════════════════════════════════════
//  LOOP — задача loopTask, ЯДРО 1, приоритет 1
// ═══════════════════════════════════════════════════════════════════
// v6.1: здесь было написано «Core 0, вместе с AsyncTCP и Wi-Fi стеком».
// Это неверно, и проверяется по исходникам фреймворка:
//   CONFIG_ARDUINO_RUNNING_CORE = 1   → loopTask на ЯДРЕ 1
//   CONFIG_ASYNC_TCP_RUNNING_CORE     → по умолчанию -1, любое ядро
// То есть loop() делит ядро 1 с taskCore1, а не с сетью.
//
// Приоритеты: taskCore1 = 2, loopTask = 1. Поэтому управляющий цикл
// всегда вытесняет эту задачу, а она работает в паузах между его
// пятидесятимиллисекундными тиками. Это правильно: MQTT-подключение
// внутри MqttMgr::loop() может блокировать на секунды, и оно не должно
// задерживать опрос датчиков и реле.
void loop() {
    esp_task_wdt_reset();

    // ── AP provisioning режим: DNS + отложенный reboot ────────────
    if (WifiProv::isActive()) {
        WifiProv::loop();
        vTaskDelay(pdMS_TO_TICKS(10));
        return;
    }

    // ── MQTT loop (синхронный клиент PubSubClient) ───────────────
    MqttMgr::loop();

    // ── OTA (Arduino OTA слушает UDP) ────────────────────────────
    OtaMgr::loop();

    // ── Отложенная перезагрузка по запросу из веб-панели ─────────
    // v6.1: раньше этим занималась отдельная задача rebootWatch.
    if (WebRoutes::rebootDue()) {
        Serial.println(F("[HTTP] Rebooting now..."));
        vTaskDelay(pdMS_TO_TICKS(200));
        ESP.restart();
    }

    // ── Плановый перезапуск ──────────────────────────────────────
    maybeScheduledReboot();

    vTaskDelay(pdMS_TO_TICKS(100));
}
