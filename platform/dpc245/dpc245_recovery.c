/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_recovery.h"
#include "dpc245_recovery_trigger.h"
#include <stdint.h>

#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))

#define FMC_BASE     0x40022000u
#define FMC_KEY      REG32(FMC_BASE + 0x04u)
#define FMC_STAT     REG32(FMC_BASE + 0x0Cu)
#define FMC_CTL      REG32(FMC_BASE + 0x10u)
#define FMC_UNLOCK_KEY0 0x45670123u
#define FMC_UNLOCK_KEY1 0xCDEF89ABu
#define FMC_STAT_BUSY   (1u << 0)
#define FMC_STAT_PGERR  (1u << 2)
#define FMC_STAT_WPERR  (1u << 4)
#define FMC_STAT_ENDF   (1u << 5)
#define FMC_CTL_PG      (1u << 0)
#define FMC_CTL_LK      (1u << 7)

#define FWDGT_BASE   0x40003000u
#define FWDGT_CTL    REG32(FWDGT_BASE + 0x00u)
#define FWDGT_PSC    REG32(FWDGT_BASE + 0x04u)
#define FWDGT_RLD    REG32(FWDGT_BASE + 0x08u)
#define FWDGT_STAT   REG32(FWDGT_BASE + 0x0Cu)
#define FWDGT_KEY_WRITE_ACCESS 0x5555u
#define FWDGT_KEY_RELOAD       0xAAAAu
#define FWDGT_KEY_ENABLE       0xCCCCu
#define FWDGT_STAT_PUD         (1u << 0)
#define FWDGT_STAT_RUD         (1u << 1)
/* Reset timing: /32 prescaler, reload 2 (about 1.6 ms
 * on the ~40 kHz internal oscillator). */
#define FWDGT_PSC_DIV32        3u
#define FWDGT_RELOAD_RESET     2u

#define SCB_AIRCR    REG32(0xE000ED0Cu)
#define AIRCR_SYSRESETREQ ((0x5FAu << 16) | (1u << 2))

#define FLASH_SPIN_LIMIT 2000000u
#define FWDGT_SPIN_LIMIT 0xFFFFFu
/* At 120 MHz this is far longer than the ~1.6 ms FWDGT period; at 8 MHz HSI it
 * is still well above it, so the fallback only runs if FWDGT is unusable. */
#define FWDGT_FIRE_SPIN  4000000u

static bool flash_wait_ready(void) {
    for (uint32_t n = 0u; n < FLASH_SPIN_LIMIT; ++n) {
        if ((FMC_STAT & FMC_STAT_BUSY) == 0u) return true;
    }
    return false;
}

bool dpc245_boot_marker_clear(void) {
    volatile uint16_t *const marker =
        (volatile uint16_t *)(uintptr_t)DPC245_APP_MARKER_ADDR;

    if (*marker == 0x0000u) return true;
    if (*marker != DPC245_APP_MARKER_VALID) return false;
    if (!flash_wait_ready()) return false;

    if ((FMC_CTL & FMC_CTL_LK) != 0u) {
        FMC_KEY = FMC_UNLOCK_KEY0;
        FMC_KEY = FMC_UNLOCK_KEY1;
    }
    if ((FMC_CTL & FMC_CTL_LK) != 0u) return false;

    /* Stale status flags would otherwise be misread as a failure of this write. */
    FMC_STAT = FMC_STAT_PGERR | FMC_STAT_WPERR | FMC_STAT_ENDF;
    FMC_CTL |= FMC_CTL_PG;
    *marker = 0x0000u;
    const bool done = flash_wait_ready();
    FMC_CTL &= ~FMC_CTL_PG;
    FMC_CTL |= FMC_CTL_LK;
    return done && *marker == 0x0000u;
}

static void fwdgt_start_reset(void) {
    FWDGT_CTL = FWDGT_KEY_WRITE_ACCESS;
    for (uint32_t n = 0u; (FWDGT_STAT & FWDGT_STAT_PUD) != 0u; ++n) {
        if (n >= FWDGT_SPIN_LIMIT) return;
    }
    FWDGT_PSC = FWDGT_PSC_DIV32;
    for (uint32_t n = 0u; (FWDGT_STAT & FWDGT_STAT_RUD) != 0u; ++n) {
        if (n >= FWDGT_SPIN_LIMIT) return;
    }
    FWDGT_RLD = FWDGT_RELOAD_RESET;
    FWDGT_CTL = FWDGT_KEY_RELOAD;
    FWDGT_CTL = FWDGT_KEY_ENABLE;
}

void dpc245_system_reset(void) {
    __asm volatile ("cpsid i" ::: "memory");
    fwdgt_start_reset();
    for (volatile uint32_t n = 0u; n < FWDGT_FIRE_SPIN; ++n) {}
    SCB_AIRCR = AIRCR_SYSRESETREQ;
    __asm volatile ("dsb" ::: "memory");
    for (;;) { __asm volatile ("nop"); }
}

void dpc245_enter_resident_bootloader(void) {
    __asm volatile ("cpsid i" ::: "memory");
    /* If every attempt fails the marker stays valid and the reset returns to
     * this application, whose early chord and preflight window serve the
     * next request; there is no other owner to hand over to. */
    (void)dpc245_marker_clear_retry(dpc245_boot_marker_clear, DPC245_MARKER_CLEAR_ATTEMPTS);
    dpc245_system_reset();
}
