#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build and inspect DPC245 V3 installation files.

An installation file is a 32-byte header followed by the payload written from
APP_BASE upwards: the application image (with its 55 AA marker), followed
in the full layout by a blank 0xFF fill up to a fixed payload length. The
short layout (--short) ends with the application. Nothing else is needed as
input.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import struct
import sys

HEADER_SIZE = 32
MAGIC = bytes.fromhex("08 42 83 00")
# Bytes 0..11: format identifier and fixed fields expected by the update path.
HEADER_FIXED = MAGIC + bytes.fromhex("40 00 00 00 00 00 00 00")
APP_BASE = 0x08006000
FILL_BASE = 0x08040000
APP_BYTES = FILL_BASE - APP_BASE
# Payload lengths: full layout (application + blank fill) and short layout
# (application only). Inspection accepts exactly these two.
PAYLOAD_BYTES = 0x6F75C
SHORT_PAYLOAD_BYTES = APP_BYTES
FILL_BYTE = 0xFF
FILE_BYTES = HEADER_SIZE + PAYLOAD_BYTES
SHORT_FILE_BYTES = HEADER_SIZE + SHORT_PAYLOAD_BYTES


def crc16_xmodem(data: bytes) -> int:
    crc = 0
    for value in data:
        crc ^= value << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def inspect(blob: bytes) -> dict[str, int | bool]:
    if len(blob) < HEADER_SIZE:
        raise ValueError("file is shorter than its header")
    header, payload = blob[:HEADER_SIZE], blob[HEADER_SIZE:]
    declared = struct.unpack(">I", header[12:16])[0]
    stored = struct.unpack(">H", header[16:18])[0]
    calculated = crc16_xmodem(payload)
    return {
        "magic_ok": header[:4] == MAGIC,
        "header_ok": header[:12] == HEADER_FIXED and header[18:] == bytes(HEADER_SIZE - 18),
        "declared_length": declared,
        "actual_length": len(payload),
        "layout": "full" if len(payload) == PAYLOAD_BYTES
        else "short" if len(payload) == SHORT_PAYLOAD_BYTES else "unknown",
        "length_ok": declared == len(payload) and len(payload) in (PAYLOAD_BYTES, SHORT_PAYLOAD_BYTES),
        "stored_crc": stored,
        "calculated_crc": calculated,
        "crc_ok": stored == calculated,
        "app_marker_ok": len(payload) >= APP_BYTES and payload[APP_BYTES - 2:APP_BYTES] == b"\x55\xAA",
        "fill_blank": len(payload) >= APP_BYTES
        and payload[APP_BYTES:] == bytes([FILL_BYTE]) * (len(payload) - APP_BYTES),
    }


CHECKS = ("magic_ok", "header_ok", "length_ok", "crc_ok", "app_marker_ok", "fill_blank")


def build(app: bytes, short: bool = False) -> bytes:
    if len(app) != APP_BYTES:
        raise ValueError(f"APP must be exactly 0x{APP_BYTES:X} bytes")
    if app[-2:] != b"\x55\xAA":
        raise ValueError("APP validity marker is not 55 AA")
    payload = app if short else app + bytes([FILL_BYTE]) * (PAYLOAD_BYTES - APP_BYTES)
    header = bytearray(HEADER_SIZE)
    header[:12] = HEADER_FIXED
    header[12:16] = struct.pack(">I", len(payload))
    header[16:18] = struct.pack(">H", crc16_xmodem(payload))
    result = bytes(header) + payload
    status = inspect(result)
    if len(result) != (SHORT_FILE_BYTES if short else FILE_BYTES) or not all(status[k] for k in CHECKS):
        raise ValueError("generated installation file failed validation")
    return result


def self_test() -> None:
    app = bytearray([0x00] * APP_BYTES)
    app[0:8] = bytes.fromhex("00 00 01 20 01 60 00 08")
    app[-2:] = b"\x55\xAA"
    out = build(bytes(app))
    st = inspect(out)
    assert all(st[k] for k in CHECKS), st
    assert len(out) == FILE_BYTES == 456572
    assert out[:12] == HEADER_FIXED and out[18:HEADER_SIZE] == bytes(14)
    assert out[HEADER_SIZE:HEADER_SIZE + APP_BYTES] == bytes(app)
    assert out[HEADER_SIZE + APP_BYTES:] == b"\xFF" * (PAYLOAD_BYTES - APP_BYTES)
    assert build(bytes(app)) == out, "build is not deterministic"
    # A known CRC16/XMODEM vector.
    assert crc16_xmodem(b"123456789") == 0x31C3
    # Any byte in the fill area must be reported, even with a matching CRC.
    tampered = bytearray(out)
    tampered[-1] = 0x47
    tampered[16:18] = struct.pack(">H", crc16_xmodem(bytes(tampered[HEADER_SIZE:])))
    st = inspect(bytes(tampered))
    assert st["crc_ok"] and not st["fill_blank"]
    corrupt = bytearray(out)
    corrupt[HEADER_SIZE + 100] ^= 0x01
    assert not inspect(bytes(corrupt))["crc_ok"]
    short = build(bytes(app), short=True)
    st = inspect(short)
    assert all(st[k] for k in CHECKS) and st["layout"] == "short", st
    assert len(short) == SHORT_FILE_BYTES == 237600
    assert short[:16] == out[:12] + struct.pack(">I", APP_BYTES)
    assert short[HEADER_SIZE:] == bytes(app)
    assert short[16:18] == struct.pack(">H", crc16_xmodem(bytes(app)))
    assert inspect(out)["layout"] == "full"
    # Any other payload length is rejected, even with a matching header.
    odd = bytearray(short + b"\xFF" * 4)
    odd[12:16] = struct.pack(">I", len(odd) - HEADER_SIZE)
    odd[16:18] = struct.pack(">H", crc16_xmodem(bytes(odd[HEADER_SIZE:])))
    st = inspect(bytes(odd))
    assert st["crc_ok"] and not st["length_ok"] and st["layout"] == "unknown"
    for bad in (bytes(app[:-4]) + b"\x55\xAA", bytes(app[:-2]) + b"\xFF\xFF"):
        try:
            build(bad)
        except ValueError:
            continue
        raise AssertionError("invalid APP was accepted")
    print("update-container: PASS")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--app", type=Path, help="application image (0x3A000 bytes, ends with 55 AA)")
    ap.add_argument("--out", type=Path, help="installation file to write")
    ap.add_argument("--inspect", dest="inspect_path", type=Path,
                    help="check an installation file; exit status 1 if any check fails")
    ap.add_argument("--short", action="store_true", help="short layout: header and application only")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    if args.self_test:
        self_test(); return
    if args.inspect_path:
        status = inspect(args.inspect_path.read_bytes())
        for key, value in status.items():
            print(f"{key}: {value}")
        if not all(status[k] for k in CHECKS):
            print("installation file: FAIL", file=sys.stderr)
            raise SystemExit(1)
        print("installation file: PASS")
        return
    if not (args.app and args.out):
        ap.error("--app and --out are required to build an installation file")
    args.out.write_bytes(build(args.app.read_bytes(), short=args.short))
    for key, value in inspect(args.out.read_bytes()).items():
        print(f"{key}: {value}")

if __name__ == "__main__":
    main()
