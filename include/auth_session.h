#pragma once
// ╔══════════════════════════════════════════════════════════════════╗
// ║  auth_session.h — сессии веб-панели                              ║
// ║                                                                  ║
// ║  Панель отдаёт логин и пароль ОДИН раз (POST /api/login, Basic)  ║
// ║  и получает случайный токен. Дальше REST идёт с заголовком       ║
// ║  `Authorization: Bearer <токен>`, веб-сокет авторизуется им же.  ║
// ║  Раньше пароль лежал в sessionStorage браузера, и XSS в панели   ║
// ║  означал его кражу — а он же был паролем ArduinoOTA.             ║
// ║  Сессии живут в RAM: после перезагрузки контроллера — вход       ║
// ║  заново. Смена пароля сбрасывает все сессии.                     ║
// ║                                                                  ║
// ║  Basic-авторизация по-прежнему принимается (страница             ║
// ║  восстановления и ручные запросы).                               ║
// ╚══════════════════════════════════════════════════════════════════╝

#include <stddef.h>
#include <stdint.h>

class AsyncWebServerRequest;

namespace AuthSess {

    // Длина токена в hex-символах (128 бит) и размер буфера под него.
    constexpr size_t TOKEN_HEX_LEN = 32;
    constexpr size_t TOKEN_BUF     = TOKEN_HEX_LEN + 1;

    // Новая сессия. При заполненной таблице вытесняется самая давняя.
    // out — буфер не короче TOKEN_BUF. Возвращает false, если не вышло.
    bool create(char* out, size_t outSize);

    // Токен действителен: есть в таблице и не простаивал дольше срока.
    // Успешная проверка продлевает сессию.
    bool valid(const char* token);

    void revoke(const char* token);
    void revokeAll();

    // Запрос авторизован: заголовок Bearer с живым токеном ИЛИ Basic
    // с верными логином и паролем.
    bool authorized(AsyncWebServerRequest* req);

    // Токен из заголовка `Authorization: Bearer ...`; nullptr — его нет.
    // Указатель живёт, пока жив запрос.
    const char* bearerOf(AsyncWebServerRequest* req);

}  // namespace AuthSess
