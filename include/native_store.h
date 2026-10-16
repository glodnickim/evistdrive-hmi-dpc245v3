/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef EVIST_NATIVE_STORE_H
#define EVIST_NATIVE_STORE_H

#include "dpc245_persist.h"
#include "evist_preferences.h"
#include "ride_mode.h"

/* One 192-byte image per 256-byte erase unit. All addresses are inclusive of
 * whole units and remain below 0x1FFF00. */
#define EVIST_NATIVE_FIRST 0x001F0100u
#define EVIST_NATIVE_LAST  0x001FDD00u
#define EVIST_NATIVE_PAGE_BYTES 256u
#define EVIST_NATIVE_RECORD_BYTES 192u
#define EVIST_NATIVE_PAGE_COUNT ((EVIST_NATIVE_LAST-EVIST_NATIVE_FIRST)/EVIST_NATIVE_PAGE_BYTES+1u)

typedef struct {
    evist_preferences_t preferences;
    dpc245_settings_t hmi;
    evist_ride_cfg_t ride;
} evist_native_values_t;

/* Erase request returns after sending 0x81. ready: 1 done, 0 busy, -1 IO
 * failure. Each callback is called at most once per foreground step. */
typedef struct {
    const dpc245_persist_io_t *memory;
    bool (*erase_start)(void *ctx, uint32_t address);
    int (*ready)(void *ctx);
    void *ctx;
} evist_native_io_t;

typedef enum {
    EVIST_NATIVE_OK = 0, EVIST_NATIVE_BUSY, EVIST_NATIVE_NO_MEMORY,
    EVIST_NATIVE_ERASE_FAILED, EVIST_NATIVE_WRITE_FAILED,
    EVIST_NATIVE_VERIFY_FAILED, EVIST_NATIVE_INVALID
} evist_native_result_t;

typedef struct {
    evist_native_values_t saved;
    evist_native_values_t target;
    uint32_t sequence;
    uint32_t previous_sequence;
    uint16_t page;
    uint16_t previous;
    uint16_t candidate;
    uint8_t phase;
    uint8_t scan_part;
    uint32_t wait_started_ms;
    bool valid;
    bool have_previous;
    bool imported;
    bool options_reset;
    bool busy;
    bool known;
} evist_native_store_t;

/* Reads the newest CRC-valid record. If none exists, loads the values found in
 * the settings logs into RAM and schedules their first write to the ring.
 * A valid record from another firmware version returns default options in
 * loaded and sets options_reset for the caller to queue one normal save.
 * No erases occur here. */
bool evist_native_store_init(evist_native_store_t *s, const evist_native_io_t *io,
                             evist_native_values_t *loaded);
bool evist_native_store_begin(evist_native_store_t *s, const evist_native_values_t *target);
evist_native_result_t evist_native_store_step(evist_native_store_t *s, const evist_native_io_t *io,
                                               uint32_t now_ms);
const char *evist_native_result_text(evist_native_result_t result);

#endif
