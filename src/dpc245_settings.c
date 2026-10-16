/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_settings.h"
#include <stddef.h>

uint8_t dpc245_settings_checksum(const dpc245_settings_t *s) {
    if (!s) return 0u;
    uint8_t sum = 0u;
    for (uint32_t i = 0u; i < DPC245_SETTINGS_CHECKSUM_OFFSET; ++i) {
        sum = (uint8_t)(sum + s->raw[i]);
    }
    return sum;
}

void dpc245_settings_defaults(dpc245_settings_t *s) {
    if (!s) return;
    for (uint32_t i = 0u; i < DPC245_SETTINGS_SIZE; ++i) s->raw[i] = 0u;

    /* Persistent settings defaults. */
    s->raw[0x06] = 4u;
    s->raw[0x07] = 5u;
    s->raw[0x08] = 5u;
    s->raw[0x0A] = 3u;
    s->raw[0x0B] = 0u;
    s->raw[0x0C] = 1u;
    dpc245_set_u32_le(s, 0x1Cu, 500000u); /* 5000.00 km service interval */
    s->raw[0x2E] = 0u;
    dpc245_settings_finalize(s);
}

bool dpc245_settings_validate(const dpc245_settings_t *s) {
    if (!s) return false;
    /* Persistent-record validation: checksum and the field ranges of the record
     * format only. Stricter rules would reject records the format allows. */
    if (s->raw[DPC245_SETTINGS_CHECKSUM_OFFSET] != dpc245_settings_checksum(s)) return false;
    if (s->raw[0x06] >= 5u) return false;
    if (s->raw[0x07] >= 11u) return false;
    if (s->raw[0x08] != 5u) return false;
    if (s->raw[0x0B] >= 2u) return false;
    if (s->raw[0x0C] >= 2u) return false;
    if (s->raw[0x05] >= 2u) return false;
    return true;
}

void dpc245_settings_finalize(dpc245_settings_t *s) {
    if (!s) return;
    s->raw[DPC245_SETTINGS_CHECKSUM_OFFSET] = dpc245_settings_checksum(s);
}

uint8_t dpc245_unit(const dpc245_settings_t *s) { return s ? s->raw[0x05] : 0u; }
void dpc245_set_unit(dpc245_settings_t *s, uint8_t value) { if (s) s->raw[0x05] = value ? 1u : 0u; }
uint8_t dpc245_service_tips(const dpc245_settings_t *s) { return s ? s->raw[0x09] : 0u; }
void dpc245_set_service_tips(dpc245_settings_t *s, uint8_t value) { if (s) s->raw[0x09] = value ? 1u : 0u; }
uint8_t dpc245_auto_off_min(const dpc245_settings_t *s) { return s ? s->raw[0x07] : 0u; }
void dpc245_set_auto_off_min(dpc245_settings_t *s, uint8_t value) { if (s) s->raw[0x07] = value > 10u ? 10u : value; }
uint8_t dpc245_brightness_level(const dpc245_settings_t *s) { return s ? s->raw[0x06] : 0u; }
void dpc245_set_brightness_level(dpc245_settings_t *s, uint8_t value) { if (s) s->raw[0x06] = value > 4u ? 4u : value; }
uint8_t dpc245_al_sensitivity(const dpc245_settings_t *s) { return s ? s->raw[0x0A] : 0u; }
void dpc245_set_al_sensitivity(dpc245_settings_t *s, uint8_t value) { if (s) s->raw[0x0A] = value; }
/* The saved code 1 is retired; code 2 retains its yellow appearance. */
uint8_t dpc245_theme(const dpc245_settings_t *s) {
    uint8_t code=s?s->raw[0x0D]:0u;
    return code>=2u&&code<=4u?(uint8_t)(code-1u):0u;
}
void dpc245_set_theme(dpc245_settings_t *s, uint8_t value) {
    if (s) s->raw[0x0D]=value>=1u&&value<=3u?(uint8_t)(value+1u):0u;
}
uint8_t dpc245_language(const dpc245_settings_t *s) { return s ? s->raw[0x0E] : 0u; }
void dpc245_set_language(dpc245_settings_t *s, uint8_t value) { if (s) s->raw[0x0E] = value; }

uint32_t dpc245_get_u32_le(const dpc245_settings_t *s, uint8_t offset) {
    if (!s || offset > DPC245_SETTINGS_SIZE - 4u) return 0u;
    return (uint32_t)s->raw[offset] |
           ((uint32_t)s->raw[offset + 1u] << 8) |
           ((uint32_t)s->raw[offset + 2u] << 16) |
           ((uint32_t)s->raw[offset + 3u] << 24);
}
void dpc245_set_u32_le(dpc245_settings_t *s, uint8_t offset, uint32_t value) {
    if (!s || offset > DPC245_SETTINGS_SIZE - 4u) return;
    s->raw[offset] = (uint8_t)value;
    s->raw[offset + 1u] = (uint8_t)(value >> 8);
    s->raw[offset + 2u] = (uint8_t)(value >> 16);
    s->raw[offset + 3u] = (uint8_t)(value >> 24);
}
uint16_t dpc245_get_u16_le(const dpc245_settings_t *s, uint8_t offset) {
    if (!s || offset > DPC245_SETTINGS_SIZE - 2u) return 0u;
    return (uint16_t)((uint16_t)s->raw[offset] | ((uint16_t)s->raw[offset + 1u] << 8));
}
void dpc245_set_u16_le(dpc245_settings_t *s, uint8_t offset, uint16_t value) {
    if (!s || offset > DPC245_SETTINGS_SIZE - 2u) return;
    s->raw[offset] = (uint8_t)value;
    s->raw[offset + 1u] = (uint8_t)(value >> 8);
}


size_t dpc245_error_count(const dpc245_settings_t *s) {
    if (!s) return 0u;
    uint8_t n = s->raw[DPC245_ERROR_HISTORY_COUNT_OFFSET];
    return n > DPC245_ERROR_HISTORY_CAPACITY ? DPC245_ERROR_HISTORY_CAPACITY : n;
}

uint8_t dpc245_error_at(const dpc245_settings_t *s, size_t index) {
    size_t n = dpc245_error_count(s);
    if (!s || index >= n) return 0u;
    return s->raw[DPC245_ERROR_HISTORY_OFFSET + index];
}

bool dpc245_error_push(dpc245_settings_t *s, uint8_t code) {
    if (!s) return false;
    /* Error-history filter. */
    if (code < 4u || code == 6u) return false;

    size_t n = dpc245_error_count(s);
    if (n != 0u && s->raw[DPC245_ERROR_HISTORY_OFFSET + n - 1u] == code) return false;

    if (n < DPC245_ERROR_HISTORY_CAPACITY) {
        s->raw[DPC245_ERROR_HISTORY_OFFSET + n] = code;
        s->raw[DPC245_ERROR_HISTORY_COUNT_OFFSET] = (uint8_t)(n + 1u);
    } else {
        for (size_t i = 1u; i < DPC245_ERROR_HISTORY_CAPACITY; ++i)
            s->raw[DPC245_ERROR_HISTORY_OFFSET + i - 1u] =
                s->raw[DPC245_ERROR_HISTORY_OFFSET + i];
        s->raw[DPC245_ERROR_HISTORY_OFFSET + DPC245_ERROR_HISTORY_CAPACITY - 1u] = code;
        s->raw[DPC245_ERROR_HISTORY_COUNT_OFFSET] = DPC245_ERROR_HISTORY_CAPACITY;
    }
    dpc245_settings_finalize(s);
    return true;
}
