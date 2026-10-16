/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_KEYS_H
#define DPC245_KEYS_H

#include <stdbool.h>
#include <stdint.h>

/* Active-high pressed bits returned by the board GPIO reader. */
#define DPC245_KEYS_RAW_DOWN  0x01u
#define DPC245_KEYS_RAW_UP    0x02u
#define DPC245_KEYS_RAW_POWER 0x04u

typedef enum {
    DPC245_KEY_EVENT_NONE            = 0u,
    DPC245_KEY_EVENT_UP_SHORT        = 1u << 0,
    DPC245_KEY_EVENT_DOWN_SHORT      = 1u << 1,
    DPC245_KEY_EVENT_POWER_SHORT     = 1u << 2,
    DPC245_KEY_EVENT_UP_HOLD         = 1u << 3,
    DPC245_KEY_EVENT_SETTINGS_CHORD  = 1u << 4,
    DPC245_KEY_EVENT_DOWN_PRESS      = 1u << 5,
    DPC245_KEY_EVENT_DOWN_HOLD       = 1u << 6,
    DPC245_KEY_EVENT_DOWN_RELEASE    = 1u << 7,
    DPC245_KEY_EVENT_POWER_DOWN_HOLD = 1u << 8,
    DPC245_KEY_EVENT_UP_REPEAT       = 1u << 9,
    DPC245_KEY_EVENT_DOWN_REPEAT     = 1u << 10,
    DPC245_KEY_EVENT_UP_REPEAT_FAST  = 1u << 11,
    DPC245_KEY_EVENT_DOWN_REPEAT_FAST= 1u << 12,
    DPC245_KEY_EVENT_UP_PRESS        = 1u << 13
} dpc245_key_event_t;

/* The board key scanner is modeled on a 10 ms sampling cadence.  A normal
 * released action requires at least the 60 ms short-action threshold.  The
 * held/alternate action is emitted at 4.01 s. */
#define DPC245_KEY_SAMPLE_MS       10u
#define DPC245_KEY_DEBOUNCE_MS     35u
#define DPC245_KEY_SHORT_MIN_MS    60u
#define DPC245_KEY_HOLD_MS       4010u

/* EVistDrive settings shortcut.  This is intentionally an application
 * shortcut, independent of the standard key timing above. */
#define DPC245_SETTINGS_CHORD_MS 500u

/* A short DOWN action opens a five-second Walk authorization window. */
#define DPC245_WALK_WINDOW_MS 5000u

/* Convert the active-high pressed mask to the low-level key code. */
uint8_t dpc245_raw_key_code(uint8_t raw_mask);

typedef enum {
    DPC245_APPKEY_NONE             = 0x00u,
    DPC245_APPKEY_CHORD            = 0x02u,
    DPC245_APPKEY_POWER_DOWN_COMBO = 0x08u,
    DPC245_APPKEY_POWER            = 0x0Au,
    DPC245_APPKEY_UP_HOLD          = 0x11u,
    DPC245_APPKEY_DOWN_HOLD        = 0x12u,
    DPC245_APPKEY_DOWN_SHORT       = 0x13u,
    DPC245_APPKEY_UP_SHORT         = 0x14u,
    DPC245_APPKEY_VERY_LONG        = 0x1Bu
} dpc245_appkey_t;

/* Translate a low-level key code and event class into the application key
 * code.  event_class 1 = released/short, 0 = held/alternate, 3 = special. */
uint8_t dpc245_translate_appkey(uint8_t raw_key_code,
                               uint8_t event_class,
                               bool walk_armed,
                               bool chord_allowed);

typedef struct {
    uint8_t sampled_raw;
    uint8_t candidate_raw;
    uint8_t active_code;
    uint32_t last_sample_ms;
    uint32_t candidate_since_ms;
    uint32_t pressed_ms;
    uint32_t chord_started_ms;
    uint32_t repeat_deadline_ms;
    bool hold_sent;
    bool repeat_fast;
    bool chord_sent;
    bool suppress_short;
} dpc245_keys_t;

void dpc245_keys_init(dpc245_keys_t *k, uint8_t raw, uint32_t now_ms);

/* Feed the instantaneous active-high pressed mask.  The state machine samples
 * it at 100 Hz, accepts changes stable for 35 ms, and returns a bitmask of
 * dpc245_key_event_t values. */
uint32_t dpc245_keys_update(dpc245_keys_t *k, uint8_t raw, uint32_t now_ms);

#endif
