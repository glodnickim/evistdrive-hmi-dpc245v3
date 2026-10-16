/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_recovery_trigger.h"
#include "dpc245_buttons.h"

#define RECOVERY_CHORD_KEYS (DPC245_KEY_POWER | DPC245_KEY_UP | DPC245_KEY_DOWN)

bool dpc245_is_can_update_request(const bafang_can_frame_t *f) {
    if (!f || f->dlc != DPC245_UPDATE_REQ_DLC) return false;
    const bafang_can_id_t id = bafang_can_id_decode(f->id);
    return id.dst == DPC245_UPDATE_REQ_DST &&
           id.type == DPC245_UPDATE_REQ_TYPE &&
           id.cmd == DPC245_UPDATE_REQ_CMD &&
           id.sub == DPC245_UPDATE_REQ_SUB &&
           f->data[0] == DPC245_UPDATE_REQ_D0 &&
           f->data[1] == DPC245_UPDATE_REQ_D1 &&
           f->data[2] == DPC245_UPDATE_REQ_D2;
}

bool dpc245_is_ble_update_request(const dpc245_ble_frame_t *f) {
    return f &&
           f->class_id == DPC245_BLE_UPDATE_CLASS &&
           f->command_id == DPC245_BLE_UPDATE_COMMAND &&
           f->operation == DPC245_BLE_UPDATE_OPERATION &&
           f->selector == DPC245_BLE_UPDATE_SELECTOR;
}

void dpc245_recovery_chord_init(dpc245_recovery_chord_t *c) {
    if (!c) return;
    c->held = false;
    c->since_ms = 0u;
}

bool dpc245_recovery_chord_update(dpc245_recovery_chord_t *c, uint8_t raw_keys,
                                  uint32_t now_ms) {
    if (!c) return false;
    if ((raw_keys & RECOVERY_CHORD_KEYS) != RECOVERY_CHORD_KEYS) {
        c->held = false;
        return false;
    }
    if (!c->held) {
        c->held = true;
        c->since_ms = now_ms;
        return false;
    }
    return (uint32_t)(now_ms - c->since_ms) >= DPC245_RECOVERY_CHORD_MS;
}

uint8_t dpc245_marker_clear_retry(bool (*clear)(void), uint8_t attempts) {
    if (!clear) return 0u;
    for (unsigned n = 1u; n <= attempts; ++n) /* wider than uint8_t: 255 must end */
        if (clear()) return (uint8_t)n;
    return 0u;
}
