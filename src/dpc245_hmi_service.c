/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_hmi_service.h"
#include <stddef.h>
void *memset(void *s, int c, size_t n);

static void copy_bytes(uint8_t *dst, const uint8_t *src, uint8_t n) {
    for (uint8_t i = 0u; i < n; ++i) dst[i] = src[i];
}

static uint8_t text_len(const char *s) {
    uint8_t n = 0u;
    while (s[n] != '\0' && n < BAFANG_SERVICE_MAX_DATA) ++n;
    return n;
}

void dpc245_hmi_identity_records_init(dpc245_hmi_identity_records_t *id) {
    if (id) memset(id, 0, sizeof(*id));
}

static bool load_record(const dpc245_persist_io_t *io, uint32_t base,
                        uint8_t *dst, uint8_t *out_len) {
    uint8_t n = 0u;
    *out_len = 0u;
    if (!io || !io->read || !io->read(io->ctx, base, &n, 1u)) return false;
    if (n == 0u || n >= 0x2Au) return true;
    if (n > DPC245_HMI_ID_RECORD_MAX) return true;
    uint8_t tmp[DPC245_HMI_ID_RECORD_MAX];
    if (!io->read(io->ctx, base, tmp, n)) return false;
    /* The record is read again with length N; bytes [1..N-1] are returned. */
    uint8_t payload = (uint8_t)(n - 1u);
    copy_bytes(dst, &tmp[1], payload);
    *out_len = payload;
    return true;
}

bool dpc245_hmi_identity_records_load(const dpc245_persist_io_t *io,
                                      dpc245_hmi_identity_records_t *id) {
    if (!id) return false;
    dpc245_hmi_identity_records_init(id);
    if (!load_record(io, 0x000000u, id->serial, &id->serial_len)) return false;
    if (!load_record(io, 0x000100u, id->customer, &id->customer_len)) return false;
    return true;
}

static bool make_text_reply(dpc245_hmi_service_reply_t *r, const char *s) {
    uint8_t n = text_len(s);
    copy_bytes(r->data, (const uint8_t *)s, n);
    r->len = n;
    return true;
}

static uint8_t hex_digit(uint8_t v) {
    v &= 0x0Fu;
    if (v < 10u) return (uint8_t)((uint8_t)'0' + v);
    return (uint8_t)((uint8_t)'A' + (uint8_t)(v - 10u));
}

bool dpc245_hmi_service_build_read_reply(const bafang_can_frame_t *request,
                                         const dpc245_hmi_identity_records_t *id,
                                         const dpc245_settings_t *settings,
                                         dpc245_hmi_service_reply_t *reply) {
    if (!request || !reply || request->dlc != 0u) return false;
    bafang_can_id_t q = bafang_can_id_decode(request->id);
    if (q.dst != BAFANG_NODE_HMI || q.type != BAFANG_CAN_OP_READ || q.cmd != 0x60u) return false;
    if (q.src >= BAFANG_NODE_BROADCAST || q.src == BAFANG_NODE_HMI) return false;

    memset(reply, 0, sizeof(*reply));
    reply->dst = q.src;
    reply->cmd = q.cmd;
    reply->sub = q.sub;

    switch (q.sub) {
    case 0x00u: return make_text_reply(reply, "DP C245.C 3.0");
    case 0x01u: return make_text_reply(reply, "DPC245CF80301.2");
    case 0x02u: return make_text_reply(reply, "DP C245.CAN");
    case 0x03u:
        if (id && id->serial_len) {
            copy_bytes(reply->data, id->serial, id->serial_len);
            reply->len = id->serial_len;
        }
        return true;
    case 0x04u:
        if (id && id->customer_len) {
            copy_bytes(reply->data, id->customer, id->customer_len);
            reply->len = id->customer_len;
        }
        return true;
    case 0x05u: return make_text_reply(reply, "BAFANG");
    case 0x07u: {
        if (!settings) return false;
        uint8_t count = dpc245_error_count(settings);
        if (count > 10u) count = 10u;
        for (uint8_t i = 0u; i < count; ++i) {
            uint8_t code = dpc245_error_at(settings, i);
            reply->data[i * 2u] = hex_digit((uint8_t)(code >> 4));
            reply->data[i * 2u + 1u] = hex_digit(code);
        }
        reply->len = (uint8_t)(count * 2u);
        return true;
    }
    case 0x08u:
        if (!id) return false;
        copy_bytes(reply->data, id->bootloader_info, 8u);
        reply->len = 8u;
        return true;
    default:
        return false;
    }
}

void dpc245_hmi_service_tx_init(dpc245_hmi_service_tx_t *tx) {
    if (tx) memset(tx, 0, sizeof(*tx));
}

bool dpc245_hmi_service_tx_start(dpc245_hmi_service_tx_t *tx,
                                 const dpc245_hmi_service_reply_t *reply) {
    if (!tx || !reply || tx->active || reply->len > BAFANG_SERVICE_MAX_DATA ||
        reply->dst >= BAFANG_NODE_BROADCAST) return false;
    memset(tx, 0, sizeof(*tx));
    tx->active = true;
    tx->dst = reply->dst;
    tx->cmd = reply->cmd;
    tx->sub = reply->sub;
    tx->len = reply->len;
    copy_bytes(tx->data, reply->data, reply->len);
    tx->phase = DPC245_HMI_TX_START;
    return true;
}

static void frame_clear(bafang_can_frame_t *f) {
    f->id = 0u; f->dlc = 0u;
    for (uint8_t i = 0u; i < BAFANG_CAN_MAX_DLC; ++i) f->data[i] = 0u;
}

static void make_id(bafang_can_frame_t *f, uint8_t dst, uint8_t op,
                    uint8_t cmd, uint8_t sub) {
    f->id = bafang_can_id_encode((bafang_can_id_t){
        .src = BAFANG_NODE_HMI, .dst = dst, .type = op, .cmd = cmd, .sub = sub
    });
}

bool dpc245_hmi_service_tx_next(dpc245_hmi_service_tx_t *tx,
                                bafang_can_frame_t *out) {
    if (!tx || !out || !tx->active || tx->waiting_ack) return false;
    frame_clear(out);

    if (tx->phase == DPC245_HMI_TX_START) {
        make_id(out, tx->dst, BAFANG_CAN_OP_MULTIFRAME_START, tx->cmd, tx->sub);
        out->dlc = 1u;
        out->data[0] = tx->len;
        tx->waiting_ack = true;
        return true;
    }

    if (tx->phase == DPC245_HMI_TX_DATA) {
        uint8_t remain = (uint8_t)(tx->len - tx->offset);
        uint8_t n;
        bool first = tx->sequence == 0u;
        bool use_end = !first && remain <= 8u;
        if (use_end) {
            n = remain;
            make_id(out, tx->dst, BAFANG_CAN_OP_MULTIFRAME_END,
                    (uint8_t)(tx->sequence >> 8), (uint8_t)tx->sequence);
            out->dlc = n;
            copy_bytes(out->data, &tx->data[tx->offset], n);
            tx->offset = (uint8_t)(tx->offset + n);
            tx->sequence++;
            tx->phase = DPC245_HMI_TX_WAIT_FINAL_ACK;
        } else {
            n = remain > 8u ? 8u : remain;
            make_id(out, tx->dst, BAFANG_CAN_OP_MULTIFRAME,
                    (uint8_t)(tx->sequence >> 8), (uint8_t)tx->sequence);
            out->dlc = n;
            copy_bytes(out->data, &tx->data[tx->offset], n);
            tx->offset = (uint8_t)(tx->offset + n);
            tx->sequence++;
            if (tx->offset >= tx->len) tx->phase = DPC245_HMI_TX_END;
        }
        tx->waiting_ack = true;
        return true;
    }

    if (tx->phase == DPC245_HMI_TX_END) {
        make_id(out, tx->dst, BAFANG_CAN_OP_MULTIFRAME_END,
                (uint8_t)(tx->sequence >> 8), (uint8_t)tx->sequence);
        out->dlc = 0u;
        tx->sequence++;
        tx->phase = DPC245_HMI_TX_WAIT_FINAL_ACK;
        tx->waiting_ack = true;
        return true;
    }
    return false;
}

bool dpc245_hmi_service_tx_on_ack(dpc245_hmi_service_tx_t *tx,
                                  const bafang_can_frame_t *frame) {
    if (!tx || !frame || !tx->active || !tx->waiting_ack) return false;
    bafang_can_id_t a = bafang_can_id_decode(frame->id);
    if (a.src != tx->dst || a.dst != BAFANG_NODE_HMI ||
        a.type != BAFANG_CAN_OP_NORMAL_ACK || frame->dlc != 0u) return false;
    tx->waiting_ack = false;
    if (tx->phase == DPC245_HMI_TX_START) {
        tx->phase = DPC245_HMI_TX_DATA;
    } else if (tx->phase == DPC245_HMI_TX_WAIT_FINAL_ACK) {
        tx->active = false;
        tx->phase = DPC245_HMI_TX_IDLE;
    }
    return true;
}
