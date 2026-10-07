#pragma once
// ╔══════════════════════════════════════════════════════════════════╗
// ║  ws_handler.h — WebSocket обработчик для веб-панели v5.0        ║
// ║                                                                  ║
// ║  Отвечает за:                                                    ║
// ║   • Broadcast состояния каждую WS_BROADCAST_MS (1 сек)           ║
// ║   • Обработка команд от клиента (set_relay, vent_position, ...)  ║
// ║   • Уведомления о событиях (notifyXxx из других модулей)        ║
// ║   • Аутентификация через HTTP Basic Auth токен                   ║
// ╚══════════════════════════════════════════════════════════════════╝

#include "config.h"
#include <stdint.h>

class AsyncWebServer;  // forward declaration

namespace WsHandler {

    // Инициализация. Создаёт AsyncWebSocket handler.
    // Вызывать ПОСЛЕ NetMgr::init() (нужен wsToken).
    void init();

    // Регистрация в AsyncWebServer. Вызывается из WebRoutes::init().
    void attachTo(AsyncWebServer& server);

    // ═══════════════════════════════════════════════════════════════
    //  УВЕДОМЛЕНИЯ ИЗ ДРУГИХ МОДУЛЕЙ
    // ═══════════════════════════════════════════════════════════════
    // v6.1: контроллер больше НЕ рассылает состояние сам — AsyncWebSocket
    // не потокобезопасен, и обращение к нему из taskCore1 приводило к
    // use-after-free при отключении клиента (подробности в ws_handler.cpp).
    // Теперь состояние отдаётся в ответ на {"cmd":"get_state"}, который
    // веб-панель шлёт раз в секунду, — и обрабатывается в задаче AsyncTCP.
    //
    // Функции ниже сохранены, чтобы не переписывать вызовы в 6 модулях,
    // но они намеренно пустые: изменение доедет до UI ближайшим опросом.

    // Изменение состояния реле (вызывается из RelayMgr::setRelay)
    void notifyRelayChange(uint8_t relayIdx);

    // Изменение позиции форточки (вызывается из VentCtrl)
    void notifyVentPosition(uint8_t ventIdx);

    // Изменение режимов (auto/manual)
    void notifyModes();

}  // namespace WsHandler
