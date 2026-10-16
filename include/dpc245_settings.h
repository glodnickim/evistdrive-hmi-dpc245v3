/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_SETTINGS_H
#define DPC245_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define DPC245_SETTINGS_SIZE 0x49u
#define DPC245_SETTINGS_CHECKSUM_OFFSET 0x48u

#define DPC245_ERROR_HISTORY_OFFSET 0x31u
#define DPC245_ERROR_HISTORY_COUNT_OFFSET 0x3Bu
#define DPC245_ERROR_HISTORY_CAPACITY 10u

/* Persistent record size/layout.
 * Keep raw[] as the canonical representation. Accessors below exist only
 * for fields with a defined UI behaviour. */
typedef struct {
    uint8_t raw[DPC245_SETTINGS_SIZE];
} dpc245_settings_t;

typedef enum {
    DPC245_UNIT_METRIC = 0,
    DPC245_UNIT_IMPERIAL = 1,
} dpc245_unit_t;

typedef enum {
    DPC245_THEME_STANDARD = 0,
    DPC245_THEME_YELLOW = 1,
    DPC245_THEME_ARC = 2,
    DPC245_THEME_WEDGE = 3,
} dpc245_theme_t;

uint8_t dpc245_settings_checksum(const dpc245_settings_t *s);
void dpc245_settings_defaults(dpc245_settings_t *s);
bool dpc245_settings_validate(const dpc245_settings_t *s);
void dpc245_settings_finalize(dpc245_settings_t *s);

/* Persistent E-Code history.
 * Codes <4 and code 6 are not appended. Consecutive duplicates are ignored.
 * Once full, the oldest entry is dropped and the newest is written at slot 9. */
size_t dpc245_error_count(const dpc245_settings_t *s);
uint8_t dpc245_error_at(const dpc245_settings_t *s, size_t index);
bool dpc245_error_push(dpc245_settings_t *s, uint8_t code);

/* Display Setting field mapping:
 * item0 Unit          -> raw[0x05]
 * item1 Service tips  -> raw[0x09]
 * item2 Auto off      -> raw[0x07]
 * item3 Trip reset    -> runtime action (not a simple persisted byte)
 * item4 Brightness    -> raw[0x06] = selection + 1
 * item5 AL sensitivity-> raw[0x0A]
 * item6 Back          -> no setting write
 */
uint8_t dpc245_unit(const dpc245_settings_t *s);
void dpc245_set_unit(dpc245_settings_t *s, uint8_t value);
uint8_t dpc245_service_tips(const dpc245_settings_t *s);
void dpc245_set_service_tips(dpc245_settings_t *s, uint8_t value);
uint8_t dpc245_auto_off_min(const dpc245_settings_t *s);
void dpc245_set_auto_off_min(dpc245_settings_t *s, uint8_t value);
uint8_t dpc245_brightness_level(const dpc245_settings_t *s);
void dpc245_set_brightness_level(dpc245_settings_t *s, uint8_t value);
uint8_t dpc245_al_sensitivity(const dpc245_settings_t *s);
void dpc245_set_al_sensitivity(dpc245_settings_t *s, uint8_t value);

/* Persistent UI selectors:
 * raw[0x0D] = theme code (0 standard, 1 retired, 2 yellow, 3 arc, 4 wedge)
 * raw[0x0E] = Language selector.
 */
uint8_t dpc245_theme(const dpc245_settings_t *s);
void dpc245_set_theme(dpc245_settings_t *s, uint8_t value);
uint8_t dpc245_language(const dpc245_settings_t *s);
void dpc245_set_language(dpc245_settings_t *s, uint8_t value);

uint32_t dpc245_get_u32_le(const dpc245_settings_t *s, uint8_t offset);
void dpc245_set_u32_le(dpc245_settings_t *s, uint8_t offset, uint32_t value);
uint16_t dpc245_get_u16_le(const dpc245_settings_t *s, uint8_t offset);
void dpc245_set_u16_le(dpc245_settings_t *s, uint8_t offset, uint16_t value);

#endif
