/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_PROBE_H
#define DPC245_PROBE_H

#include "dpc245_preflight.h"
#include <stdbool.h>
#include <stdint.h>

/* One-shot hardware probe (EVIST_PROFILE_PROBE).
 *
 * The first image to flash on a new display. Reset_Handler invalidates its
 * application marker as the very first action, before any clock change, wait
 * or peripheral access. Only when that succeeded does a later power cycle
 * return the display to the resident update environment. If it failed, the
 * marker stays valid: the probe then says so in red, keeps retrying every
 * second and never claims that a power cycle reaches the updater.
 * It shows a read-only diagnostic screen, never transmits on CAN or BLE and
 * never writes external memory. */

typedef struct {
    bool marker_cleared;
    uint8_t clear_attempt;      /* 1..3 in Reset_Handler, 0 = failed there */
    uint16_t late_retries;      /* successful only if marker_cleared turns true later */
    uint16_t marker_after;
    bool clock_pll;             /* 120 MHz HXTAL tree, else HSI 8 MHz */
    bool clock_kept;            /* that tree was left by the updater (HSI not confirmed) */
} dpc245_probe_boot_t;

/* Written by Reset_Handler after .bss initialisation: result of
 * dpc245_marker_clear_retry() run before anything else. */
extern uint8_t dpc245_probe_early_disarm_attempt;

void dpc245_probe_boot_init(dpc245_probe_boot_t *boot);

void dpc245_probe_run(dpc245_probe_boot_t *boot, dpc245_millis_fn millis,
                      void (*delay_ms)(uint32_t), dpc245_preflight_stats_t *stats)
    __attribute__((noreturn));

#endif
