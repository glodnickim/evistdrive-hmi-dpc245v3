/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_probe.h"
#include "app_config.h"
#include "dpc245_adc.h"
#include "dpc245_backlight.h"
#include "dpc245_buttons.h"
#include "dpc245_hwinfo.h"
#include "dpc245_recovery.h"
#include "minifont.h"
#include "st7789_8080.h"

#define PROBE_BG     0x0000u
#define PROBE_FG     0xFFFFu
#define PROBE_DIM    0x7BEFu
#define PROBE_OK     0x07E0u
#define PROBE_BAD    0xF800u
#define PROBE_VALUE_X 120u
#define PROBE_ROW_Y(n) ((uint16_t)(24u + (n) * 12u))
#define PROBE_REFRESH_MS 250u
#define PROBE_BACKLIGHT_LEVEL 2u

#define PROBE_RETRY_MS 1000u

uint8_t dpc245_probe_early_disarm_attempt;

static uint16_t marker_value(void) {
    return *(volatile uint16_t *)(uintptr_t)DPC245_APP_MARKER_ADDR;
}

void dpc245_probe_boot_init(dpc245_probe_boot_t *boot) {
    boot->clear_attempt = dpc245_probe_early_disarm_attempt;
    boot->marker_after = marker_value();
    boot->marker_cleared = boot->clear_attempt != 0u && boot->marker_after == 0x0000u;
    boot->late_retries = 0u;
    boot->clock_pll = false;
    boot->clock_kept = false;
}

static void row_label(unsigned row, const char *label) {
    ui_text(4u, PROBE_ROW_Y(row), label, PROBE_DIM, PROBE_BG, 1u);
}

static void row_u32(unsigned row, uint32_t value) {
    dpc245_lcd_fill_rect(PROBE_VALUE_X, PROBE_ROW_Y(row), 196u, 8u, PROBE_BG);
    ui_u32(PROBE_VALUE_X, PROBE_ROW_Y(row), value, PROBE_FG, PROBE_BG, 1u);
}

static void row_hex(unsigned row, uint32_t value, uint8_t digits) {
    ui_hex32(PROBE_VALUE_X, PROBE_ROW_Y(row), value, digits, PROBE_FG, PROBE_BG, 1u);
}

/* Row 0 and the bottom line depend on the marker state; the bottom line is
 * the only instruction the tester acts on, so it never promises the updater
 * unless the marker really reads 0000. */
static void draw_marker(const dpc245_probe_boot_t *boot) {
    dpc245_lcd_fill_rect(PROBE_VALUE_X, PROBE_ROW_Y(0u), 196u, 8u, PROBE_BG);
    ui_text(PROBE_VALUE_X, PROBE_ROW_Y(0u), boot->marker_cleared ? "CLEARED" : "CLEAR FAILED",
            boot->marker_cleared ? PROBE_OK : PROBE_BAD, PROBE_BG, 1u);
    ui_hex32(PROBE_VALUE_X + 80u, PROBE_ROW_Y(0u), boot->marker_after, 4u, PROBE_FG, PROBE_BG, 1u);
    ui_text(PROBE_VALUE_X + 120u, PROBE_ROW_Y(0u), "TRY", PROBE_DIM, PROBE_BG, 1u);
    ui_u32(PROBE_VALUE_X + 146u, PROBE_ROW_Y(0u), boot->clear_attempt, PROBE_FG, PROBE_BG, 1u);
    ui_u32(PROBE_VALUE_X + 160u, PROBE_ROW_Y(0u), boot->late_retries, PROBE_FG, PROBE_BG, 1u);
    dpc245_lcd_fill_rect(0u, 160u, 248u, 10u, PROBE_BG);
    if (boot->marker_cleared)
        ui_text(4u, 160u, "POWER CYCLE - UPDATER MODE NEXT", PROBE_DIM, PROBE_BG, 1u);
    else
        ui_text(4u, 160u, "STOP TEST - DO NOT FLASH - PHOTO", PROBE_BAD, PROBE_BG, 1u);
}

static void draw_static(const dpc245_probe_boot_t *boot, const dpc245_hwinfo_t *hw,
                        const dpc245_preflight_stats_t *stats) {
    dpc245_lcd_fill(PROBE_BG);
    ui_text(4u, 4u, "EVISTDRIVE NATIVE PROBE", PROBE_FG, PROBE_BG, 1u);
    ui_text(200u, 4u, EVISTDRIVE_HMI_VERSION " " EVIST_BUILD_GIT, PROBE_DIM, PROBE_BG, 1u);

    row_label(0u, "APP MARKER");
    draw_marker(boot);
    row_label(1u, "CLOCK");
    ui_text(PROBE_VALUE_X, PROBE_ROW_Y(1u),
            boot->clock_kept ? "PLL 120 MHZ KEPT - HSI NOT READY" :
            boot->clock_pll ? "PLL 120 MHZ" : "HSI 8 MHZ FALLBACK",
            boot->clock_pll && !boot->clock_kept ? PROBE_OK : PROBE_BAD, PROBE_BG, 1u);
    row_label(2u, "FLASH KIB");  row_u32(2u, hw->flash_kib);
    row_label(3u, "SRAM KIB");   row_u32(3u, hw->sram_kib);
    row_label(4u, "DBGID");      row_hex(4u, hw->dbg_idcode, 8u);
    row_label(5u, "RESET FLAGS"); row_hex(5u, hw->reset_flags, 8u);
    ui_text(PROBE_VALUE_X + 80u, PROBE_ROW_Y(5u), "CFG0", PROBE_DIM, PROBE_BG, 1u);
    ui_hex32(PROBE_VALUE_X + 110u, PROBE_ROW_Y(5u), *(volatile uint32_t *)(uintptr_t)0x40021004u,
             8u, PROBE_FG, PROBE_BG, 1u);
    row_label(6u, "BOOT TAIL");
    for (unsigned i = 0u; i < sizeof hw->boot_tail; ++i)
        ui_hex8((uint16_t)(PROBE_VALUE_X + i * 16u), PROBE_ROW_Y(6u), hw->boot_tail[i],
                PROBE_FG, PROBE_BG, 1u);
    row_label(7u, "CAN INIT");
    ui_text(PROBE_VALUE_X, PROBE_ROW_Y(7u), stats->can_ok ? "OK" : "FAIL",
            stats->can_ok ? PROBE_OK : PROBE_BAD, PROBE_BG, 1u);
    row_label(8u, "CAN RX");
    row_label(9u, "BLE RX BYTES FRAMES");
    row_label(10u, "KEYS P U D");
    row_label(11u, "PA1  ALS  ADC");
}

static void draw_dynamic(const dpc245_preflight_stats_t *stats, const dpc245_adc_snapshot_t *adc,
                         uint32_t uptime_ms) {
    row_u32(8u, stats->can_frames);
    dpc245_lcd_fill_rect(PROBE_VALUE_X, PROBE_ROW_Y(9u), 196u, 8u, PROBE_BG);
    ui_u32(PROBE_VALUE_X, PROBE_ROW_Y(9u), stats->ble_bytes, PROBE_FG, PROBE_BG, 1u);
    ui_u32(PROBE_VALUE_X + 80u, PROBE_ROW_Y(9u), stats->ble_frames, PROBE_FG, PROBE_BG, 1u);

    const uint8_t keys = dpc245_buttons_raw();
    dpc245_lcd_fill_rect(PROBE_VALUE_X, PROBE_ROW_Y(10u), 196u, 8u, PROBE_BG);
    ui_u32(PROBE_VALUE_X, PROBE_ROW_Y(10u), (keys & DPC245_KEY_POWER) ? 1u : 0u, PROBE_FG, PROBE_BG, 1u);
    ui_u32(PROBE_VALUE_X + 16u, PROBE_ROW_Y(10u), (keys & DPC245_KEY_UP) ? 1u : 0u, PROBE_FG, PROBE_BG, 1u);
    ui_u32(PROBE_VALUE_X + 32u, PROBE_ROW_Y(10u), (keys & DPC245_KEY_DOWN) ? 1u : 0u, PROBE_FG, PROBE_BG, 1u);

    dpc245_lcd_fill_rect(PROBE_VALUE_X, PROBE_ROW_Y(11u), 196u, 8u, PROBE_BG);
    ui_u32(PROBE_VALUE_X, PROBE_ROW_Y(11u), adc->button_signal_instant, PROBE_FG, PROBE_BG, 1u);
    ui_u32(PROBE_VALUE_X + 80u, PROBE_ROW_Y(11u), adc->als_instant, PROBE_FG, PROBE_BG, 1u);

    dpc245_lcd_fill_rect(250u, 160u, 70u, 8u, PROBE_BG);
    ui_u32(250u, 160u, uptime_ms / 1000u, PROBE_DIM, PROBE_BG, 1u);
}

void dpc245_probe_run(dpc245_probe_boot_t *boot, dpc245_millis_fn millis,
                      void (*delay_ms)(uint32_t), dpc245_preflight_stats_t *stats) {
    static dpc245_hwinfo_t hw;
    static dpc245_adc_snapshot_t adc;
    dpc245_hwinfo_read(&hw);
    const bool adc_ok = dpc245_adc_init();

    dpc245_lcd_init(delay_ms);
    dpc245_backlight_init();
    dpc245_backlight_set_level(PROBE_BACKLIGHT_LEVEL);
    draw_static(boot, &hw, stats);

    uint32_t last = 0u, last_retry = 0u;
    for (;;) {
        const uint32_t now = millis();
        if (!boot->marker_cleared && (uint32_t)(now - last_retry) >= PROBE_RETRY_MS) {
            last_retry = now;
            if (boot->late_retries < 0xFFFFu) boot->late_retries++;
            boot->marker_cleared = dpc245_boot_marker_clear();
            boot->marker_after = marker_value();
            draw_marker(boot);
        }
        /* Keep counting traffic; a request still resets into the updater. */
        dpc245_preflight_poll(now, stats);
        if ((uint32_t)(now - last) >= PROBE_REFRESH_MS) {
            if (adc_ok) dpc245_adc_sample32(&adc);
            draw_dynamic(stats, &adc, now);
            last = now;
        }
    }
}
