// ╔══════════════════════════════════════════════════════════════════╗
// ║  auth_session.cpp — сессии веб-панели (см. auth_session.h)       ║
// ╚══════════════════════════════════════════════════════════════════╝

#include "auth_session.h"
#include "network_manager.h"
#include "config.h"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <string.h>

#define SESS_SLOTS        8
#define SESS_IDLE_MS      (24ULL * 3600ULL * 1000ULL)   // сутки без запросов — сессия истекла

struct Session {
    char     token[AuthSess::TOKEN_BUF];   // "" — слот свободен
    uint64_t lastUsedMs;
};
static Session s_sess[SESS_SLOTS] = {};

// Все обращения идут из задачи AsyncTCP, но смена пароля и сброс сессий
// короткие — защищаем их спинлоком, чтобы не зависеть от этого допущения.
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

// Сравнение за время, не зависящее от места первого несовпадения.
static bool tokenEq(const char* a, const char* b) {
    uint8_t diff = 0;
    for (size_t i = 0; i < AuthSess::TOKEN_HEX_LEN; i++) {
        diff |= (uint8_t)(a[i] ^ b[i]);
        if (!a[i] || !b[i]) return false;   // длины проверены до вызова; страховка
    }
    return diff == 0;
}

static bool wellFormed(const char* t) {
    if (!t || strlen(t) != AuthSess::TOKEN_HEX_LEN) return false;
    for (size_t i = 0; i < AuthSess::TOKEN_HEX_LEN; i++) {
        const char c = t[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

bool AuthSess::create(char* out, size_t outSize) {
    if (!out || outSize < TOKEN_BUF) return false;
    // esp_fill_random — аппаратный ГСЧ; при работающем Wi-Fi он криптостойкий.
    uint8_t raw[TOKEN_HEX_LEN / 2];
    esp_fill_random(raw, sizeof(raw));
    for (size_t i = 0; i < sizeof(raw); i++) snprintf(out + i * 2, 3, "%02x", raw[i]);

    const uint64_t now = steadyMs();
    portENTER_CRITICAL(&s_mux);
    Session* slot = &s_sess[0];
    for (uint8_t i = 0; i < SESS_SLOTS; i++) {
        if (!s_sess[i].token[0]) { slot = &s_sess[i]; break; }
        if (s_sess[i].lastUsedMs < slot->lastUsedMs) slot = &s_sess[i];
    }
    memcpy(slot->token, out, TOKEN_BUF);
    slot->lastUsedMs = now;
    portEXIT_CRITICAL(&s_mux);
    return true;
}

bool AuthSess::valid(const char* token) {
    if (!wellFormed(token)) return false;
    const uint64_t now = steadyMs();
    bool ok = false;
    portENTER_CRITICAL(&s_mux);
    for (uint8_t i = 0; i < SESS_SLOTS; i++) {
        Session& s = s_sess[i];
        if (!s.token[0]) continue;
        if (now - s.lastUsedMs > SESS_IDLE_MS) { s.token[0] = '\0'; continue; }
        if (tokenEq(s.token, token)) { s.lastUsedMs = now; ok = true; }
    }
    portEXIT_CRITICAL(&s_mux);
    return ok;
}

void AuthSess::revoke(const char* token) {
    if (!wellFormed(token)) return;
    portENTER_CRITICAL(&s_mux);
    for (uint8_t i = 0; i < SESS_SLOTS; i++) {
        if (s_sess[i].token[0] && tokenEq(s_sess[i].token, token)) s_sess[i].token[0] = '\0';
    }
    portEXIT_CRITICAL(&s_mux);
}

void AuthSess::revokeAll() {
    portENTER_CRITICAL(&s_mux);
    for (uint8_t i = 0; i < SESS_SLOTS; i++) s_sess[i].token[0] = '\0';
    portEXIT_CRITICAL(&s_mux);
}

const char* AuthSess::bearerOf(AsyncWebServerRequest* req) {
    const AsyncWebHeader* h = req->getHeader("Authorization");
    if (!h) return nullptr;
    const char* v = h->value().c_str();
    if (strncmp(v, "Bearer ", 7) != 0) return nullptr;
    return v + 7;
}

bool AuthSess::authorized(AsyncWebServerRequest* req) {
    const char* bearer = bearerOf(req);
    if (bearer) return valid(bearer);
    const NetCache& nc = NetMgr::cache();
    return req->authenticate(nc.webUser, nc.webPass);
}
