/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_BLE_PROTO_H
#define DPC245_BLE_PROTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DPC245_BLE_SYNC0 0x55u
#define DPC245_BLE_SYNC1 0xAAu
#define DPC245_BLE_MAX_PAYLOAD 255u
#define DPC245_BLE_WIRE_OVERHEAD 9u
#define DPC245_BLE_MAX_WIRE (DPC245_BLE_MAX_PAYLOAD + DPC245_BLE_WIRE_OVERHEAD)

typedef struct {
    uint8_t length;
    /* Field names below are descriptive. RX dispatch treats class_id/command_id
     * as nested selectors, operation as read/write/response-like selector, and
     * selector as the command-specific subselector. */
    uint8_t class_id;   /* wire H1 */
    uint8_t command_id; /* wire H2 */
    uint8_t operation;  /* wire H3 */
    uint8_t selector;   /* wire H4 */
    uint8_t payload[DPC245_BLE_MAX_PAYLOAD];
    uint16_t checksum;
} dpc245_ble_frame_t;

typedef struct {
    uint8_t wire[DPC245_BLE_MAX_WIRE];
    uint16_t pos;
    uint16_t expected;
    uint32_t valid_frames;
    uint32_t bad_checksums;
    uint32_t framing_resets;
} dpc245_ble_parser_t;

void dpc245_ble_parser_init(dpc245_ble_parser_t *p);
/* Returns true exactly when one checksum-valid frame is completed. */
bool dpc245_ble_parser_feed(dpc245_ble_parser_t *p, uint8_t byte, dpc245_ble_frame_t *out);

/* Checksum: one's complement of the 16-bit byte sum. */
uint16_t dpc245_ble_checksum(const uint8_t *data, size_t len);

/* Builds the DPC245 wire form:
 * 55 AA LEN H1 H2 H3 H4 PAYLOAD... CHKlo CHKhi
 * Returns number of bytes written, or 0 on invalid arguments/capacity. */
size_t dpc245_ble_frame_build(uint8_t *out, size_t capacity,
                              uint8_t class_id, uint8_t command_id,
                              uint8_t operation, uint8_t selector,
                              const uint8_t *payload, uint8_t payload_len);

#endif
