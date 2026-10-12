#pragma once
// ╔══════════════════════════════════════════════════════════════════╗
// ║                                                                  ║
// ║      (прошивка + файловая система), HTTP Basic Auth              ║
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

    // Вызывать до OtaMgr::attachTo и до connectSTA().
    void init();

    // Вызывается из WebRoutes::init().
    void attachTo(AsyncWebServer& server);

    // Периодический вызов из основного loop().
    void loop();

    // должны избегать тяжёлых операций.
    bool isUpdating();

}  // namespace OtaMgr
