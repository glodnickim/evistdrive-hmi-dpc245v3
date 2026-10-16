/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_PREFLIGHT_H
#define DPC245_PREFLIGHT_H

#include <stdbool.h>
#include <stdint.h>

/* Pre-runtime recovery layer.
 *
 * Runs after .data/.bss and clock setup but before LCD, SPI memory, RTC and
 * UI. It brings up only CAN RX/TX, BLE UART RX and the key inputs, and listens
 * for the three update requests (CAN 0x4000 08 42 03, BLE 11/A5/30/00, and
 * POWER+UP+DOWN). A request never returns: the application marker is cleared
 * and the MCU resets into the resident update environment.
 *
 * CAN and the BLE UART stay initialised when the window ends, so the
 * application continues with the same transport state. */

#define DPC245_PREFLIGHT_FOREVER 0xFFFFFFFFu

typedef uint32_t (*dpc245_millis_fn)(void);

typedef struct {
    bool can_ok;
    uint32_t can_frames;
    uint32_t ble_bytes;
    uint32_t ble_frames;
} dpc245_preflight_stats_t;

typedef enum {
    DPC245_PREFLIGHT_CLOCK_PLL = 0, /* APB1 60 MHz / APB2 120 MHz */
    DPC245_PREFLIGHT_CLOCK_HSI = 1  /* 8 MHz safe mode after a clock failure */
} dpc245_preflight_clock_t;

void dpc245_preflight_begin(dpc245_preflight_clock_t clock, dpc245_preflight_stats_t *stats);
/* One non-blocking service pass. Does not return when a request is accepted. */
void dpc245_preflight_poll(uint32_t now_ms, dpc245_preflight_stats_t *stats);
void dpc245_preflight_run(uint32_t window_ms, dpc245_millis_fn millis,
                          dpc245_preflight_stats_t *stats);

#endif
