/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef EVIST_ADVANCED_SETTINGS_H
#define EVIST_ADVANCED_SETTINGS_H

/* ADVANCED SETTINGS menu model (no drawing, no CAN).
 *
 * WHEEL, ASSIST and ACCEL edit private full-block buffers populated by a page
 * READ or the verified BLE startup cache. Edits survive until SAVE or BACK.
 * The main loop executes requests through controller_write.h and reports the
 * outcome with evist_advanced_on_result(). */

#include "evist_preferences.h"
#include "controller_write.h"
#include "ride_mode.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { EVIST_ADV_KEY_NONE=0, EVIST_ADV_KEY_UP, EVIST_ADV_KEY_DOWN, EVIST_ADV_KEY_POWER } evist_adv_key_t;
typedef enum { EVIST_ADV_MENU=0, EVIST_ADV_WHEEL, EVIST_ADV_ASSIST, EVIST_ADV_ACCEL,
               EVIST_ADV_GRAPH, EVIST_ADV_FIRMWARE, EVIST_ADV_BATTERY, EVIST_ADV_LIGHT,
               EVIST_ADV_PANELS, EVIST_ADV_QUICK_WALK, EVIST_ADV_MODE, EVIST_ADV_DIAG,
               EVIST_ADV_TEMP_VIEW, EVIST_ADV_MODES } evist_adv_screen_t;

/* DIAG is a read-only, page-based screen (no CAN, no writes). */
#define EVIST_DIAG_PAGES 7u

typedef enum {
    EVIST_ADV_REQ_NONE = 0,
    EVIST_ADV_REQ_READ_WHEEL,
    EVIST_ADV_REQ_WRITE_WHEEL,
    EVIST_ADV_REQ_READ_ASSIST,
    EVIST_ADV_REQ_WRITE_ASSIST,
    EVIST_ADV_REQ_DISCOVER_ACCEL,
    EVIST_ADV_REQ_READ_ACCEL_P0,
    EVIST_ADV_REQ_READ_ACCEL_P2,
    EVIST_ADV_REQ_WRITE_ACCEL_P0,
    EVIST_ADV_REQ_WRITE_ACCEL_P2
} evist_adv_request_t;

typedef enum { EVIST_ACCEL_UNKNOWN=0, EVIST_ACCEL_PER_LEVEL, EVIST_ACCEL_GLOBAL } evist_accel_mode_t;

/* WHEEL page rows */
enum { EVIST_WHEEL_ROW_SPEED = 0, EVIST_WHEEL_ROW_CIRC, EVIST_WHEEL_ROW_DIAMETER,
       EVIST_WHEEL_ROW_SAVE, EVIST_WHEEL_ROW_BACK, EVIST_WHEEL_ROWS };

/* Validated limits for the 0x3203 write */
#define EVIST_WHEEL_SPEED_MIN   1000u  /* 10.00 km/h */
#define EVIST_WHEEL_SPEED_MAX   7000u  /* 70.00 km/h */
#define EVIST_WHEEL_SPEED_STEP    50u
#define EVIST_WHEEL_CIRC_MIN     500u
#define EVIST_WHEEL_CIRC_MAX    3000u
#define EVIST_WHEEL_CIRC_STEP     10u
#define EVIST_WHEEL_DIAMETERS      7u  /* index 0 = keep the code read from the controller */

typedef struct {
    evist_adv_screen_t screen;
    uint8_t cursor;
    bool edit;
    bool repeat_active; /* transient: repeat steps clamp instead of wrapping */
    evist_preferences_t working;
    evist_preferences_t committed;
    bool preferences_changed;
    bool exit_requested;
    const char *status;
    uint8_t assist_level_count;

    evist_adv_request_t request;  /* waiting to be taken by the main loop */
    evist_adv_request_t active_request; /* request currently owned by the transaction engine */
    bool busy;                    /* taken, result not reported yet */

    bool wheel_valid;
    evist_wheel_config_t wheel_read;  /* last value confirmed by the controller */
    uint16_t wheel_speed_centi_kph;   /* edit buffer */
    uint16_t wheel_circumference_mm;
    uint8_t wheel_diameter_index;
    bool wheel_dirty;

    bool assist_valid;
    uint8_t assist_block[64];         /* edit buffer */
    uint8_t assist_committed[64];     /* last controller-confirmed block */
    bool assist_dirty;

    evist_accel_mode_t accel_mode; /* retained across ADV visits until restart */
    bool accel_valid;
    bool accel_dirty;
    uint8_t accel_block[64];
    uint8_t accel_committed[64];
    uint8_t accel_values[9];
    uint8_t accel_global;

    uint8_t diag_page;                /* 0..EVIST_DIAG_PAGES-1, read-only */

    bool light_runtime_dirty;         /* LIGHT working values must reach the runtime */
    uint8_t menu_cursor;              /* MENU row to restore when a page returns to it */
    evist_ride_cfg_t ride_working;
    evist_ride_cfg_t ride_committed;
    evist_ride_mode_t ride_effective_mode;
    bool ride_changed;
} evist_advanced_t;

void evist_advanced_open(evist_advanced_t *a, const evist_preferences_t *prefs, uint8_t assist_level_count);
void evist_advanced_open_page(evist_advanced_t *a, evist_adv_screen_t screen);
void evist_advanced_key(evist_advanced_t *a, evist_adv_key_t key);
/* Discard the current page's unsaved edits, as if BACK was selected. */
void evist_advanced_abandon(evist_advanced_t *a);
bool evist_advanced_take_preferences(evist_advanced_t *a, evist_preferences_t *out);
/* LIGHT edits act on the runtime at once. Returns the working
 * values after each LIGHT edit, and after BACK the committed values again. */
bool evist_advanced_take_light_runtime(evist_advanced_t *a, evist_preferences_t *out);
bool evist_advanced_take_exit(evist_advanced_t *a);
void evist_advanced_set_status(evist_advanced_t *a, const char *status);
void evist_advanced_set_ride(evist_advanced_t *a, const evist_ride_cfg_t *cfg);
bool evist_advanced_take_ride(evist_advanced_t *a, evist_ride_cfg_t *out);

/* Controller requests. take_request marks the page busy until on_result. */
bool evist_advanced_take_request(evist_advanced_t *a, evist_adv_request_t *out);
void evist_advanced_wheel_target(const evist_advanced_t *a, evist_wheel_config_t *out);
void evist_advanced_on_result(evist_advanced_t *a, evist_txn_kind_t kind,
                              evist_txn_result_t result, const uint8_t data[64]);
/* Report that a taken request could not even be started. */
void evist_advanced_request_failed(evist_advanced_t *a, const char *status);

/* View helpers */
size_t evist_advanced_row_count(const evist_advanced_t *a);
const char *evist_advanced_menu_label(size_t index);
const char *evist_advanced_screen_title(evist_adv_screen_t screen);
const char *evist_advanced_wheel_diameter_label(const evist_advanced_t *a);
/* ASSIST shows POWER and SPEED for each enabled visible mode, then SAVE/BACK. */
uint8_t evist_advanced_assist_cells(const evist_advanced_t *a);
uint8_t evist_advanced_assist_cell_level(const evist_advanced_t *a, uint8_t cell);
uint8_t evist_advanced_assist_cell_value(const evist_advanced_t *a, uint8_t cell);
#endif
