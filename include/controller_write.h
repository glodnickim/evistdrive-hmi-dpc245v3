/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef EVIST_CONTROLLER_WRITE_H
#define EVIST_CONTROLLER_WRITE_H

/* Controller configuration transactions: WHEEL (0x3203), ASSIST (0x6011),
 * ACCEL discovery (0xF203), P0 (0x6010), and P2 (0x6012).
 *
 * Long-write frame order uses the controller transaction contract. The sender advances
 * after transmit completion, without a controller ACK gate per fragment:
 *   - every frame is sent as the display node (3) to the controller (2);
 *   - WHEEL: one 6-byte WRITE, ACK (DLC 0) within 600 ms, 400 ms settle,
 *     READ 0x3203, fresh 6-byte reply within 600 ms, byte-exact compare;
 *     an ERROR_ACK during either wait is recorded until the deadline;
 *   - parameter blocks: declaration + 8 data frames as the CAN mailbox becomes
 *     idle, with a 334 ms limit for each pending send, then a 500 ms settle
 *     and READ/compare. Validator ERROR_ACK rejects the write; other
 *     ERROR_ACK frames are recorded while verification continues;
 *     NORMAL_ACK frames are optional. P1 uses up
 *     to three READs (450 ms each, 150 ms apart); P0/P2 use one 1050 ms READ.
 *
 * A reply counts only while this transaction waits for the READ it has just
 * sent: frames received earlier, broadcasts and unrelated traffic never
 * complete a transaction. Every receive window is half-open: ACK or data is
 * accepted only while now < deadline, whatever the order in which the caller
 * runs RX and tick. A reassembled 0x6011 block counts only if this
 * transaction saw its START frame after its own READ (the caller passes every
 * received frame to on_frame before the router), for a fresh transaction
 * clearing its multi-frame state before each READ. Without a request id the
 * protocol cannot tell a late reply to an earlier READ whose START arrives
 * after this one, nor frames still queued in the receive FIFO when the READ
 * leaves. Pure logic; the caller moves frames. */

#include "bafang_can.h"
#include "bafang_service.h"
#include "evist_preferences.h"
#include <stdbool.h>
#include <stdint.h>

#define EVIST_WHEEL_PAYLOAD_LEN 6u

typedef struct {
    uint16_t speed_limit_centi_kph;
    uint16_t wheel_code;          /* on-wire diameter code, e.g. 0x01D0 = 29" */
    uint16_t circumference_mm;
} evist_wheel_config_t;

typedef enum {
    EVIST_TXN_READ_WHEEL = 0,
    EVIST_TXN_WRITE_WHEEL,
    EVIST_TXN_READ_ASSIST,
    EVIST_TXN_WRITE_ASSIST,
    EVIST_TXN_DISCOVER_ACCEL,
    EVIST_TXN_READ_ACCEL_P0,
    EVIST_TXN_READ_ACCEL_P2,
    EVIST_TXN_WRITE_APP_P1,
    EVIST_TXN_WRITE_APP_P0,
    EVIST_TXN_WRITE_APP_P2,
    EVIST_TXN_READ_START_ANGLE
} evist_txn_kind_t;

typedef enum {
    EVIST_TXN_OK = 0,
    EVIST_TXN_NO_ACK,
    EVIST_TXN_TX_TIMEOUT,
    EVIST_TXN_NO_READBACK,
    EVIST_TXN_CHECKSUM_BAD,
    EVIST_TXN_MISMATCH,
    EVIST_TXN_NACK,
    EVIST_TXN_REJECTED
} evist_txn_result_t;

typedef enum {
    EVIST_TXN_STEP_IDLE = 0,
    EVIST_TXN_STEP_SEND_WRITE,
    EVIST_TXN_STEP_WAIT_ACK,
    EVIST_TXN_STEP_SETTLE,
    EVIST_TXN_STEP_SEND_READ,
    EVIST_TXN_STEP_WAIT_READ,
    EVIST_TXN_STEP_READ_GAP,
    EVIST_TXN_STEP_DONE
} evist_txn_step_t;

typedef struct {
    evist_txn_kind_t kind;
    evist_txn_step_t step;
    evist_txn_result_t result;
    uint8_t frame_index;
    uint8_t write_attempts;
    uint8_t read_attempts;
    bool ack_seen;
    bool alternate_read;
    bool block_start_seen;     /* START of a 0x6011 reply seen after our READ */
    bool read_error_seen;
    bool error_seen;
    bool final_ack_seen;
    uint8_t ack_count;
    uint8_t error_payload[4];
    uint8_t error_len;
    uint32_t final_frame_ms;
    uint8_t p0_read_flag;       /* P0[63] is canonical read flag, write uses sum8 */
    uint32_t due_ms;
    uint8_t expected[64];
    uint8_t readback[64];
} evist_ctrl_txn_t;

void evist_wheel_config_encode(const evist_wheel_config_t *c, uint8_t out[EVIST_WHEEL_PAYLOAD_LEN]);
void evist_wheel_config_decode(const uint8_t in[EVIST_WHEEL_PAYLOAD_LEN], evist_wheel_config_t *c);

void evist_ctrl_txn_init(evist_ctrl_txn_t *t);
bool evist_ctrl_txn_busy(const evist_ctrl_txn_t *t);

/* Starting fails while another transaction runs or when the request breaks the
 * write policy (ASSIST: evist_assist_write_allowed for the given profile). */
bool evist_ctrl_txn_read_wheel(evist_ctrl_txn_t *t, uint32_t now_ms);
bool evist_ctrl_txn_write_wheel(evist_ctrl_txn_t *t, const evist_wheel_config_t *c, uint32_t now_ms);
bool evist_ctrl_txn_read_assist(evist_ctrl_txn_t *t, uint32_t now_ms);
bool evist_ctrl_txn_discover_accel(evist_ctrl_txn_t *t, uint32_t now_ms);
bool evist_ctrl_txn_read_accel_p0(evist_ctrl_txn_t *t, uint32_t now_ms);
bool evist_ctrl_txn_read_accel_p2(evist_ctrl_txn_t *t, uint32_t now_ms);
bool evist_ctrl_txn_read_start_angle(evist_ctrl_txn_t *t, uint32_t now_ms);
bool evist_ctrl_txn_write_assist(evist_ctrl_txn_t *t, const uint8_t block[64],
                                 evist_firmware_profile_t profile, uint32_t now_ms);
/* Phone and menu block writes share the timed long-write sender. */
bool evist_ctrl_txn_write_app_p1(evist_ctrl_txn_t *t, const uint8_t block[64],
                                 evist_firmware_profile_t profile, uint32_t now_ms);
bool evist_ctrl_txn_write_app_p0(evist_ctrl_txn_t *t, const uint8_t block[64],
                                 uint8_t read_flag, uint32_t now_ms);
bool evist_ctrl_txn_write_app_p2(evist_ctrl_txn_t *t, const uint8_t block[64],
                                 uint32_t now_ms);

/* Drive the transaction from the main loop. next_tx yields at most one frame. */
bool evist_ctrl_txn_next_tx(evist_ctrl_txn_t *t, uint32_t now_ms, bafang_can_frame_t *out);
void evist_ctrl_txn_on_frame(evist_ctrl_txn_t *t, const bafang_can_frame_t *f, uint32_t now_ms);
void evist_ctrl_txn_on_service(evist_ctrl_txn_t *t, const bafang_service_message_t *m, uint32_t now_ms);
void evist_ctrl_txn_tick(evist_ctrl_txn_t *t, uint32_t now_ms);

/* Returns true once per finished transaction. data receives the readback
 * (6 bytes for WHEEL, 64 for parameter blocks) on OK or MISMATCH. */
bool evist_ctrl_txn_take_result(evist_ctrl_txn_t *t, evist_txn_kind_t *kind,
                                evist_txn_result_t *result, uint8_t data[64]);

const char *evist_txn_result_text(evist_txn_result_t result);

#endif
