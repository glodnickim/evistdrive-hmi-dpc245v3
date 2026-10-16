/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_HWINFO_H
#define DPC245_HWINFO_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t cpuid;
    uint32_t dbg_idcode;
    uint16_t flash_kib;
    uint16_t sram_kib;      /* GD32F30x density register upper half; 0xFFFF if absent */
    uint32_t reset_flags;   /* RCU_RSTSCK as found at boot (bits 26..31) */
    uint16_t app_marker;
    uint8_t boot_tail[8];
    bool pa0_high;
    bool spi_probe_valid;
    uint8_t spi_status;
    uint8_t spi_jedec[3];
} dpc245_hwinfo_t;

void dpc245_hwinfo_read(dpc245_hwinfo_t *out);

#endif
