/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Selected public screenshots, rendered by the same UI code as make sim.
 * Usage: build/render_screenshots sim/out/screenshots
 * make screenshots compiles this file, converts its RGB565 frames to grid-free
 * 2x PNGs, and copies the named PNGs to docs/images/. */

#include "dpc245_app.h"
#include "dpc245_settings.h"
#include "ui_dashboard.h"
#include "can_router.h"
#include "advanced_settings.h"
#include "assist_profile.h"
#include "dpc245_hwinfo.h"
#include "dpc245_adc.h"
#include "st7789_8080.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern uint16_t sim_fb[DPC245_LCD_HEIGHT][DPC245_LCD_WIDTH];

static dpc245_app_t app;
static evist_can_router_t router;
static dpc245_adc_snapshot_t adc;
static dpc245_hwinfo_t hw;
static dpc245_ui_status_t status;
static const char *output_dir;

static bool save_frame(const char *name, uint32_t now_ms) {
    char path[512];
    if (snprintf(path, sizeof(path), "%s/%s.rgb565", output_dir, name) >= (int)sizeof(path)) {
        fputs("screenshot output path is too long\n", stderr);
        return false;
    }
    memset(sim_fb, 0, sizeof(sim_fb));
    ui_dashboard_draw(&app, &router, &adc, &hw, 0u, true, true, true, now_ms, &status);
    FILE *file = fopen(path, "wb");
    if (!file) { perror(path); return false; }
    for (unsigned y = 0u; y < DPC245_LCD_HEIGHT; ++y)
        for (unsigned x = 0u; x < DPC245_LCD_WIDTH; ++x) {
            uint16_t pixel = sim_fb[y][x];
            fputc((int)(pixel & 0xFFu), file);
            fputc((int)(pixel >> 8), file);
        }
    bool ok = fclose(file) == 0;
    if (!ok) perror(path);
    return ok;
}

static void set_dashboard(uint8_t theme, uint8_t assist, uint8_t panel,
                          uint32_t power_w, bool offroad) {
    app.screen = DPC245_APP_SCREEN_DASHBOARD;
    app.assist_level = assist;
    app.preferences.lower_panel = panel;
    app.power_graph_w = power_w;
    app.ride_indicator = offroad ? DPC245_RIDE_IND_OFFROAD : DPC245_RIDE_IND_LEGAL;
    app.temp_warning_sources = 0u;
    app.temp_warning_text_until_ms = 0u;
    dpc245_set_theme(&app.runtime.settings, theme);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fputs("usage: render_screenshots OUT_DIR\n", stderr);
        return 2;
    }
    output_dir = argv[1];

    dpc245_settings_t settings;
    dpc245_settings_defaults(&settings);
    dpc245_app_init(&app, &settings);
    evist_can_router_init(&router);
    bafang_telemetry_t *t = &router.telemetry;

    /* Settled, plausible ride sample. Distances are in 0.01 km; the power
     * graph scale is 1000 W, so 500/700/400 W occupy 50/70/40 percent. */
    t->have_speed = true;           t->speed_centi_kph = 2340u;
    t->have_cadence = true;         t->cadence_rpm = 76u;
    t->have_voltage = true;         t->voltage_centi_volt = 3660u;
    t->have_current = true;         t->current_centi_amp = 950u;
    t->have_battery_pct = true;     t->battery_pct = 76u;
    t->have_range = true;           t->range_centi_km = 4500u;
    t->have_controller_temp = true; t->controller_temp_c = 41;
    t->have_motor_temp = true;      t->motor_temp_c = 38;
    adc.valid = true;
    adc.als_raw = 2140u;
    adc.button_signal_raw = 3100u;
    status.light_on = true;
    status.ble_online = true;
    app.preferences.graph_scale_w = 1000u;
    app.preferences.battery_mask = EVIST_BATTERY_ALL;
    app.ride_apply_result = EVIST_RIDE_APPLY_OK;
    dpc245_set_u32_le(&app.runtime.settings, DPC245_OFS_TRIP_DISTANCE_RAW, 1450u);
    dpc245_set_u32_le(&app.runtime.settings, DPC245_OFS_TOTAL_DISTANCE_RAW, 78045u);

    set_dashboard(DPC245_THEME_STANDARD, 1u, 0u, 500u, false);
    if (!save_frame("standard_dashboard", 1000u)) return 1;
    set_dashboard(DPC245_THEME_YELLOW, 2u, 0u, 550u, false);
    if (!save_frame("yellow_dashboard", 1000u)) return 1;
    set_dashboard(DPC245_THEME_ARC, 4u, 7u, 700u, true);
    if (!save_frame("arc_dashboard", 1000u)) return 1;
    set_dashboard(DPC245_THEME_WEDGE, 1u, 0u, 400u, false);
    if (!save_frame("wedge_dashboard", 1000u)) return 1;

    set_dashboard(DPC245_THEME_STANDARD, 1u, 0u, 500u, false);
    app.temp_warning_sources = 2u;
    app.temp_ctrl_c = 102;
    app.temp_warning_text_until_ms = 4000u;
    if (!save_frame("overheat_warning", 1000u)) return 1;
    app.temp_warning_sources = 0u;

    app.screen = DPC245_APP_SCREEN_ROOT_MENU;
    app.selection = 0u;
    if (!save_frame("main_menu", 1000u)) return 1;
    app.screen = DPC245_APP_SCREEN_THEMES_MENU;
    app.selection = 2u;
    app.stored_theme = DPC245_THEME_STANDARD;
    app.pending_theme = DPC245_THEME_ARC;
    if (!save_frame("themes_page", 1000u)) return 1;

    app.screen = DPC245_APP_SCREEN_ADVANCED;
    evist_advanced_open(&app.advanced, &app.preferences, 5u);
    app.advanced.screen = EVIST_ADV_TEMP_VIEW;
    app.advanced.working.motor_temp_limit_c = 125u;
    app.advanced.working.ctrl_temp_limit_c = 95u;
    app.advanced.cursor = 3u;
    if (!save_frame("temp_view_limits", 1000u)) return 1;
    app.preferences.assist_level = 1u;
    app.preferences.assist_mode_mask = (uint8_t)((1u << 0u) | (1u << 3u) | (1u << 4u));
    evist_advanced_open(&app.advanced, &app.preferences, 5u);
    app.advanced.screen = EVIST_ADV_ASSIST;
    {
        /* Sample 0x6011 block as returned by a controller READ: POWER % and
         * SPEED % for the nine slots, both non-decreasing (BAFANG profile). */
        static const uint8_t power[EVIST_ASSIST_SLOT_COUNT] = { 15u, 25u, 35u, 45u, 55u, 65u, 75u, 90u, 100u };
        static const uint8_t speed[EVIST_ASSIST_SLOT_COUNT] = { 40u, 50u, 60u, 70u, 80u, 85u, 90u, 95u, 100u };
        uint8_t block[EVIST_ASSIST_BLOCK_LEN] = { 0u };
        block[0] = 48u;
        for (unsigned i = 0u; i < EVIST_ASSIST_SLOT_COUNT; ++i) {
            block[EVIST_ASSIST_POWER_OFF + i] = power[i];
            block[EVIST_ASSIST_SPEED_OFF + i] = speed[i];
        }
        block[EVIST_ASSIST_BLOCK_LEN - 1u] = evist_assist_checksum(block);
        evist_advanced_take_request(&app.advanced, NULL);
        evist_advanced_on_result(&app.advanced, EVIST_TXN_READ_ASSIST, EVIST_TXN_OK, block);
    }
    if (!save_frame("advanced_assist", 1000u)) return 1;

    evist_advanced_open(&app.advanced, &app.preferences, 5u);
    app.advanced.screen = EVIST_ADV_MODES;
    app.advanced.cursor = 3u;
    if (!save_frame("assist_modes", 1000u)) return 1;

    puts("screenshots: ten frames rendered");
    return 0;
}
