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
GH_REQUIRE_SIGNED=1 — без ключа сборка не идёт вовсе (для выпуска).

Ключ ЕСТЬ, а публичный из него не получен (нет openssl, битый файл), или
GH_OTA_KEY указывает на несуществующий файл — сборка прошивки останавливается: молча собрать прошивку без проверки подписи
там, где владелец явно положил ключ, хуже, чем не собрать её.
"""

import os
import shutil
import subprocess

Import("env")  # noqa: F821  — внедряется PlatformIO

BUILD_DIR = env.subst("$BUILD_DIR")  # noqa: F821
GEN_DIR = os.path.join(BUILD_DIR, "generated")
HEADER = os.path.join(GEN_DIR, "ota_pubkey.h")


# Цели, которые прошивку не собирают: на них отказ по ключу только
# предупреждение, иначе без openssl перестали бы работать erase и uploadfs.
NO_FIRMWARE_TARGETS = {"clean", "erase", "buildfs", "uploadfs", "uploadfsota", "nobuild",
                       "monitor", "envdump", "idedata", "__idedata", "compiledb", "menuconfig"}


def key_path():
    """(путь, задан_явно). Явно заданный GH_OTA_KEY без файла — ошибка, а не «без ключа»."""
    explicit = os.environ.get("GH_OTA_KEY")
    p = explicit or os.path.join(os.path.expanduser("~"), ".greenhouse", "ota_signing_key.pem")
    return (p if os.path.isfile(p) else None), bool(explicit)


def openssl_exe():
    exe = os.environ.get("GH_OPENSSL") or shutil.which("openssl")
    if exe:
        return exe
    # PlatformIO IDE часто запускается без mingw64\bin в PATH, хотя Git для
    # Windows с openssl установлен.
    for base in (os.environ.get("ProgramFiles"), os.environ.get("ProgramW6432")):
        for sub in (("Git", "mingw64", "bin"), ("Git", "usr", "bin")):
            cand = os.path.join(base or "", *sub, "openssl.exe")
            if base and os.path.isfile(cand):
                return cand
    return None


def public_pem(key):
    exe = openssl_exe()
    if not exe:
        print("[ota-key] openssl не найден (PATH, GH_OPENSSL, Git для Windows)")
        return None
    # Упаковщик контейнера (build_container.py → pack_firmware.py) наследует
    # окружение и подпишет тем же openssl.
    os.environ["GH_OPENSSL"] = exe
    try:
        r = subprocess.run([exe, "pkey", "-in", key, "-pubout"], capture_output=True, text=True)
    except OSError as e:
        print("[ota-key] openssl не запустился (%s): %s" % (exe, e))
        return None
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


def fail(msg):
    print("")
    targets = set(COMMAND_LINE_TARGETS)  # noqa: F821
    if env.GetOption("clean"):  # noqa: F821 — «pio run -t clean» идёт как scons -c, без цели
        targets = {"clean"}
    if targets and targets <= NO_FIRMWARE_TARGETS:
        print("[ota-key] !!! %s (цель %s прошивку не собирает — продолжаю)" % (msg, ", ".join(sorted(targets))))
        print("")
        return True
    print("[ota-key] !!! %s — сборка остановлена" % msg)
    print("")
    env.Exit(1)  # noqa: F821
    return False


def main():
    key, explicit = key_path()
    pem = public_pem(key) if key else None
    problem = None
    if key and not pem:
        problem = "ключ подписи есть (%s), но публичный ключ из него не получен" % key
    elif not key and explicit:
        problem = "GH_OTA_KEY=%s — файла нет" % os.environ.get("GH_OTA_KEY")
    elif not key and os.environ.get("GH_REQUIRE_SIGNED") == "1":
        problem = "GH_REQUIRE_SIGNED=1, а ключ подписи не найден"
    if problem:
        # Цель без сборки прошивки: заголовок не трогаем (иначе следующая
        # сборка пересобрала бы прошивку зря) и «ключ не найден» не печатаем.
        fail(problem)
        env.Append(CPPPATH=[GEN_DIR])  # noqa: F821
        return
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
