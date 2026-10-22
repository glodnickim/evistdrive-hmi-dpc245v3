/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "app_config.h"
#include "assist_profile.h"
#include "dpc245_ble_app.h"
#include "dpc245_settings.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Sample P1 block. */
static const uint8_t p1_log[64] = {
    0x28,0x0F,0x3B,0xA8,0x57,0x00,0x00,0xB0,
    0x36,0x42,0xFF,0xFF,0x0A,0x00,0x01,0x00,
    0x00,0x00,0x00,0x50,0x01,0xFF,0x00,0x00,
    0xCA,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x08,0x17,0x19,0x64,0x01,0x09,
    0x0F,0x14,0x1E,0x28,0x3C,0x3C,0x46,0x50,
    0x64,0x00,0x64,0x00,0x64,0x00,0x64,0x00,
    0x64,0x64,0x00,0x00,0x1E,0x00,0x00,0x4F
};

static bafang_can_frame_t motor_frame(uint8_t op, uint8_t cmd, uint8_t sub) {
    bafang_can_frame_t f = {0};
    f.id = bafang_can_id_encode((bafang_can_id_t){
        .src = BAFANG_NODE_CONTROLLER, .dst = BAFANG_NODE_HMI,
        .type = op, .cmd = cmd, .sub = sub});
    return f;
}

static void stage_result(dpc245_ble_app_t *b, evist_ctrl_txn_t *t,
                         evist_txn_kind_t kind, evist_txn_result_t result,
                         const uint8_t data[64]) {
    dpc245_ble_app_on_controller_result(b, kind, result, data);
    evist_ctrl_txn_init(t); /* main consumes the finished transaction */
}

static void test_download(void) {
    dpc245_ble_app_t b; dpc245_ble_app_init(&b);
    evist_ctrl_txn_t t; evist_ctrl_txn_init(&t);
    b.controller_seen = true;
    assert(dpc245_ble_app_start_controller(&b, &t, EVIST_PROFILE_BAFANG, 0u));
    bafang_can_frame_t f;
    assert(evist_ctrl_txn_next_tx(&t, 0u, &f));
    assert(bafang_can_id_decode(f.id).cmd == 0xF2u);
    stage_result(&b, &t, EVIST_TXN_DISCOVER_ACCEL, EVIST_TXN_NO_READBACK, NULL);
    assert(dpc245_ble_app_start_controller(&b, &t, EVIST_PROFILE_BAFANG, 700u));
    stage_result(&b, &t, EVIST_TXN_DISCOVER_ACCEL, EVIST_TXN_NO_READBACK, NULL);
    assert(b.download_stage == 1u && !b.per_level);
    assert(dpc245_ble_app_start_controller(&b, &t, EVIST_PROFILE_BAFANG, 1400u));
    assert(b.download_status[5] == 1u);
    assert(evist_ctrl_txn_next_tx(&t, 1400u, &f));
    assert(bafang_can_id_decode(f.id).sub == 0x11u);
    stage_result(&b, &t, EVIST_TXN_READ_ASSIST, EVIST_TXN_OK, p1_log);
    assert(b.download_status[5] == 3u && b.p1[40] == 0x0Fu);
    assert(dpc245_ble_app_start_controller(&b, &t, EVIST_PROFILE_BAFANG, 1500u));
    assert(evist_ctrl_txn_next_tx(&t, 1500u, &f));
    assert(bafang_can_id_decode(f.id).sub == 0x12u);
    stage_result(&b, &t, EVIST_TXN_READ_ACCEL_P2, EVIST_TXN_NO_READBACK, NULL);
    assert(dpc245_ble_app_start_controller(&b, &t, EVIST_PROFILE_BAFANG, 2600u));
    stage_result(&b, &t, EVIST_TXN_READ_ACCEL_P2, EVIST_TXN_NO_READBACK, NULL);
    assert(b.download_status[6] == 4u && b.download_stage == 3u);
    assert(!dpc245_ble_app_start_controller(&b, &t, EVIST_PROFILE_BAFANG, 3700u));
    assert(b.download_stage == 4u && b.download_status[4] == 0u);
    assert(dpc245_ble_app_start_controller(&b, &t, EVIST_PROFILE_BAFANG, 3800u));
    assert(b.download_status[7] == 1u);
    bafang_can_frame_t angle_reply = motor_frame(BAFANG_CAN_OP_NORMAL_ACK, 0x62u, 0xD9u);
    angle_reply.dlc = 2u; angle_reply.data[0] = 4u; angle_reply.data[1] = 0u;
    evist_ctrl_txn_on_frame(&t, &angle_reply, 3800u); /* reply before our READ is ignored */
    assert(evist_ctrl_txn_busy(&t));
    assert(evist_ctrl_txn_next_tx(&t, 3800u, &f));
    bafang_can_id_t angle_read = bafang_can_id_decode(f.id);
    assert(angle_read.src == BAFANG_NODE_HMI && angle_read.dst == BAFANG_NODE_CONTROLLER &&
           angle_read.type == BAFANG_CAN_OP_READ && angle_read.cmd == 0x62u && angle_read.sub == 0xD9u);
    evist_ctrl_txn_on_frame(&t, &angle_reply, 3802u);
    evist_txn_kind_t angle_kind; evist_txn_result_t angle_result; uint8_t angle_data[64] = {0};
    assert(evist_ctrl_txn_take_result(&t, &angle_kind, &angle_result, angle_data));
    assert(angle_kind == EVIST_TXN_READ_START_ANGLE && angle_result == EVIST_TXN_OK);
    dpc245_ble_app_on_controller_result(&b, angle_kind, angle_result, angle_data);
    assert(b.download_status[7] == 3u && b.start_angle[0] == 4u && b.start_angle[1] == 0u);
    assert(b.download_stage == 5u);

    dpc245_ble_app_init(&b); evist_ctrl_txn_init(&t); b.controller_seen = true;
    assert(dpc245_ble_app_start_controller(&b, &t, EVIST_PROFILE_BAFANG, 0u));
    stage_result(&b, &t, EVIST_TXN_DISCOVER_ACCEL, EVIST_TXN_OK, NULL);
    assert(b.per_level && b.download_stage == 1u);
    assert(dpc245_ble_app_start_controller(&b, &t, EVIST_PROFILE_BAFANG, 1u));
    stage_result(&b, &t, EVIST_TXN_READ_ASSIST, EVIST_TXN_OK, p1_log);
    uint8_t p2[64] = {0}; p2[54] = 7u; p2[63] = evist_assist_checksum(p2);
    assert(dpc245_ble_app_start_controller(&b, &t, EVIST_PROFILE_BAFANG, 2u));
    stage_result(&b, &t, EVIST_TXN_READ_ACCEL_P2, EVIST_TXN_OK, p2);
    assert(dpc245_ble_app_start_controller(&b, &t, EVIST_PROFILE_BAFANG, 3u));
    assert(evist_ctrl_txn_next_tx(&t, 3u, &f));
    assert(bafang_can_id_decode(f.id).sub == 0x10u);
    uint8_t p0[64] = {0};
    for (uint8_t i = 1u; i <= 9u; ++i) p0[i] = i < 8u ? i : 8u;
    stage_result(&b, &t, EVIST_TXN_READ_ACCEL_P0, EVIST_TXN_OK, p0);
    assert(b.download_stage == 4u && b.download_status[4] == 3u);
    assert(dpc245_ble_app_start_controller(&b, &t, EVIST_PROFILE_BAFANG, 4u));
    assert(evist_ctrl_txn_next_tx(&t, 4u, &f));
    angle_read = bafang_can_id_decode(f.id);
    assert(angle_read.cmd == 0x62u && angle_read.sub == 0xD9u);
    angle_reply = motor_frame(BAFANG_CAN_OP_NORMAL_ACK, 0x62u, 0xD9u);
    angle_reply.dlc = 2u; angle_reply.data[0] = 4u; angle_reply.data[1] = 0u;
    evist_ctrl_txn_on_frame(&t, &angle_reply, 5u);
    assert(evist_ctrl_txn_take_result(&t, &angle_kind, &angle_result, angle_data));
    assert(angle_result == EVIST_TXN_OK);
    dpc245_ble_app_on_controller_result(&b, angle_kind, angle_result, angle_data);

    dpc245_settings_t settings; dpc245_settings_defaults(&settings);
    dpc245_app_t app; dpc245_app_init(&app, &settings);
    app.advanced.committed.firmware_profile = EVIST_PROFILE_FAKE_TAXI;
    evist_can_router_t router; evist_can_router_init(&router);
    dpc245_hmi_identity_records_t id; dpc245_hmi_identity_records_init(&id);
    dpc245_update_t update; dpc245_update_init(&update);
    dpc245_ble_app_result_t result;
    dpc245_ble_frame_t q = {0};
    q.class_id = 0x11u; q.command_id = 0xA3u; q.operation = 1u;
    q.selector = 215u; q.length = 1u; q.payload[0] = 19u;
    assert(dpc245_ble_app_handle(&b, &q, &app, &router, &id, false, &update, &result));
    assert(b.a3[211] == 4u && b.a3[212] == 0u);
    assert(b.a3[213] == 7u && b.a3[215] == p1_log[49] && b.a3[225] == p1_log[40]);
    q.command_id = 0xA9u; q.selector = 64u; q.payload[0] = 51u;
    assert(dpc245_ble_app_handle(&b, &q, &app, &router, &id, false, &update, &result));
    assert(b.a9[64] == 1u && b.a9[65] == 4u && b.a9[66] == 0u &&
           b.a9[83] == 4u && b.a9[84] == 0u && b.a9[85] == 7u && b.a9[86] == 1u);
    assert(b.a9[96] == p1_log[49] && b.a9[106] == p1_log[40]);
    puts("download order/status and A3/A9 fixture PASS");
}

static void test_app_write(void) {
    dpc245_ble_app_t b; dpc245_ble_app_init(&b);
    b.controller_seen = true; b.download_stage = 5u;
    b.download_status[5] = b.download_status[6] = 3u;
    memcpy(b.p1, p1_log, 64u);
    dpc245_settings_t settings; dpc245_settings_defaults(&settings);
    dpc245_app_t app; dpc245_app_init(&app, &settings);
    app.advanced.committed.firmware_profile = EVIST_PROFILE_FAKE_TAXI;
    evist_can_router_t router; evist_can_router_init(&router);
    dpc245_hmi_identity_records_t id; dpc245_hmi_identity_records_init(&id);
    dpc245_update_t update; dpc245_update_init(&update);
    dpc245_ble_app_result_t result;
    dpc245_ble_frame_t q = {0};
    q.class_id = 0x11u; q.command_id = 0xA3u; q.operation = 2u;
    evist_ctrl_txn_t t; evist_ctrl_txn_init(&t);
#if EVIST_ENABLE_CONFIG_WRITES
    b.download_status[5] = 4u;
    q.selector = 226u; q.length = 1u; q.payload[0] = 0x15u;
    assert(!dpc245_ble_app_handle(&b, &q, &app, &router, &id, false, &update, &result));
    assert(!b.pending_write && !result.have_tx);
    assert(b.rejected_write_frames == 1u && b.rejected_class == 0x11u &&
           b.rejected_command == 0xA3u && b.rejected_operation == 2u &&
           b.rejected_selector == 226u);
    assert(b.rejected_reason == DPC245_BLE_REJECT_READ_REQUIRED);
    q.command_id = 0xA9u;
    assert(!dpc245_ble_app_handle(&b, &q, &app, &router, &id, false, &update, &result));
    assert(b.rejected_write_frames == 2u && b.rejected_reason == DPC245_BLE_REJECT_GLOBAL_MODE);
    q.command_id = 0xA3u;
    assert(!dpc245_ble_app_start_controller(&b, &t, EVIST_PROFILE_FAKE_TAXI, 0u));
    assert(!evist_ctrl_txn_next_tx(&t, 0u, &(bafang_can_frame_t){0}));
    b.download_status[5] = 3u;
    for (uint8_t i = 0u; i < 9u; ++i) {
        b.a3[215u + i] = b.p1[49u + i];
        b.a3[225u + i] = b.p1[40u + i];
    }
    b.download_status[6] = 4u;
    q.selector = 160u; q.payload[0] = 0x31u;
    assert(dpc245_ble_app_handle(&b, &q, &app, &router, &id, false, &update, &result));
    assert(result.have_tx && b.pending_write == 1u);
    assert(memcmp(b.write_candidate, b.p1, 64u) == 0);
    b.pending_write = 0u;
    b.download_status[6] = 3u;
#endif
    q.selector = 226u; q.length = 1u; q.payload[0] = 0x15u;
    assert(dpc245_ble_app_handle(&b, &q, &app, &router, &id, false, &update, &result));
    assert(result.have_tx && result.tx[5] == 0x05u);
#if EVIST_ENABLE_CONFIG_WRITES
    assert(b.pending_write == 1u);
    assert(evist_ctrl_txn_read_wheel(&t, 0u));
    assert(!dpc245_ble_app_start_controller(&b, &t, EVIST_PROFILE_FAKE_TAXI, 0u));
    evist_ctrl_txn_init(&t);
    assert(dpc245_ble_app_start_controller(&b, &t, EVIST_PROFILE_FAKE_TAXI, 0u));
    assert(!evist_ctrl_txn_read_wheel(&t, 0u));
    uint8_t expected[64]; memcpy(expected, p1_log, 64u);
    expected[41] = 0x15u; expected[63] = evist_assist_checksum(expected);
    assert(memcmp(expected, b.write_candidate, 64u) == 0);
    uint32_t now = 0u;
    bafang_can_frame_t read;
    for (uint8_t i = 0u; i < 9u; ++i) {
        bafang_can_frame_t f;
        assert(evist_ctrl_txn_next_tx(&t, now, &f));
        bafang_can_id_t cid = bafang_can_id_decode(f.id);
        assert(cid.src == BAFANG_NODE_HMI && cid.dst == BAFANG_NODE_CONTROLLER);
        assert(cid.type == (i == 0u ? BAFANG_CAN_OP_WRITE : i == 1u ?
                           BAFANG_CAN_OP_MULTIFRAME_START : i == 8u ?
                           BAFANG_CAN_OP_MULTIFRAME_END : BAFANG_CAN_OP_MULTIFRAME));
        if (i == 0u) assert(f.dlc == 1u && f.data[0] == 64u && cid.sub == 0x11u);
        else {
            assert(f.dlc == 8u && memcmp(f.data, &expected[(i - 1u) * 8u], 8u) == 0);
            assert(cid.sub == (i == 1u ? 0x11u : i == 8u ? 6u : i - 2u));
        }
        now += 1u;
    }
    evist_ctrl_txn_tick(&t, now + 499u);
    assert(evist_ctrl_txn_next_tx(&t, now + 499u, &read));
    assert(bafang_can_id_decode(read.id).type == BAFANG_CAN_OP_READ &&
           bafang_can_id_decode(read.id).sub == 0x11u);
    bafang_can_frame_t start = motor_frame(BAFANG_CAN_OP_MULTIFRAME_START, 0x60u, 0x11u);
    start.dlc = 1u; start.data[0] = 64u;
    evist_ctrl_txn_on_frame(&t, &start, now + 500u);
    bafang_service_message_t msg = {0};
    msg.src = BAFANG_NODE_CONTROLLER; msg.dst = BAFANG_NODE_HMI;
    msg.cmd = 0x60u; msg.sub = 0x11u; msg.len = 64u;
    memcpy(msg.data, p1_log, 64u);
    evist_ctrl_txn_on_service(&t, &msg, now + 501u);
    evist_txn_kind_t kind; evist_txn_result_t txres; uint8_t data[64];
    assert(evist_ctrl_txn_take_result(&t, &kind, &txres, data));
    assert(kind == EVIST_TXN_WRITE_APP_P1 && txres == EVIST_TXN_MISMATCH);
    dpc245_ble_app_on_controller_result(&b, kind, txres, data);
    assert(b.download_status[5] == 3u && memcmp(b.p1, p1_log, 64u) == 0);
    b.download_status[6] = 4u;
    q.operation = 1u; q.selector = 225u;
    assert(dpc245_ble_app_handle(&b, &q, &app, &router, &id, false, &update, &result));
    assert(b.a3[225] == p1_log[40]);
    q.operation = 2u;

    /* A controller without fragment ACKs can still apply and verify P1. */
    b.download_status[5] = 3u;
    q.selector = 226u; q.payload[0] = 0x15u;
    assert(dpc245_ble_app_handle(&b, &q, &app, &router, &id, false, &update, &result));
    assert(result.have_tx && b.pending_write == 1u);
    assert(dpc245_ble_app_start_controller(&b, &t, EVIST_PROFILE_FAKE_TAXI, 200u));
    for (uint8_t i = 0u; i < 9u; ++i) {
        bafang_can_frame_t f;
        assert(evist_ctrl_txn_next_tx(&t, 200u + i, &f));
        bafang_can_id_t cid = bafang_can_id_decode(f.id);
        assert(cid.src == BAFANG_NODE_HMI && cid.dst == BAFANG_NODE_CONTROLLER);
        assert(cid.type == (i == 0u ? BAFANG_CAN_OP_WRITE : i == 1u ?
                           BAFANG_CAN_OP_MULTIFRAME_START : i == 8u ?
                           BAFANG_CAN_OP_MULTIFRAME_END : BAFANG_CAN_OP_MULTIFRAME));
        if (i == 0u) assert(f.id == 0x03106011u && f.dlc == 1u && f.data[0] == 64u);
        else assert(f.dlc == 8u && memcmp(f.data, &expected[(i - 1u) * 8u], 8u) == 0);
    }
    evist_ctrl_txn_tick(&t, 708u);
    assert(evist_ctrl_txn_next_tx(&t, 708u, &read));
    assert(read.id == 0x03116011u && read.dlc == 0u);
    evist_ctrl_txn_on_frame(&t, &start, 709u);
    memcpy(msg.data, expected, 64u);
    evist_ctrl_txn_on_service(&t, &msg, 710u);
    assert(evist_ctrl_txn_take_result(&t, &kind, &txres, data) && txres == EVIST_TXN_OK);
    dpc245_ble_app_on_controller_result(&b, kind, txres, data);
    assert(b.download_status[5] == 3u && memcmp(b.p1, expected, 64u) == 0);

    b.per_level = true; b.download_status[4] = 3u;
    for (uint8_t i = 1u; i <= 9u; ++i) b.p0[i] = i < 8u ? i : 8u;
    q.command_id = 0xA9u; q.operation = 1u; q.selector = 0u;
    assert(dpc245_ble_app_handle(&b, &q, &app, &router, &id, false, &update, &result));
    q.operation = 2u;
    q.command_id = 0xA9u; q.selector = 65u; q.payload[0] = 3u;
    assert(dpc245_ble_app_handle(&b, &q, &app, &router, &id, false, &update, &result));
    assert(result.have_tx && b.pending_write == 2u);
    b.pending_write = 0u;
    q.command_id = 0xA9u; q.selector = 94u; q.payload[0] = 7u;
    assert(dpc245_ble_app_handle(&b, &q, &app, &router, &id, false, &update, &result));
    assert(result.have_tx && result.tx[5] == 0x05u &&
           b.pending_write == 2u && b.write_candidate[9] == 7u);
    uint8_t expected_p0[64]; memcpy(expected_p0, b.p0, 64u);
    expected_p0[9] = 7u; expected_p0[63] = evist_assist_checksum(expected_p0);
    assert(memcmp(expected_p0, b.write_candidate, 64u) == 0);
    assert(dpc245_ble_app_start_controller(&b, &t, EVIST_PROFILE_FAKE_TAXI, 1000u));
    now = 1000u;
    for (uint8_t i = 0u; i < 9u; ++i) {
        bafang_can_frame_t f;
        assert(evist_ctrl_txn_next_tx(&t, now, &f));
        bafang_can_id_t cid = bafang_can_id_decode(f.id);
        if (i == 0u) assert(cid.type == BAFANG_CAN_OP_WRITE && cid.sub == 0x10u);
        else assert(memcmp(f.data, &b.write_candidate[(i - 1u) * 8u], 8u) == 0);
        bafang_can_frame_t ack = motor_frame(BAFANG_CAN_OP_NORMAL_ACK, 0x60u, 0x10u);
        evist_ctrl_txn_on_frame(&t, &ack, now + 1u);
        now += 1u;
    }
    evist_ctrl_txn_tick(&t, now + 499u);
    assert(evist_ctrl_txn_next_tx(&t, now + 499u, &read));
    assert(bafang_can_id_decode(read.id).sub == 0x10u);
    start = motor_frame(BAFANG_CAN_OP_MULTIFRAME_START, 0x60u, 0x10u);
    start.dlc = 1u; start.data[0] = 64u;
    evist_ctrl_txn_on_frame(&t, &start, now + 500u);
    msg.sub = 0x10u; memcpy(msg.data, b.write_candidate, 64u);
    msg.data[63] = b.p0[63]; /* P0 read flag is retained by the motor. */
    evist_ctrl_txn_on_service(&t, &msg, now + 501u);
    assert(evist_ctrl_txn_take_result(&t, &kind, &txres, data));
    assert(kind == EVIST_TXN_WRITE_APP_P0 && txres == EVIST_TXN_OK);
    dpc245_ble_app_on_controller_result(&b, kind, txres, data);
    assert(b.download_status[4] == 3u && b.p0[9] == 7u);

    assert(evist_ctrl_txn_write_app_p1(&t, expected, EVIST_PROFILE_FAKE_TAXI, 3000u));
    assert(evist_ctrl_txn_next_tx(&t, 3000u, &read));
    for (uint8_t i = 1u; i < 9u; ++i)
        assert(evist_ctrl_txn_next_tx(&t, 3000u + i, &read));
    evist_ctrl_txn_tick(&t, 3508u);
    assert(evist_ctrl_txn_next_tx(&t, 3508u, &read));
    evist_ctrl_txn_tick(&t, 3958u);
    evist_ctrl_txn_tick(&t, 4108u);
    assert(evist_ctrl_txn_next_tx(&t, 4108u, &read));
    evist_ctrl_txn_tick(&t, 4558u);
    evist_ctrl_txn_tick(&t, 4708u);
    assert(evist_ctrl_txn_next_tx(&t, 4708u, &read));
    evist_ctrl_txn_tick(&t, 5158u);
    assert(evist_ctrl_txn_take_result(&t, &kind, &txres, data) && txres == EVIST_TXN_NO_ACK);
#else
    assert(!b.pending_write && !dpc245_ble_app_start_controller(&b, &t, EVIST_PROFILE_BAFANG, 0u));
    assert(!evist_ctrl_txn_write_app_p1(&t, p1_log, EVIST_PROFILE_FAKE_TAXI, 0u));
    assert(!evist_ctrl_txn_write_app_p0(&t, b.p0, 0u, 0u));
    assert(!evist_ctrl_txn_next_tx(&t, 0u, &(bafang_can_frame_t){0}));
#endif
    puts("app write frame sequence/readback/cache PASS");
}

static void test_start_angle_timeout(void) {
    dpc245_ble_app_t b; dpc245_ble_app_init(&b);
    evist_ctrl_txn_t t; evist_ctrl_txn_init(&t);
    b.controller_seen = true; b.download_stage = 4u;
    b.download_status[5] = 3u; b.download_status[6] = 3u;
    for (uint8_t attempt = 0u; attempt < 2u; ++attempt) {
        uint32_t now = 1000u + (uint32_t)attempt * 2000u;
        assert(dpc245_ble_app_start_controller(&b, &t, EVIST_PROFILE_BAFANG, now));
        for (uint8_t read = 0u; read < 3u; ++read) {
            bafang_can_frame_t f;
            uint32_t send_at = now + (uint32_t)read * 600u;
            if (read != 0u) evist_ctrl_txn_tick(&t, send_at);
            assert(evist_ctrl_txn_next_tx(&t, send_at, &f));
            bafang_can_id_t id = bafang_can_id_decode(f.id);
            assert(id.type == BAFANG_CAN_OP_READ && id.cmd == 0x62u && id.sub == 0xD9u);
            evist_ctrl_txn_tick(&t, send_at + 450u);
        }
        evist_txn_kind_t kind; evist_txn_result_t result; uint8_t data[64] = {0};
        assert(evist_ctrl_txn_take_result(&t, &kind, &result, data));
        assert(kind == EVIST_TXN_READ_START_ANGLE && result == EVIST_TXN_NO_READBACK);
        dpc245_ble_app_on_controller_result(&b, kind, result, data);
    }
    assert(b.download_status[7] == 4u && b.download_stage == 5u);
    assert(b.start_angle[0] == 0u && b.start_angle[1] == 0u);
    assert(b.download_status[5] == 3u && b.download_status[6] == 3u);
    puts("start-angle bounded timeout/status/zero-cache PASS");
}

static void test_assist_startup_interleaving(void) {
    dpc245_ble_app_t b; dpc245_ble_app_init(&b);
    evist_preferences_t prefs; evist_preferences_defaults(&prefs);
    prefs.assist_level = 1u;
    prefs.assist_mode_mask = (uint8_t)((1u << 0u) | (1u << 2u) | (1u << 4u));
    evist_advanced_t a = {0}; evist_advanced_open(&a, &prefs, 5u);
    assert(evist_advanced_assist_cells(&a) == 6u);
    b.download_status[5] = 3u; memcpy(b.p1, p1_log, 64u);
    evist_advanced_key(&a, EVIST_ADV_KEY_DOWN);
    evist_advanced_key(&a, EVIST_ADV_KEY_POWER);
    dpc245_ble_app_apply_advanced_cache(&b, &a);
    assert(a.assist_valid && a.request == EVIST_ADV_REQ_READ_ASSIST);
    evist_advanced_key(&a, EVIST_ADV_KEY_POWER);
    assert(a.edit); /* cached values must be editable while page READ is queued */
    evist_advanced_key(&a, EVIST_ADV_KEY_UP);
    assert(a.assist_dirty);
    evist_advanced_key(&a, EVIST_ADV_KEY_POWER);
    a.cursor = evist_advanced_assist_cells(&a);
    evist_advanced_key(&a, EVIST_ADV_KEY_POWER);
    assert(a.request == EVIST_ADV_REQ_WRITE_ASSIST);

    evist_advanced_open(&a, &prefs, 5u);
    evist_advanced_key(&a, EVIST_ADV_KEY_DOWN);
    evist_advanced_key(&a, EVIST_ADV_KEY_POWER);
    evist_ctrl_txn_t txn; evist_ctrl_txn_init(&txn);
    b.controller_seen = true;
    assert(dpc245_ble_app_start_controller(&b, &txn, prefs.firmware_profile, 0u));
    assert(txn.kind == EVIST_TXN_DISCOVER_ACCEL);
    /* Startup P1 has completed while the page READ is still queued. */
    evist_ctrl_txn_init(&txn);
    dpc245_ble_app_apply_advanced_cache(&b, &a);
    assert(a.assist_valid && a.request == EVIST_ADV_REQ_READ_ASSIST);
    evist_adv_request_t req;
    assert(evist_advanced_take_request(&a, &req) && req == EVIST_ADV_REQ_READ_ASSIST);
    assert(strcmp(a.status, "READING") == 0);
    assert(evist_ctrl_txn_read_assist(&txn, 10u));
    evist_advanced_key(&a, EVIST_ADV_KEY_POWER);
    assert(a.edit); /* page READ in flight must not block POWER */
    evist_advanced_key(&a, EVIST_ADV_KEY_UP);
    evist_advanced_key(&a, EVIST_ADV_KEY_POWER);
    a.cursor = evist_advanced_assist_cells(&a);
    evist_advanced_key(&a, EVIST_ADV_KEY_POWER);
    assert(a.request == EVIST_ADV_REQ_WRITE_ASSIST && a.busy);
    assert(strcmp(a.status, "WAITING TO WRITE") == 0);
    evist_advanced_on_result(&a, EVIST_TXN_READ_ASSIST, EVIST_TXN_OK, p1_log);
    assert(a.assist_dirty && evist_advanced_assist_cell_value(&a, 0u) != p1_log[41]);
    assert(evist_advanced_take_request(&a, &req) && req == EVIST_ADV_REQ_WRITE_ASSIST);
    assert(strcmp(a.status, "WRITING") == 0);
    puts("ASSIST startup interleaving PASS");
}

#if EVIST_ENABLE_CONFIG_WRITES
static void test_assist_sender_without_acks(void) {
    evist_ctrl_txn_t t; evist_ctrl_txn_init(&t);
    assert(evist_ctrl_txn_write_assist(&t, p1_log, EVIST_PROFILE_FAKE_TAXI, 100u));
    for (uint8_t i = 0u; i < 9u; ++i) {
        bafang_can_frame_t f;
        assert(evist_ctrl_txn_next_tx(&t, 100u + i, &f));
        if (i == 0u) assert(f.id == 0x03106011u && f.dlc == 1u && f.data[0] == 64u);
        else assert(f.dlc == 8u && memcmp(f.data, &p1_log[(i - 1u) * 8u], 8u) == 0);
    }
    evist_ctrl_txn_tick(&t, 608u);
    bafang_can_frame_t read;
    assert(evist_ctrl_txn_next_tx(&t, 608u, &read) && read.id == 0x03116011u);
    bafang_can_frame_t start = motor_frame(BAFANG_CAN_OP_MULTIFRAME_START, 0x60u, 0x11u);
    start.dlc = 1u; start.data[0] = 64u;
    evist_ctrl_txn_on_frame(&t, &start, 609u);
    bafang_service_message_t msg = {0};
    msg.src = BAFANG_NODE_CONTROLLER; msg.dst = BAFANG_NODE_HMI;
    msg.cmd = 0x60u; msg.sub = 0x11u; msg.len = 64u;
    memcpy(msg.data, p1_log, 64u);
    evist_ctrl_txn_on_service(&t, &msg, 610u);
    evist_txn_kind_t kind; evist_txn_result_t result; uint8_t data[64];
    assert(evist_ctrl_txn_take_result(&t, &kind, &result, data));
    assert(kind == EVIST_TXN_WRITE_ASSIST && result == EVIST_TXN_OK);
    assert(memcmp(data, p1_log, 64u) == 0);

    assert(evist_ctrl_txn_write_assist(&t, p1_log, EVIST_PROFILE_FAKE_TAXI, 1000u));
    assert(evist_ctrl_txn_next_tx(&t, 1000u, &read));
    evist_ctrl_txn_tick(&t, 1333u);
    assert(evist_ctrl_txn_busy(&t));
    evist_ctrl_txn_tick(&t, 1334u);
    assert(evist_ctrl_txn_take_result(&t, &kind, &result, data));
    assert(result == EVIST_TXN_TX_TIMEOUT);

    assert(evist_ctrl_txn_write_assist(&t, p1_log, EVIST_PROFILE_FAKE_TAXI, 2000u));
    assert(evist_ctrl_txn_next_tx(&t, 2000u, &read));
    bafang_can_frame_t nack = motor_frame(BAFANG_CAN_OP_ERROR_ACK, 0x60u, 0x11u);
    nack.dlc = 4u; nack.data[0] = 1u; nack.data[2] = 2u; nack.data[3] = 6u;
    evist_ctrl_txn_on_frame(&t, &nack, 2001u);
    assert(evist_ctrl_txn_busy(&t) && t.error_seen && t.error_payload[2] == 2u);
    for (uint8_t i = 1u; i < 9u; ++i) assert(evist_ctrl_txn_next_tx(&t, 2000u + i, &read));
    evist_ctrl_txn_tick(&t, 2508u);
    assert(evist_ctrl_txn_next_tx(&t, 2508u, &read) && read.id == 0x03116011u);
    evist_ctrl_txn_on_frame(&t, &nack, 2509u);
    start = motor_frame(BAFANG_CAN_OP_MULTIFRAME_START, 0x60u, 0x11u);
    start.dlc = 1u; start.data[0] = 64u;
    evist_ctrl_txn_on_frame(&t, &start, 2510u);
    evist_ctrl_txn_on_service(&t, &msg, 2511u);
    assert(evist_ctrl_txn_take_result(&t, &kind, &result, data) && result == EVIST_TXN_OK);

    assert(evist_ctrl_txn_write_assist(&t, p1_log, EVIST_PROFILE_FAKE_TAXI, 3000u));
    assert(evist_ctrl_txn_next_tx(&t, 3000u, &read));
    assert(evist_ctrl_txn_next_tx(&t, 3001u, &read));
    assert(evist_ctrl_txn_next_tx(&t, 3002u, &read));
    nack = motor_frame(BAFANG_CAN_OP_ERROR_ACK, 0x60u, 0x11u);
    nack.dlc = 4u; nack.data[0] = 1u; nack.data[2] = 4u; nack.data[3] = 6u;
    evist_ctrl_txn_on_frame(&t, &nack, 3003u);
    assert(evist_ctrl_txn_take_result(&t, &kind, &result, data));
    assert(result == EVIST_TXN_REJECTED);
    puts("ASSIST timed sender, reason-2 verify and validator reject PASS");
}

static void test_trace_ack_sequence(void) {
    evist_ctrl_txn_t t; evist_ctrl_txn_init(&t);
    bafang_can_frame_t error = motor_frame(BAFANG_CAN_OP_ERROR_ACK, 0x60u, 0x11u);
    error.dlc = 4u; error.data[0] = 1u; error.data[2] = 2u; error.data[3] = 6u;
    evist_ctrl_txn_on_frame(&t, &error, 90u);
    assert(!t.error_seen);
    assert(evist_ctrl_txn_write_assist(&t, p1_log, EVIST_PROFILE_FAKE_TAXI, 100u));
    bafang_can_frame_t other = motor_frame(BAFANG_CAN_OP_ERROR_ACK, 0x60u, 0x12u);
    other.dlc = 4u; other.data[0] = 1u; other.data[2] = 2u; other.data[3] = 6u;
    evist_ctrl_txn_on_frame(&t, &other, 100u);
    assert(!t.error_seen);
    bafang_can_frame_t ack = motor_frame(BAFANG_CAN_OP_NORMAL_ACK, 0x60u, 0x11u);
    for (uint8_t i = 0u; i < 9u; ++i) {
        bafang_can_frame_t tx;
        assert(evist_ctrl_txn_next_tx(&t, 100u + i, &tx));
        if (i < 8u) evist_ctrl_txn_on_frame(&t, &ack, 101u + i);
    }
    assert(t.ack_count == 8u && !t.final_ack_seen);
    evist_ctrl_txn_on_frame(&t, &ack, 139u);
    assert(t.ack_count == 9u && t.final_ack_seen);
    evist_ctrl_txn_on_frame(&t, &error, 318u);
    evist_ctrl_txn_on_frame(&t, &error, 400u);
    assert(t.error_seen && evist_ctrl_txn_busy(&t));
    evist_ctrl_txn_tick(&t, 608u);
    bafang_can_frame_t read;
    assert(evist_ctrl_txn_next_tx(&t, 608u, &read) && read.id == 0x03116011u);
    evist_ctrl_txn_on_frame(&t, &error, 609u);
    bafang_can_frame_t start = motor_frame(BAFANG_CAN_OP_MULTIFRAME_START, 0x60u, 0x11u);
    start.dlc = 1u; start.data[0] = 64u;
    evist_ctrl_txn_on_frame(&t, &start, 610u);
    evist_can_router_t router; evist_can_router_init(&router);
    evist_can_router_rx(&router, &start, 610u);
    bafang_can_frame_t reply_ack;
    assert(evist_can_router_take_service_ack(&router, &reply_ack));
    for (uint8_t i = 0u; i < 8u; ++i) {
        if (i == 4u) {
            evist_ctrl_txn_on_frame(&t, &error, 615u);
            evist_can_router_rx(&router, &error, 615u);
            assert(evist_ctrl_txn_busy(&t));
        }
        bafang_can_frame_t part = motor_frame(i == 7u ? BAFANG_CAN_OP_MULTIFRAME_END :
                                               BAFANG_CAN_OP_MULTIFRAME, 0u, i);
        part.dlc = 8u;
        memcpy(part.data, &p1_log[i * 8u], 8u);
        evist_ctrl_txn_on_frame(&t, &part, 611u + i);
        evist_can_router_rx(&router, &part, 611u + i);
        assert(evist_can_router_take_service_ack(&router, &reply_ack));
        if (router.service_event)
            evist_ctrl_txn_on_service(&t, &router.last_service, 611u + i);
    }
    evist_txn_kind_t kind; evist_txn_result_t result;
    assert(evist_ctrl_txn_take_result(&t, &kind, &result, NULL));
    assert(kind == EVIST_TXN_WRITE_ASSIST && result == EVIST_TXN_OK &&
           t.ack_count == 9u && t.final_ack_seen);

    assert(evist_ctrl_txn_write_assist(&t, p1_log, EVIST_PROFILE_FAKE_TAXI, 1000u));
    for (uint8_t i = 0u; i < 9u; ++i) {
        assert(evist_ctrl_txn_next_tx(&t, 1000u + i, &read));
        evist_ctrl_txn_on_frame(&t, &ack, 1000u + i);
    }
    evist_ctrl_txn_on_frame(&t, &ack, 1010u);
    assert(t.ack_count == 10u && t.final_ack_seen);
    puts("trace ACK sequence and final ACK PASS");
}

static void check_accel_write_frames(evist_ctrl_txn_t *t, uint8_t sub,
                                     const uint8_t expected[64], const uint8_t reply[64],
                                     uint8_t read_flag, bool ack_each) {
    uint32_t now = 100u;
    for (uint8_t i = 0u; i < 9u; ++i) {
        bafang_can_frame_t f;
        assert(evist_ctrl_txn_next_tx(t, now, &f));
        bafang_can_id_t id = bafang_can_id_decode(f.id);
        assert(id.src == BAFANG_NODE_HMI && id.dst == BAFANG_NODE_CONTROLLER);
        assert(id.type == (i == 0u ? BAFANG_CAN_OP_WRITE : i == 1u ?
                           BAFANG_CAN_OP_MULTIFRAME_START : i == 8u ?
                           BAFANG_CAN_OP_MULTIFRAME_END : BAFANG_CAN_OP_MULTIFRAME));
        if (i == 0u) assert(id.cmd == 0x60u && id.sub == sub && f.dlc == 1u && f.data[0] == 64u);
        else assert(f.dlc == 8u && memcmp(f.data, &expected[(i - 1u) * 8u], 8u) == 0);
        if (ack_each) {
            bafang_can_frame_t ack = motor_frame(BAFANG_CAN_OP_NORMAL_ACK, 0x60u, sub);
            evist_ctrl_txn_on_frame(t, &ack, now);
        }
        now += 1u;
    }
    evist_ctrl_txn_tick(t, now + 499u);
    bafang_can_frame_t read;
    assert(evist_ctrl_txn_next_tx(t, now + 499u, &read));
    bafang_can_id_t rid = bafang_can_id_decode(read.id);
    assert(rid.type == BAFANG_CAN_OP_READ && rid.cmd == 0x60u && rid.sub == sub);
    bafang_can_frame_t start = motor_frame(BAFANG_CAN_OP_MULTIFRAME_START, 0x60u, sub);
    start.dlc = 1u; start.data[0] = 64u;
    evist_ctrl_txn_on_frame(t, &start, now + 500u);
    bafang_service_message_t msg = {0};
    msg.src = BAFANG_NODE_CONTROLLER; msg.dst = BAFANG_NODE_HMI;
    msg.cmd = 0x60u; msg.sub = sub; msg.len = 64u;
    memcpy(msg.data, reply, 64u);
    if (sub == 0x10u) msg.data[63] = read_flag;
    evist_ctrl_txn_on_service(t, &msg, now + 501u);
}
#endif

static void test_accel_page_edit(void) {
    evist_preferences_t prefs; evist_preferences_defaults(&prefs);
    dpc245_ble_app_t b; dpc245_ble_app_init(&b);
    evist_advanced_t a = {0}; evist_ctrl_txn_t t;
    uint8_t p0[64] = {0}, p2[64] = {0};
    for (uint8_t i = 1u; i <= 9u; ++i) p0[i] = i < 8u ? i : 8u;
    p0[63] = 0xA5u; /* P0 read flag is not the write checksum. */
    p2[54] = 7u; p2[63] = evist_assist_checksum(p2);
    for (uint8_t mode = 0u; mode < 2u; ++mode) {
        evist_advanced_open(&a, &prefs, 5u);
        evist_advanced_key(&a, EVIST_ADV_KEY_DOWN);
        evist_advanced_key(&a, EVIST_ADV_KEY_DOWN);
        evist_advanced_key(&a, EVIST_ADV_KEY_POWER);
        dpc245_ble_app_init(&b);
        b.discovery_done = true; b.per_level = mode == 0u;
        b.download_status[mode == 0u ? 4u : 6u] = 3u;
        memcpy(mode == 0u ? b.p0 : b.p2, mode == 0u ? p0 : p2, 64u);
        dpc245_ble_app_apply_advanced_cache(&b, &a);
        assert(a.accel_valid && a.accel_mode == (mode == 0u ? EVIST_ACCEL_PER_LEVEL : EVIST_ACCEL_GLOBAL));
        evist_advanced_key(&a, EVIST_ADV_KEY_POWER);
        assert(a.edit);
        evist_advanced_key(&a, EVIST_ADV_KEY_UP);
        evist_advanced_key(&a, EVIST_ADV_KEY_POWER);
        assert(a.accel_dirty);
        a.cursor = (uint8_t)(evist_advanced_row_count(&a) - 2u);
        evist_advanced_key(&a, EVIST_ADV_KEY_POWER);
#if EVIST_ENABLE_CONFIG_WRITES
        assert(a.request == (mode == 0u ? EVIST_ADV_REQ_WRITE_ACCEL_P0 : EVIST_ADV_REQ_WRITE_ACCEL_P2));
        uint8_t expected[64]; memcpy(expected, mode == 0u ? p0 : p2, 64u);
        if (mode == 0u) expected[2] = 3u; /* visible L1 maps to P0 slot 1 */
        else expected[54] = 8u;
        expected[63] = evist_assist_checksum(expected);
        assert(memcmp(a.accel_block, expected, 64u) == 0);
        evist_adv_request_t req;
        assert(evist_advanced_take_request(&a, &req));
        evist_ctrl_txn_init(&t);
        assert(mode == 0u ? evist_ctrl_txn_write_app_p0(&t, a.accel_block, p0[63], 100u) :
                            evist_ctrl_txn_write_app_p2(&t, a.accel_block, 100u));
        check_accel_write_frames(&t, mode == 0u ? 0x10u : 0x12u, expected, expected,
                                 p0[63], mode != 0u);
        evist_txn_kind_t kind; evist_txn_result_t result; uint8_t data[64] = {0};
        assert(evist_ctrl_txn_take_result(&t, &kind, &result, data) && result == EVIST_TXN_OK);
        evist_advanced_on_result(&a, kind, result, data);
        dpc245_ble_app_capture_adv_result(&b, kind, result, data);
        assert(!a.accel_dirty && strcmp(a.status, "VERIFY OK") == 0);
        assert(memcmp(mode == 0u ? b.p0 : b.p2, data, 64u) == 0);
        /* A changed readback keeps the edit for an explicit retry. */
        evist_advanced_key(&a, EVIST_ADV_KEY_UP); /* from SAVE to last value */
        a.cursor = 0u;
        evist_advanced_key(&a, EVIST_ADV_KEY_POWER);
        evist_advanced_key(&a, EVIST_ADV_KEY_DOWN);
        evist_advanced_key(&a, EVIST_ADV_KEY_POWER);
        a.cursor = (uint8_t)(evist_advanced_row_count(&a) - 2u);
        evist_advanced_key(&a, EVIST_ADV_KEY_POWER);
        assert(evist_advanced_take_request(&a, &req));
        evist_ctrl_txn_init(&t);
        assert(mode == 0u ? evist_ctrl_txn_write_app_p0(&t, a.accel_block, p0[63], 100u) :
                            evist_ctrl_txn_write_app_p2(&t, a.accel_block, 100u));
        uint8_t changed[64]; memcpy(changed, a.accel_block, 64u);
        uint8_t stale[64]; memcpy(stale, changed, 64u);
        if (mode == 0u) stale[2] = 3u;
        else { stale[54] = 8u; stale[63] = evist_assist_checksum(stale); }
        check_accel_write_frames(&t, mode == 0u ? 0x10u : 0x12u, changed, stale,
                                 p0[63], mode != 0u);
        assert(evist_ctrl_txn_take_result(&t, &kind, &result, data) && result == EVIST_TXN_MISMATCH);
        evist_advanced_on_result(&a, kind, result, data);
        assert(a.accel_dirty && strcmp(a.status, "VERIFY MISMATCH") == 0);
        assert(mode == 0u ? a.accel_values[1] == changed[2] : a.accel_global == changed[54]);
#else
        assert(a.request != EVIST_ADV_REQ_WRITE_ACCEL_P0 && a.request != EVIST_ADV_REQ_WRITE_ACCEL_P2);
        assert(strcmp(a.status, "WRITES DISABLED IN THIS BUILD") == 0);
        evist_ctrl_txn_init(&t);
        assert(!evist_ctrl_txn_write_app_p2(&t, p2, 100u));
        assert(!evist_ctrl_txn_next_tx(&t, 100u, &(bafang_can_frame_t){0}));
#endif
    }
    puts("ACCEL page P0/P2 edit, timed frames and compare PASS");
}

int main(void) {
    assert(evist_assist_checksum(p1_log) == p1_log[63]);
    test_download();
    test_start_angle_timeout();
    test_app_write();
    test_assist_startup_interleaving();
#if EVIST_ENABLE_CONFIG_WRITES
    test_assist_sender_without_acks();
    test_trace_ack_sequence();
#endif
    test_accel_page_edit();
    return 0;
}
