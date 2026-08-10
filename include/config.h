#pragma once

// ═══════════════════════════════════════════════════════════════════
//  ВЕРСИЯ ПРОШИВКИ (v6.1+)
// ═══════════════════════════════════════════════════════════════════
// Используется в:
//   • контейнере OTA greenhouse_vX.Y.bin (для проверки совместимости)
//   • выводе версии в Serial при старте
//   • разделе About веб-интерфейса
// При выпуске новой версии меняй здесь и пересоздавай контейнер
// через tools/pack_firmware.py.
#define FW_VERSION_MAJOR  6
#define FW_VERSION_MINOR  1

// ╔══════════════════════════════════════════════════════════════════╗
// ║  config.h — Конфигурация «Умная теплица» ESP32                  ║
// ║                                                                  ║
// ║  Номер версии задаётся ВЫШЕ, в FW_VERSION_MAJOR/MINOR, и это    ║
// ║  единственное его место во всём проекте. В комментариях версию  ║
// ║  не дублировать: здесь стояло «Версия 5.0» на прошивке 6.1.     ║
// ║                                                                  ║
// ║  Аппаратная конфигурация:                                        ║
// ║  • 9 реле: 3 полива, 4 на H-мост двух форточек, увлажнитель,    ║
// ║    насос. Пары форточек: P1+P2 верхняя, P3+P4 нижняя.           ║
// ║  • 5 кнопок: 3 полива (нажатие) + «Открыть» и «Закрыть»         ║
// ║    форточек (удержание).                                         ║
// ║  • DHT22 на GPIO13, TRIG ультразвука на GPIO0.                   ║
// ║                                                                  ║
// ║  Значения ниже — источник истины. Формально их можно            ║
// ║  переопределить через build_flags, но так делать не стоит:      ║
// ║  из-за #ifndef внешний -D побеждает молча, и правка в этом      ║
// ║  файле просто перестаёт работать. Именно по этой причине из     ║
// ║  platformio.ini убраны флаги пинов I²C.                          ║
// ╚══════════════════════════════════════════════════════════════════╝

#include <esp_timer.h>
#include <stdint.h>

// ─── Монотонные часы (замена millis()) ──────────────────────────
// esp_timer_get_time() возвращает uint64_t микросекунд,
// переполнение через ~584 542 года (не доживём).
// steadyMs() — замена millis() без ограничения 49.7 дней.
static inline uint64_t steadyMs() {
    return (uint64_t)esp_timer_get_time() / 1000ULL;
}

// ─── Версия прошивки и веб-контента ──────────────────────────────
// v6.0+: используем макросную склейку из FW_VERSION_MAJOR/MINOR
#define _FW_STR_HELPER(x) #x
#define _FW_STR(x) _FW_STR_HELPER(x)
#define FW_VERSION  _FW_STR(FW_VERSION_MAJOR) "." _FW_STR(FW_VERSION_MINOR)

// ═══════════════════════════════════════════════════════════════════
//  РЕЛЕ
// ═══════════════════════════════════════════════════════════════════
// Тип модуля: HIGH-active с оптопарами (EL817/PC817).
// HIGH на GPIO ESP32 → открытие транзистора-инвертора → LED оптопары
// → светодиод светится → реле включается.
//
// ВАЖНО: в момент boot ESP32 все реле на оптопарных модулях имеют
// склонность к кратковременному включению (~300-500 мс), потому что
// вход модуля тянется к VCC через pull-up оптопары. По этой причине
// strapping pin (GPIO2, GPIO12) под реле через оптопару НЕ используем.
//
// Пары реле форточек работают в H-bridge конфигурации:
//   OPEN  = HIGH → движение в сторону открытия
//   CLOSE = HIGH → движение в сторону закрытия
//   OPEN + CLOSE одновременно = ЗАПРЕЩЕНО (защита в прошивке)
//   OPEN=LOW + CLOSE=LOW = стоп (обмотка обесточена)

#ifndef RELAY_PIN_IRR1
#  define RELAY_PIN_IRR1             25   // Полив 1
#endif
#ifndef RELAY_PIN_IRR2
#  define RELAY_PIN_IRR2             26   // Полив 2
#endif
#ifndef RELAY_PIN_IRR3
#  define RELAY_PIN_IRR3             27   // Полив 3
#endif

// ── H-bridge верхней форточки (время хода 42 сек) ──────────────
#ifndef RELAY_PIN_VENT_UPPER_OPEN
#  define RELAY_PIN_VENT_UPPER_OPEN  32   // P1: движение на открытие
#endif
#ifndef RELAY_PIN_VENT_UPPER_CLOSE
#  define RELAY_PIN_VENT_UPPER_CLOSE 4    // P2: движение на закрытие
// GPIO4: безопасный OUTPUT пин, не strapping. Освобождён от
// кнопки увлажнителя — её перенесли в веб-интерфейс.
#endif

// ── H-bridge нижней форточки (время хода 24 сек) ───────────────
#ifndef RELAY_PIN_VENT_LOWER_OPEN
#  define RELAY_PIN_VENT_LOWER_OPEN  33   // P3: движение на открытие
#endif
#ifndef RELAY_PIN_VENT_LOWER_CLOSE
#  define RELAY_PIN_VENT_LOWER_CLOSE 15   // P4: движение на закрытие
// GPIO15: strapping MTDO, pull-up при boot → риск кратковременного
// срабатывания реле при старте. ОБЯЗАТЕЛЬНО: внешний pull-down
// резистор 10 кОм между GPIO15 и GND.
#endif

#ifndef RELAY_PIN_HUMID
#  define RELAY_PIN_HUMID            14   // Увлажнитель
#endif
#ifndef RELAY_PIN_WATER_PUMP
#  define RELAY_PIN_WATER_PUMP       5    // Насос скважины
#endif

// ─── Именованные индексы реле ─────────────────────────────────────
// Реле нумеруются единым массивом. Пары H-bridge идут подряд —
// так проще для атомарных групповых операций.
#define RELAY_IDX_IRR1               0
#define RELAY_IDX_IRR2               1
#define RELAY_IDX_IRR3               2
#define RELAY_IDX_VENT_UPPER_OPEN    3
#define RELAY_IDX_VENT_UPPER_CLOSE   4
#define RELAY_IDX_VENT_LOWER_OPEN    5
#define RELAY_IDX_VENT_LOWER_CLOSE   6
#define RELAY_IDX_HUMID              7
#define RELAY_IDX_WATER_PUMP         8

#define NUM_RELAYS                   9
#define NUM_IRRIGATION               3   // реле 0-2
#define NUM_VENTS                    2   // форточки (по 2 реле каждая)

// ─── Количество физических форточек ──────────────────────────────
#define VENT_IDX_UPPER               0
#define VENT_IDX_LOWER               1

// ═══════════════════════════════════════════════════════════════════
//  ДАТЧИКИ
// ═══════════════════════════════════════════════════════════════════

// ── I²C: BME280 (0x76) + BH1750 (0x23) ────────────────────────
#ifndef BME280_SDA
#  define BME280_SDA            21
#endif
#ifndef BME280_SCL
#  define BME280_SCL            22
#endif
#ifndef BME280_I2C_ADDRESS
#  define BME280_I2C_ADDRESS    0x76
#endif
#define  BH1750_I2C_ADDRESS     0x23   // GY-30, A0 на GND

// ── DHT22 (наружный T, H) ───────────────────────────────────────
// ИЗМЕНЕНО В v5.0: переехал с GPIO0 (strapping) на GPIO13 (обычный).
// Теперь DHT22 не влияет на boot ESP32, и внешних подтяжек не требуется —
// достаточно стандартного 10 кОм pull-up в комплекте с модулем.
//
// GPIO13 — не strapping, не ADC2 (ADC2 заблокирован при Wi-Fi),
// поддерживает двунаправленный режим, что нужно для 1-Wire протокола DHT22.
#ifndef DHT22_PIN
#  define DHT22_PIN             13
#endif

// ── Датчик дождя YL-83 (цифровой DO) ────────────────────────────
// DO модуля → GPIO39 (input-only). Модуль FC-37 имеет компаратор
// и свой pull-up на линии DO. Внешних подтяжек не нужно.
// СУХО: DO = HIGH. ДОЖДЬ: DO = LOW.
#ifndef RAIN_SENSOR_PIN
#  define RAIN_SENSOR_PIN       39
#endif
#define RAIN_ACTIVE_LEVEL       LOW    // Уровень при дожде

// ── Датчик уровня воды AJ-SR04M (ультразвуковой) ────────────────
// ИЗМЕНЕНО В v5.0: TRIG переехал с GPIO13 на GPIO0.
// GPIO0 — strapping (при boot должен быть HIGH = default pull-up).
// TRIG работает ТОЛЬКО как OUTPUT: генерирует короткие импульсы
// длительностью 10 мкс. На состояние GPIO0 при boot не влияет, т.к.
// pinMode(OUTPUT) + digitalWrite(LOW) выполняется уже после boot.
//
// ВАЖНО: AJ-SR04M работает от 5V, ECHO-выход до 5V!
// Для GPIO34 ОБЯЗАТЕЛЬНО резистивный делитель:
//
//     AJ-SR04M ECHO ──┬── 1 кОм ──── GPIO34 (ESP32)
//                      └── 2 кОм ──── GND
//
// Чем меньше расстояние от датчика до воды — тем больше уровень воды.
#ifndef WATER_TRIG_PIN
#  define WATER_TRIG_PIN        0      // ИЗМЕНЕНО v5.0 (был 13)
#endif
#ifndef WATER_ECHO_PIN
#  define WATER_ECHO_PIN        34     // input-only, делитель 5V→3.3V!
#endif

// ═══════════════════════════════════════════════════════════════════
//  КНОПКИ (INPUT_PULLUP, active LOW)
// ═══════════════════════════════════════════════════════════════════
// В v5.0 убраны:
//   • кнопка увлажнителя (GPIO4 занят под реле H-bridge форточки)
//   • кнопка насоса     (GPIO15 занят под реле H-bridge форточки)
// Управление этими устройствами — только через веб-интерфейс.

#ifndef BTN_IRR1_PIN
#  define BTN_IRR1_PIN          18
#endif
#ifndef BTN_IRR2_PIN
#  define BTN_IRR2_PIN          17
#endif
#ifndef BTN_IRR3_PIN
#  define BTN_IRR3_PIN          16
#endif
#ifndef BTN_VENT_OPEN_PIN
#  define BTN_VENT_OPEN_PIN     23     // v6.0: Кнопка ОТКРЫТЬ (эстафета верх→низ, hold)
#endif
#ifndef BTN_VENT_CLOSE_PIN
#  define BTN_VENT_CLOSE_PIN    19     // v6.0: Кнопка ЗАКРЫТЬ (эстафета низ→верх, hold)
#endif


#define NUM_BUTTONS             5   // было 7 в v4

// ═══════════════════════════════════════════════════════════════════
//  ИНДИКАЦИЯ
// ═══════════════════════════════════════════════════════════════════
// GPIO2 — встроенный синий LED платы DOIT DevKit V1.
// strapping pin с pull-down при boot: с LED совместимо (светодиод
// не светится до старта прошивки — это нормально).
//
// v5.0: LED теперь работает как HEARTBEAT (мигает 1 Гц = система жива),
// а не как индикатор режима auto-форточек. Режим виден в веб-интерфейсе.
#ifndef LED_HEARTBEAT_PIN
#  define LED_HEARTBEAT_PIN     2
#endif
#define LED_HEARTBEAT_PERIOD_MS 1000ULL   // 1 Гц

// ═══════════════════════════════════════════════════════════════════
//  WIFI AP PROVISIONING
// ═══════════════════════════════════════════════════════════════════
// При первом запуске (или если SSID не настроен) ESP32 поднимает
// Wi-Fi AP для настройки. Пользователь подключается телефоном к
// "Greenhouse-XXXXXX", открывает браузер — captive portal
// перенаправляет на форму настройки Wi-Fi.

#ifndef WIFI_AP_SSID_PREFIX
#  define WIFI_AP_SSID_PREFIX   "Greenhouse-"
#endif
#ifndef WIFI_AP_PASSWORD
#  define WIFI_AP_PASSWORD      "12345678"   // минимум 8 символов для WPA2
#endif
// Таймаут ожидания подключения к STA перед откатом в AP режим
#define WIFI_STA_TIMEOUT_MS     30000ULL    // 30 сек
// ─── Аварийная точка доступа ─────────────────────────────────────
// Если учётные данные сохранены, но подключиться к сети за
// WIFI_STA_TIMEOUT_MS не удалось — поднять свою точку доступа
// ПАРАЛЛЕЛЬНО с продолжающимися попытками подключиться (режим AP_STA).
//
// Зачем: при ошибке в пароле от Wi-Fi контроллер сохранял его,
// перезагружался, не подключался — и оставался в этом состоянии
// навсегда. В сети его нет, точку доступа он не поднимал, зайти и
// исправить пароль было нельзя. Помогал только приезд с USB-кабелем.
//
// Автоматика теплицы при этом продолжает работать: полив, форточки
// и насос не зависят от наличия сети. Точка доступа — только путь
// доступа к панели, она гаснет сама, как только STA подключится.
//
// v6.1: до этой версии константа была объявлена, но не использовалась
// в коде НИГДЕ — отката в AP не существовало в принципе.
#define WIFI_AP_FALLBACK        true

// ─── WiFi: умолчания для первого запуска ─────────────────────────
// Если в NVS нет сохранённых учётных данных — будет поднят AP.
// Эти константы — только fallback для случая, когда кто-то
// захочет зашить SSID/пароль напрямую в прошивку через build_flags.
#ifndef WIFI_DEFAULT_SSID
#  define WIFI_DEFAULT_SSID     ""
#endif
#ifndef WIFI_DEFAULT_PASS
#  define WIFI_DEFAULT_PASS     ""
#endif

// ─── Статический IP — опционально ────────────────────────────────
#ifndef NET_DEFAULT_IP
#  define NET_DEFAULT_IP        "192.168.1.100"
#endif
#ifndef NET_DEFAULT_MASK
#  define NET_DEFAULT_MASK      "255.255.255.0"
#endif
#ifndef NET_DEFAULT_GW
#  define NET_DEFAULT_GW        "192.168.1.1"
#endif
#ifndef NET_DEFAULT_DNS1
#  define NET_DEFAULT_DNS1      "8.8.8.8"
#endif
#ifndef NET_DEFAULT_DNS2
#  define NET_DEFAULT_DNS2      "8.8.4.4"
#endif

// ─── Веб-панель ──────────────────────────────────────────────────
#ifndef WEB_DEFAULT_USER
#  define WEB_DEFAULT_USER      "admin"
#endif
#ifndef WEB_DEFAULT_PASS
#  define WEB_DEFAULT_PASS      "admin"
#endif

// ═══════════════════════════════════════════════════════════════════
//  NTP (нативный esp_sntp, не NTPClient!)
// ═══════════════════════════════════════════════════════════════════
// v5.0: используем esp_sntp из ESP-IDF. Поддерживает:
//   • Автоматическую синхронизацию
//   • Коллбэк при первом успехе
//   • POSIX timezone rules (включая летнее/зимнее время)
//   • Несколько серверов с fallback

#ifndef NTP_SERVER_PRIMARY
#  define NTP_SERVER_PRIMARY    "pool.ntp.org"
#endif
#ifndef NTP_SERVER_SECONDARY
#  define NTP_SERVER_SECONDARY  "time.google.com"
#endif
// POSIX TZ string для Москвы (UTC+3, без перехода на летнее время).
// Формат: STD<offset>DST<offset>,start,end
// "MSK-3" = стандарт MSK, смещение -3 от локального (т.е. локальное = UTC+3)
#ifndef NTP_TZ_STRING
#  define NTP_TZ_STRING         "MSK-3"
#endif

// ═══════════════════════════════════════════════════════════════════
//  ФОРТОЧКИ: ступенчатое управление
// ═══════════════════════════════════════════════════════════════════
// В v5.0 форточки управляются как сервоприводы с позицией 0..100%.
// Позиция рассчитывается по времени движения, т.к. физического
// датчика положения штока нет.
//
// Полное время хода (от концевика до концевика, с запасом 10%):
//   Верхняя: 42 сек → 46 сек с запасом (для достижения 100% и срабатывания концевика)
//   Нижняя:  24 сек → 26 сек с запасом
//
// Шаг позиционирования: 5% (можно задавать 0, 5, 10, ..., 95, 100).
// Для верхней 1% = 420 мс, минимальная различимая позиция ~2%.
// Для нижней 1% = 240 мс, минимальная различимая позиция ~4%.

// Полное время хода актуаторов, мс (без запаса)
#define VENT_UPPER_TRAVEL_MS         42000UL
#define VENT_LOWER_TRAVEL_MS         24000UL

// Запас времени для принудительного достижения концевика (10%)
#define VENT_TRAVEL_OVERRUN_PCT      10

// Таймаут движения (безопасность при залипании концевика)
// Если реле работает дольше этого времени — принудительное отключение.
#define VENT_MOVE_TIMEOUT_MS(travelMs)   ((travelMs) * (100 + VENT_TRAVEL_OVERRUN_PCT) / 100)

// Break-before-make пауза для H-bridge (мс).
// При смене направления: сначала ВЫКЛЮЧАЕМ текущее реле,
// ждём эту паузу, ТОЛЬКО ПОТОМ включаем противоположное.
// Защищает контакты реле от искры и КЗ при перекрытии.
#define HBRIDGE_BREAK_BEFORE_MAKE_MS 100UL




// ═══════════════════════════════════════════════════════════════════
//  v6.0: ПАРАМЕТРЫ КАЛИБРОВКИ ФОРТОЧЕК (ручная)
// ═══════════════════════════════════════════════════════════════════
// Время полного хода (0% → 100%) в секундах.
// Пользователь измеряет вручную, вводит в веб-интерфейсе.
// Сохраняется в NVS, изменяется только пользователем.
#define TOP_CALIB_SEC                42      // Верхняя форточка
#define BOTTOM_CALIB_SEC             24      // Нижняя форточка

// ═══════════════════════════════════════════════════════════════════
//  v6.0: АВТО-ЛОГИКА ФОРТОЧЕК
// ═══════════════════════════════════════════════════════════════════
// Шаг открывания/закрывания (5 ступеней: 20%, 40%, 60%, 80%, 100%)
#define VENT_AUTO_STEP_PCT           20

// Пауза между ступенями в авто-режиме (секунды).
// За эту паузу контроллер сравнивает изменение температуры,
// принимает решение — продолжить ступенчатое открытие или остановиться.
#define STEP_DELAY_MIN               50

// Минимальный рост температуры за паузу для перехода к следующей ступени, °C.
// Защита от микроколебаний датчика BME280/DHT22.
#define STEP_TEMP_RISE               0.3f

// Break-before-make для эстафеты между актуаторами в РУЧНОМ режиме (мс).
// Защита БП от скачка тока при одновременном переключении двух моторов.
#define MANUAL_RELAY_HANDOFF_MS      200

// Максимальное время непрерывного движения от одной кнопки в ручном режиме.
// Защита: если связь оборвалась с зажатой кнопкой → мотор остановится сам.
// Сумма обоих ходов с запасом 20%.
#define MANUAL_HOLD_TIMEOUT_MS       ((TOP_CALIB_SEC + BOTTOM_CALIB_SEC) * 1200UL)

// ═══════════════════════════════════════════════════════════════════
//  ИНТЕРВАЛЫ (мс)
// ═══════════════════════════════════════════════════════════════════
#define WDT_TIMEOUT_SEC            30
#define WIFI_CHECK_MS              15000ULL
#define WIFI_RECONNECT_MS          15000ULL
#define WIFI_BACKOFF_MAX_MS        300000ULL  // макс. интервал reconnect: 5 минут
#define BME280_POLL_MS             2000ULL
#define DHT22_POLL_MS              3500ULL
#define BH1750_POLL_MS             2500ULL
#define WATER_LEVEL_POLL_MS        2000ULL
// v6.1: WS_BROADCAST_MS удалён. Контроллер больше не рассылает состояние
// сам — панель запрашивает его командой get_state раз в секунду, и период
// задаётся на стороне клиента (startStatePoll в app.js).
#define NVS_SAVE_DELAY_MS          5000ULL
#define BTN_DEBOUNCE_US            30000ULL   // 30 мс
#define HISTORY_INTERVAL_MS        3600000ULL

// Антидребезг реле: минимум между переключениями одного реле, мс.
// Защита от осцилляции на границе гистерезиса и от залипания.
// Исключение: реле H-bridge внутри пары переключаются немедленно
// (через break-before-make паузу).
#define RELAY_MIN_TOGGLE_INTERVAL_MS  1000UL

// ═══════════════════════════════════════════════════════════════════
//  ПАРАМЕТРЫ ИСТОРИИ
// ═══════════════════════════════════════════════════════════════════
#define HISTORY_POINTS          24

// ═══════════════════════════════════════════════════════════════════
//  ПОРОГИ ПО УМОЛЧАНИЮ
// ═══════════════════════════════════════════════════════════════════
#define DEF_VENT_UPPER_HIGH     26.0f  // Открыть при T >= high
#define DEF_VENT_UPPER_LOW      22.0f  // Закрыть при T <= low
#define DEF_VENT_LOWER_HIGH     29.0f
#define DEF_VENT_LOWER_LOW      22.0f
#define DEF_HUMID_HIGH          95.0f
#define DEF_HUMID_LOW           90.0f

// ── Накопительная ёмкость (насос скважины) ───────────────────────
#define DEF_WATER_LOW_PCT       5
#define DEF_WATER_HIGH_PCT      95
#define DEF_WATER_DEPTH_CM      100
#define DEF_WATER_PUMP_TIMEOUT  30     // минут

// Мёртвая зона AJ-SR04M (ультразвук не измеряет ближе этого расстояния)
#define WATER_SENSOR_MIN_DIST_CM  2

// ═══════════════════════════════════════════════════════════════════
//  WebSocket
// ═══════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════
//  ДОЛГОВРЕМЕННАЯ СТАБИЛЬНОСТЬ
// ═══════════════════════════════════════════════════════════════════
#ifndef PLANNED_REBOOT_DAYS
#  define PLANNED_REBOOT_DAYS    180
#endif

// ═══════════════════════════════════════════════════════════════════
//  ПОРОГИ ОСВЕЩЁННОСТИ ДЛЯ ИКОНОК
// ═══════════════════════════════════════════════════════════════════
#ifndef LUX_NIGHT
#  define LUX_NIGHT    30
#endif
#ifndef LUX_DAY
#  define LUX_DAY      4000
#endif

// ═══════════════════════════════════════════════════════════════════
//  MQTT
// ═══════════════════════════════════════════════════════════════════
#ifndef MQTT_DEFAULT_SERVER
#  define MQTT_DEFAULT_SERVER    ""          // пусто = MQTT отключён
#endif
#ifndef MQTT_DEFAULT_PORT
#  define MQTT_DEFAULT_PORT      1883
#endif
#ifndef MQTT_DEFAULT_USER
#  define MQTT_DEFAULT_USER      ""
#endif
#ifndef MQTT_DEFAULT_PASS
#  define MQTT_DEFAULT_PASS      ""
#endif
#ifndef MQTT_BASE_TOPIC
#  define MQTT_BASE_TOPIC        "greenhouse"
#endif
#define MQTT_PUBLISH_MS          10000ULL
#define MQTT_RECONNECT_MS        15000ULL
#define MQTT_KEEPALIVE_SEC       30
#define MQTT_BUFFER_SIZE         512

// MQTT discovery (Home Assistant): отправлять только при изменении
// настроек, а не при каждом reconnect. v5.0 fix — снижает нагрузку на HA.
// Сбрасывается в 0 при изменении настроек через веб, или при ручном
// запросе пересылки discovery.
#define MQTT_DISCOVERY_RESEND_INTERVAL_MS   (24ULL * 3600ULL * 1000ULL)   // раз в сутки
