/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "bafang_service.h"
#include <stddef.h>
void *memset(void *s, int c, size_t n);

static void copy_bytes(uint8_t *dst, const uint8_t *src, uint8_t n) {
    for (uint8_t i = 0u; i < n; ++i) dst[i] = src[i];
}

static uint8_t append_bytes(uint8_t *dst, uint8_t have,
                            const uint8_t *src, uint8_t n) {
    uint16_t room = (uint16_t)BAFANG_SERVICE_MAX_DATA - have;
    if (n > room) n = (uint8_t)room;
    for (uint8_t i = 0u; i < n; ++i) dst[have + i] = src[i];
    return (uint8_t)(have + n);
}

void bafang_service_rx_init(bafang_service_rx_t *rx) {
    if (rx) memset(rx, 0, sizeof(*rx));
}

static bool service_is_to_hmi(const bafang_can_id_t *id) {
    return id && id->dst == BAFANG_NODE_HMI;
}

bool bafang_service_rx_feed(bafang_service_rx_t *rx,
                            const bafang_can_frame_t *frame,
                            bafang_service_message_t *out,
                            bool *ack_needed) {
    if (ack_needed) *ack_needed = false;
    if (!rx || !frame || !out || frame->dlc > BAFANG_CAN_MAX_DLC) return false;
    bafang_can_id_t id = bafang_can_id_decode(frame->id);
    if (!service_is_to_hmi(&id) || id.src >= BAFANG_SERVICE_TRACKED_NODES) return false;

    bafang_service_reassembly_slot_t *s = &rx->slot[id.src];
    if (id.type == BAFANG_CAN_OP_NORMAL_ACK || id.type == BAFANG_CAN_OP_ERROR_ACK) {
        memset(out, 0, sizeof(*out));
        out->src = id.src; out->dst = id.dst; out->cmd = id.cmd; out->sub = id.sub;
        out->operation = id.type; out->error = id.type == BAFANG_CAN_OP_ERROR_ACK;
        out->len = frame->dlc;
        copy_bytes(out->data, frame->data, frame->dlc);
        if (id.type == BAFANG_CAN_OP_NORMAL_ACK) s->active = false;
        return true;
    }

    if (id.type == BAFANG_CAN_OP_MULTIFRAME_START) {
        memset(s, 0, sizeof(*s));
        s->active = true;
        s->cmd = id.cmd;
        s->sub = id.sub;
        /* START carries framing metadata only;
         * content is delivered in MULTIFRAME and END packets. */
        if (ack_needed) *ack_needed = true;
        return false;
    }

    if ((id.type == BAFANG_CAN_OP_MULTIFRAME || id.type == BAFANG_CAN_OP_MULTIFRAME_END) && s->active) {
        if (id.cmd != 0u || id.sub != s->next_index) {
            s->active = false;
            return false;
        }
        s->len = append_bytes(s->data, s->len, frame->data, frame->dlc);
        s->next_index++;
        if (ack_needed) *ack_needed = true;
        if (id.type != BAFANG_CAN_OP_MULTIFRAME_END) return false;

        memset(out, 0, sizeof(*out));
        out->src = id.src; out->dst = id.dst;
        out->cmd = s->cmd; out->sub = s->sub;
        out->operation = BAFANG_CAN_OP_NORMAL_ACK;
        out->len = s->len;
        copy_bytes(out->data, s->data, s->len);
        s->active = false;
        return true;
    }

    if (id.type == BAFANG_CAN_OP_MULTIFRAME_WARNING) {
        s->active = false;
    }
    return false;
}

bool bafang_build_service_ack(bafang_can_frame_t *out,
                              uint8_t dst,
                              uint8_t cmd,
                              uint8_t sub) {
    if (!out || dst >= BAFANG_NODE_BROADCAST) return false;
    memset(out, 0, sizeof(*out));
    out->id = bafang_can_id_encode((bafang_can_id_t){
        .src = BAFANG_NODE_HMI,
        .dst = dst,
        .type = BAFANG_CAN_OP_NORMAL_ACK,
        .cmd = cmd,
        .sub = sub,
    });
    out->dlc = 0u;
    return true;
}

static void copy_text(char *dst, const uint8_t *src, uint8_t n) {
    uint8_t limit = n > BAFANG_IDENTITY_TEXT_MAX ? BAFANG_IDENTITY_TEXT_MAX : n;
    uint8_t w = 0u;
    for (uint8_t i = 0u; i < limit; ++i) {
        uint8_t c = src[i];
        if (c == 0u || c == 0xFFu) break;
        dst[w++] = (c >= 0x20u && c <= 0x7Eu) ? (char)c : '?';
    }
    dst[w] = '\0';
}

bool bafang_identity_apply_service(bafang_device_identity_t *ident,
                                   const bafang_service_message_t *msg) {
    if (!ident || !msg || msg->error || msg->cmd != 0x60u || msg->sub > 0x05u) return false;
    char *dst = NULL;
    switch (msg->sub) {
    case 0x00u: dst = ident->hardware; break;
    case 0x01u: dst = ident->software; break;
    case 0x02u: dst = ident->model; break;
    case 0x03u: dst = ident->serial; break;
    case 0x04u: dst = ident->customer; break;
    case 0x05u: dst = ident->manufacturer; break;
    default: return false;
    }
    copy_text(dst, msg->data, msg->len);
    ident->have_mask |= (uint8_t)(1u << msg->sub);
    return true;
}

bool bafang_battery_service_apply(bafang_battery_service_info_t *info,
                                  const bafang_service_message_t *msg) {
    if (!info || !msg || msg->error || msg->src != BAFANG_NODE_BATTERY) return false;
    if (msg->cmd != 0x64u) return false;

    if (msg->sub >= 0x02u && msg->sub <= 0x09u) {
        uint8_t base = (uint8_t)((msg->sub - 0x02u) * 4u);
        uint8_t pairs = (uint8_t)(msg->len / 2u);
        bool wrote = false;
        for (uint8_t i = 0u; i < pairs && i < 4u; ++i) {
            uint8_t idx = (uint8_t)(base + i);
            if (idx >= BAFANG_BATTERY_SERVICE_MAX_CELLS) break;
            uint16_t mv = (uint16_t)msg->data[i * 2u] | ((uint16_t)msg->data[i * 2u + 1u] << 8);
            info->cell_mv[idx] = mv;
            info->cell_valid_mask |= (uint32_t)1u << idx;
            if ((uint8_t)(idx + 1u) > info->cell_count) info->cell_count = (uint8_t)(idx + 1u);
            wrote = true;
        }
        return wrote;
    }
    if (msg->sub == 0x01u) {
        info->charging_len = msg->len;
        copy_bytes(info->charging_raw, msg->data, msg->len);
        return true;
    }
    if (msg->sub == 0x15u) {
        info->characteristics_len = msg->len;
        copy_bytes(info->characteristics_raw, msg->data, msg->len);
        return true;
    }
    return false;
}
