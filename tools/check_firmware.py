#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import struct, subprocess, sys
from pathlib import Path

APP_BASE=0x08006000
MARKER_ADDR=0x0803FFFE
RAM_BASE=0x20000000
RAM_END=0x20010000
EXPECTED_SIZE=MARKER_ADDR+2-APP_BASE

def fail(msg):
    print(f"firmware-check: FAIL: {msg}", file=sys.stderr); raise SystemExit(1)

def main():
    if len(sys.argv)!=3: fail("usage: check_firmware.py firmware.bin firmware.elf")
    bp,ep=map(Path,sys.argv[1:])
    data=bp.read_bytes()
    if len(data)!=EXPECTED_SIZE: fail(f"binary size {len(data)} != {EXPECTED_SIZE}")
    if data[-2:]!=b'\x55\xAA': fail("application marker is not 55 AA")
    sp,rv=struct.unpack_from('<II',data,0)
    if not (RAM_BASE <= sp <= RAM_END): fail(f"initial SP 0x{sp:08X} outside RAM")
    if (rv & 1)==0 or not (APP_BASE <= (rv & ~1) < MARKER_ADDR): fail(f"reset vector 0x{rv:08X} invalid")
    out=subprocess.check_output(['size',str(ep)],text=True).splitlines()
    fields=out[-1].split()
    text,data_sz,bss=map(int,fields[:3])
    if text+data_sz > (MARKER_ADDR-APP_BASE): fail("loadable image exceeds APP region")
    if data_sz+bss > (RAM_END-RAM_BASE): fail("RAM use exceeds 64 KiB")
    print(f"firmware-check: PASS; text={text} data={data_sz} bss={bss} bin={len(data)} SP=0x{sp:08X} reset=0x{rv:08X}")
if __name__=='__main__': main()
