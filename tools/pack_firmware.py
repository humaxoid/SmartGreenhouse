#!/usr/bin/env python3
"""
pack_firmware.py — упаковщик прошивки v6.1+

Создаёт контейнер greenhouse_vX.Y.bin из двух файлов:
  - firmware.bin   (прошивка, обычно .pio/build/<env>/firmware.bin)
  - littlefs.bin   (файловая система, .pio/build/<env>/littlefs.bin)

Формат контейнера (32 байта заголовок + два бинарника):
  Offset  Size  Field
  ────────────────────────────────
  0       4     "GHFW"   (magic)
  4       1     formatVer = 1
  5       1     fwMajor
  6       1     fwMinor
  7       1     reserved = 0
  8       4     fwSize   (little-endian uint32)
  12      4     fsSize   (LE uint32)
  16      4     fwCrc    (LE uint32, CRC32 IEEE)
  20      4     fsCrc    (LE uint32)
  24      8     reserved zeros
  ────────────────────────────────
  32      ...   firmware.bin содержимое
  32+szfw ...   littlefs.bin содержимое

Формат 2 (подписанный, с --sign-key) — заголовок 176 байт:
  0       32    как в формате 1, но formatVer = 2
  32      32    SHA-256 firmware.bin
  64      32    SHA-256 littlefs.bin
  96      2     sigLen   (LE uint16)
  98      2     reserved = 0
  100     72    подпись ECDSA P-256 (DER) байтов 0..95, дополнена нулями
  172     4     reserved zeros
  176     ...   firmware.bin, затем littlefs.bin
Подпись делает openssl (из Git для Windows или любой другой в PATH;
путь можно задать GH_OPENSSL). Прошивка, собранная с публичным ключом
(tools/ota_pubkey.py), принимает только формат 2 с верной подписью.

Использование:
  python tools/pack_firmware.py \\
      --fw .pio/build/greenhouse/firmware.bin \\
      --fs .pio/build/greenhouse/littlefs.bin \\
      --major 6 --minor 1 \\
      --out greenhouse_v6.1.bin
"""

import argparse
import hashlib
import shutil
import struct
import subprocess
import zlib
import sys
import os

HDR_V2_SIZE = 176
SIGNED_PART = 96     # подписываются байты 0..95 заголовка v2
SIG_MAX = 72         # DER-подпись ECDSA P-256 — не длиннее 72 байт


def sign(data, key_path):
    """Подпись ECDSA P-256 / SHA-256 в DER через openssl. None — не вышло."""
    exe = os.environ.get("GH_OPENSSL") or shutil.which("openssl")
    if not exe:
        print("ERROR: openssl not found (PATH or GH_OPENSSL)", file=sys.stderr)
        return None
    r = subprocess.run([exe, "dgst", "-sha256", "-sign", key_path],
                       input=data, capture_output=True)
    if r.returncode != 0 or not r.stdout:
        print("ERROR: openssl sign failed: %s" % r.stderr.decode(errors="replace").strip(),
              file=sys.stderr)
        return None
    if len(r.stdout) > SIG_MAX:
        print("ERROR: signature too long (%d) — key must be ECDSA P-256" % len(r.stdout),
              file=sys.stderr)
        return None
    return r.stdout


def main():
    ap = argparse.ArgumentParser(description="Pack firmware container greenhouse_vX.Y.bin")
    ap.add_argument("--fw",    required=True, help="Path to firmware.bin")
    ap.add_argument("--fs",    required=True, help="Path to littlefs.bin")
    ap.add_argument("--major", type=int, required=True, help="FW version major (e.g. 6)")
    ap.add_argument("--minor", type=int, required=True, help="FW version minor (e.g. 1)")
    ap.add_argument("--out",   default=None, help="Output filename (default: greenhouse_vMAJOR.MINOR.bin)")
    ap.add_argument("--sign-key", default=None,
                    help="ECDSA P-256 private key (PEM): build a signed format-2 container")
    args = ap.parse_args()

    # Проверяем версии
    if not (0 <= args.major <= 255 and 0 <= args.minor <= 255):
        print("ERROR: major/minor must be 0..255", file=sys.stderr)
        return 1

    # Читаем оба бинарника
    if not os.path.exists(args.fw):
        print(f"ERROR: firmware not found: {args.fw}", file=sys.stderr)
        return 1
    if not os.path.exists(args.fs):
        print(f"ERROR: littlefs not found: {args.fs}", file=sys.stderr)
        return 1

    with open(args.fw, "rb") as f:
        fw_data = f.read()
    with open(args.fs, "rb") as f:
        fs_data = f.read()

    fw_size = len(fw_data)
    fs_size = len(fs_data)
    fw_crc  = zlib.crc32(fw_data) & 0xFFFFFFFF
    fs_crc  = zlib.crc32(fs_data) & 0xFFFFFFFF

    # Sanity checks
    if fw_size < 100_000 or fw_size > 1_769_472:
        print(f"ERROR: firmware size out of range ({fw_size} bytes)", file=sys.stderr)
        return 1
    if fs_size < 1_000 or fs_size > 589_824:
        print(f"ERROR: littlefs size out of range ({fs_size} bytes)", file=sys.stderr)
        return 1

    # Сборка заголовка
    # < = little-endian
    # 4s = 4 байта (magic)
    # B  = uint8
    # I  = uint32
    # 8s = 8 байт reserved
    fmt = 2 if args.sign_key else 1
    header = struct.pack("<4sBBBBIIII8s",
        b"GHFW",               # magic
        fmt,                   # formatVer
        args.major,
        args.minor,
        0,                     # reserved1
        fw_size,
        fs_size,
        fw_crc,
        fs_crc,
        b"\x00" * 8            # reserved2
    )
    assert len(header) == 32

    if fmt == 2:
        if not os.path.isfile(args.sign_key):
            print(f"ERROR: signing key not found: {args.sign_key}", file=sys.stderr)
            return 1
        signed = header + hashlib.sha256(fw_data).digest() + hashlib.sha256(fs_data).digest()
        assert len(signed) == SIGNED_PART
        sig = sign(signed, args.sign_key)
        if sig is None:
            return 1
        header = (signed + struct.pack("<HH", len(sig), 0)
                  + sig.ljust(SIG_MAX, b"\x00") + b"\x00" * 4)
        assert len(header) == HDR_V2_SIZE

    # Имя выхода
    out_name = args.out or f"greenhouse_v{args.major}.{args.minor}.bin"

    # Запись контейнера
    with open(out_name, "wb") as f:
        f.write(header)
        f.write(fw_data)
        f.write(fs_data)

    total = len(header) + fw_size + fs_size
    print(f"OK: {out_name} ({total:,} bytes)")
    print(f"  Header:   {len(header)} bytes (magic GHFW, format {fmt}, v{args.major}.{args.minor}"
          f"{', SIGNED' if fmt == 2 else ', UNSIGNED'})")
    print(f"  Firmware: {fw_size:,} bytes (CRC32 {fw_crc:08X})")
    print(f"  LittleFS: {fs_size:,} bytes (CRC32 {fs_crc:08X})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
