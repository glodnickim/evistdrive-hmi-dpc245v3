/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef EVIST_PREFERENCES_H
#define EVIST_PREFERENCES_H

#include "dpc245_persist.h"
#include <stdbool.h>
#include <stdint.h>

#define EVIST_PANEL_COUNT 10u
#define EVIST_BASE_PANEL_COUNT 9u
#define EVIST_PANEL_MASK_ALL 0x03FFu
#define EVIST_ASSIST_MODE_COUNT 5u
#define EVIST_ASSIST_MODE_MASK_ALL 0x1Fu

typedef enum {
    EVIST_TEMP_BOTH = 0,
    EVIST_TEMP_ALTERNATE,
    EVIST_TEMP_1,
    EVIST_TEMP_2
} evist_temp_view_t;

#define EVIST_BATTERY_SOC  0x01u
#define EVIST_BATTERY_VOLT 0x02u
#define EVIST_BATTERY_ICON 0x04u
#define EVIST_BATTERY_BAR  0x08u
#define EVIST_BATTERY_ALL  0x0Fu

typedef enum {
    EVIST_PROFILE_BAFANG = 0,
    EVIST_PROFILE_FAKE_TAXI = 1
} evist_firmware_profile_t;

typedef enum {
    EVIST_PREF_LIGHT_AUTO = 0,
    EVIST_PREF_LIGHT_MANUAL = 1
} evist_pref_light_mode_t;

/* Default upper scale of the power graph, in watts. */
#define EVIST_PREF_GRAPH_DEFAULT_W 600u

typedef struct {
    uint8_t assist_level;       /* 0..9 */
    uint8_t assist_mode_mask;   /* bits 0..4: enabled E, T, S, S+, B levels */
    uint8_t lower_panel;        /* 0..9 */
    evist_temp_view_t temp_view;
    uint8_t motor_temp_limit_c; /* 0=OFF, otherwise 40..150 C */
    uint8_t ctrl_temp_limit_c;  /* 0=OFF, otherwise 40..150 C */
    evist_firmware_profile_t firmware_profile;
    uint16_t graph_scale_w;     /* 250..3000, 25 W step, default EVIST_PREF_GRAPH_DEFAULT_W */
    uint8_t battery_mask;       /* EVIST_BATTERY_* */
    evist_pref_light_mode_t light_mode;
    uint8_t light_gain;         /* 1..9 */
    uint8_t light_dwell;        /* 1..30 */
    uint16_t light_threshold;   /* 1800..5000, 100 step */
    uint8_t light_level;        /* 0..15 */
    uint16_t panel_mask;        /* bits 0..9, never zero */
    bool quick_walk_enabled;
    uint8_t quick_walk_hold_steps; /* 1..10, each 0.5 s */
} evist_preferences_t;

/* External SPI memory layout of the settings logs:
 *   0x1F0000  16-byte header "EVP1..."
 *   0x1F0010  assist/panel pair log, 2-byte records     (up to 0x1FDE00)
 *   0x1FDE00  RIDE MODE log, 6-byte records (reserved, never written here)
 *   0x1FE000  settings code log, 2-byte records
 *   0x1FFF00  read-only settings code log (read on load, never written) */
#define EVIST_PREF_REGION_BASE   0x001F0000u
#define EVIST_PREF_REGION_END    0x00200000u
#define EVIST_PREF_PAIR_BASE     0x001F0010u
#define EVIST_PREF_RIDE_BASE     0x001FDE00u
#define EVIST_PREF_RIDE_END      0x001FE000u
#define EVIST_PREF_SET_BASE      0x001FE000u
#define EVIST_PREF_SET_END       0x001FFF00u

typedef enum {
    EVIST_PREF_SAVE_OK = 0,
    EVIST_PREF_SAVE_INVALID,      /* rejected before touching the memory */
    EVIST_PREF_SAVE_NO_MEMORY,    /* memory missing, unreadable or foreign data */
    EVIST_PREF_SAVE_LOG_FULL,     /* append-only log has no room (never erased) */
    EVIST_PREF_SAVE_WRITE_FAILED, /* a record did not program as written */
    EVIST_PREF_SAVE_VERIFY_FAILED, /* a fresh load does not return the saved values */
    EVIST_PREF_SAVE_IN_PROGRESS
} evist_pref_save_result_t;

/* One slot per stored setting: eleven values and nine PANELS bits.
 * TEMP VIEW and the TEMP panel bit use the unused 0x40 increment record. */
#define EVIST_PREF_SETTING_SLOTS 20u
#define EVIST_PREF_MAX_CODES (EVIST_PREF_SETTING_SLOTS + 7u)

/* What the memory holds, before defaults are applied: the newest valid code
 * of every setting (settings log first, then the read-only log), the
 * newest valid pair record and where both append-only logs end. */
typedef struct {
    uint32_t found;                       /* bit n: code[n] was read from memory */
    uint8_t code[EVIST_PREF_SETTING_SLOTS];
    bool header;                          /* false: blank area, nothing stored yet */
    bool have_pair;
    uint8_t pair;                         /* byte 0 of the newest valid pair record */
    uint32_t set_end;                     /* address of the first blank settings record */
    uint32_t pair_end;                    /* index of the first blank pair record */
    uint8_t temp_state;                   /* valid 0x40 records modulo eight */
} evist_pref_image_t;

void evist_preferences_defaults(evist_preferences_t *p);
bool evist_preferences_validate(const evist_preferences_t *p);
/* False when the area has no header, a read fails or the result is invalid;
 * `p` then holds the defaults, which say nothing about the memory. */
bool evist_preferences_load(const dpc245_persist_io_t *io, evist_preferences_t *p);
/* Appends the fields of `after` that differ from `before` (`before` must be
 * what is stored; NULL = unknown, every field is written), then reloads the
 * whole memory: OK means a fresh load returns exactly `after`. A failure can
 * leave part of the records written. The firmware uses evist_pref_store_*. */
evist_pref_save_result_t evist_preferences_save(const dpc245_persist_io_t *io,
                                                const evist_preferences_t *before,
                                                const evist_preferences_t *after);

/* Owner of the verified memory snapshot. known is true only after a complete
 * successful load/blank proof or verified append. Runtime recovery of UNKNOWN
 * is cooperative: begin takes a validated immutable target; step advances
 * one bounded scan/search/save per service loop. No failure triggers a full
 * synchronous re-scan. Idle step is INVALID, never an unsolicited SAVED.
 * The application coalesces later requests outside the active store job. */
typedef struct {
    evist_preferences_t persisted;
    evist_pref_image_t image;
    bool known;
    bool saving;
    uint8_t state;
    uint32_t scan_addr;
    evist_preferences_t target;
    uint8_t codes[EVIST_PREF_MAX_CODES];
    size_t code_count;
    size_t code_pos;
    bool write_pair;
} evist_pref_store_t;

void evist_pref_store_init(evist_pref_store_t *s, const dpc245_persist_io_t *io,
                           evist_preferences_t *loaded);
bool evist_pref_store_busy(const evist_pref_store_t *s);
bool evist_pref_store_begin(evist_pref_store_t *s, const evist_preferences_t *after);
evist_pref_save_result_t evist_pref_store_save_step(evist_pref_store_t *s, const dpc245_persist_io_t *io);
/* Synchronous compatibility helper; not used by the firmware service loop. */
evist_pref_save_result_t evist_pref_store_save(evist_pref_store_t *s, const dpc245_persist_io_t *io,
                                               const evist_preferences_t *after);
const char *evist_pref_save_text(evist_pref_save_result_t result);

/* Pair records store a field code, not the panel index. */
uint8_t evist_pref_panel_from_record_field(uint8_t field);
uint8_t evist_pref_record_field_from_panel(uint8_t panel);
/* -1 means no existing single-record setting owns this code. */
int evist_pref_setting_slot_of_code(uint8_t code);

#endif
