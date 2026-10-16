/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_ble_proto.h"

uint16_t dpc245_ble_checksum(const uint8_t *data, size_t len) {
    uint16_t sum = 0u;
    if (!data && len) return 0u;
    for (size_t i = 0; i < len; ++i) sum = (uint16_t)(sum + data[i]);
    return (uint16_t)~sum;
}

static void copy_bytes(uint8_t *dst, const uint8_t *src, size_t n) {
    for(size_t i=0;i<n;++i) dst[i]=src[i];
}

void dpc245_ble_parser_init(dpc245_ble_parser_t *p) {
    if (!p) return;
    uint8_t *b=(uint8_t *)(void *)p;
    for(size_t i=0;i<sizeof(*p);++i) b[i]=0u;
}

static void parser_reset(dpc245_ble_parser_t *p) {
    p->pos = 0u;
    p->expected = 0u;
}

bool dpc245_ble_parser_feed(dpc245_ble_parser_t *p, uint8_t byte, dpc245_ble_frame_t *out) {
    if (!p || !out) return false;

    if (p->pos == 0u) {
        if (byte == DPC245_BLE_SYNC0) {
            p->wire[0] = byte;
            p->pos = 1u;
        }
        return false;
    }

    if (p->pos == 1u) {
        if (byte == DPC245_BLE_SYNC1) {
            p->wire[1] = byte;
            p->pos = 2u;
        } else if (byte == DPC245_BLE_SYNC0) {
            /* The parser also tolerates a repeated 0x55 while syncing. */
            p->wire[0] = byte;
            p->pos = 1u;
        } else {
            p->framing_resets++;
            parser_reset(p);
        }
        return false;
    }

    if (p->pos >= DPC245_BLE_MAX_WIRE) {
        p->framing_resets++;
        parser_reset(p);
        return false;
    }

    p->wire[p->pos++] = byte;
    if (p->pos == 3u) {
        p->expected = (uint16_t)p->wire[2] + DPC245_BLE_WIRE_OVERHEAD;
        if (p->expected > DPC245_BLE_MAX_WIRE) {
            p->framing_resets++;
            parser_reset(p);
            return false;
        }
    }

    if (p->expected == 0u || p->pos < p->expected) return false;
    if (p->pos != p->expected) {
        p->framing_resets++;
        parser_reset(p);
        return false;
    }

    const uint8_t len = p->wire[2];
    const uint16_t calc = dpc245_ble_checksum(&p->wire[2], (size_t)len + 5u);
    const uint16_t recv = (uint16_t)p->wire[(size_t)len + 7u] |
                          ((uint16_t)p->wire[(size_t)len + 8u] << 8);
    if (calc != recv) {
        p->bad_checksums++;
        parser_reset(p);
        return false;
    }

    out->length = len;
    out->class_id = p->wire[3];
    out->command_id = p->wire[4];
    out->operation = p->wire[5];
    out->selector = p->wire[6];
    if (len) copy_bytes(out->payload, &p->wire[7], len);
    out->checksum = recv;
    p->valid_frames++;
    parser_reset(p);
    return true;
}

size_t dpc245_ble_frame_build(uint8_t *out, size_t capacity,
                              uint8_t class_id, uint8_t command_id,
                              uint8_t operation, uint8_t selector,
                              const uint8_t *payload, uint8_t payload_len) {
    const size_t total = (size_t)payload_len + DPC245_BLE_WIRE_OVERHEAD;
    if (!out || capacity < total || (payload_len && !payload)) return 0u;
    out[0] = DPC245_BLE_SYNC0;
    out[1] = DPC245_BLE_SYNC1;
    out[2] = payload_len;
    out[3] = class_id;
    out[4] = command_id;
    out[5] = operation;
    out[6] = selector;
    if (payload_len) copy_bytes(&out[7], payload, payload_len);
    const uint16_t chk = dpc245_ble_checksum(&out[2], (size_t)payload_len + 5u);
    out[(size_t)payload_len + 7u] = (uint8_t)chk;
    out[(size_t)payload_len + 8u] = (uint8_t)(chk >> 8);
    return total;
}
