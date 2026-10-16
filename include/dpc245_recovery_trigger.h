/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_RECOVERY_TRIGGER_H
#define DPC245_RECOVERY_TRIGGER_H

#include "bafang_can.h"
#include "dpc245_ble_proto.h"
#include <stdbool.h>
#include <stdint.h>

/* Requests that hand the HMI over to the resident update environment.
 *
 * CAN update entry accepts exactly one frame shape
 * with these fields:
 *
 *     destination = 3 (display), type = 1, command = 0x40, sub = 0x00,
 *     DLC = 4, data[0..2] = 08 42 03   (source and data[3] are not checked)
 *
 * A compatible updater sends it as 0x05194000 # 08 42 03 xx every
 * ~60 ms until the resident bootloader answers 0x032A4000.
 *
 * The 0x05FF3005 announcement is deliberately NOT a trigger: updaters keep
 * broadcasting it for 26..34 s after a successful transfer, while the freshly
 * written application is already running, and when another node is updated.
 * Treating it as a request would clear the new marker.
 *
 * BLE: class 0x11, command 0xA5, operation 0x30, selector 0x00
 * selects update entry through the UART application protocol.
 */
#define DPC245_UPDATE_REQ_DST       BAFANG_NODE_HMI
#define DPC245_UPDATE_REQ_TYPE      1u
#define DPC245_UPDATE_REQ_CMD       0x40u
#define DPC245_UPDATE_REQ_SUB       0x00u
#define DPC245_UPDATE_REQ_DLC       4u
#define DPC245_UPDATE_REQ_D0        0x08u
#define DPC245_UPDATE_REQ_D1        0x42u
#define DPC245_UPDATE_REQ_D2        0x03u

#define DPC245_BLE_UPDATE_CLASS     0x11u
#define DPC245_BLE_UPDATE_COMMAND   0xA5u
#define DPC245_BLE_UPDATE_OPERATION 0x30u
#define DPC245_BLE_UPDATE_SELECTOR  0x00u

/* POWER+UP+DOWN held continuously for this long requests the updater. */
#define DPC245_RECOVERY_CHORD_MS    1500u

bool dpc245_is_can_update_request(const bafang_can_frame_t *f);
bool dpc245_is_ble_update_request(const dpc245_ble_frame_t *f);

typedef struct {
    bool held;
    uint32_t since_ms;
} dpc245_recovery_chord_t;

void dpc245_recovery_chord_init(dpc245_recovery_chord_t *c);
/* raw_keys uses DPC245_KEY_* bits. Returns true once the chord has been held
 * for DPC245_RECOVERY_CHORD_MS. */
bool dpc245_recovery_chord_update(dpc245_recovery_chord_t *c, uint8_t raw_keys,
                                  uint32_t now_ms);

/* Attempts made to invalidate the application marker before giving up. A
 * failed attempt is retried at once; FMC errors are usually transient
 * (busy, stale status, lock not yet released). */
#define DPC245_MARKER_CLEAR_ATTEMPTS 3u

/* Calls clear() until it reports success, at most `attempts` times.
 * Returns the 1-based attempt that succeeded, or 0 when every attempt failed.
 * Uses only the stack, so Reset_Handler may call it before .data/.bss exist. */
uint8_t dpc245_marker_clear_retry(bool (*clear)(void), uint8_t attempts);

#endif
