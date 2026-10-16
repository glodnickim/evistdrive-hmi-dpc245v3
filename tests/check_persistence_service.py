#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Scoped foreground call-site check + qualified starvation controls.
No assertion about physical target or complete foreground execution time.
"""
import hashlib
import pathlib
import re
import shutil
import subprocess

native = pathlib.Path(__file__).resolve().parents[1]
source = native / 'src/main.c'
compiler = shutil.which('clang') or shutil.which('gcc')
text = subprocess.check_output([
    compiler, '-E', '-P', '-I'+str(native/'include'),
    '-I'+str(native/'platform/dpc245'), '-DEVIST_BUILD_PROFILE=0',
    '-DEVIST_ENABLE_PERSISTENCE=1', '-DEVIST_ENABLE_CAN_TX=1',
    '-DEVIST_ENABLE_BLE_TX=1', str(source)], text=True)

def close(text, start, left, right):
    depth = 0
    for i in range(start, len(text)):
        if text[i] == left:
            depth += 1
        elif text[i] == right:
            depth -= 1
            if depth == 0:
                return i
    raise ValueError('unbalanced source shape')

def inspect(text):
    body = text.split('int main(void)', 1)[1]
    loops = list(re.finditer(r'for\s*\(\s*;\s*;\s*\)\s*\{', body))
    if not loops:
        return ['missing foreground loop']
    start = loops[0].end()-1
    loop = body[start+1:close(body, start, '{', '}')]
    calls = [
        'dpc245_can_receive_poll', 'dpc245_ble_uart_poll',
        'dpc245_ble_parser_feed', 'dpc245_ble_app_handle',
        'dpc245_buttons_key_event', 'controller_config_service',
        'dpc245_spi_mem_set_limits', 'evist_native_store_step',
        'dpc245_spi_mem_end_step', 'dpc245_v3_tx_schedule_poll',
        'dpc245_build_periodic_frame']
    failures = []
    positions = []
    for name in calls:
        found = list(re.finditer(r'\b'+name+r'\s*\(', loop))
        if len(found) != 1:
            failures.append(name+' expected exactly one foreground call site')
        else:
            positions.append(found[0].start())
    if len(positions) == len(calls) and positions != sorted(positions):
        failures.append('service/step call-site ordering changed')
    if re.search(r'\b(?:evist_pref_store_save|evist_preferences_save|dpc245_persist_save|evist_ride_store_append)\s*\(', loop):
        failures.append('synchronous preference save in foreground')
    step = re.search(r'\bevist_native_store_step\s*\(', loop)
    if step:
        for nested in re.finditer(r'\b(?:for|while)\s*\(', loop):
            op = loop.index('(', nested.start())
            end = close(loop, op, '(', ')')
            block = end+1
            while block < len(loop) and loop[block].isspace():
                block += 1
            if block < len(loop) and loop[block] == '{':
                endblock = close(loop, block, '{', '}')
                if block < step.start() < endblock:
                    failures.append('persistence step inside nested drain loop')
    return failures

failures = inspect(text)
print('foreground source SHA256', hashlib.sha256(source.read_bytes()).hexdigest())
print('actual integration', 'PASS' if not failures else failures)
assert not failures, failures
controls = {
    'lost BLE opportunity': text.replace('dpc245_ble_uart_poll();', ';'),
    'lost key event opportunity': text.replace('dpc245_buttons_key_event(&key_sample)',
                                               'dpc245_keys_update(&key_fsm,0u,now)'),
    'restored synchronous save': text.replace(
        'evist_native_store_step(&native_store,&native_io,now)',
        'evist_pref_store_save(&pref_store,&persist_io,&preferences)'),
    'drain all persistence steps': text.replace(
        'if(store_ok&&native_store.busy)',
        'while(store_ok&&native_store.busy)'),
}
for name, mutated in controls.items():
    assert mutated != text, 'control shape missing: '+name
    errors = inspect(mutated)
    print('negative control', name, 'DETECTED', errors)
    assert errors, 'insensitive negative control: '+name
print('scope: source call sites and controls; service behavior and CPU model in the sanitizer suite; not run on hardware')
