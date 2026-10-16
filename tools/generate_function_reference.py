#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
from __future__ import annotations
import argparse, json, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCES = sorted((ROOT/'src').glob('*.c')) + sorted((ROOT/'platform'/'dpc245').glob('*.c'))
OUT = ROOT/'docs'/'FUNCTION_REFERENCE.md'

MODULE_PURPOSE = {
    'advanced_settings.c':'Advanced settings state machine and controller-configuration editing.',
    'bafang_can.c':'CAN identifier handling, frame classification, telemetry decode and common frame serialization.',
    'bafang_config.c':'Controller wheel/speed configuration conversion and validation.',
    'bafang_service.c':'Long-transfer service receive path and decoded identity/battery service data.',
    'can_router.c':'Central CAN receive routing, telemetry ownership and service ACK arbitration.',
    'controller_write.c':'ACK-gated controller profile write transaction state machine.',
    'dpc245_app.c':'Top-level user-interface state machine and user-event ownership.',
    'dpc245_ble_proto.c':'BLE UART packet framing, checksum and parser state machine.',
    'dpc245_hmi_service.c':'HMI service identity replies and long-transfer transmit state machine.',
    'dpc245_keys.c':'Debounced key gesture interpretation and Walk/settings event generation.',
    'dpc245_key_sampler.c':'Clock-driven key sampling and timestamped event queue.',
    'dpc245_persist.c':'Persistent settings journal selection, loading and atomic append.',
    'dpc245_runtime.c':'Ride statistics, service counters and telemetry-derived runtime state.',
    'dpc245_settings.c':'Canonical settings record defaults, validation, accessors and error history.',
    'dpc245_tx.c':'Periodic HMI frames and service query scheduling/building.',
    'dpc245_v3_profile.c':'Periodic HMI status-frame schedule and serialization.',
    'evist_preferences.c':'EVistDrive preferences validation and persistent journal format.',
    'evistdrive_adapter.c':'Motor-family adapter functions between generic runtime and Bafang telemetry.',
    'light_controller.c':'Ambient-light filtering, automatic lamp state and display-lighting policy.',
    'main.c':'Platform bootstrap and cooperative application scheduler.',
    'menu_model.c':'Static menu labels and menu structure helpers.',
    'bxcan_raw.c':'Low-level CAN FIFO receive, acceptance filter and transmit mailbox driver.',
    'dpc245_adc.c':'ADC initialization, ALS/Button Signal sampling and filtering.',
    'dpc245_backlight.c':'Pulse-position backlight controller driver.',
    'dpc245_ble_uart.c':'USART transport for the external BLE module.',
    'dpc245_board.c':'Board-level debug/JTAG and reserved-input initialization.',
    'dpc245_buttons.c':'Physical key GPIO sampling and debounce helpers.',
    'dpc245_can_init.c':'CAN peripheral clock/timing initialization.',
    'dpc245_clock.c':'120 MHz system clock and bus-clock initialization.',
    'dpc245_hwinfo.c':'MCU hardware-information readout.',
    'dpc245_recovery.c':'APP marker invalidation and system reset entry path.',
    'dpc245_spi_mem.c':'External SPI memory driver and persistence I/O adapter.',
    'minifont.c':'Minimal text/numeric renderer used by the UI.',
    'runtime.c':'Freestanding C/AEABI runtime primitives required by the linker.',
    'st7789_8080.c':'8080-style parallel LCD bus and RGB565 drawing primitives.',
    'startup.c':'Vector table, reset initialization and early recovery guard.',
}

def ast_for(path: Path):
    cmd=['clang','-std=c11','-I'+str(ROOT/'include'),'-I'+str(ROOT/'platform'/'dpc245'),
         '-Xclang','-ast-dump=json','-fsyntax-only',str(path)]
    p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True)
    if p.returncode:
        sys.stderr.write(p.stderr)
        raise SystemExit(p.returncode)
    return json.loads(p.stdout)

def collect():
    defs=[]
    for src in SOURCES:
        ast=ast_for(src)
        src_name=src.name
        rel=src.relative_to(ROOT).as_posix()
        def walk(n, current=None):
            kind=n.get('kind')
            if kind=='FunctionDecl' and any(ch.get('kind')=='CompoundStmt' for ch in n.get('inner',[])):
                loc=n.get('loc',{})
                file=loc.get('file') or n.get('range',{}).get('begin',{}).get('file')
                if file is None or Path(file).name==src_name:
                    item={
                        'name':n.get('name','?'), 'file':rel, 'line':loc.get('line') or 0,
                        'linkage':'static' if n.get('storageClass')=='static' else 'global',
                        'type':n.get('type',{}).get('qualType',''), 'calls':set()
                    }
                    defs.append(item)
                    current=item
            if kind=='CallExpr' and current is not None:
                stack=list(n.get('inner',[]))
                while stack:
                    x=stack.pop()
                    rd=x.get('referencedDecl') or {}
                    if rd.get('kind')=='FunctionDecl' and rd.get('name'):
                        current['calls'].add(rd['name']); break
                    stack.extend(x.get('inner',[]))
            for ch in n.get('inner',[]): walk(ch,current)
        walk(ast,None)
    callers={d['name']:set() for d in defs}
    for d in defs:
        for c in d['calls']:
            if c in callers: callers[c].add(d['name'])
    return defs,callers

def human_role(name, linkage, module):
    n=name.lower()
    if name in {'main','Reset_Handler','SysTick_Handler','Default_Handler'}:
        return {'main':'Runs platform initialization and the cooperative application loop.',
                'Reset_Handler':'Initializes memory, executes the early recovery guard, then enters main().',
                'SysTick_Handler':'Advances the 1 ms system time base.',
                'Default_Handler':'Safe trap for unhandled interrupt vectors.'}[name]
    patterns=[
        ('_init','Initialization'),('_defaults','Default values'),('_validate','Validation'),
        ('_load','Load operation'),('_save','Save operation'),('_read','Read operation'),('_write','Write operation'),
        ('_build','Frame or record construction'),('_decode','Decoding'),('_encode','Encoding'),('_feed','Input handling'),
        ('_update','State update'),('_tick','Periodic step'),('_start','Start operation'),('_next','Next-step selection'),
        ('_on_rx','RX frame handler'),('_on_can','CAN frame handler'),('_set_','Setter'),('_get_','Getter'),
        ('_take_','Consume-once accessor'),('_apply','Apply operation'),('_sync','Synchronization'),
        ('_draw','Drawing routine'),('draw_','Drawing routine'),('_fill','Fill routine'),('_transmit','Transmit operation'),
        ('_receive','Receive operation'),('checksum','Checksum calculation or check'),('copy','Copy helper'),
        ('wait','Bounded wait for a hardware or state condition'),
    ]
    for key,noun in patterns:
        if key in n:
            return f'{noun} in the {module} module.'
    if linkage=='static':
        return f'Internal helper of the {module} module.'
    return f'Public function of the {module} module.'

def render(defs,callers):
    lines=['# Complete Function Reference','',
           'This file is generated from the current source tree and is part of the release gate. '
           'Every C function definition, including static helpers, interrupt handlers and freestanding runtime functions, is listed here.', '',
           f'- Function definitions: **{len(defs)}**',
           f'- Source files: **{len(set(d["file"] for d in defs))}**','']
    byfile={}
    for d in defs: byfile.setdefault(d['file'],[]).append(d)
    for file in sorted(byfile):
        basename=Path(file).name
        lines += [f'## `{file}`','', MODULE_PURPOSE.get(basename,'Module implementation.'),'', f'Functions: **{len(byfile[file])}**','']
        for d in sorted(byfile[file], key=lambda x:(x['line'],x['name'])):
            module=basename[:-2]
            cs=', '.join(f'`{x}`' for x in sorted(d['calls'])) or '—'
            cr=', '.join(f'`{x}`' for x in sorted(callers.get(d['name'],()))) or '—'
            lines += [f'### `{d["name"]}`','',
                      f'- Location: `{d["file"]}:{d["line"]}`',
                      f'- Linkage: `{d["linkage"]}`',
                      f'- Type: `{d["type"]}`',
                      f'- Responsibility: {human_role(d["name"],d["linkage"],module)}',
                      f'- Direct callers: {cr}',
                      f'- Direct callees: {cs}','']
    return '\n'.join(lines).rstrip()+'\n'

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--check',action='store_true')
    args=ap.parse_args()
    defs,callers=collect(); text=render(defs,callers)
    if args.check:
        if not OUT.exists() or OUT.read_text()!=text:
            print('function-reference: FAIL: docs/FUNCTION_REFERENCE.md is not synchronized')
            return 1
        print(f'function-reference: PASS ({len(defs)} functions)')
        return 0
    OUT.write_text(text)
    print(f'wrote {OUT.relative_to(ROOT)} ({len(defs)} functions)')
    return 0
if __name__=='__main__': raise SystemExit(main())
