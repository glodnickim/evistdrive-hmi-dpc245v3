/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef BAFANG_SERVICE_H
#define BAFANG_SERVICE_H

#include "bafang_can.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BAFANG_CAN_OP_WRITE = 0x00,
    BAFANG_CAN_OP_READ = 0x01,
    BAFANG_CAN_OP_NORMAL_ACK = 0x02,
    BAFANG_CAN_OP_ERROR_ACK = 0x03,
    BAFANG_CAN_OP_MULTIFRAME_START = 0x04,
    BAFANG_CAN_OP_MULTIFRAME = 0x05,
    BAFANG_CAN_OP_MULTIFRAME_END = 0x06,
    BAFANG_CAN_OP_MULTIFRAME_WARNING = 0x07
} bafang_can_operation_t;

#define BAFANG_SERVICE_MAX_DATA 128u
#define BAFANG_SERVICE_TRACKED_NODES 6u
#define BAFANG_IDENTITY_TEXT_MAX 32u
#define BAFANG_BATTERY_SERVICE_MAX_CELLS 32u

typedef struct {
    uint8_t src;
    uint8_t dst;
    uint8_t cmd;
    uint8_t sub;
    uint8_t operation;
    uint8_t data[BAFANG_SERVICE_MAX_DATA];
    uint8_t len;
    bool error;
} bafang_service_message_t;

typedef struct {
    bool active;
    uint8_t cmd;
    uint8_t sub;
    uint8_t data[BAFANG_SERVICE_MAX_DATA];
    uint8_t len;
    uint8_t next_index;
} bafang_service_reassembly_slot_t;

typedef struct {
    bafang_service_reassembly_slot_t slot[BAFANG_SERVICE_TRACKED_NODES];
} bafang_service_rx_t;

typedef struct {
    char hardware[BAFANG_IDENTITY_TEXT_MAX + 1u];
    char software[BAFANG_IDENTITY_TEXT_MAX + 1u];
    char model[BAFANG_IDENTITY_TEXT_MAX + 1u];
    char serial[BAFANG_IDENTITY_TEXT_MAX + 1u];
    char customer[BAFANG_IDENTITY_TEXT_MAX + 1u];
    char manufacturer[BAFANG_IDENTITY_TEXT_MAX + 1u];
    uint8_t have_mask; /* bit N corresponds to 0x60/N, N=0..5 */
} bafang_device_identity_t;

typedef struct {
    uint16_t cell_mv[BAFANG_BATTERY_SERVICE_MAX_CELLS];
    uint8_t cell_count;
    uint32_t cell_valid_mask;
    uint8_t charging_raw[BAFANG_SERVICE_MAX_DATA];
    uint8_t charging_len;
    uint8_t characteristics_raw[BAFANG_SERVICE_MAX_DATA];
    uint8_t characteristics_len;
} bafang_battery_service_info_t;

void bafang_service_rx_init(bafang_service_rx_t *rx);

/* Feed a raw CAN frame addressed to the HMI service endpoint.
 * Returns true only when a complete logical response is produced in `out`.
 * If `ack_needed` is non-NULL it is set for MULTIFRAME START/CONT/END frames;
 * the caller may then send bafang_build_service_ack() if TX is enabled. */
bool bafang_service_rx_feed(bafang_service_rx_t *rx,
                            const bafang_can_frame_t *frame,
                            bafang_service_message_t *out,
                            bool *ack_needed);

bool bafang_build_service_ack(bafang_can_frame_t *out,
                              uint8_t dst,
                              uint8_t cmd,
                              uint8_t sub);

bool bafang_identity_apply_service(bafang_device_identity_t *id,
                                   const bafang_service_message_t *msg);

bool bafang_battery_service_apply(bafang_battery_service_info_t *info,
                                  const bafang_service_message_t *msg);

#ifdef __cplusplus
}
#endif
#endif
