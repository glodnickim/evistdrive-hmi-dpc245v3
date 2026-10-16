#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write BUILD_INFO.txt and SHA256SUMS.txt for a release directory."""
from __future__ import annotations
import argparse
import hashlib
import subprocess
from pathlib import Path

APP_BYTES = 0x0803FFFE - 0x08006000
RAM_BYTES = 64 * 1024


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def memory_line(elf: Path) -> str:
    fields = subprocess.check_output(["size", str(elf)], text=True).splitlines()[-1].split()
    text, data, bss = map(int, fields[:3])
    flash = text + data
    ram = data + bss
    return (f"{elf.stem}: flash {flash} B ({100.0 * flash / APP_BYTES:.1f} % of APP), "
            f"RAM static {ram} B ({100.0 * ram / RAM_BYTES:.1f} % of 64 KiB)")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("dist", type=Path)
    ap.add_argument("--source", default="UNKNOWN")
    ap.add_argument("--toolchain", default="UNKNOWN")
    args = ap.parse_args()

    info = [f"source     {args.source}", f"toolchain  {args.toolchain}", ""]
    info.extend(memory_line(elf) for elf in sorted(args.dist.glob("*.elf")))
    (args.dist / "BUILD_INFO.txt").write_text("\n".join(info) + "\n")

    lines = [f"{sha256(f)}  {f.name}" for f in sorted(args.dist.iterdir())
             if f.is_file() and f.name != "SHA256SUMS.txt"]
    (args.dist / "SHA256SUMS.txt").write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
