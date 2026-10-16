/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_UPDATE_H
#define DPC245_UPDATE_H

#include "bafang_can.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    DPC245_UPDATE_IDLE = 0,
    DPC245_UPDATE_SEND_COORDINATION,
    DPC245_UPDATE_WAIT_CAN_DRAIN,
    DPC245_UPDATE_READY_TO_RESET
} dpc245_update_state_t;

typedef struct {
    dpc245_update_state_t state;
    uint32_t coordination_sent_ms;
} dpc245_update_t;

void dpc245_update_init(dpc245_update_t *u);
/* Starts the protocol-requested transition to the platform update environment. */
bool dpc245_update_request(dpc245_update_t *u);
/* Exact coordination frame sent before leaving the running APP. */
bool dpc245_update_next_can(dpc245_update_t *u, bafang_can_frame_t *out);
void dpc245_update_can_sent(dpc245_update_t *u, uint32_t now_ms);
/* The update coordinator allows the coordination transmission to leave the node
 * before invalidating the APP. A direct bxCAN implementation can prove this
 * by waiting for all TX mailboxes to become empty; a 1 ms floor also provides
 * a short settling interval before the transition. */
bool dpc245_update_tick(dpc245_update_t *u, uint32_t now_ms, bool can_tx_idle);

#endif
