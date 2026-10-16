/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_HMI_SERVICE_H
#define DPC245_HMI_SERVICE_H

#include "bafang_can.h"
#include "bafang_service.h"
#include "dpc245_persist.h"
#include "dpc245_settings.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DPC245_HMI_ID_RECORD_MAX 40u

typedef struct {
    uint8_t serial[DPC245_HMI_ID_RECORD_MAX];
    uint8_t serial_len;
    uint8_t customer[DPC245_HMI_ID_RECORD_MAX];
    uint8_t customer_len;
    /* 0x60/08: eight raw bytes from the resident bootloader tail
     * at the resident metadata address. */
    uint8_t bootloader_info[8];
} dpc245_hmi_identity_records_t;

typedef struct {
    uint8_t dst;
    uint8_t cmd;
    uint8_t sub;
    uint8_t data[BAFANG_SERVICE_MAX_DATA];
    uint8_t len;
} dpc245_hmi_service_reply_t;

/* Length-prefixed identity records in external SPI memory:
 *   0x000: [N][serial bytes...], response length N-1
 *   0x100: [N][customer bytes...], response length N-1
 * N==0 or N>=0x2A is treated as absent/invalid. */
void dpc245_hmi_identity_records_init(dpc245_hmi_identity_records_t *id);
bool dpc245_hmi_identity_records_load(const dpc245_persist_io_t *io,
                                      dpc245_hmi_identity_records_t *id);

/* Build the logical payload for HMI command 0x60 READ requests. The fixed
 * strings are the standard DPC245 V3 identification, answered for
 * interoperability with the controller and service tools (docs/CAN_PROTOCOL.md):
 *   00 HW           "DP C245.C 3.0"
 *   01 SW           "DPC245CF80301.2"
 *   02 Model        "DP C245.CAN"
 *   03 Serial       external record 0x000
 *   04 Customer     external record 0x100
 *   05 Manufacturer "BAFANG"
 *   07 Error history as two uppercase ASCII hex chars per stored code
 *   08 bootLoad: 8 raw bytes from resident bootloader metadata,
 *      the final 16-byte area of the resident 24 KiB bootloader. */
bool dpc245_hmi_service_build_read_reply(const bafang_can_frame_t *request,
                                         const dpc245_hmi_identity_records_t *id,
                                         const dpc245_settings_t *settings,
                                         dpc245_hmi_service_reply_t *reply);

/* Multiframe responder. The HMI uses the long transfer engine even
 * for short 0x60 replies: START(length), one or more data frames, END, with a
 * NORMAL_ACK handshake after each frame. */
typedef enum {
    DPC245_HMI_TX_IDLE = 0,
    DPC245_HMI_TX_START,
    DPC245_HMI_TX_DATA,
    DPC245_HMI_TX_END,
    DPC245_HMI_TX_WAIT_FINAL_ACK
} dpc245_hmi_service_tx_phase_t;

typedef struct {
    bool active;
    bool waiting_ack;
    uint8_t dst;
    uint8_t cmd;
    uint8_t sub;
    uint8_t data[BAFANG_SERVICE_MAX_DATA];
    uint8_t len;
    uint8_t offset;
    uint16_t sequence;
    dpc245_hmi_service_tx_phase_t phase;
} dpc245_hmi_service_tx_t;

void dpc245_hmi_service_tx_init(dpc245_hmi_service_tx_t *tx);
bool dpc245_hmi_service_tx_start(dpc245_hmi_service_tx_t *tx,
                                 const dpc245_hmi_service_reply_t *reply);
bool dpc245_hmi_service_tx_next(dpc245_hmi_service_tx_t *tx,
                                bafang_can_frame_t *out);
bool dpc245_hmi_service_tx_on_ack(dpc245_hmi_service_tx_t *tx,
                                  const bafang_can_frame_t *frame);

#ifdef __cplusplus
}
#endif
#endif
