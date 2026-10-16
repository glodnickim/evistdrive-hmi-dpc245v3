/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "assist_profile.h"

uint8_t evist_assist_level_count_normalize(uint8_t level_count) {
    if (level_count == 3u || level_count == 5u || level_count == 9u) return level_count;
    return 5u;
}

uint8_t evist_assist_visible_levels(uint8_t level_count) {
    uint8_t n = evist_assist_level_count_normalize(level_count);
    return n > EVIST_ASSIST_VISIBLE_MAX ? EVIST_ASSIST_VISIBLE_MAX : n;
}

uint8_t evist_assist_level_slot(uint8_t level_count, uint8_t level) {
    static const uint8_t slots3[3] = {2u, 4u, 8u};
    static const uint8_t slots5[5] = {1u, 3u, 5u, 7u, 8u};
    uint8_t n = evist_assist_level_count_normalize(level_count);
    if (level == 0u) return 0u;
    if (n == 3u && level <= 3u) return slots3[level - 1u];
    if (n == 5u && level <= 5u) return slots5[level - 1u];
    return (uint8_t)(level <= EVIST_ASSIST_SLOT_COUNT ? level - 1u : EVIST_ASSIST_SLOT_COUNT - 1u);
}

uint8_t evist_assist_column_offset(evist_assist_column_t column) {
    return column == EVIST_ASSIST_COLUMN_SPEED ? EVIST_ASSIST_SPEED_OFF : EVIST_ASSIST_POWER_OFF;
}

uint8_t evist_assist_checksum(const uint8_t block[EVIST_ASSIST_BLOCK_LEN]) {
    uint8_t sum = 0u;
    for (uint8_t i = 0u; i < EVIST_ASSIST_BLOCK_LEN - 1u; ++i) sum = (uint8_t)(sum + block[i]);
    return sum;
}

bool evist_assist_block_valid(const uint8_t block[EVIST_ASSIST_BLOCK_LEN]) {
    return block && evist_assist_checksum(block) == block[EVIST_ASSIST_BLOCK_LEN - 1u];
}

static bool assist_values_in_range(const uint8_t block[EVIST_ASSIST_BLOCK_LEN]) {
    for (uint8_t i = 0u; i < EVIST_ASSIST_SLOT_COUNT; ++i) {
        if (block[EVIST_ASSIST_POWER_OFF + i] > EVIST_ASSIST_VALUE_MAX) return false;
        if (block[EVIST_ASSIST_SPEED_OFF + i] > EVIST_ASSIST_VALUE_MAX) return false;
    }
    return true;
}

static uint8_t level_value(const uint8_t *block, uint8_t off, uint8_t level_count, uint8_t level) {
    return block[off + evist_assist_level_slot(level_count, level)];
}

static bool visible_monotonic(const uint8_t *block, uint8_t level_count, uint8_t off) {
    uint8_t show = evist_assist_visible_levels(level_count);
    for (uint8_t level = 2u; level <= show; ++level)
        if (level_value(block, off, level_count, level) < level_value(block, off, level_count, level - 1u))
            return false;
    return true;
}

/* Hidden slots follow the visible ones: below level 1 they never exceed it,
 * between two visible levels they are clamped into that interval, above the
 * last visible level they never fall below it. */
static void reconcile_hidden(uint8_t *block, uint8_t level_count, uint8_t off) {
    uint8_t show = evist_assist_visible_levels(level_count);
    uint8_t first = evist_assist_level_slot(level_count, 1u);
    uint8_t last = evist_assist_level_slot(level_count, show);

    uint8_t next = block[off + first];
    for (uint8_t j = first; j > 0u;) {
        --j;
        if (block[off + j] > next) block[off + j] = next;
        next = block[off + j];
    }
    for (uint8_t level = 1u; level < show; ++level) {
        uint8_t a = evist_assist_level_slot(level_count, level);
        uint8_t b = evist_assist_level_slot(level_count, (uint8_t)(level + 1u));
        uint8_t high = block[off + b];
        uint8_t prev = block[off + a];
        for (uint8_t j = (uint8_t)(a + 1u); j < b; ++j) {
            uint8_t v = block[off + j];
            if (v < prev) v = prev;
            if (v > high) v = high;
            block[off + j] = v;
            prev = v;
        }
    }
    uint8_t prev = block[off + last];
    for (uint8_t j = (uint8_t)(last + 1u); j < EVIST_ASSIST_SLOT_COUNT; ++j) {
        if (block[off + j] < prev) block[off + j] = prev;
        prev = block[off + j];
    }
}

bool evist_assist_column_monotonic(const uint8_t block[EVIST_ASSIST_BLOCK_LEN],
                                   evist_assist_column_t column) {
    uint8_t off = evist_assist_column_offset(column);
    for (uint8_t i = 1u; i < EVIST_ASSIST_SLOT_COUNT; ++i)
        if (block[off + i] < block[off + i - 1u]) return false;
    return true;
}

void evist_assist_edit_bounds(const uint8_t block[EVIST_ASSIST_BLOCK_LEN],
                              evist_firmware_profile_t profile, uint8_t level_count,
                              evist_assist_column_t column, uint8_t level,
                              uint8_t *lo, uint8_t *hi) {
    uint8_t low = 0u, high = EVIST_ASSIST_VALUE_MAX;
    if (profile == EVIST_PROFILE_BAFANG) {
        uint8_t off = evist_assist_column_offset(column);
        uint8_t show = evist_assist_visible_levels(level_count);
        if (level > 1u) low = level_value(block, off, level_count, (uint8_t)(level - 1u));
        if (level < show) high = level_value(block, off, level_count, (uint8_t)(level + 1u));
        if (low > high) { uint8_t t = low; low = high; high = t; }
    }
    if (lo) *lo = low;
    if (hi) *hi = high;
}

bool evist_assist_edit_step(uint8_t block[EVIST_ASSIST_BLOCK_LEN],
                            evist_firmware_profile_t profile, uint8_t level_count,
                            evist_assist_column_t column, uint8_t level, bool up) {
    uint8_t show = evist_assist_visible_levels(level_count);
    if (!block || level == 0u || level > show) return false;
    uint8_t off = evist_assist_column_offset(column);
    uint8_t slot = evist_assist_level_slot(level_count, level);
    uint8_t lo, hi, v = block[off + slot];
    evist_assist_edit_bounds(block, profile, level_count, column, level, &lo, &hi);
    if (up && v < hi) ++v;
    else if (!up && v > lo) --v;
    else return false;
    block[off + slot] = v;
    if (profile == EVIST_PROFILE_BAFANG) reconcile_hidden(block, level_count, off);
    return true;
}

bool evist_assist_prepare_write(uint8_t block[EVIST_ASSIST_BLOCK_LEN],
                                evist_firmware_profile_t profile, uint8_t level_count) {
    if (!block) return false;
    if (profile == EVIST_PROFILE_BAFANG) {
        if (!visible_monotonic(block, level_count, EVIST_ASSIST_POWER_OFF) ||
            !visible_monotonic(block, level_count, EVIST_ASSIST_SPEED_OFF)) return false;
        reconcile_hidden(block, level_count, EVIST_ASSIST_POWER_OFF);
        reconcile_hidden(block, level_count, EVIST_ASSIST_SPEED_OFF);
        if (!evist_assist_column_monotonic(block, EVIST_ASSIST_COLUMN_POWER) ||
            !evist_assist_column_monotonic(block, EVIST_ASSIST_COLUMN_SPEED)) return false;
    }
    block[EVIST_ASSIST_BLOCK_LEN - 1u] = evist_assist_checksum(block);
    return true;
}

bool evist_assist_write_allowed(const uint8_t block[EVIST_ASSIST_BLOCK_LEN],
                                evist_firmware_profile_t profile) {
    if (!evist_assist_block_valid(block) || !assist_values_in_range(block)) return false;
    if (profile == EVIST_PROFILE_FAKE_TAXI) return true;
    if (profile != EVIST_PROFILE_BAFANG) return false;
    return evist_assist_column_monotonic(block, EVIST_ASSIST_COLUMN_POWER) &&
           evist_assist_column_monotonic(block, EVIST_ASSIST_COLUMN_SPEED);
}
