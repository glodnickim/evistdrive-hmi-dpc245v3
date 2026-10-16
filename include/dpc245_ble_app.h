/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_BLE_APP_H
#define DPC245_BLE_APP_H

#include "can_router.h"
#include "dpc245_app.h"
#include "dpc245_ble_proto.h"
#include "dpc245_hmi_service.h"
#include "dpc245_update.h"
#include "controller_write.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DPC245_BLE_A1_SIZE 255u
#define DPC245_BLE_A2_SIZE 255u
#define DPC245_BLE_A3_SIZE 237u
#define DPC245_BLE_A4_SIZE 244u
#define DPC245_BLE_A5_SIZE 198u
#define DPC245_BLE_A7_SIZE 47u
#define DPC245_BLE_A9_SIZE 115u

typedef enum {
    DPC245_BLE_REJECT_UNSUPPORTED = 0,
    DPC245_BLE_REJECT_CLASS,
    DPC245_BLE_REJECT_READ_REQUIRED,
    DPC245_BLE_REJECT_BUSY,
    DPC245_BLE_REJECT_GLOBAL_MODE,
    DPC245_BLE_REJECT_PAYLOAD,
    DPC245_BLE_REJECT_POLICY
} dpc245_ble_reject_t;

typedef struct {
    uint8_t a1[DPC245_BLE_A1_SIZE];
    uint8_t a2[DPC245_BLE_A2_SIZE];
    uint8_t a3[DPC245_BLE_A3_SIZE];
    uint8_t a4[DPC245_BLE_A4_SIZE];
    uint8_t a5[DPC245_BLE_A5_SIZE];
    uint8_t a7[DPC245_BLE_A7_SIZE];
    uint8_t a9[DPC245_BLE_A9_SIZE];
    uint8_t p0[64];
    uint8_t p1[64];
    uint8_t p2[64];
    uint8_t write_candidate[64];
    uint8_t download_status[8]; /* indices 4=P0, 5=P1, 6=P2, 7=start angle */
    uint8_t download_stage;     /* F203, P1, P2, P0, angle, complete */
    uint8_t start_angle[2];
    uint8_t stage_attempts;
    uint8_t pending_write;      /* 0=none, 1=P1, 2=P0 */
    bool per_level;
    bool discovery_done;
    bool controller_seen;
    bool controller_txn_active;
    bool active_is_write;
    uint8_t status_flags;
    uint8_t pending_hmi_event_bits;
    uint32_t handled_frames;
    uint32_t unsupported_frames;
    uint32_t rejected_write_frames;
    uint8_t rejected_class;
    uint8_t rejected_command;
    uint8_t rejected_operation;
    uint8_t rejected_selector;
    dpc245_ble_reject_t rejected_reason;
} dpc245_ble_app_t;

typedef struct {
    bool handled;
    bool have_tx;
    bool settings_dirty;
    bool update_requested;
    size_t tx_len;
    uint8_t tx[DPC245_BLE_MAX_WIRE];
} dpc245_ble_app_result_t;

void dpc245_ble_app_init(dpc245_ble_app_t *b);
/* One shared controller engine serves the staged phone cache and ADV pages. */
bool dpc245_ble_app_start_controller(dpc245_ble_app_t *b, evist_ctrl_txn_t *txn,
                                     evist_firmware_profile_t profile, uint32_t now_ms);
void dpc245_ble_app_on_controller_result(dpc245_ble_app_t *b, evist_txn_kind_t kind,
                                         evist_txn_result_t result, const uint8_t data[64]);
void dpc245_ble_app_capture_adv_result(dpc245_ble_app_t *b, evist_txn_kind_t kind,
                                       evist_txn_result_t result, const uint8_t data[64]);
void dpc245_ble_app_apply_advanced_cache(const dpc245_ble_app_t *b, evist_advanced_t *adv);

/* Handles one checksum-valid application frame.  Known fields are exposed
 * through typed application state; unlabelled bulk fields are retained in
 * fixed-size compatibility blocks instead of being assigned speculative names. */
bool dpc245_ble_app_handle(dpc245_ble_app_t *b,
                           const dpc245_ble_frame_t *request,
                           dpc245_app_t *app,
                           evist_can_router_t *router,
                           const dpc245_hmi_identity_records_t *hmi_identity,
                           bool light_on,
                           dpc245_update_t *update,
                           dpc245_ble_app_result_t *result);

/* Periodic phone-facing status envelope.  The payload is 39 bytes and the
 * header is 10/11/06/01. */
size_t dpc245_ble_app_build_periodic(uint8_t *out, size_t capacity,
                                     const dpc245_app_t *app,
                                     const evist_can_router_t *router,
                                     bool light_on);

/* One-shot HMI event bits consumed by the 0x6300 heartbeat. */
uint8_t dpc245_ble_app_take_hmi_event_bits(dpc245_ble_app_t *b);

#endif
