#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ota_pubkey.py — pre-действие PlatformIO: встраивает в прошивку публичный
ключ проверки подписи контейнера обновления.

Подключается из platformio.ini строкой
    extra_scripts = pre:tools/ota_pubkey.py

Ключ подписи (закрытый, ECDSA P-256) лежит ВНЕ репозитория:
    переменная окружения GH_OTA_KEY — путь к PEM-файлу,
    иначе ~/.greenhouse/ota_signing_key.pem
Создать:  openssl ecparam -name prime256v1 -genkey -noout -out <путь>

Скрипт получает из него публичный ключ (openssl pkey -pubout) и пишет
$BUILD_DIR/generated/ota_pubkey.h с макросом OTA_PUBKEY_PEM. Прошивка с
этим макросом принимает ТОЛЬКО подписанные контейнеры (формат v2).

Ключа нет — заголовок пустой, сборка «без подписи»: принимает и
неподписанные контейнеры, как раньше. Об этом печатается предупреждение.
"""

import os
import shutil
import subprocess

Import("env")  # noqa: F821  — внедряется PlatformIO

BUILD_DIR = env.subst("$BUILD_DIR")  # noqa: F821
GEN_DIR = os.path.join(BUILD_DIR, "generated")
HEADER = os.path.join(GEN_DIR, "ota_pubkey.h")


def key_path():
    p = os.environ.get("GH_OTA_KEY") or os.path.join(
        os.path.expanduser("~"), ".greenhouse", "ota_signing_key.pem")
    return p if os.path.isfile(p) else None


def openssl_exe():
    return os.environ.get("GH_OPENSSL") or shutil.which("openssl")


def public_pem(key):
    exe = openssl_exe()
    if not exe:
        print("[ota-key] openssl не найден (PATH или GH_OPENSSL) — сборка БЕЗ проверки подписи")
        return None
    r = subprocess.run([exe, "pkey", "-in", key, "-pubout"], capture_output=True, text=True)
    if r.returncode != 0 or "BEGIN PUBLIC KEY" not in r.stdout:
        print("[ota-key] не удалось получить публичный ключ: %s" % (r.stderr or r.stdout).strip())
        return None
    return r.stdout.strip()


def render(pem):
    lines = ["// Сгенерировано tools/ota_pubkey.py — не править и не коммитить.", "#pragma once"]
    if pem:
        body = "\\n".join(pem.splitlines())
        lines.append('#define OTA_PUBKEY_PEM "%s\\n"' % body)
    return "\n".join(lines) + "\n"


def main():
    key = key_path()
    pem = public_pem(key) if key else None
    if pem:
        print("[ota-key] прошивка будет принимать только подписанные контейнеры (ключ: %s)" % key)
    else:
        print("")
        print("[ota-key] !!! КЛЮЧ ПОДПИСИ НЕ НАЙДЕН — прошивка примет и НЕПОДПИСАННЫЕ обновления.")
        print("[ota-key] !!! Для выпуска: GH_OTA_KEY=<путь> или ~/.greenhouse/ota_signing_key.pem")
        print("")
    text = render(pem)
    os.makedirs(GEN_DIR, exist_ok=True)
    old = None
    if os.path.isfile(HEADER):
        with open(HEADER, encoding="utf-8") as f:
            old = f.read()
    if old != text:   # переписываем только при изменении — иначе пересборка каждый раз
        with open(HEADER, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    env.Append(CPPPATH=[GEN_DIR])  # noqa: F821


main()
