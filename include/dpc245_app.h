/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_APP_H
#define DPC245_APP_H

#include "dpc245_keys.h"
#include "dpc245_runtime.h"
#include "evist_preferences.h"
#include "advanced_settings.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Application navigation and state model. Rendering is isolated so the model can be host-tested. */
typedef enum {
    DPC245_APP_SCREEN_DASHBOARD = 0,
    DPC245_APP_SCREEN_ROOT_MENU,
    DPC245_APP_SCREEN_DISPLAY_MENU,
    DPC245_APP_SCREEN_BIKE_MENU,
    DPC245_APP_SCREEN_INFO_MENU,
    DPC245_APP_SCREEN_THEMES_MENU,
    DPC245_APP_SCREEN_EDIT_UNIT,
    DPC245_APP_SCREEN_EDIT_SERVICE_TIPS,
    DPC245_APP_SCREEN_EDIT_AUTO_OFF,
    DPC245_APP_SCREEN_TRIP_RESET,
    DPC245_APP_SCREEN_EDIT_BRIGHTNESS,
    DPC245_APP_SCREEN_EDIT_AL_SENS,
    DPC245_APP_SCREEN_INFO_HMI,
    DPC245_APP_SCREEN_INFO_WHEEL,
    DPC245_APP_SCREEN_INFO_SPEED_LIMIT,
    DPC245_APP_SCREEN_INFO_BATTERY,
    DPC245_APP_SCREEN_INFO_CONTROLLER,
    DPC245_APP_SCREEN_INFO_SENSOR,
    DPC245_APP_SCREEN_INFO_ERRORS,
    DPC245_APP_SCREEN_ADVANCED
} dpc245_app_screen_t;

typedef enum {
    DPC245_APP_KEY_NONE = 0,
    DPC245_APP_KEY_UP,
    DPC245_APP_KEY_DOWN,
    DPC245_APP_KEY_POWER
} dpc245_app_key_t;

/* Order of the short-POWER multifunction cycle. */
typedef enum {
    DPC245_DASH_FIELD_TRIP = 0,
    DPC245_DASH_FIELD_ODO,
    DPC245_DASH_FIELD_MAX,
    DPC245_DASH_FIELD_AVG,
    DPC245_DASH_FIELD_RANGE,
    DPC245_DASH_FIELD_CADENCE,
    DPC245_DASH_FIELD_CAL,
    DPC245_DASH_FIELD_TIME,
    DPC245_DASH_FIELD_WATT,
    DPC245_DASH_FIELD_COUNT
} dpc245_dashboard_field_t;

enum { DPC245_RIDE_IND_NONE = 0, DPC245_RIDE_IND_LEGAL = 1, DPC245_RIDE_IND_OFFROAD = 2 };

typedef struct {
    uint32_t uptime_s;
    uint32_t sample_age_ms;
    uint8_t sources;
    int16_t motor_c;
    int16_t ctrl_c;
    bool motor_valid;
    bool ctrl_valid;
    uint8_t motor_limit_c;
    uint8_t ctrl_limit_c;
    uint8_t raw6;
    uint8_t raw7;
} dpc245_temp_warning_record_t;

typedef struct {
    int16_t sampled_c;
    uint8_t limit_c;
    uint32_t sample_age_ms;
    uint32_t pending_ms;
    bool sample_received;
    bool sensor_valid;
    bool fresh;
    bool decision_valid;
    bool transition;
    bool pending;
} dpc245_temp_live_source_t;

typedef struct {
    dpc245_runtime_t runtime;
    evist_preferences_t preferences;
    evist_advanced_t advanced;
    dpc245_app_screen_t screen;
    dpc245_dashboard_field_t dashboard_field;
    uint8_t selection;
    uint8_t detail_page; /* Information sub-page selector */
    uint8_t list_selection; /* row of the list the user left; restored on return */
    uint8_t pending_theme;
    uint8_t stored_theme;
    uint8_t theme_save_target;
    bool theme_save_pending;
    bool theme_save_failed;
    const char *theme_status;
    dpc245_app_screen_t advanced_parent;
    uint8_t advanced_parent_selection;
    uint8_t assist_level_count;
    uint8_t assist_level;
    bool walk; /* wire-level walk state sent in the 0x6300 heartbeat */
    bool down_held; /* physical DOWN key state, independent of Walk request state */
    bool up_repeat_seen;
    bool down_repeat_seen;
    bool walk_armed;
    bool walk_active;
    bool walk_quick; /* true only for the EVistDrive press-and-hold shortcut */
    bool walk_hold_latched; /* standard Walk held action has occurred */
    bool suppress_down_short_once;
    uint32_t walk_deadline_ms; /* standard: expiry; quick pending: activation */
    bool light_toggle_requested;
    bool settings_dirty;
    bool preferences_dirty;
    bool trip_reset_done;
    uint32_t key_events;
    uint32_t last_key_event_ms;
    bool idle_controller_busy;
    uint16_t light_als_filtered;      /* read-only LIGHT preview, mirrored by main loop */
    uint8_t light_backlight_position; /* 0..16 */
    uint32_t loop_max_us;     /* longest main-loop pass since start (DIAG timing page) */
    uint32_t loop_1s_max_us;  /* longest pass in the last completed second */
    uint32_t stage_max_us[8]; /* CAN RX, BLE RX, ADC, APP, SPI, CAN TX, BLE TX, LCD */
    uint8_t ride_indicator; /* DPC245_RIDE_IND_*: what the dashboard shows above
                               the power bar; NONE until RIDE MODE reports */
    evist_ride_cfg_t ride_cfg;
    /* NONE follows the saved record; the other choices last until restart. */
    enum { DPC245_RIDE_SESSION_NONE, DPC245_RIDE_SESSION_LEGAL,
           DPC245_RIDE_SESSION_OFFROAD } ride_session;
    bool ride_confirm_pending;
    uint8_t ride_combo_step;
    uint32_t ride_combo_started_ms;
    uint32_t ride_confirm_deadline_ms;
    bool ride_apply_requested;
    bool ride_cfg_dirty;
    uint8_t ride_pending_session; /* DPC245_RIDE_SESSION_* */
    bool ride_combo_apply;
    bool ride_toast_active;
    uint32_t ride_toast_deadline_ms;
    uint8_t temp_warning_sources; /* bit 0 motor, bit 1 controller */
    bool temp_screen_seen;
    uint8_t temp_screen_sources; /* warning bits in the last completed dashboard render */
    uint32_t temp_screen_uptime_s;
    uint32_t temp_screen_render_ms;
    uint32_t temp_warning_activations;
    dpc245_temp_warning_record_t temp_warning_last;
    int16_t temp_motor_max_c;
    int16_t temp_ctrl_max_c;
    bool temp_motor_max_valid;
    bool temp_ctrl_max_valid;
    dpc245_temp_live_source_t temp_live[2]; /* motor, controller; last warning tick */
    uint32_t temp_warning_text_until_ms;
    uint32_t temp_transition_started_ms[2];
    bool temp_transition_pending[2];
    bool temp_sample_received;
    uint32_t temp_sample_rx_ms;
    int16_t temp_motor_c;
    int16_t temp_ctrl_c;
    bool temp_motor_valid;
    bool temp_ctrl_valid;
    uint8_t temp_raw6;
    uint8_t temp_raw7;
    uint16_t ride_last_applied_x100;
    uint32_t power_graph_w;
    uint32_t power_graph_tick_ms;
    uint16_t start_angle_raw;
    uint8_t start_angle_status; /* 0=not started, 1=in progress, 3=done, 4=error */
    evist_ride_apply_phase_t ride_apply_result;
    evist_txn_result_t ride_last_error;
    bool last_write_seen;
    uint16_t last_write_command;
    uint8_t last_write_acks;
    bool last_write_final_ack;
    bool last_write_error;
    uint8_t last_write_error_payload[4];
    evist_txn_result_t last_write_result;
    uint32_t ble_rejected_writes;
    uint8_t ble_rejected_class;
    uint8_t ble_rejected_command;
    uint8_t ble_rejected_operation;
    uint8_t ble_rejected_selector;
    uint8_t ble_rejected_reason;
} dpc245_app_t;

void dpc245_app_init(dpc245_app_t *c,
                             const dpc245_settings_t *settings);

/* Discrete key entry point used by menu unit tests.  Dashboard
 * semantics: POWER cycles the multifunction field; Settings is
 * entered through DPC245_KEY_EVENT_SETTINGS_CHORD. */
void dpc245_app_key(dpc245_app_t *c, dpc245_app_key_t key);
void dpc245_app_apply_preferences(dpc245_app_t *c, const evist_preferences_t *prefs);

/* Timed input events from dpc245_keys.c. */
void dpc245_app_event(dpc245_app_t *c,
                              dpc245_key_event_t event,
                              uint32_t now_ms);
/* The physical release also clears an active held Walk when event delivery is delayed. */
void dpc245_app_down_state(dpc245_app_t *c,bool down_pressed,uint32_t now_ms);
void dpc245_app_tick(dpc245_app_t *c, uint32_t now_ms);
void dpc245_app_temperature_sample(dpc245_app_t *c, const bafang_telemetry_t *t);
/* Call only after a complete dashboard frame, with the app snapshot used to draw it. */
void dpc245_app_dashboard_render_complete(dpc245_app_t *c,
                                          const dpc245_app_t *rendered,uint32_t completed_at_ms);
void dpc245_app_power_graph_tick(dpc245_app_t *c, int32_t measured_w, uint32_t now_ms);

/* One-shot hardware request generated by the application. */
bool dpc245_app_take_light_toggle(dpc245_app_t *c);

/* Returns true once after an edit that changed the persistent record. */
bool dpc245_app_take_settings_dirty(dpc245_app_t *c);
void dpc245_app_theme_store_result(dpc245_app_t *c, bool saved, uint8_t theme);
const char *dpc245_app_theme_row_value(const dpc245_app_t *c, uint8_t theme);
bool dpc245_app_take_preferences_dirty(dpc245_app_t *c, evist_preferences_t *out);
void dpc245_app_set_ride(dpc245_app_t *c,const evist_ride_cfg_t *cfg);
bool dpc245_app_take_ride_cfg_dirty(dpc245_app_t *c,evist_ride_cfg_t *out);
bool dpc245_app_take_ride_apply(dpc245_app_t *c,uint16_t *target);
void dpc245_app_ride_applied(dpc245_app_t *c,const evist_ride_apply_t *result);
void dpc245_app_ride_wheel_verified(dpc245_app_t *c,const uint8_t data[64]);

/* Menu labels/order. */
size_t dpc245_app_menu_count(dpc245_app_screen_t screen);
const char *dpc245_app_menu_label(dpc245_app_screen_t screen, size_t index);
const char *dpc245_app_dashboard_field_label(dpc245_dashboard_field_t field);

/* Number of information pages for the selected detail screen.
 * The app uses UP/DOWN to page and POWER to return to Information. */
size_t dpc245_app_detail_page_count(dpc245_app_screen_t screen);

#endif
