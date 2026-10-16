/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef EVIST_ASSIST_PROFILE_H
#define EVIST_ASSIST_PROFILE_H

/* Controller assist table (0x6011, 64-byte block): editing rules.
 *
 * The block holds two 9-slot columns: POWER (current limit, %) from byte 40 and
 * SPEED (BAFANG profile) or ASSIST % (FAKE TAXI profile) from byte 49. The user
 * sees at most five levels; each visible level maps to one slot and the hidden
 * slots are kept consistent with their visible neighbours.
 *
 * BAFANG profile: both columns must be non-decreasing, visible and hidden
 * slots alike. FAKE TAXI firmware accepts any order and is left free.
 * Pure logic, no hardware access. */

#include "evist_preferences.h"
#include <stdbool.h>
#include <stdint.h>

#define EVIST_ASSIST_BLOCK_LEN   64u
#define EVIST_ASSIST_SLOT_COUNT  9u
#define EVIST_ASSIST_POWER_OFF   40u
#define EVIST_ASSIST_SPEED_OFF   49u
#define EVIST_ASSIST_VALUE_MAX   100u
#define EVIST_ASSIST_VISIBLE_MAX 5u

typedef enum {
    EVIST_ASSIST_COLUMN_POWER = 0,
    EVIST_ASSIST_COLUMN_SPEED = 1
} evist_assist_column_t;

/* 3, 5 or 9 levels; anything else is treated as 5. */
uint8_t evist_assist_level_count_normalize(uint8_t level_count);
uint8_t evist_assist_visible_levels(uint8_t level_count);
/* Slot (0..8) that holds visible level 1..n. */
uint8_t evist_assist_level_slot(uint8_t level_count, uint8_t level);
uint8_t evist_assist_column_offset(evist_assist_column_t column);

uint8_t evist_assist_checksum(const uint8_t block[EVIST_ASSIST_BLOCK_LEN]);
/* Read validity is the 64-byte block checksum. Byte 0 is controller system
 * voltage and is preserved untouched; write policy separately validates the
 * ASSIST value range and profile ordering. */
bool evist_assist_block_valid(const uint8_t block[EVIST_ASSIST_BLOCK_LEN]);

/* Allowed value range for one visible level while editing. */
void evist_assist_edit_bounds(const uint8_t block[EVIST_ASSIST_BLOCK_LEN],
                              evist_firmware_profile_t profile, uint8_t level_count,
                              evist_assist_column_t column, uint8_t level,
                              uint8_t *lo, uint8_t *hi);

/* One UP/DOWN step on a visible level. Returns true when the value changed.
 * For BAFANG the hidden slots of that column are reconciled afterwards. */
bool evist_assist_edit_step(uint8_t block[EVIST_ASSIST_BLOCK_LEN],
                            evist_firmware_profile_t profile, uint8_t level_count,
                            evist_assist_column_t column, uint8_t level, bool up);

bool evist_assist_column_monotonic(const uint8_t block[EVIST_ASSIST_BLOCK_LEN],
                                   evist_assist_column_t column);

/* Final check before a write, in ascending order: visible levels non-decreasing,
 * hidden slots reconciled, all nine slots non-decreasing; checksum refreshed.
 * FAKE TAXI only gets the checksum. Returns false (block untouched) when the
 * visible levels are out of order. */
bool evist_assist_prepare_write(uint8_t block[EVIST_ASSIST_BLOCK_LEN],
                                evist_firmware_profile_t profile, uint8_t level_count);

/* Policy check at the transaction boundary (independent of the UI). */
bool evist_assist_write_allowed(const uint8_t block[EVIST_ASSIST_BLOCK_LEN],
                                evist_firmware_profile_t profile);

#endif
