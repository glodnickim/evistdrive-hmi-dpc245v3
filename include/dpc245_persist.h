/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_PERSIST_H
#define DPC245_PERSIST_H

#include "dpc245_settings.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DPC245_JOURNAL_MARKER_BASE 0x000300u
#define DPC245_JOURNAL_MARKER_BYTES 0x000200u
#define DPC245_JOURNAL_RECORD_BASE 0x000500u
#define DPC245_JOURNAL_RECORD_STRIDE 0x000100u
#define DPC245_JOURNAL_SLOT_COUNT 256u
#define DPC245_JOURNAL_MAGIC 0x5AA5u

typedef struct {
    bool (*read)(void *ctx, uint32_t address, uint8_t *dst, size_t len);
    bool (*write)(void *ctx, uint32_t address, const uint8_t *src, size_t len);
    bool (*erase_page)(void *ctx, uint32_t address);
    void *ctx;
} dpc245_persist_io_t;

/* Returns 0..255 for newest valid slot, or 0x100 when none exists. */
uint16_t dpc245_persist_find_latest(const dpc245_persist_io_t *io,
                                    dpc245_settings_t *scratch);

/* Load the newest valid record; otherwise use the defaults and
 * return 0xFF. A successful load returns the slot index truncated to uint8_t. */
uint8_t dpc245_persist_load(const dpc245_persist_io_t *io,
                            dpc245_settings_t *settings);
/* Distinguishes an unreadable marker/record from an empty journal. */
bool dpc245_persist_load_checked(const dpc245_persist_io_t *io,
                                 dpc245_settings_t *settings, bool *found);

/* Journal save. Returns true after marker + finalized record
 * are written. The backend erase primitive is the 0x81
 * address-based erase operation. */
bool dpc245_persist_save(const dpc245_persist_io_t *io,
                         dpc245_settings_t *settings,
                         uint8_t *slot_written);

#endif
