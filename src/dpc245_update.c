/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_update.h"

void dpc245_update_init(dpc245_update_t *u) {
    if (!u) return;
    u->state = DPC245_UPDATE_IDLE;
    u->coordination_sent_ms = 0u;
}

bool dpc245_update_request(dpc245_update_t *u) {
    if (!u || u->state != DPC245_UPDATE_IDLE) return false;
    u->state = DPC245_UPDATE_SEND_COORDINATION;
    return true;
}

bool dpc245_update_next_can(dpc245_update_t *u, bafang_can_frame_t *out) {
    if (!u || !out || u->state != DPC245_UPDATE_SEND_COORDINATION) return false;
    out->id = 0x13FF3005u;
    out->dlc = 1u;
    out->data[0] = 0u;
    for (uint8_t i = 1u; i < 8u; ++i) out->data[i] = 0u;
    return true;
}

void dpc245_update_can_sent(dpc245_update_t *u, uint32_t now_ms) {
    if (!u || u->state != DPC245_UPDATE_SEND_COORDINATION) return;
    u->coordination_sent_ms = now_ms;
    u->state = DPC245_UPDATE_WAIT_CAN_DRAIN;
}

bool dpc245_update_tick(dpc245_update_t *u, uint32_t now_ms, bool can_tx_idle) {
    if (!u) return false;
    if (u->state == DPC245_UPDATE_READY_TO_RESET) return true;
    if (u->state != DPC245_UPDATE_WAIT_CAN_DRAIN) return false;
    if (!can_tx_idle || (uint32_t)(now_ms - u->coordination_sent_ms) < 1u) return false;
    u->state = DPC245_UPDATE_READY_TO_RESET;
    return true;
}
