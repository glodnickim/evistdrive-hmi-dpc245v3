#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Convert host-rendered RGB565 frames into PNG files and one index.html.

The HTML shows every frame at 2x with an optional coordinate grid (labels every
50 panel pixels). --no-grid exports clean 2x PNGs. No third-party packages are needed.

usage: render_preview.py [--no-grid] OUT_DIR
"""
from __future__ import annotations

import argparse
import html
import struct
import zlib
from pathlib import Path

WIDTH, HEIGHT = 320, 170
SCALE = 2


def rgb565_to_rgb888(raw: bytes) -> bytes:
    out = bytearray(WIDTH * HEIGHT * 3)
    for i in range(WIDTH * HEIGHT):
        v = raw[2 * i] | (raw[2 * i + 1] << 8)
        r5, g6, b5 = (v >> 11) & 31, (v >> 5) & 63, v & 31
        out[3 * i:3 * i + 3] = bytes(((r5 << 3) | (r5 >> 2), (g6 << 2) | (g6 >> 4), (b5 << 3) | (b5 >> 2)))
    return bytes(out)


def add_grid(rgb: bytes) -> bytes:
    """Faint grid every 10 px, stronger every 50 px (in panel pixels)."""
    buf = bytearray(rgb)
    for y in range(HEIGHT):
        for x in range(WIDTH):
            if x % 50 == 0 or y % 50 == 0:
                shade = (90, 90, 90)
            elif x % 10 == 0 or y % 10 == 0:
                shade = (45, 45, 45)
            else:
                continue
            i = 3 * (y * WIDTH + x)
            buf[i:i + 3] = bytes(shade)
    return bytes(buf)


def scale2(rgb: bytes) -> bytes:
    out = bytearray(WIDTH * SCALE * HEIGHT * SCALE * 3)
    row = WIDTH * SCALE * 3
    for y in range(HEIGHT):
        src = rgb[y * WIDTH * 3:(y + 1) * WIDTH * 3]
        line = bytearray(row)
        for x in range(WIDTH):
            px = src[3 * x:3 * x + 3]
            line[6 * x:6 * x + 6] = px + px
        for k in range(SCALE):
            start = (y * SCALE + k) * row
            out[start:start + row] = line
    return bytes(out)


def png(width: int, height: int, rgb: bytes) -> bytes:
    def chunk(tag: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    raw = bytearray()
    stride = width * 3
    for y in range(height):
        raw.append(0)
        raw += rgb[y * stride:(y + 1) * stride]
    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")


def label_grid(prefix: str) -> str:
    parts = []
    for x in range(0, WIDTH + 1, 50):
        parts.append(f'<div class="lx" style="left:{x * SCALE}px">{x}</div>')
    for y in range(0, HEIGHT + 1, 50):
        parts.append(f'<div class="ly" style="top:{y * SCALE}px">{y}</div>')
    return "".join(parts)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("out_dir", nargs="?", default="sim/out")
    ap.add_argument("--no-grid", action="store_true", help="omit coordinate marks from PNGs and HTML")
    args = ap.parse_args()
    out = Path(args.out_dir)
    frames = sorted(out.glob("*.rgb565"))
    if not frames:
        raise SystemExit(f"no frames in {out}")
    cards = []
    for path in frames:
        rgb = rgb565_to_rgb888(path.read_bytes())
        (out / f"{path.stem}.png").write_bytes(png(WIDTH * SCALE, HEIGHT * SCALE, scale2(rgb if args.no_grid else add_grid(rgb))))
        cards.append(
            f'<figure><figcaption>{html.escape(path.stem)} &middot; {WIDTH}x{HEIGHT} px</figcaption>'
            f'<div class="frame"><img src="{html.escape(path.stem)}.png" width="{WIDTH * SCALE}" height="{HEIGHT * SCALE}">'
            f'{"" if args.no_grid else label_grid(path.stem)}</div></figure>'
        )
    page = f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8"><title>EVistDrive HMI screen preview</title>
<style>
body {{ font-family: system-ui, sans-serif; background:#1b1b1b; color:#eee; margin:24px; }}
h1 {{ font-size:20px; }}
p.note {{ color:#aaa; max-width:900px; }}
figure {{ display:inline-block; margin:0 24px 28px 0; vertical-align:top; }}
figcaption {{ font-size:13px; margin-bottom:6px; color:#ccc; }}
.frame {{ position:relative; width:{WIDTH * SCALE}px; height:{HEIGHT * SCALE}px; }}
.frame img {{ display:block; }}
.lx, .ly {{ position:absolute; font-size:10px; color:#ffd060; background:rgba(0,0,0,.55); padding:0 2px; }}
.lx {{ top:-14px; transform:translateX(-50%); }}
.ly {{ left:-26px; transform:translateY(-50%); }}
</style></head><body>
<h1>EVistDrive HMI screen preview</h1>
<p class="note">Frames are rendered by the firmware screen code with sample telemetry. PNGs are shown at twice the panel resolution.</p>
{''.join(cards)}
</body></html>"""
    (out / "index.html").write_text(page, encoding="utf-8")
    print(f"wrote {len(frames)} previews and {out / 'index.html'}")


if __name__ == "__main__":
    main()
