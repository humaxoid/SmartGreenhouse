// ╔══════════════════════════════════════════════════════════════════╗
// ║  network_manager.cpp — Реализация Wi-Fi + NTP v5.0              ║
// ║                                                                  ║
// ║  Главные отличия от v4:                                          ║
// ║   1. NTPClient → esp_sntp (нативный, потокобезопасный)          ║
// ║   2. polling reconnect → WiFi.onEvent (event-driven)            ║
// ║   3. POSIX TZ через setenv("TZ", ...)                           ║
// ║   4. Уменьшен размер кода за счёт удаления мьютексов NTP        ║
// ╚══════════════════════════════════════════════════════════════════╝

#include "network_manager.h"
#include "settings.h"
#include <WiFi.h>
#include <Preferences.h>
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_task_wdt.h>
#include <esp_sntp.h>        // нативный SNTP из ESP-IDF
#include <time.h>
#include <sys/time.h>
#include <atomic>

// ═══════════════════════════════════════════════════════════════════
//  ВНУТРЕННЕЕ СОСТОЯНИЕ
// ═══════════════════════════════════════════════════════════════════
static NetCache      s_cache;
static char          s_wsToken[96];
static Preferences   s_prefs;

// NTP: time_set флаг атомарный — выставляется в коллбэке из другой задачи.
static std::atomic<bool> s_timeSet{false};

// Коллбэк "первая NTP-синхронизация"
static void (*s_onFirstTimeSet)() = nullptr;
static std::atomic<bool> s_firstSyncFired{false};
// v6.0 fix: флаг "пользовательский коллбэк уже вызван" — защита от двойного вызова
static std::atomic<bool> s_userCbInvoked{false};

// Флаг "мы подключены к STA" (обновляется через WiFi events)
static std::atomic<bool> s_wifiConnected{false};

// v6.1: поднята ли аварийная точка доступа (см. NetMgr::startRescueAp)
static std::atomic<bool> s_rescueAp{false};

// Буфер для getFormattedTime
static char s_timeBuf[12];

// ═══════════════════════════════════════════════════════════════════
//  BASE64 (RFC 4648)
// ═══════════════════════════════════════════════════════════════════
// Для HTTP Basic Auth. Самописный, чтобы не тянуть лишнюю зависимость.
static const char B64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static uint32_t base64Encode(const char* src, char* dst, uint32_t dstSize) {
    uint32_t in  = 0, out = 0;
    uint32_t len = (uint32_t)strlen(src);
    while (in < len && out + 5 < dstSize) {
        uint32_t v  = 0;
        int      nb = 0;
        v  = (uint8_t)src[in++] << 16; nb = 1;
        if (in < len) { v |= (uint8_t)src[in++] << 8; nb = 2; }
        if (in < len) { v |= (uint8_t)src[in++];      nb = 3; }
        dst[out++] = B64[(v >> 18) & 0x3F];
        dst[out++] = B64[(v >> 12) & 0x3F];
        dst[out++] = (nb >= 2) ? B64[(v >>  6) & 0x3F] : '=';
        dst[out++] = (nb >= 3) ? B64[(v >>  0) & 0x3F] : '=';
    }
    dst[out] = '\0';
    return out;
}

static void rebuildToken() {
    char plain[68];
    snprintf(plain, sizeof(plain), "%s:%s", s_cache.webUser, s_cache.webPass);
    base64Encode(plain, s_wsToken, sizeof(s_wsToken));
}

// ═══════════════════════════════════════════════════════════════════
//  NVS загрузка/сохранение
// ═══════════════════════════════════════════════════════════════════
static void loadCache() {
    // Используем read-only mode (true) для быстрого чтения
    s_prefs.begin("net3", true);

    s_cache.isDHCP = s_prefs.getBool("dhcp", true);

    // Хелпер: прочитать строку, а если ключа НЕТ — подставить default.
    //
    // v6.1: раньше условием подстановки было `if (!dst[0])`, то есть
    // пустая строка. Это разные вещи, и разница критична для SSID:
    // «Забыть Wi-Fi» записывает в NVS именно пустую строку, и она —
    // единственный признак «сеть не настроена», по которому setup()
    // уходит в режим точки доступа. При WIFI_DEFAULT_SSID == "" разницы
    // не было, но стоит задать SSID через build_flags — и кнопка молча
    // превращалась бы в «подключиться к вшитой сети».
    //
    // Отличить отсутствие ключа от пустого значения можно по возврату:
    // getString() возвращает 0, если ключа нет, и strlen+1 (то есть
    // минимум 1), если ключ есть — даже когда в нём пустая строка.
    auto readStr = [&](const char* key, char* dst, size_t dstSize, const char* def) {
        dst[0] = '\0';
        if (s_prefs.getString(key, dst, dstSize) == 0) strlcpy(dst, def, dstSize);
    };

    readStr("ssid", s_cache.ssid,    sizeof(s_cache.ssid),    WIFI_DEFAULT_SSID);
    readStr("pass", s_cache.pass,    sizeof(s_cache.pass),    WIFI_DEFAULT_PASS);
    readStr("ip",   s_cache.ip,      sizeof(s_cache.ip),      NET_DEFAULT_IP);
    readStr("sub",  s_cache.subnet,  sizeof(s_cache.subnet),  NET_DEFAULT_MASK);
    readStr("gw",   s_cache.gateway, sizeof(s_cache.gateway), NET_DEFAULT_GW);
    readStr("dns1", s_cache.dns1,    sizeof(s_cache.dns1),    NET_DEFAULT_DNS1);
    readStr("dns2", s_cache.dns2,    sizeof(s_cache.dns2),    NET_DEFAULT_DNS2);
    readStr("wusr", s_cache.webUser, sizeof(s_cache.webUser), WEB_DEFAULT_USER);
    readStr("wpas", s_cache.webPass, sizeof(s_cache.webPass), WEB_DEFAULT_PASS);

    s_prefs.end();
    rebuildToken();
    // v6.1: печатаем то, от чего зависит ключевая развилка в setup().
    // Без этой строки «контроллер не поднял точку доступа» неотличимо
    // от «контроллер не стёр SSID» — а лечится это по-разному.
    Serial.printf("[NET] Cache loaded from NVS. SSID=\"%s\" (%s)\n",
        s_cache.ssid,
        s_cache.ssid[0] ? "configured" : "EMPTY -> AP provisioning at boot");
}

// ═══════════════════════════════════════════════════════════════════
//  WIFI EVENT HANDLER
// ═══════════════════════════════════════════════════════════════════
// Реакция на события Wi-Fi стека. Выполняется в контексте события
// (в задаче Event Task на Core 0), поэтому тяжёлые операции здесь
// запрещены — только быстрые обновления флагов.
//
// ARDUINO_EVENT_WIFI_STA_GOT_IP: подключились, получили IP.
//   • Устанавливаем s_wifiConnected = true
//   • Запускаем NTP (если ещё не запущен)
//
// ARDUINO_EVENT_WIFI_STA_DISCONNECTED: потеряли соединение.
//   • Сбрасываем s_wifiConnected
//   • NTP не трогаем — он будет сам переподключаться
static void onWiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP: {
            s_wifiConnected.store(true);
            IPAddress ip = WiFi.localIP();
            Serial.printf("[NET] Got IP: %u.%u.%u.%u\n",
                ip[0], ip[1], ip[2], ip[3]);

            // Запускаем/перезапускаем SNTP (он сам пойдёт на сервер)
            if (!sntp_enabled()) {
                sntp_init();
                Serial.println(F("[NTP] SNTP started"));
            }
            break;
        }
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
            if (s_wifiConnected.load()) {
                Serial.println(F("[NET] Disconnected"));
            }
            s_wifiConnected.store(false);
            break;
        }
        case ARDUINO_EVENT_WIFI_STA_CONNECTED: {
            Serial.println(F("[NET] WiFi connected, waiting for IP..."));
            break;
        }
        default:
            break;
    }
}

// ═══════════════════════════════════════════════════════════════════
//  SNTP SYNC CALLBACK
// ═══════════════════════════════════════════════════════════════════
// Вызывается ESP-IDF при каждой успешной синхронизации с NTP сервером.
// ВНИМАНИЕ: выполняется в контексте LWIP task — строго ограничить
// работу (atomic flags, минимум кода).
static void sntpSyncCallback(struct timeval* tv) {
    s_timeSet.store(true);

    // Первая синхронизация — выставляем флаг.
    // Сам коллбэк пользователя вызовется из NetMgr::loop() —
    // ВАЖНО: нельзя вызывать здесь (LWIP task имеет небольшой стек,
    // а коллбэк может пересчитывать расписание таймеров и т.п.)
    if (!s_firstSyncFired.exchange(true)) {
        Serial.println(F("[NTP] First sync received — callback pending"));
    }
}

// ═══════════════════════════════════════════════════════════════════
//  ПРИМЕНЕНИЕ НАСТРОЕК WIFI
// ═══════════════════════════════════════════════════════════════════
static void applyWiFi() {
    if (!s_cache.ssid[0]) {
        Serial.println(F("[NET] SSID empty — skip applyWiFi"));
        return;
    }

    // v6.1: если поднята аварийная точка доступа — режим AP_STA, иначе
    // переход в чистый STA погасил бы её на первом же цикле переподключения,
    // и пользователь потерял бы доступ ровно тогда, когда чинит пароль.
    WiFi.mode(s_rescueAp.load() ? WIFI_AP_STA : WIFI_STA);
    // setAutoReconnect(true) — ESP сам переподключается после disconnect.
    // Это дополнение к нашей фоновой задаче (которая нужна на случай
    // длительного отсутствия точки).
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);  // не писать в NVS Arduino-ядра (у нас свой NVS)

    // v6.1: выключаем энергосбережение радио.
    // По умолчанию Arduino включает modem sleep: между beacon-интервалами
    // приёмник засыпает. Для устройства на батарее это правильно, а для
    // стационарного контроллера даёт только вред — растут задержки ответа
    // веб-панели, теряются MQTT keepalive и учащаются разрывы на слабом
    // сигнале. Питание тут сетевое, экономить нечего.
    WiFi.setSleep(false);

    if (s_cache.isDHCP) {
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
        Serial.println(F("[NET] Mode: DHCP"));
    } else {
        IPAddress ip, sub, gw, d1, d2;
        bool ok = ip.fromString(s_cache.ip)
               && sub.fromString(s_cache.subnet)
               && gw.fromString(s_cache.gateway);
        if (!ok) {
            Serial.println(F("[NET] Bad static IP — fallback DHCP"));
            WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
        } else {
            d1.fromString(s_cache.dns1);
            d2.fromString(s_cache.dns2);
            WiFi.config(ip, gw, sub, d1, d2);
            Serial.printf("[NET] Static IP=%s\n", s_cache.ip);
        }
    }

    WiFi.begin(s_cache.ssid, s_cache.pass);
    Serial.printf("[NET] Connecting to \"%s\"...\n", s_cache.ssid);
}

// ═══════════════════════════════════════════════════════════════════
//  БЛОКИРУЮЩЕЕ ОЖИДАНИЕ STA ПОДКЛЮЧЕНИЯ
// ═══════════════════════════════════════════════════════════════════
// Используется только при первом подключении в setup().
// Опрашивает s_wifiConnected (обновляется в event handler).
static bool waitForConnect(uint32_t timeoutMs) {
    uint64_t start = steadyMs();
    while (!s_wifiConnected.load() && (steadyMs() - start < timeoutMs)) {
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    return s_wifiConnected.load();
}

// ═══════════════════════════════════════════════════════════════════
//  ФОНОВАЯ ЗАДАЧА WIFI WATCHDOG
// ═══════════════════════════════════════════════════════════════════
// В v5.0 роль задачи упростилась: event handler сам ловит disconnect
// и WiFi-стек сам пытается переподключиться (setAutoReconnect).
// Но для случая "точка доступа исчезла надолго" мы добавляем
// периодический полный reset с exponential backoff.
static void taskWiFiWatchdog(void* /*param*/) {
    esp_task_wdt_add(NULL);
    uint64_t backoffMs = WIFI_CHECK_MS;  // начинаем с 15 сек

    for (;;) {
        esp_task_wdt_reset();

        // v6.0 fix: проверяем NTP коллбэк часто (раз в 500 мс),
        // а не раз в WIFI_CHECK_MS (15 сек)
        uint64_t waitStart = steadyMs();
        while (steadyMs() - waitStart < backoffMs) {
            esp_task_wdt_reset();
            vTaskDelay(pdMS_TO_TICKS(500));

            // Если NTP синхронизировался И есть отложенный коллбэк И
            // мы его ещё не вызвали — вызываем его в контексте этой задачи.
            if (s_firstSyncFired.load()
                    && s_onFirstTimeSet != nullptr
                    && !s_userCbInvoked.exchange(true)) {
                Serial.println(F("[NTP] Invoking user callback now"));
                void (*cb)() = s_onFirstTimeSet;
                s_onFirstTimeSet = nullptr;
                cb();   // тут может быть тяжёлая работа — поэтому в задаче, не в SNTP context
            }

            // v6.1: домашняя сеть поднялась — аварийная точка доступа
            // больше не нужна. Гасим здесь, а не в обработчике события
            // Wi-Fi: там контекст event task, тяжёлые операции запрещены.
            if (s_rescueAp.load() && s_wifiConnected.load()) {
                NetMgr::stopRescueAp();
            }
        }

        if (s_wifiConnected.load()) {
            // Подключено — сбрасываем backoff
            if (backoffMs != WIFI_CHECK_MS) {
                backoffMs = WIFI_CHECK_MS;
            }
            continue;
        }

        Serial.printf("[NET] Still disconnected — full reset (backoff %llu sec)\n",
            (unsigned long long)(backoffMs / 1000ULL));

        // Полный reset Wi-Fi стека (поможет если точка вернулась
        // но Wi-Fi по каким-то причинам "застрял" в disconnect).
        //
        // v6.1: при поднятой аварийной точке доступа радио целиком НЕ гасим —
        // WIFI_OFF уронил бы её, а именно через неё пользователь сейчас
        // и подключается, чтобы исправить пароль.
        if (s_rescueAp.load()) {
            WiFi.disconnect(false);
            vTaskDelay(pdMS_TO_TICKS(100));
        } else {
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            vTaskDelay(pdMS_TO_TICKS(300));
        }
        applyWiFi();

        // Ждём результат (через event handler)
        if (waitForConnect(WIFI_RECONNECT_MS)) {
            backoffMs = WIFI_CHECK_MS;
        } else {
            backoffMs *= 2;
            if (backoffMs > WIFI_BACKOFF_MAX_MS) backoffMs = WIFI_BACKOFF_MAX_MS;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
//  ПУБЛИЧНЫЙ API: Wi-Fi
// ═══════════════════════════════════════════════════════════════════
void NetMgr::init() {
    loadCache();

    // Регистрируем event handler ДО первой попытки подключения —
    // чтобы не пропустить event GOT_IP.
    WiFi.onEvent(onWiFiEvent);
    Serial.println(F("[NET] WiFi event handler registered"));
}

bool NetMgr::hasCredentials() {
    return s_cache.ssid[0] != '\0';
}

bool NetMgr::connectSTA() {
    if (!hasCredentials()) {
        Serial.println(F("[NET] No credentials — cannot connect STA"));
        return false;
    }
    applyWiFi();
    bool ok = waitForConnect(WIFI_STA_TIMEOUT_MS);
    if (!ok) {
        Serial.println(F("[NET] STA timeout"));
    }
    return ok;
}

void NetMgr::startWifiTask() {
    xTaskCreatePinnedToCore(
        taskWiFiWatchdog,
        "WiFiWD",
        3072,     // стек
        nullptr,
        1,        // приоритет
        nullptr,
        0         // Core 0
    );
    Serial.println(F("[NET] WiFi watchdog task started"));
}

bool NetMgr::isConnected() {
    return s_wifiConnected.load();
}

// ═══════════════════════════════════════════════════════════════════
//  АВАРИЙНАЯ ТОЧКА ДОСТУПА (v6.1)
// ═══════════════════════════════════════════════════════════════════
// Поднимается рядом с работающим STA (режим AP_STA), когда учётные
// данные есть, но подключиться не удалось. Своего веб-сервера и DNS
// не поднимает — обычная панель уже слушает порт 80 на всех интерфейсах,
// поэтому по адресу 192.168.4.1 доступен полный интерфейс, включая
// раздел «Сеть», где и правится неверный пароль.
void NetMgr::startRescueAp() {
    if (s_rescueAp.load()) return;

    char apSsid[32];
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(apSsid, sizeof(apSsid), "%s%02X%02X%02X",
        WIFI_AP_SSID_PREFIX, mac[3], mac[4], mac[5]);

    // Флаг ставим ДО смены режима: applyWiFi() из фоновой задачи
    // читает его и должен увидеть AP_STA, а не вернуть чистый STA.
    s_rescueAp.store(true);

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1),
                      IPAddress(192, 168, 4, 1),
                      IPAddress(255, 255, 255, 0));
    bool ok = WiFi.softAP(apSsid, WIFI_AP_PASSWORD, 6, 0, 4);

    if (!ok) {
        s_rescueAp.store(false);
        Serial.println(F("[NET] Rescue AP: FAILED to start"));
        return;
    }

    Serial.println(F("═══════════════════════════════════════════"));
    Serial.println(F("[NET] Could not connect to the home network."));
    Serial.printf ("[NET] Rescue access point is up: \"%s\"\n", apSsid);
    Serial.printf ("[NET] Password: %s\n", WIFI_AP_PASSWORD);
    Serial.println(F("[NET] Panel: http://192.168.4.1  ->  Settings -> Network"));
    Serial.println(F("[NET] Retries to the home network continue;"));
    Serial.println(F("[NET] the AP shuts down as soon as they succeed."));
    Serial.println(F("═══════════════════════════════════════════"));
}

void NetMgr::stopRescueAp() {
    if (!s_rescueAp.load()) return;
    s_rescueAp.store(false);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    Serial.println(F("[NET] Home network is up - rescue AP disabled"));
}

bool NetMgr::isRescueApActive() {
    return s_rescueAp.load();
}

const NetCache& NetMgr::cache() {
    return s_cache;
}

const char* NetMgr::wsToken() {
    return s_wsToken;
}

// ═══════════════════════════════════════════════════════════════════
//  NTP: ESP_SNTP API
// ═══════════════════════════════════════════════════════════════════

void NetMgr::initNtp() {
    // Остановим если уже работает (после reinit)
    if (sntp_enabled()) {
        sntp_stop();
    }

    sntp_setoperatingmode(SNTP_OPMODE_POLL);

    // Основной и резервный серверы
    sntp_setservername(0, g_settings.ntpServer[0]
        ? g_settings.ntpServer
        : (char*)NTP_SERVER_PRIMARY);
    sntp_setservername(1, (char*)NTP_SERVER_SECONDARY);

    // Коллбэк при успешной синхронизации
    sntp_set_time_sync_notification_cb(sntpSyncCallback);

    // Режим умеренной подстройки (не делать резких скачков часов)
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);

    // POSIX TZ для localtime_r (переход на летнее/зимнее автоматически,
    // если в TZ прописаны правила — для "MSK-3" DST отсутствует).
    const char* tz = g_settings.ntpTzString[0]
        ? g_settings.ntpTzString
        : NTP_TZ_STRING;
    setenv("TZ", tz, 1);
    tzset();
    Serial.printf("[NTP] TZ set to \"%s\"\n", tz);

    // Запускать sntp будет WiFi event при получении IP
    // (см. onWiFiEvent ARDUINO_EVENT_WIFI_STA_GOT_IP)
    // Если мы уже подключены (повторный reinit) — запустить сразу.
    if (s_wifiConnected.load()) {
        sntp_init();
        Serial.println(F("[NTP] SNTP started (already connected)"));
    }

    Serial.printf("[NTP] Initialized: server=%s, TZ=%s\n",
        g_settings.ntpServer, tz);
}

void NetMgr::reinitNtp() {
    // Сохраняем текущее s_timeSet — не сбрасываем при reinit
    // (пользователь мог просто поменять сервер, а время уже знаем).
    initNtp();
    Serial.println(F("[NTP] Reinit complete"));
}

bool NetMgr::isTimeSet() {
    return s_timeSet.load();
}

void NetMgr::setOnFirstTimeSet(void (*cb)()) {
    s_onFirstTimeSet = cb;
    // Если синхронизация уже была — вызываем cb сразу
    if (s_firstSyncFired.load() && cb && !s_userCbInvoked.exchange(true)) {
        s_onFirstTimeSet = nullptr;
        cb();
    }
}

// ═══════════════════════════════════════════════════════════════════
//  ГЕТТЕРЫ ВРЕМЕНИ
// ═══════════════════════════════════════════════════════════════════
// localtime_r потокобезопасен (в отличие от localtime) —
// можно вызывать из любого ядра/задачи без мьютекса.

static void readLocalTime(struct tm* t) {
    time_t now;
    time(&now);
    localtime_r(&now, t);
}

int NetMgr::getHour() {
    if (!s_timeSet.load()) return -1;
    struct tm t;
    readLocalTime(&t);
    return t.tm_hour;
}

int NetMgr::getMinute() {
    if (!s_timeSet.load()) return -1;
    struct tm t;
    readLocalTime(&t);
    return t.tm_min;
}

int NetMgr::getSecond() {
    if (!s_timeSet.load()) return 0;
    struct tm t;
    readLocalTime(&t);
    return t.tm_sec;
}

const char* NetMgr::getFormattedTime() {
    if (!s_timeSet.load()) {
        strlcpy(s_timeBuf, "--:--:--", sizeof(s_timeBuf));
        return s_timeBuf;
    }
    struct tm t;
    readLocalTime(&t);
    snprintf(s_timeBuf, sizeof(s_timeBuf), "%02d:%02d:%02d",
        t.tm_hour, t.tm_min, t.tm_sec);
    return s_timeBuf;
}

// ═══════════════════════════════════════════════════════════════════
//  СОХРАНЕНИЕ НАСТРОЕК
// ═══════════════════════════════════════════════════════════════════
bool NetMgr::saveNetSettings(const NetCache& nc, bool savePass) {
    // v6.1: begin() возвращает false, если объект Preferences уже открыт —
    // и тогда КАЖДАЯ последующая запись молча проваливается, потому что
    // putString() при _readOnly просто возвращает 0. Проверяем.
    if (!s_prefs.begin("net3", false)) {
        Serial.println(F("[NET] !!! NVS begin(net3) FAILED — settings NOT saved !!!"));
        return false;
    }

    // putString() возвращает strlen(value) при успехе и 0 при ошибке.
    // Для пустой строки успех — это тоже 0, поэтому сравниваем с длиной,
    // а не с нулём: иначе очистка SSID выглядела бы как сбой записи.
    auto put = [&](const char* key, const char* val) -> bool {
        bool ok = (s_prefs.putString(key, val) == strlen(val));
        if (!ok) Serial.printf("[NET] !!! NVS write failed: %s\n", key);
        return ok;
    };

    s_prefs.putBool("dhcp", nc.isDHCP);
    bool ok = put("ssid", nc.ssid);
    if (savePass) ok &= put("pass", nc.pass);
    ok &= put("ip",   nc.ip);
    ok &= put("sub",  nc.subnet);
    ok &= put("gw",   nc.gateway);
    ok &= put("dns1", nc.dns1);
    ok &= put("dns2", nc.dns2);

    // Контрольное чтение SSID: от него зависит, уйдёт ли контроллер
    // в режим точки доступа после перезагрузки. Ошибиться тут — значит
    // отправить владельца к теплице с USB-кабелем.
    char back[sizeof(nc.ssid)] = {0};
    s_prefs.getString("ssid", back, sizeof(back));
    if (strcmp(back, nc.ssid) != 0) {
        ok = false;
        Serial.printf("[NET] !!! SSID readback MISMATCH: wrote \"%s\", read \"%s\"\n",
            nc.ssid, back);
    }
    s_prefs.end();
    if (!ok) Serial.println(F("[NET] !!! Network settings saved INCOMPLETELY !!!"));

    // Обновляем локальный кэш
    char savedPass[64];
    if (!savePass) strlcpy(savedPass, s_cache.pass, sizeof(savedPass));
    s_cache = nc;
    if (!savePass) strlcpy(s_cache.pass, savedPass, sizeof(s_cache.pass));
    Serial.println(F("[NET] Settings saved to NVS"));
    return ok;
}

// v6.1: отдельный путь очистки Wi-Fi. Записи пустой строки мало:
// если она по какой-то причине не доходит до флеша, контроллер после
// перезагрузки молча вернётся в ту же сеть, и снаружи это выглядит как
// неработающая кнопка. Поэтому после записи ключи ещё и удаляются,
// а результат проверяется чтением из NVS, а не по кэшу в RAM —
// кэш обновляется в любом случае и об ошибке записи не знает.
bool NetMgr::clearWifiCredentials() {
    NetCache nc = s_cache;
    nc.ssid[0] = 0;
    nc.pass[0] = 0;
    bool ok = saveNetSettings(nc, true);

    if (!s_prefs.begin("net3", false)) {
        Serial.println(F("[NET] !!! clearWifiCredentials: NVS begin FAILED !!!"));
        return false;
    }
    s_prefs.remove("ssid");
    s_prefs.remove("pass");

    char back[sizeof(nc.ssid)] = {0};
    s_prefs.getString("ssid", back, sizeof(back));
    s_prefs.end();

    if (back[0]) {
        Serial.printf("[NET] !!! WiFi credentials SURVIVED erase: SSID=\"%s\"\n", back);
        return false;
    }
    Serial.printf("[NET] WiFi credentials erased (write %s, keys removed)\n",
        ok ? "ok" : "FAILED");
    return true;
}

void NetMgr::saveWebCreds(const char* user, const char* pass) {
    strlcpy(s_cache.webUser, user, sizeof(s_cache.webUser));
    strlcpy(s_cache.webPass, pass, sizeof(s_cache.webPass));
    s_prefs.begin("net3", false);
    s_prefs.putString("wusr", user);
    s_prefs.putString("wpas", pass);
    s_prefs.end();
    rebuildToken();
    Serial.println(F("[NET] Web credentials saved"));
}
