/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef UI_DASHBOARD_H
#define UI_DASHBOARD_H
#include "dpc245_app.h"
#include "can_router.h"
#include "dpc245_adc.h"
#include "dpc245_hwinfo.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool light_on;
    bool ble_online; /* true while valid app requests arrive */
    bool controller_error;
} dpc245_ui_status_t;

void ui_boot_splash_draw(void);

void ui_dashboard_lower_pair_text(const dpc245_app_t *app, const bafang_telemetry_t *telemetry,
                                  uint8_t panel, uint32_t now_ms, char left[6], char right[6],
                                  const char **left_label, const char **right_label,
                                  const char **left_unit, const char **right_unit);

void ui_dashboard_temp_limit_text(char text[6], uint8_t limit);

void ui_dashboard_draw(const dpc245_app_t *app,
                         const evist_can_router_t *router,
                         const dpc245_adc_snapshot_t *adc,
                         const dpc245_hwinfo_t *hw,
                         uint8_t persist_slot,
                         bool persist_loaded,
                         bool persist_write_enabled,
                         bool can_tx_enabled,
                         uint32_t now_ms,
                         const dpc245_ui_status_t *status);

#ifdef UI_FONT1_TRACE
extern void (*ui_dashboard_speed_cells_trace)(int units_x0, int dot_x, int tenths_x0,
                                              int tens_x0, int tens_x1, int units_x1,
                                              int tenths_x1, int tens_drawn);
#endif
#endif
