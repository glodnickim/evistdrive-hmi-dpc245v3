/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "can_router.h"
#include <stddef.h>
void *memset(void *s, int c, size_t n);

void evist_can_router_init(evist_can_router_t *r) {
    if (!r) return;
    memset(r, 0, sizeof(*r));
    bafang_service_rx_init(&r->service_rx);
    r->tx_mode = EVIST_TX_READONLY;
}

void evist_can_router_rx(evist_can_router_t *r, const bafang_can_frame_t *f, uint32_t now_ms) {
    if (!r || !f || f->dlc > 8u) return;
    r->service_event = false;

    /* Crucial policy: log EVERY valid extended frame before attempting decode. */
    evist_can_log_entry_t *e = &r->log[r->log_head];
    e->frame = *f;
    e->sequence = ++r->rx_sequence;
    e->kind = bafang_frame_classify(f->id);
    r->log_head = (r->log_head + 1u) % EVIST_CAN_LOG_CAPACITY;
    if (r->log_count < EVIST_CAN_LOG_CAPACITY) r->log_count++;

    (void)bafang_decode_telemetry(f, &r->telemetry);
    if ((f->id & 0x1FFFFFFFu) == 0x02F83201u && f->dlc >= 8u)
        r->telemetry.controller_fast1_rx_ms = now_ms;
    if ((f->id & 0x1FFFFFFFu) == 0x02F83202u && f->dlc >= 1u)
        r->telemetry.controller_fast2_rx_ms = now_ms;

    /* Service/on-demand responses feed the Information pages. */
    {
        bafang_service_message_t msg;
        bool ack = false;
        if (bafang_service_rx_feed(&r->service_rx, f, &msg, &ack)) {
            r->last_service = msg;
            r->have_last_service = true;
            r->service_event = true;
            if (msg.src == BAFANG_NODE_CONTROLLER)
                (void)bafang_identity_apply_service(&r->controller_identity, &msg);
            else if (msg.src == BAFANG_NODE_TORQUE_SENSOR)
                (void)bafang_identity_apply_service(&r->sensor_identity, &msg);
            else if (msg.src == BAFANG_NODE_BATTERY) {
                (void)bafang_identity_apply_service(&r->battery_identity, &msg);
                (void)bafang_battery_service_apply(&r->battery_service, &msg);
            }
        }
        if (ack) {
            bafang_can_id_t id = bafang_can_id_decode(f->id);
            uint8_t cmd, sub;
            if (id.type == BAFANG_CAN_OP_MULTIFRAME_START) {
                cmd = id.cmd;
                sub = id.sub;
            } else {
                bafang_service_reassembly_slot_t *slot = &r->service_rx.slot[id.src];
                cmd = slot->cmd;
                sub = slot->sub;
                if (!slot->active && r->have_last_service && r->last_service.src == id.src) {
                    cmd = r->last_service.cmd;
                    sub = r->last_service.sub;
                }
            }
            if (r->service_ack_count < 8u) {
                uint8_t tail = (uint8_t)((r->service_ack_head + r->service_ack_count) % 8u);
                if (bafang_build_service_ack(&r->service_ack_queue[tail], id.src, cmd, sub))
                    r->service_ack_count++;
            }
        }
    }
}

const evist_can_log_entry_t *evist_can_router_log_newest(const evist_can_router_t *r,
                                                         size_t index_from_newest) {
    if (!r || index_from_newest >= r->log_count) return NULL;
    size_t idx = (r->log_head + EVIST_CAN_LOG_CAPACITY - 1u - index_from_newest)
               % EVIST_CAN_LOG_CAPACITY;
    return &r->log[idx];
}

static bool is_read_query(const bafang_can_frame_t *f) {
    bafang_can_id_t id = bafang_can_id_decode(f->id);
    return id.src == BAFANG_NODE_HMI && id.type == BAFANG_CAN_OP_READ && f->dlc == 0u;
}

static bool is_service_ack(const bafang_can_frame_t *f) {
    bafang_can_id_t id = bafang_can_id_decode(f->id);
    return id.src == BAFANG_NODE_HMI && id.type == BAFANG_CAN_OP_NORMAL_ACK && f->dlc == 0u;
}

static bool is_hmi_service_reply(const bafang_can_frame_t *f) {
    bafang_can_id_t id = bafang_can_id_decode(f->id);
    if (id.src != BAFANG_NODE_HMI || id.dst >= BAFANG_NODE_BROADCAST) return false;
    if (id.type == BAFANG_CAN_OP_MULTIFRAME_START ||
        id.type == BAFANG_CAN_OP_MULTIFRAME ||
        id.type == BAFANG_CAN_OP_MULTIFRAME_END) return true;
    return false;
}

static bool is_periodic_hmi(const bafang_can_frame_t *f) {
    switch (f->id & 0x1FFFFFFFu) {
    case 0x03106300u: case 0x03106301u: case 0x03106302u:
    case 0x03106303u: case 0x03106304u: case 0x03986207u:
        return true;
    default: return false;
    }
}

bool evist_can_router_tx_allowed(const evist_can_router_t *r,
                                 const bafang_can_frame_t *f,
                                 bool caller_marks_as_write) {
    if (!r || !f || f->dlc > 8u) return false;
    if (r->tx_mode == EVIST_TX_EXPERT) return true;
    if (caller_marks_as_write) return false;
    if (is_read_query(f) || is_service_ack(f) || is_hmi_service_reply(f)) return true;
    if (r->tx_mode == EVIST_TX_NORMAL_HMI && is_periodic_hmi(f)) return true;
    return false;
}


bool evist_can_router_take_service_ack(evist_can_router_t *r,
                                       bafang_can_frame_t *out) {
    if (!evist_can_router_peek_service_ack(r, out)) return false;
    evist_can_router_pop_service_ack(r);
    return true;
}

bool evist_can_router_peek_service_ack(const evist_can_router_t *r,
                                       bafang_can_frame_t *out) {
    if (!r || !out || r->service_ack_count == 0u) return false;
    *out = r->service_ack_queue[r->service_ack_head];
    return true;
}

void evist_can_router_pop_service_ack(evist_can_router_t *r) {
    if (!r || r->service_ack_count == 0u) return;
    r->service_ack_head = (uint8_t)((r->service_ack_head + 1u) % 8u);
    r->service_ack_count--;
}
