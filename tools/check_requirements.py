#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
from pathlib import Path
import re, sys
R=Path(__file__).resolve().parents[1]
def has(path,text): return text in (R/path).read_text()
checks={
 'APP base 0x08006000': has('platform/dpc245/dpc245.ld','ORIGIN = 0x08006000'),
 'APP marker 0x0803FFFE': has('platform/dpc245/dpc245.ld','ORIGIN = 0x0803FFFE'),
 'RAM 64 KiB (proven window)': has('platform/dpc245/dpc245.ld','LENGTH = 64K'),
 'CAN update request 0x4000 contract': has('include/dpc245_recovery_trigger.h','#define DPC245_UPDATE_REQ_CMD       0x40u'),
 'no 0x3005 announcement trigger': not has('src/main.c','fw_update_enter'),
 'preflight recovery before LCD': (lambda m: m.index('dpc245_preflight_run(') < m.index('dpc245_lcd_init('))((R/'src/main.c').read_text()),
 'PROBE invalidates its marker before any wait': (lambda t: t.index('dpc245_marker_clear_retry(dpc245_boot_marker_clear,') < t.index('clock_force_hsi8()'))((R/'platform/dpc245/startup.c').read_text().split('void Reset_Handler(void) {')[1]),
 'every loop before main is bounded': all(any(k in l for k in ('LIMIT','&_edata','&_ebss','i < ms','for (;;)')) for l in (R/'platform/dpc245/startup.c').read_text().splitlines() if re.search(r'\b(for|while)\s*\(', l)) and (R/'src/dpc245_clock_tree.c').read_text().count('if (++n >= spin_limit)') == 2,
 'clock tree checked before use': has('platform/dpc245/startup.c','if (core_hz == 0u) dpc245_system_reset();') and has('src/main.c','if(clock_tree==DPC245_CLOCK_TREE_OTHER) dpc245_system_reset();') and has('src/main.c','if(!dpc245_clock_init()) return dpc245_clock_fallback_hsi();'),
 'faults reset instead of hanging': has('platform/dpc245/startup.c','void Default_Handler(void) { dpc245_system_reset(); }'),
 'PLL x15 from 16 MHz HXTAL/2 (no PLLMF[4])': has('platform/dpc245/dpc245_clock.c','cfg |= RCU_CFG0_PLLSEL | RCU_CFG0_PREDV0 | RCU_CFG0_PLL_MUL15;') and has('platform/dpc245/dpc245_clock.c','#define RCU_CFG0_PLL_MUL15     (13u << 18)'),
 'no flash wait-state writes': not has('platform/dpc245/dpc245_clock.c','FMC_WS'),
 'SYSCLK 120 MHz': has('platform/dpc245/dpc245_clock.h','DPC245_SYSCLK_HZ 120000000u'),
 'APB1 60 MHz': has('platform/dpc245/dpc245_clock.h','DPC245_APB1_HZ    60000000u'),
 'CAN 250k timing': has('platform/dpc245/dpc245_can_init.c','can_init_btr(0x00340017u)'),
 'BLE 230400 timing': has('platform/dpc245/dpc245_ble_uart.c','ble_uart_init_brr(0x209u)'),
 'LCD 320x170': has('platform/dpc245/st7789_8080.h','DPC245_LCD_WIDTH  320u') and has('platform/dpc245/st7789_8080.h','DPC245_LCD_HEIGHT 170u'),
 'CAN TX enabled': has('include/app_config.h','#define EVIST_ENABLE_CAN_TX 1'),
 'service TX enabled': has('include/app_config.h','#define EVIST_ENABLE_SERVICE_TX 1'),
 'persistence enabled': has('include/app_config.h','#define EVIST_ENABLE_PERSISTENCE 1'),
 'BLE TX enabled': has('include/app_config.h','#define EVIST_ENABLE_BLE_TX 1'),
 'walk dead-man policy': has('include/app_config.h','#define EVIST_WALK_DEADMAN 1'),
 'ten lower panels': has('include/evist_preferences.h','#define EVIST_PANEL_COUNT 10u'),
 'BLE application dispatcher': has('src/dpc245_ble_app.c','case 0xA1u:') and has('src/dpc245_ble_app.c','case 0xA9u:'),
 'BLE periodic 39-byte status': has('src/dpc245_ble_app.c','uint8_t p[39]'),
 'BLE update handoff': has('src/dpc245_ble_app.c','dpc245_update_request(update)'),
 'RTC register contract': has('include/dpc245_rtc.h','#define DPC245_RTC_REG_SECONDS 0x80u') and has('include/dpc245_rtc.h','#define DPC245_RTC_REG_WP      0x8Eu'),
 'controller raw status preserved': has('include/bafang_can.h','controller_status_bit0') and has('include/bafang_can.h','controller_fast5_raw'),
 'five battery information pages': has('src/dpc245_app.c','case DPC245_APP_SCREEN_INFO_BATTERY: return 5u;'),
 'complete identity query range': has('src/dpc245_tx.c','{BAFANG_NODE_CONTROLLER, 0x60u, 0x05u}') and has('src/dpc245_tx.c','{BAFANG_NODE_BATTERY, 0x60u, 0x05u}'),
}
failed=[k for k,v in checks.items() if not v]
for k,v in checks.items(): print(f"requirement: {'PASS' if v else 'FAIL'}: {k}")
if failed: sys.exit(1)
