/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_hwinfo.h"
#include "dpc245_board.h"
#include "dpc245_recovery.h"
#include <stdint.h>

#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define REG16(a) (*(volatile uint16_t *)(uintptr_t)(a))

void dpc245_hwinfo_read(dpc245_hwinfo_t *out) {
    if (!out) return;
    out->cpuid = REG32(0xE000ED00u);
    /* GD32F30x debug MCU ID register. Diagnostic only: the firmware never refuses
     * to boot because the reported part differs from the expected one. */
    out->dbg_idcode = REG32(0xE0042000u);
    /* GD32F30x memory density register: [15:0] flash KiB, [31:16] SRAM KiB. */
    out->flash_kib = REG16(0x1FFFF7E0u);
    out->sram_kib = REG16(0x1FFFF7E2u);
    out->reset_flags = REG32(0x40021024u) & 0xFC000000u;
    out->app_marker = REG16(DPC245_APP_MARKER_ADDR);

    const volatile uint8_t *tail = (const volatile uint8_t *)(uintptr_t)0x08005FF0u;
    for (unsigned i = 0u; i < sizeof out->boot_tail; ++i) out->boot_tail[i] = tail[i];

    dpc245_unknown_pa0_init_pullup();
    out->pa0_high = dpc245_unknown_pa0_high();
    out->spi_probe_valid = false;
    out->spi_status = 0xFFu;
    out->spi_jedec[0] = out->spi_jedec[1] = out->spi_jedec[2] = 0xFFu;
}
