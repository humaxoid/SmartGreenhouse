"""Проверки приёмника: py -m pytest tools/telemetry_server (нужны fastapi, httpx, pytest)."""
import base64
import os
import sys
import tempfile

import pytest

_tmp = tempfile.mkdtemp()
os.environ["GH_DB"] = os.path.join(_tmp, "t.db")
os.environ["GH_TRUST_PROXY"] = "1"
sys.path.insert(0, os.path.dirname(__file__))
import app as appmod  # noqa: E402
from fastapi.testclient import TestClient  # noqa: E402

PASSWORD = "correct horse battery"
appmod.ADMIN_HASH = appmod.make_hash(PASSWORD, iterations=1000)
client = TestClient(appmod.app, base_url="http://testserver")

GOOD = {"id": "0123456789abcdef", "fw": "6.3", "inst": 1760000000, "up": 3600, "tz": "MSK-3",
        "rr": "POWERON", "hm": 98000, "mq": True, "lan": "192.168.1.42", "boot": True}


def basic(user, pwd):
    return {"Authorization": "Basic " + base64.b64encode(f"{user}:{pwd}".encode()).decode()}


def post(body, ip="203.0.113.7"):
    return client.post("/api/v1/hb", json=body, headers={"X-Real-IP": ip})


@pytest.fixture(autouse=True)
def clean():
    appmod.PROXY_PEERS = ("testclient",)   # как будто запрос пришёл от локального nginx
    with appmod.closing(appmod.db()) as con:
        con.execute("DELETE FROM devices")
        con.execute("DELETE FROM events")
        con.commit()
    appmod._fails.clear()
    yield


def kinds():
    with appmod.closing(appmod.db()) as con:
        return [r["kind"] for r in con.execute("SELECT kind FROM events ORDER BY id")]


def test_first_report_creates_device_with_external_ip():
    assert post(GOOD).status_code == 204
    r = client.get("/admin/api/devices", headers=basic("admin", PASSWORD)).json()
    assert r[0]["id"] == GOOD["id"] and r[0]["ip"] == "203.0.113.7" and r[0]["fw"] == "6.3"
    assert kinds() == ["new"]


def test_ip_change_is_recorded_and_current_ip_updates():
    post(GOOD)
    later = dict(GOOD, boot=False, up=7200)
    with appmod.closing(appmod.db()) as con:        # прошлый отчёт — час назад
        con.execute("UPDATE devices SET last_seen=last_seen-3600")
        con.commit()
    assert post(later, ip="198.51.100.9").status_code == 204
    assert "ip" in kinds()
    r = client.get("/admin/api/devices", headers=basic("admin", PASSWORD)).json()
    assert r[0]["ip"] == "198.51.100.9"


def test_reboot_detected_by_boot_flag_and_by_uptime_drop():
    post(GOOD)
    with appmod.closing(appmod.db()) as con:
        con.execute("UPDATE devices SET last_seen=last_seen-3600, uptime=90000")
        con.commit()
    post(dict(GOOD, boot=False, up=100, rr="BROWNOUT"))
    assert "reboot" in kinds()


def test_too_frequent_reports_are_refused_but_boot_is_accepted():
    post(GOOD)
    assert post(dict(GOOD, boot=False)).status_code == 429
    assert post(GOOD).status_code == 204


@pytest.mark.parametrize("bad", [
    dict(GOOD, id="../../etc/passwd"), dict(GOOD, id="0123456789ABCDEF"), dict(GOOD, fw="6.3<script>"),
    dict(GOOD, lan="1.2.3.4; DROP"), dict(GOOD, tz="x" * 60), dict(GOOD, up=-1), dict(GOOD, rr="po wer"),
    {"id": GOOD["id"]},
])
def test_malformed_reports_rejected(bad):
    assert post(bad).status_code == 422


def test_forwarded_header_ignored_when_peer_not_local():
    # Запрос пришёл НЕ от доверенного прокси — подмена X-Real-IP не действует
    appmod.PROXY_PEERS = ("127.0.0.1",)
    post(GOOD, ip="1.2.3.4")
    r = client.get("/admin/api/devices", headers=basic("admin", PASSWORD)).json()
    assert r[0]["ip"] == "testclient"


def test_admin_requires_password():
    assert client.get("/admin").status_code == 401
    assert client.get("/admin/api/devices").status_code == 401
    assert client.get("/admin", headers=basic("admin", "wrong")).status_code == 401
    assert client.get("/admin", headers=basic("admin", PASSWORD)).status_code == 200
    assert client.get("/api/v1/hb").status_code == 405


def test_admin_locks_out_after_five_failures():
    for _ in range(5):
        client.get("/admin", headers=basic("admin", "wrong"))
    assert client.get("/admin", headers=basic("admin", PASSWORD)).status_code == 429


def test_admin_empty_hash_denies_everyone():
    saved = appmod.ADMIN_HASH
    appmod.ADMIN_HASH = ""
    try:
        assert client.get("/admin", headers=basic("admin", "")).status_code == 401
    finally:
        appmod.ADMIN_HASH = saved


def test_device_page_escapes_html():
    post(GOOD)
    with appmod.closing(appmod.db()) as con:
        con.execute("UPDATE devices SET tz=?", ("<img src=x onerror=alert(1)>",))
        con.commit()
    page = client.get("/admin", headers=basic("admin", PASSWORD)).text
    assert "<img src=x" not in page and "&lt;img" in page
    assert client.get("/admin/device/zzzz", headers=basic("admin", PASSWORD)).status_code == 404
