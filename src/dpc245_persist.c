/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_persist.h"

static bool read_u16_le(const dpc245_persist_io_t *io, uint32_t addr, uint16_t *v) {
    uint8_t b[2];
    if (!io || !io->read || !v || !io->read(io->ctx, addr, b, 2u)) return false;
    *v = (uint16_t)((uint16_t)b[0] | ((uint16_t)b[1] << 8));
    return true;
}

uint16_t dpc245_persist_find_latest(const dpc245_persist_io_t *io,
                                    dpc245_settings_t *scratch) {
    if (!io || !io->read || !scratch) return DPC245_JOURNAL_SLOT_COUNT;

    /* Equivalent to one backwards scan of the full 0x200-byte marker region,
     * without a large stack buffer. */
    for (int32_t slot = (int32_t)DPC245_JOURNAL_SLOT_COUNT - 1; slot >= 0; --slot) {
        uint16_t marker = 0u;
        uint32_t marker_addr = DPC245_JOURNAL_MARKER_BASE + (uint32_t)slot * 2u;
        if (!read_u16_le(io, marker_addr, &marker)) return DPC245_JOURNAL_SLOT_COUNT;
        if (marker != DPC245_JOURNAL_MAGIC) continue;

        uint32_t record_addr = DPC245_JOURNAL_RECORD_BASE + (uint32_t)slot * DPC245_JOURNAL_RECORD_STRIDE;
        if (!io->read(io->ctx, record_addr, scratch->raw, DPC245_SETTINGS_SIZE)) continue;
        if (dpc245_settings_validate(scratch)) return (uint16_t)slot;
    }
    return DPC245_JOURNAL_SLOT_COUNT;
}

uint8_t dpc245_persist_load(const dpc245_persist_io_t *io,
                            dpc245_settings_t *settings) {
    if (!settings) return 0xFFu;
    dpc245_settings_t scratch;
    uint16_t slot = dpc245_persist_find_latest(io, &scratch);
    if (slot < DPC245_JOURNAL_SLOT_COUNT) {
        *settings = scratch;
        return (uint8_t)slot;
    }
    dpc245_settings_defaults(settings);
    return 0xFFu;
}

bool dpc245_persist_load_checked(const dpc245_persist_io_t *io,
                                 dpc245_settings_t *settings, bool *found) {
    if (!io || !io->read || !settings || !found) return false;
    *found = false;
    dpc245_settings_defaults(settings);
    dpc245_settings_t scratch;
    for (int32_t slot = (int32_t)DPC245_JOURNAL_SLOT_COUNT - 1; slot >= 0; --slot) {
        uint16_t marker;
        if (!read_u16_le(io, DPC245_JOURNAL_MARKER_BASE + (uint32_t)slot * 2u, &marker))
            return false;
        if (marker != DPC245_JOURNAL_MAGIC) continue;
        uint32_t address = DPC245_JOURNAL_RECORD_BASE +
                           (uint32_t)slot * DPC245_JOURNAL_RECORD_STRIDE;
        if (!io->read(io->ctx, address, scratch.raw, DPC245_SETTINGS_SIZE)) return false;
        if (dpc245_settings_validate(&scratch)) {
            *settings = scratch;
            *found = true;
            return true;
        }
    }
    return true;
}

bool dpc245_persist_save(const dpc245_persist_io_t *io,
                         dpc245_settings_t *settings,
                         uint8_t *slot_written) {
    if (!io || !io->read || !io->write || !io->erase_page || !settings) return false;

    dpc245_settings_t scratch;
    uint16_t latest = dpc245_persist_find_latest(io, &scratch);
    uint16_t slot;
    if (latest >= 0xFFu) {
        /* Journal wrap behavior. The
         * marker region spans pages 0x300 and 0x400; record slot0 is 0x500. */
        if (!io->erase_page(io->ctx, 0x000300u)) return false;
        if (!io->erase_page(io->ctx, 0x000400u)) return false;
        if (!io->erase_page(io->ctx, 0x000500u)) return false;
        slot = 0u;
    } else {
        slot = latest + 1u;
        uint32_t record_addr = DPC245_JOURNAL_RECORD_BASE + (uint32_t)slot * DPC245_JOURNAL_RECORD_STRIDE;
        if (!io->erase_page(io->ctx, record_addr)) return false;
    }

    uint8_t marker[2] = {0xA5u, 0x5Au};
    uint32_t marker_addr = DPC245_JOURNAL_MARKER_BASE + (uint32_t)slot * 2u;
    if (!io->write(io->ctx, marker_addr, marker, 2u)) return false;

    dpc245_settings_finalize(settings);
    uint32_t record_addr = DPC245_JOURNAL_RECORD_BASE + (uint32_t)slot * DPC245_JOURNAL_RECORD_STRIDE;
    if (!io->write(io->ctx, record_addr, settings->raw, DPC245_SETTINGS_SIZE)) return false;

    if (slot_written) *slot_written = (uint8_t)slot;
    return true;
}
