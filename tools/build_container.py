#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
build_container.py — post-действие PlatformIO.

Подключается из platformio.ini строкой
    extra_scripts = post:tools/build_container.py

Что делает после сборки прошивки:
  1. Читает FW_VERSION_MAJOR / FW_VERSION_MINOR из include/config.h.
  2. Если рядом лежит собранный образ файловой системы — вызывает
     tools/pack_firmware.py и кладёт greenhouse_vX.Y.bin в корень проекта.

Зачем: раньше версию контейнера набирали руками в аргументах
pack_firmware.py, и она разъезжалась с config.h. Теперь источник
версии ровно один.

Образ LittleFS собирается отдельной целью, поэтому порядок такой:
    PlatformIO → Build Filesystem Image
    PlatformIO → Build
После второго шага контейнер появляется автоматически.
"""

import os
import re
import subprocess
import sys

Import("env")  # noqa: F821  — внедряется PlatformIO


PROJECT_DIR = env.subst("$PROJECT_DIR")          # noqa: F821
BUILD_DIR = env.subst("$BUILD_DIR")              # noqa: F821
CONFIG_H = os.path.join(PROJECT_DIR, "include", "config.h")
PACKER = os.path.join(PROJECT_DIR, "tools", "pack_firmware.py")


def read_version():
    """Достаёт (major, minor) из config.h. None, если не нашли."""
    try:
        with open(CONFIG_H, encoding="utf-8") as f:
            text = f.read()
    except OSError as exc:
        print("[container] не читается config.h: %s" % exc)
        return None

    def grab(name):
        m = re.search(r"^\s*#define\s+%s\s+(\d+)" % name, text, re.M)
        return int(m.group(1)) if m else None

    major, minor = grab("FW_VERSION_MAJOR"), grab("FW_VERSION_MINOR")
    if major is None or minor is None:
        print("[container] в config.h не найдены FW_VERSION_MAJOR/MINOR")
        return None
    return major, minor


def sign_key_path():
    """Закрытый ключ подписи вне репозитория (см. tools/ota_pubkey.py)."""
    p = os.environ.get("GH_OTA_KEY") or os.path.join(
        os.path.expanduser("~"), ".greenhouse", "ota_signing_key.pem")
    return p if os.path.isfile(p) else None


def find_fs_image():
    """Образ файловой системы, если он уже собран."""
    for name in ("littlefs.bin", "spiffs.bin"):
        path = os.path.join(BUILD_DIR, name)
        if os.path.isfile(path):
            return path
    return None


def build_container(source, target, env):  # noqa: ARG001 — сигнатура SCons
    firmware = os.path.join(BUILD_DIR, "firmware.bin")
    if not os.path.isfile(firmware):
        return

    version = read_version()
    if version is None:
        return
    major, minor = version

    fs_image = find_fs_image()
    if fs_image is None:
        print("")
        print("[container] greenhouse_v%d.%d.bin НЕ собран: нет образа "
              "файловой системы." % (major, minor))
        print("[container] Сначала выполните PlatformIO -> Build Filesystem "
              "Image, затем пересоберите проект.")
        print("")
        return

    out_name = "greenhouse_v%d.%d.bin" % (major, minor)
    out_path = os.path.join(PROJECT_DIR, out_name)

    cmd = [sys.executable, PACKER,
           "--fw", firmware,
           "--fs", fs_image,
           "--major", str(major),
           "--minor", str(minor),
           "--out", out_path]
    # Подписанный контейнер (формат 2), если есть ключ (тот же, что встраивает
    # tools/ota_pubkey.py). GH_CONTAINER_V1=1 — неподписанный формат 1: нужен один
    # раз, чтобы перевести устройство с прошивки, которая формат 2 ещё не знает.
    key = sign_key_path()
    if not key and os.environ.get("GH_OTA_KEY"):
        # Ключ указан явно, но файла нет: неподписанный контейнер под тем же
        # именем легко принять за выпускной. Старый файл, если есть, — прежний.
        print("[container] GH_OTA_KEY=%s — файла нет, контейнер НЕ собран "
              "(%s в корне, если есть, — от прошлой сборки)" % (os.environ["GH_OTA_KEY"], out_name))
        return
    if key and os.environ.get("GH_CONTAINER_V1") != "1":
        cmd += ["--sign-key", key]
    elif key:
        print("[container] GH_CONTAINER_V1=1 — собираю НЕПОДПИСАННЫЙ контейнер формата 1")
    result = subprocess.run(cmd, capture_output=True, text=True)

    print("")
    if result.returncode == 0:
        print("[container] собран %s" % out_name)
        for line in result.stdout.strip().splitlines():
            print("[container]   %s" % line)
    else:
        print("[container] упаковка не удалась (%s в корне, если есть, — от прошлой сборки):" % out_name)
        for line in (result.stderr or result.stdout).strip().splitlines():
            print("[container]   %s" % line)
    print("")


# Пересобирать контейнер надо и после прошивки, и после файловой системы.
#
# Только на прошивку вешать нельзя: если поправить что-то в data/ и запустить
# Build Filesystem Image, образ обновится, а прошивка останется нетронутой —
# post-действие на неё не сработает, и в контейнере останется СТАРАЯ файловая
# система. Поймано ровно на этом: littlefs.bin в сборке и внутри контейнера
# разошлись.
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", build_container)  # noqa: F821
env.AddPostAction("$BUILD_DIR/littlefs.bin", build_container)     # noqa: F821
env.AddPostAction("$BUILD_DIR/spiffs.bin", build_container)       # noqa: F821
