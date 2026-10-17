"""Приёмник анонимной статистики контроллеров Greenhouse + админ-страница.

Запуск (см. README.md рядом):  uvicorn app:app --host 127.0.0.1 --port 8088
Хэш пароля админа:             py app.py hash

Что делает сервер:
  * принимает POST /api/v1/hb от контроллеров (раз в час и после загрузки);
  * внешний IP берёт из соединения (за nginx — из X-Real-IP, только от 127.0.0.1),
    страну определяет по базе GeoLite2 (если файл указан), сам контроллер их не шлёт;
  * хранит последнее состояние каждого устройства и историю изменений
    (новый IP, страна, версия, часовой пояс, перезагрузка);
  * отдаёт /admin под паролем — только владелец.
"""
from __future__ import annotations

import base64
import hashlib
import hmac
import html
import ipaddress
import os
import re
import secrets
import sqlite3
import sys
import threading
import time
from contextlib import closing
from typing import Optional

from fastapi import FastAPI, HTTPException, Request, Response
from fastapi.responses import HTMLResponse, JSONResponse
from pydantic import BaseModel, Field, field_validator

DB_PATH = os.environ.get("GH_DB", "telemetry.db")
GEOIP_PATH = os.environ.get("GH_GEOIP", "")          # GeoLite2-Country.mmdb, необязательно
ADMIN_USER = os.environ.get("GH_ADMIN_USER", "admin")
ADMIN_HASH = os.environ.get("GH_ADMIN_HASH", "")     # результат `py app.py hash`
TRUST_PROXY = os.environ.get("GH_TRUST_PROXY", "1") == "1"
MIN_INTERVAL_S = 300          # чаще раза в 5 минут от одного устройства не принимаем (кроме «boot»)
OFFLINE_AFTER_S = 2 * 3600 + 900   # нет сообщений больше 2 ч 15 мин — «нет связи»
KEEP_EVENTS_DAYS = 730
PROXY_PEERS = ("127.0.0.1", "::1")   # заголовку X-Real-IP верим только от своего nginx

app = FastAPI(title="Greenhouse telemetry", docs_url=None, redoc_url=None, openapi_url=None)
_lock = threading.Lock()
_geo = None


def _geo_reader():
    global _geo
    if _geo is None and GEOIP_PATH and os.path.exists(GEOIP_PATH):
        try:
            import geoip2.database  # type: ignore
            _geo = geoip2.database.Reader(GEOIP_PATH)
        except Exception:  # noqa: BLE001 — без базы страны просто «?»
            _geo = False
    return _geo or None


def country_of(ip: str) -> str:
    reader = _geo_reader()
    if not reader:
        return "?"
    try:
        return reader.country(ip).country.iso_code or "?"
    except Exception:  # noqa: BLE001 — адрес не найден в базе
        return "?"


# ──────────────────────────── база ────────────────────────────
def db() -> sqlite3.Connection:
    con = sqlite3.connect(DB_PATH, timeout=10)
    con.row_factory = sqlite3.Row
    return con


def init_db() -> None:
    with closing(db()) as con:
        con.executescript(
            """
            CREATE TABLE IF NOT EXISTS devices(
              id TEXT PRIMARY KEY, first_seen INTEGER, last_seen INTEGER,
              ip TEXT, country TEXT, fw TEXT, inst INTEGER, uptime INTEGER,
              tz TEXT, rr TEXT, hm INTEGER, mq INTEGER, lan TEXT, reports INTEGER DEFAULT 0);
            CREATE TABLE IF NOT EXISTS events(
              id INTEGER PRIMARY KEY AUTOINCREMENT, ts INTEGER, device TEXT,
              kind TEXT, old TEXT, new TEXT);
            CREATE INDEX IF NOT EXISTS ev_dev ON events(device, ts);
            """
        )
        con.commit()


init_db()


# ──────────────────────────── приём ────────────────────────────
class Report(BaseModel):
    id: str = Field(pattern=r"^[0-9a-f]{16}$")
    fw: str = Field(max_length=16, pattern=r"^[0-9A-Za-z._-]*$")
    inst: int = Field(ge=0, le=4102444800)
    up: int = Field(ge=0, le=10**9)
    tz: str = Field(max_length=48)
    rr: str = Field(max_length=16, pattern=r"^[A-Z_]*$")
    hm: int = Field(ge=0, le=10**8)
    mq: bool
    lan: str = Field(max_length=15, pattern=r"^[0-9.]*$")
    boot: bool = False

    @field_validator("tz")
    @classmethod
    def _tz(cls, v: str) -> str:
        if re.search(r"[^\x20-\x7e]", v):
            raise ValueError("tz")
        return v


def client_ip(request: Request) -> str:
    peer = request.client.host if request.client else ""
    if TRUST_PROXY and peer in PROXY_PEERS:
        real = request.headers.get("x-real-ip", "").strip()
        try:
            return str(ipaddress.ip_address(real))
        except ValueError:
            pass
    return peer


def _event(con: sqlite3.Connection, ts: int, dev: str, kind: str, old: object, new: object) -> None:
    con.execute("INSERT INTO events(ts,device,kind,old,new) VALUES(?,?,?,?,?)",
                (ts, dev, kind, "" if old is None else str(old), "" if new is None else str(new)))


@app.post("/api/v1/hb", status_code=204)
async def heartbeat(report: Report, request: Request) -> Response:
    if int(request.headers.get("content-length") or 0) > 2048:
        raise HTTPException(413)
    now = int(time.time())
    ip = client_ip(request)
    country = country_of(ip)
    with _lock, closing(db()) as con:
        old = con.execute("SELECT * FROM devices WHERE id=?", (report.id,)).fetchone()
        if old and not report.boot and now - old["last_seen"] < MIN_INTERVAL_S:
            raise HTTPException(429, "too frequent")
        if old is None:
            _event(con, now, report.id, "new", "", f"fw {report.fw}")
            con.execute(
                "INSERT INTO devices(id,first_seen,last_seen,ip,country,fw,inst,uptime,tz,rr,hm,mq,lan,reports)"
                " VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,1)",
                (report.id, now, now, ip, country, report.fw, report.inst, report.up, report.tz,
                 report.rr, report.hm, int(report.mq), report.lan))
        else:
            if old["ip"] != ip:
                _event(con, now, report.id, "ip", old["ip"], ip)
            if old["country"] != country and country != "?":
                _event(con, now, report.id, "country", old["country"], country)
            if old["fw"] != report.fw:
                _event(con, now, report.id, "fw", old["fw"], report.fw)
            if old["tz"] != report.tz:
                _event(con, now, report.id, "tz", old["tz"], report.tz)
            if report.boot or report.up < (old["uptime"] or 0):
                _event(con, now, report.id, "reboot", f"аптайм {old['uptime']} с", report.rr)
            if now - old["last_seen"] > OFFLINE_AFTER_S:
                _event(con, now, report.id, "online", f"молчал {(now - old['last_seen']) // 3600} ч", "")
            con.execute(
                "UPDATE devices SET last_seen=?,ip=?,country=?,fw=?,inst=?,uptime=?,tz=?,rr=?,hm=?,mq=?,lan=?,"
                "reports=reports+1 WHERE id=?",
                (now, ip, country if country != "?" else old["country"], report.fw,
                 report.inst or old["inst"], report.up, report.tz, report.rr, report.hm,
                 int(report.mq), report.lan, report.id))
        con.execute("DELETE FROM events WHERE ts < ?", (now - KEEP_EVENTS_DAYS * 86400,))
        con.commit()
    return Response(status_code=204)


# ──────────────────────────── админ ────────────────────────────
_fails: dict[str, list[float]] = {}
FAILS_MAX_IPS = 1000


def make_hash(password: str, iterations: int = 200_000) -> str:
    salt = secrets.token_bytes(16)
    dk = hashlib.pbkdf2_hmac("sha256", password.encode(), salt, iterations)
    return f"pbkdf2_sha256${iterations}${salt.hex()}${dk.hex()}"


def check_password(password: str) -> bool:
    try:
        algo, it, salt, want = ADMIN_HASH.split("$")
        if algo != "pbkdf2_sha256":
            return False
        dk = hashlib.pbkdf2_hmac("sha256", password.encode(), bytes.fromhex(salt), int(it))
        return hmac.compare_digest(dk.hex(), want)
    except (ValueError, TypeError):
        return False


def require_admin(request: Request) -> None:
    """Basic-авторизация; после 5 неудач за 5 минут с одного адреса — 429."""
    ip = client_ip(request)
    now = time.time()
    # Словарь не должен расти с каждым новым адресом: пустые списки не
    # храним, а когда адресов набирается много — выметаем устаревшие.
    if len(_fails) > FAILS_MAX_IPS:
        for k in [k for k, v in _fails.items() if not v or now - v[-1] >= 300]:
            del _fails[k]
        if len(_fails) > FAILS_MAX_IPS:   # свежих слишком много — оставляем новейшую половину
            keep = sorted(_fails.items(), key=lambda kv: kv[1][-1], reverse=True)[:FAILS_MAX_IPS // 2]
            _fails.clear()
            _fails.update(keep)
    recent = [t for t in _fails.get(ip, []) if now - t < 300]
    if recent:
        _fails[ip] = recent
    else:
        _fails.pop(ip, None)
    if len(recent) >= 5:
        raise HTTPException(429, "too many attempts")
    hdr = request.headers.get("authorization", "")
    ok = False
    if hdr.lower().startswith("basic ") and ADMIN_HASH:
        try:
            user, _, pwd = base64.b64decode(hdr[6:]).decode().partition(":")
            ok = hmac.compare_digest(user, ADMIN_USER) and check_password(pwd)
        except (ValueError, UnicodeDecodeError):
            ok = False
    if not ok:
        _fails.setdefault(ip, []).append(now)
        raise HTTPException(401, headers={"WWW-Authenticate": 'Basic realm="Greenhouse admin"'})


def fmt_ts(ts: Optional[int]) -> str:
    if not ts:
        return "—"
    return time.strftime("%d.%m.%Y %H:%M", time.localtime(ts))


def fmt_up(sec: Optional[int]) -> str:
    sec = int(sec or 0)
    return f"{sec // 86400} д {sec % 86400 // 3600:02d}:{sec % 3600 // 60:02d}"


SECURITY_HEADERS = {
    "Content-Security-Policy": "default-src 'none'; style-src 'unsafe-inline'; base-uri 'none'; form-action 'none'",
    "X-Content-Type-Options": "nosniff", "Referrer-Policy": "no-referrer", "Cache-Control": "no-store",
}
BAD_RESET = {"PANIC", "INT_WDT", "TASK_WDT", "WDT", "BROWNOUT"}
PAGE_CSS = """body{font:14px system-ui,sans-serif;margin:20px;color:#1a2e1a;background:#f0f4f0}
h1{font-size:20px}table{border-collapse:collapse;width:100%;background:#fff}
th,td{padding:6px 10px;border-bottom:1px solid #d4e0d4;text-align:left;white-space:nowrap}
th{background:#2d7035;color:#fff;position:sticky;top:0}tr:hover td{background:#f7faf7}
.tiles{display:flex;gap:12px;flex-wrap:wrap;margin:12px 0}.tile{background:#fff;border:1px solid #d4e0d4;border-radius:8px;padding:10px 16px}
.tile b{font-size:22px;display:block}.off{color:#c62828;font-weight:600}.on{color:#2e7d32;font-weight:600}.bad{color:#c62828;font-weight:600}
a{color:#1565c0}.wrap{overflow-x:auto}small{color:#666}"""


def e(v: object) -> str:
    return html.escape("" if v is None else str(v), quote=True)


@app.get("/admin", response_class=HTMLResponse)
async def admin_index(request: Request) -> HTMLResponse:
    require_admin(request)
    now = int(time.time())
    with closing(db()) as con:
        rows = con.execute("SELECT * FROM devices ORDER BY last_seen DESC").fetchall()
    online = [r for r in rows if now - r["last_seen"] <= OFFLINE_AFTER_S]
    by_country: dict[str, int] = {}
    by_fw: dict[str, int] = {}
    for r in rows:
        by_country[r["country"] or "?"] = by_country.get(r["country"] or "?", 0) + 1
        by_fw[r["fw"]] = by_fw.get(r["fw"], 0) + 1
    tiles = (f"<div class=tile><b>{len(rows)}</b>устройств</div><div class=tile><b>{len(online)}</b>на связи</div>"
             f"<div class=tile>страны: {e(', '.join(f'{k} {v}' for k, v in sorted(by_country.items())))}</div>"
             f"<div class=tile>версии: {e(', '.join(f'{k} ×{v}' for k, v in sorted(by_fw.items())))}</div>")
    body = []
    for r in rows:
        up = now - r["last_seen"] <= OFFLINE_AFTER_S
        rr = r["rr"] or ""
        body.append(
            f"<tr><td><a href='/admin/device/{e(r['id'])}'>{e(r['id'])}</a></td>"
            f"<td class={'on' if up else 'off'}>{'на связи' if up else 'нет связи'}</td>"
            f"<td>{e(r['country'])}</td><td>{e(r['ip'])}</td><td>{e(r['fw'])}</td>"
            f"<td>{e(fmt_ts(r['inst']))}</td><td>{e(fmt_up(r['uptime']))}</td><td>{e(r['tz'])}</td>"
            f"<td class={'bad' if rr in BAD_RESET else ''}>{e(rr)}</td><td>{e(round((r['hm'] or 0) / 1024))} КБ</td>"
            f"<td>{'да' if r['mq'] else 'нет'}</td><td>{e(r['lan'])}</td><td>{e(fmt_ts(r['last_seen']))}</td></tr>")
    page = (f"<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width'>"
            f"<title>Контроллеры</title><style>{PAGE_CSS}</style><h1>Контроллеры теплиц</h1><div class=tiles>{tiles}</div>"
            "<div class=wrap><table><tr><th>ID<th>Состояние<th>Страна<th>Внешний IP (текущий)<th>Версия<th>Установлен"
            "<th>Время работы<th>Часовой пояс<th>Посл. запуск<th>Мин. память<th>MQTT<th>Адрес в сети<th>Последний отчёт</tr>"
            f"{''.join(body)}</table></div><p><small>Обновлено {e(fmt_ts(now))}. «Нет связи» — отчётов нет более 2 ч 15 мин.</small></p>")
    return HTMLResponse(page, headers=SECURITY_HEADERS)


@app.get("/admin/device/{dev}", response_class=HTMLResponse)
async def admin_device(dev: str, request: Request) -> HTMLResponse:
    require_admin(request)
    if not re.fullmatch(r"[0-9a-f]{16}", dev):
        raise HTTPException(404)
    labels = {"new": "Первое появление", "ip": "Смена внешнего IP", "country": "Смена страны",
              "fw": "Смена версии", "tz": "Смена часового пояса", "reboot": "Перезапуск", "online": "Снова на связи"}
    with closing(db()) as con:
        d = con.execute("SELECT * FROM devices WHERE id=?", (dev,)).fetchone()
        evs = con.execute("SELECT * FROM events WHERE device=? ORDER BY ts DESC LIMIT 500", (dev,)).fetchall()
    if d is None:
        raise HTTPException(404)
    rows = "".join(
        f"<tr><td>{e(fmt_ts(x['ts']))}</td><td>{e(labels.get(x['kind'], x['kind']))}</td>"
        f"<td>{e(x['old'])}</td><td>{e(x['new'])}</td></tr>" for x in evs)
    page = (f"<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width'>"
            f"<title>{e(dev)}</title><style>{PAGE_CSS}</style><p><a href='/admin'>← все контроллеры</a></p>"
            f"<h1>Устройство {e(dev)}</h1><p>Страна {e(d['country'])}, внешний IP сейчас <b>{e(d['ip'])}</b>, версия {e(d['fw'])}, "
            f"установлен {e(fmt_ts(d['inst']))}, первый отчёт {e(fmt_ts(d['first_seen']))}, отчётов {e(d['reports'])}.</p>"
            f"<div class=wrap><table><tr><th>Когда<th>Событие<th>Было<th>Стало</tr>{rows}</table></div>")
    return HTMLResponse(page, headers=SECURITY_HEADERS)


@app.get("/admin/api/devices")
async def admin_api(request: Request) -> JSONResponse:
    require_admin(request)
    with closing(db()) as con:
        rows = [dict(r) for r in con.execute("SELECT * FROM devices ORDER BY last_seen DESC")]
    return JSONResponse(rows, headers=SECURITY_HEADERS)


@app.get("/healthz")
async def healthz() -> dict:
    return {"ok": True}


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "hash":
        import getpass
        pw = getpass.getpass("Пароль админа: ")
        if len(pw) < 12:
            sys.exit("Пароль короче 12 символов")
        print(make_hash(pw))
    else:
        sys.exit("py app.py hash — получить хэш пароля; запуск сервера: uvicorn app:app")
