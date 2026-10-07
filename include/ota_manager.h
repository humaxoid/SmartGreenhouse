#pragma once
// ╔══════════════════════════════════════════════════════════════════╗
// ║  ota_manager.h — OTA обновления прошивки v5.0                   ║
// ║                                                                  ║
// ║  Два канала обновления:                                          ║
// ║   1. Веб-OTA: POST /update, контейнер greenhouse_vX.Y.bin        ║
// ║      (прошивка + файловая система), HTTP Basic Auth              ║
// ║   2. ArduinoOTA: UDP + mDNS, только прошивка.                    ║
// ║      v6.1: пароль = пароль веб-панели (NetMgr::cache().webPass). ║
// ║      Раньше здесь было написано «пароль в платформио», хотя      ║
// ║      setPassword() не вызывался вообще — канал был открыт.       ║
// ║                                                                  ║
// ║  Обновление:                                                     ║
// ║   • Firmware (app) — двойная партиция, безопасно                ║
// ║   • LittleFS (data) — однократная запись, требует осторожности ║
// ║                                                                  ║
// ║  Перед началом — graceful shutdown (реле OFF, позиция в NVS).   ║
// ╚══════════════════════════════════════════════════════════════════╝

class AsyncWebServer;

namespace OtaMgr {

    // Инициализация. Настраивает ArduinoOTA (не стартует его).
    // Вызывать до OtaMgr::attachTo и до connectSTA().
    void init();

    // Регистрация HTTP маршрутов для веб-OTA.
    // Вызывается из WebRoutes::init().
    void attachTo(AsyncWebServer& server);

    // Периодический вызов из основного loop().
    // Обрабатывает ArduinoOTA запросы (mDNS, handshake).
    void loop();

    // Идёт ли сейчас обновление? Если true, внешние модули
    // должны избегать тяжёлых операций.
    bool isUpdating();

}  // namespace OtaMgr
