/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Host tests: controller configuration transactions, ASSIST table rules,
 * ADVANCED SETTINGS edit buffers and the preference memory layout.
 * The tests follow the configuration specification and cover range checks,
 * transactions and storage faults. */
#include "advanced_settings.h"
#include "assist_profile.h"
#include "can_router.h"
#include "controller_write.h"
#include "evist_preferences.h"
#include <stdio.h>
#include <string.h>

static unsigned failures;
static void check(int ok, const char *name) {
    if (!ok) { printf("FAIL: %s\n", name); ++failures; }
}

/* ---- fixtures ---------------------------------------------------------- */

static void block_fixture(uint8_t b[64]) {
    memset(b, 0, 64);
    b[0] = 0x18u;
    for (uint8_t i = 0u; i < 9u; ++i) {
        b[EVIST_ASSIST_POWER_OFF + i] = (uint8_t)(20u + i * 5u);
        b[EVIST_ASSIST_SPEED_OFF + i] = (uint8_t)(30u + i * 5u);
    }
    b[63] = evist_assist_checksum(b);
}

static bafang_can_frame_t controller_frame(uint8_t dst, uint8_t op, uint8_t cmd, uint8_t sub,
                                           const uint8_t *data, uint8_t dlc) {
    bafang_can_frame_t f;
    memset(&f, 0, sizeof f);
    f.id = bafang_can_id_encode((bafang_can_id_t){
        .src = BAFANG_NODE_CONTROLLER, .dst = dst, .type = op, .cmd = cmd, .sub = sub});
    f.dlc = dlc;
    if (data) memcpy(f.data, data, dlc);
    return f;
}

static bafang_service_message_t block_message(const uint8_t b[64]) {
    bafang_service_message_t m;
    memset(&m, 0, sizeof m);
    m.src = BAFANG_NODE_CONTROLLER; m.dst = BAFANG_NODE_HMI;
    m.cmd = 0x60u; m.sub = 0x11u; m.len = 64u;
    memcpy(m.data, b, 64);
    return m;
}

/* A real reassembled 0x6011 reply: START frame first (on_frame), then the
 * completed message (on_service), both at `now`. */
static void reply_block(evist_ctrl_txn_t *t, const bafang_service_message_t *m, uint32_t now) {
    bafang_can_frame_t start = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_MULTIFRAME_START,
                                                m->cmd, m->sub, NULL, 0u);
    evist_ctrl_txn_on_frame(t, &start, now);
    evist_ctrl_txn_on_service(t, m, now);
}

/* Run tick + next_tx; returns true and the frame when one is due. */
static bool pump(evist_ctrl_txn_t *t, uint32_t now, bafang_can_frame_t *out) {
    evist_ctrl_txn_tick(t, now);
    return evist_ctrl_txn_next_tx(t, now, out);
}

static bool is_read_query(const bafang_can_frame_t *f, uint8_t cmd, uint8_t sub) {
    bafang_can_id_t id = bafang_can_id_decode(f->id);
    return id.src == BAFANG_NODE_HMI && id.dst == BAFANG_NODE_CONTROLLER &&
           id.type == BAFANG_CAN_OP_READ && id.cmd == cmd && id.sub == sub && f->dlc == 0u;
}

/* Advance time in 1 ms steps until a read query is emitted. */
static uint32_t run_until_read(evist_ctrl_txn_t *t, uint32_t now, uint8_t cmd, uint8_t sub) {
    bafang_can_frame_t f;
    for (uint32_t end = now + 5000u; now < end; ++now)
        if (pump(t, now, &f) && is_read_query(&f, cmd, sub)) return now;
    return 0u;
}

/* ---- ASSIST table rules ------------------------------------ */

static void test_assist_profile(void) {
    check(evist_assist_level_slot(5u, 1u) == 1u && evist_assist_level_slot(5u, 5u) == 8u, "5-level slots 1,3,5,7,8");
    check(evist_assist_level_slot(3u, 2u) == 4u, "3-level slots 2,4,8");
    check(evist_assist_level_slot(9u, 4u) == 3u, "9-level slots identity");
    check(evist_assist_level_count_normalize(7u) == 5u, "unknown level count falls back to 5");

    uint8_t b[64];
    block_fixture(b);
    b[0] = 0x28u; b[63] = evist_assist_checksum(b);
    check(evist_assist_block_valid(b), "0x6011 system voltage 0x28 with checksum is readable");
    b[63] ^= 1u;
    check(!evist_assist_block_valid(b), "0x6011 wrong checksum is not readable");
    block_fixture(b);
    b[0] = 0x28u;
    b[EVIST_ASSIST_POWER_OFF + 0u] = 101u;
    b[63] = evist_assist_checksum(b);
    check(evist_assist_block_valid(b), "out-of-range unused slot does not block 0x6011 read");
    check(!evist_assist_write_allowed(b, EVIST_PROFILE_FAKE_TAXI), "out-of-range 0x6011 block cannot be written");
    block_fixture(b);
    check(evist_assist_write_allowed(b, EVIST_PROFILE_BAFANG), "valid rising table allowed");

    /* In-range, checksummed, descending slots 0/1. */
    b[EVIST_ASSIST_POWER_OFF] = 100u; b[EVIST_ASSIST_POWER_OFF + 1u] = 0u; b[63] = evist_assist_checksum(b);
    check(!evist_assist_write_allowed(b, EVIST_PROFILE_BAFANG), "BAFANG descending POWER column rejected at write boundary");
    check(evist_assist_write_allowed(b, EVIST_PROFILE_FAKE_TAXI), "FAKE TAXI keeps free order");
    block_fixture(b);
    b[EVIST_ASSIST_SPEED_OFF + 8u] = 0u; b[63] = evist_assist_checksum(b);
    check(!evist_assist_write_allowed(b, EVIST_PROFILE_BAFANG), "BAFANG descending hidden SPEED slot rejected");
    block_fixture(b);
    b[EVIST_ASSIST_POWER_OFF + 2u] = 101u; b[63] = evist_assist_checksum(b);
    check(!evist_assist_write_allowed(b, EVIST_PROFILE_FAKE_TAXI), "value 101 rejected for every profile");

    /* Visible levels cannot cross their neighbours while editing (BAFANG). */
    block_fixture(b); /* 5 levels: L1 slot1=25, L2 slot3=35 */
    uint8_t lo, hi;
    evist_assist_edit_bounds(b, EVIST_PROFILE_BAFANG, 5u, EVIST_ASSIST_COLUMN_POWER, 1u, &lo, &hi);
    check(lo == 0u && hi == 35u, "L1 bounds 0..L2");
    evist_assist_edit_bounds(b, EVIST_PROFILE_FAKE_TAXI, 5u, EVIST_ASSIST_COLUMN_POWER, 1u, &lo, &hi);
    check(lo == 0u && hi == 100u, "FAKE TAXI bounds 0..100");
    for (int i = 0; i < 20; ++i) (void)evist_assist_edit_step(b, EVIST_PROFILE_BAFANG, 5u, EVIST_ASSIST_COLUMN_POWER, 1u, true);
    check(b[EVIST_ASSIST_POWER_OFF + 1u] == 35u, "L1 stops at L2");
    check(b[EVIST_ASSIST_POWER_OFF + 2u] == 35u, "hidden slot between L1 and L2 reconciled");
    check(evist_assist_column_monotonic(b, EVIST_ASSIST_COLUMN_POWER), "column stays non-decreasing");

    /* Descending visible levels from the controller block are refused at SAVE. */
    block_fixture(b);
    b[EVIST_ASSIST_SPEED_OFF + 3u] = 10u; /* L2 below L1 */
    uint8_t before[64]; memcpy(before, b, 64);
    check(!evist_assist_prepare_write(b, EVIST_PROFILE_BAFANG, 5u), "prepare_write refuses descending visible SPEED");
    check(memcmp(before, b, 64) == 0, "refused block untouched");
    check(evist_assist_prepare_write(b, EVIST_PROFILE_FAKE_TAXI, 5u) && b[63] == evist_assist_checksum(b), "FAKE TAXI only refreshes checksum");
}

/* ---- WHEEL transaction ------------------------------------- */

static void test_wheel_write(void) {
    evist_ctrl_txn_t t; evist_ctrl_txn_init(&t);
    evist_wheel_config_t c = {2600u, 0x01D0u, 2240u};
    uint8_t want[6]; evist_wheel_config_encode(&c, want);
    uint8_t old[6] = {0xC4u, 0x09u, 0xD0u, 0x01u, 0xC0u, 0x08u}; /* 25.00, 29", 2240 */
    bafang_can_frame_t f, rx;
    evist_txn_kind_t kind; evist_txn_result_t res; uint8_t data[64];

    check(evist_ctrl_txn_write_wheel(&t, &c, 0u), "wheel write starts");
    check(!evist_ctrl_txn_read_assist(&t, 0u), "second transaction refused while busy");
    check(pump(&t, 0u, &f), "write frame emitted");
    bafang_can_id_t id = bafang_can_id_decode(f.id);
    check(id.src == BAFANG_NODE_HMI && id.dst == BAFANG_NODE_CONTROLLER && id.type == BAFANG_CAN_OP_WRITE &&
          id.cmd == 0x32u && id.sub == 0x03u && f.dlc == 6u && memcmp(f.data, want, 6) == 0,
          "WHEEL write = display node 3, 6-byte payload (configuration contract)");

    /* Old values arriving before our READ (broadcast or earlier reply) never verify. */
    rx = controller_frame(BAFANG_NODE_BROADCAST, 0u, 0x32u, 0x03u, want, 6u);
    evist_ctrl_txn_on_frame(&t, &rx, 5u);
    rx = controller_frame(BAFANG_NODE_BESST, BAFANG_CAN_OP_NORMAL_ACK, 0x32u, 0x03u, NULL, 0u);
    evist_ctrl_txn_on_frame(&t, &rx, 6u);
    check(t.step == EVIST_TXN_STEP_WAIT_ACK && !t.ack_seen, "WHEEL ignores another requester's ACK");
    rx = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_NORMAL_ACK, 0x32u, 0x03u, NULL, 0u);
    evist_ctrl_txn_on_frame(&t, &rx, 10u);
    rx = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_NORMAL_ACK, 0x32u, 0x03u, want, 6u);
    evist_ctrl_txn_on_frame(&t, &rx, 20u); /* reply-shaped frame during settle */
    check(!evist_ctrl_txn_take_result(&t, &kind, &res, data), "no result before own READ");
    uint32_t read_at = run_until_read(&t, 11u, 0x32u, 0x03u);
    check(read_at >= 410u && read_at <= 412u, "READ 0x3203 after ACK + 400 ms settle");

    /* Unrelated frames do not complete it; a fresh matching reply does. */
    rx = controller_frame(BAFANG_NODE_BROADCAST, 0u, 0x32u, 0x01u, old, 6u);
    evist_ctrl_txn_on_frame(&t, &rx, read_at + 1u);
    rx = controller_frame(BAFANG_NODE_BROADCAST, 0u, 0x32u, 0x03u, old, 6u);
    evist_ctrl_txn_on_frame(&t, &rx, read_at + 2u);
    check(!evist_ctrl_txn_take_result(&t, &kind, &res, data), "broadcast 0x3203 is not a readback");
    rx = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_NORMAL_ACK, 0x32u, 0x03u, want, 6u);
    evist_ctrl_txn_on_frame(&t, &rx, read_at + 3u);
    check(evist_ctrl_txn_take_result(&t, &kind, &res, data) && kind == EVIST_TXN_WRITE_WHEEL &&
          res == EVIST_TXN_OK && memcmp(data, want, 6) == 0, "fresh equal readback -> VERIFY OK");

    /* Mismatch */
    check(evist_ctrl_txn_write_wheel(&t, &c, 1000u) && pump(&t, 1000u, &f), "second write");
    rx = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_NORMAL_ACK, 0x32u, 0x03u, NULL, 0u);
    evist_ctrl_txn_on_frame(&t, &rx, 1001u);
    read_at = run_until_read(&t, 1001u, 0x32u, 0x03u);
    rx = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_NORMAL_ACK, 0x32u, 0x03u, old, 6u);
    evist_ctrl_txn_on_frame(&t, &rx, read_at + 1u);
    check(evist_ctrl_txn_take_result(&t, &kind, &res, data) && res == EVIST_TXN_MISMATCH, "different readback -> MISMATCH");

    /* No ACK */
    check(evist_ctrl_txn_write_wheel(&t, &c, 2000u) && pump(&t, 2000u, &f), "third write");
    for (uint32_t now = 2001u; now <= 2600u; ++now) (void)pump(&t, now, &f);
    check(evist_ctrl_txn_take_result(&t, &kind, &res, data) && res == EVIST_TXN_NO_ACK, "no ACK in 600 ms -> NO ACK");

    /* No readback */
    check(evist_ctrl_txn_write_wheel(&t, &c, 3000u) && pump(&t, 3000u, &f), "fourth write");
    rx = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_NORMAL_ACK, 0x32u, 0x03u, NULL, 0u);
    evist_ctrl_txn_on_frame(&t, &rx, 3001u);
    read_at = run_until_read(&t, 3001u, 0x32u, 0x03u);
    for (uint32_t now = read_at + 1u; now <= read_at + 600u; ++now) (void)pump(&t, now, &f);
    check(evist_ctrl_txn_take_result(&t, &kind, &res, data) && res == EVIST_TXN_NO_READBACK, "silence after READ -> NO READBACK");

    /* Plain read uses 0x3203 then the 0xF203 form. */
    check(evist_ctrl_txn_read_wheel(&t, 4000u) && pump(&t, 4000u, &f) && is_read_query(&f, 0x32u, 0x03u), "read 0x3203");
    read_at = run_until_read(&t, 4001u, 0xF2u, 0x03u);
    check(read_at == 4220u, "0xF203 fallback after 220 ms");
    rx = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_NORMAL_ACK, 0xF2u, 0x03u, old, 6u);
    evist_ctrl_txn_on_frame(&t, &rx, read_at + 5u);
    check(evist_ctrl_txn_take_result(&t, &kind, &res, data) && kind == EVIST_TXN_READ_WHEEL && res == EVIST_TXN_OK &&
          memcmp(data, old, 6) == 0, "F203 reply accepted for read");
}

/* ---- ASSIST transaction --------------------------------- */

static void test_assist_write(void) {
    evist_ctrl_txn_t t; evist_ctrl_txn_init(&t);
    uint8_t b[64], old[64];
    block_fixture(b); block_fixture(old);
    b[EVIST_ASSIST_POWER_OFF + 8u] = 70u; b[63] = evist_assist_checksum(b);
    bafang_can_frame_t f, frames[9], rx;
    uint32_t times[9]; unsigned n = 0u;
    evist_txn_kind_t kind; evist_txn_result_t res; uint8_t data[64];

    uint8_t bad[64]; block_fixture(bad);
    bad[EVIST_ASSIST_POWER_OFF] = 100u; bad[EVIST_ASSIST_POWER_OFF + 1u] = 0u; bad[63] = evist_assist_checksum(bad);
    check(!evist_ctrl_txn_write_assist(&t, bad, EVIST_PROFILE_BAFANG, 0u), "BAFANG descending block never starts a write");

    check(evist_ctrl_txn_write_assist(&t, b, EVIST_PROFILE_BAFANG, 0u), "assist write starts");
    for (uint32_t now = 0u; now < 400u && n < 9u; ++now)
        if (pump(&t, now, &f)) {
            frames[n] = f; times[n] = now; ++n;
            rx = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_NORMAL_ACK, 0x60u, 0x11u, NULL, 0u);
            evist_ctrl_txn_on_frame(&t, &rx, now + 1u);
        }
    check(n == 9u, "nine frames sent");
    bool ok = true;
    for (unsigned i = 0u; i < n; ++i) {
        bafang_can_id_t id = bafang_can_id_decode(frames[i].id);
        ok = ok && id.src == BAFANG_NODE_HMI && id.dst == BAFANG_NODE_CONTROLLER;
        if (i > 0u) ok = ok && times[i] - times[i - 1u] == 1u;
    }
    check(ok, "all frames from node 3 without an ACK wait");
    check(frames[0].dlc == 1u && frames[0].data[0] == 64u &&
          bafang_can_id_decode(frames[1].id).type == BAFANG_CAN_OP_MULTIFRAME_START &&
          bafang_can_id_decode(frames[8].id).type == BAFANG_CAN_OP_MULTIFRAME_END &&
          frames[8].data[7] == b[63] && memcmp(frames[1].data, b, 8) == 0, "frame layout and payload");

    /* A stale block delivered before our READ must not verify. */
    bafang_service_message_t stale = block_message(b);
    evist_ctrl_txn_on_service(&t, &stale, times[8] + 1u);
    evist_ctrl_txn_tick(&t, times[8] + 1u);
    check(!evist_ctrl_txn_take_result(&t, &kind, &res, data), "stale block ignored");
    uint32_t read_at = run_until_read(&t, times[8] + 3u, 0x60u, 0x11u);
    check(read_at == times[8] + 500u, "READ 0x6011 after 500 ms settle");

    /* Unrelated service message, then a bad checksum. */
    bafang_service_message_t other = block_message(b); other.sub = 0x12u;
    reply_block(&t, &other, read_at + 1u);
    check(!evist_ctrl_txn_take_result(&t, &kind, &res, data), "0x6012 not accepted as 0x6011");
    bafang_service_message_t corrupt = block_message(b); corrupt.data[63] ^= 1u;
    reply_block(&t, &corrupt, read_at + 2u);
    check(evist_ctrl_txn_take_result(&t, &kind, &res, data) && res == EVIST_TXN_CHECKSUM_BAD, "bad checksum reported");

    /* Mismatch on the fresh reply. */
    n = 0u;
    check(evist_ctrl_txn_write_assist(&t, b, EVIST_PROFILE_BAFANG, 10000u), "assist write 2");
    for (uint32_t now = 10000u; now < 10200u; ++now) {
        if (pump(&t, now, &f)) {
            ++n;
            rx = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_NORMAL_ACK, 0x60u, 0x11u, NULL, 0u);
            evist_ctrl_txn_on_frame(&t, &rx, now + 1u);
        }
    }
    read_at = run_until_read(&t, 10200u, 0x60u, 0x11u);
    bafang_service_message_t prev = block_message(old);
    reply_block(&t, &prev, read_at + 100u);
    check(evist_ctrl_txn_take_result(&t, &kind, &res, data) && res == EVIST_TXN_MISMATCH && memcmp(data, old, 64) == 0,
          "fresh but different block -> MISMATCH");

    /* A node-5 declaration ACK does not confirm that this write was applied. */
    n = 0u;
    unsigned reads = 0u;
    check(evist_ctrl_txn_write_assist(&t, b, EVIST_PROFILE_BAFANG, 20000u), "assist write 3");
    for (uint32_t now = 20000u; now < 22500u && evist_ctrl_txn_busy(&t); ++now)
        if (pump(&t, now, &f)) {
            if (is_read_query(&f, 0x60u, 0x11u)) ++reads;
            else ++n;
        }
    check(n == 9u && reads == 3u &&
          evist_ctrl_txn_take_result(&t, &kind, &res, data) && res == EVIST_TXN_NO_ACK,
          "node-5 ACK does not verify source-3 write");

    /* Read: three attempts, then NO READBACK. */
    n = 0u;
    check(evist_ctrl_txn_read_assist(&t, 30000u), "assist read");
    for (uint32_t now = 30000u; now < 33000u; ++now) if (pump(&t, now, &f) && is_read_query(&f, 0x60u, 0x11u)) ++n;
    check(n == 3u && evist_ctrl_txn_take_result(&t, &kind, &res, data) && res == EVIST_TXN_NO_READBACK, "three READs then NO READBACK");

    /* Read success on the second attempt. */
    check(evist_ctrl_txn_read_assist(&t, 40000u), "assist read 2");
    read_at = run_until_read(&t, 40000u, 0x60u, 0x11u);
    read_at = run_until_read(&t, read_at + 1u, 0x60u, 0x11u);
    check(read_at == 40600u, "second READ 450 + 150 ms later");
    reply_block(&t, &prev, read_at + 30u);
    check(evist_ctrl_txn_take_result(&t, &kind, &res, data) && kind == EVIST_TXN_READ_ASSIST && res == EVIST_TXN_OK, "read OK");
}

/* Two controller variants: one acknowledges parameter declarations to node 5
 * only, without staging node-3 data; the other acknowledges every parameter
 * fragment to node 3. */
static void test_controller_revisions(void) {
    for (uint8_t revision = 0u; revision < 2u; ++revision) {
        for (uint8_t path = 0u; path < 5u; ++path) {
            evist_ctrl_txn_t t; evist_ctrl_txn_init(&t);
            uint8_t original[64], wanted[64], stored[64], received[64];
            block_fixture(original);
            memcpy(wanted, original, 64u);
            memcpy(stored, original, 64u);
            uint8_t sub = path == 3u ? 0x10u : path == 4u ? 0x12u : 0x11u;
            if (path == 2u) {
                wanted[41] = 26u;
                wanted[63] = evist_assist_checksum(wanted);
            } else if (path == 3u) {
                for (uint8_t i = 1u; i <= 9u; ++i) wanted[i] = 4u;
                wanted[2] = 5u;
                wanted[63] = evist_assist_checksum(wanted);
                stored[63] = 0u; /* P0 read flag, ignored on WRITE. */
            } else if (path == 4u) {
                wanted[54] = 5u;
                wanted[63] = evist_assist_checksum(wanted);
            }
            evist_wheel_config_t wheel = {2600u, 0x01D0u, 2240u};
            bool started = path < 2u ? evist_ctrl_txn_write_wheel(&t, &wheel, 0u) :
                path == 2u ? evist_ctrl_txn_write_assist(&t, wanted, EVIST_PROFILE_BAFANG, 0u) :
                path == 3u ? evist_ctrl_txn_write_app_p0(&t, wanted, 0u, 0u) :
                             evist_ctrl_txn_write_app_p2(&t, wanted, 0u);
            check(started, "controller revision model starts each write path");
            uint8_t sent = 0u;
            for (uint32_t now = 0u; now < 2500u && evist_ctrl_txn_busy(&t); ++now) {
                bafang_can_frame_t tx;
                if (!pump(&t, now, &tx)) continue;
                bafang_can_id_t id = bafang_can_id_decode(tx.id);
                if (id.type == BAFANG_CAN_OP_READ) {
                    if (path < 2u) {
                        uint8_t wheel_data[6]; evist_wheel_config_encode(&wheel, wheel_data);
                        bafang_can_frame_t rx = controller_frame(BAFANG_NODE_HMI,
                            BAFANG_CAN_OP_NORMAL_ACK, 0x32u, 0x03u, wheel_data, 6u);
                        evist_ctrl_txn_on_frame(&t, &rx, now + 1u);
                    } else {
                        uint8_t len = 64u;
                        bafang_can_frame_t rx = controller_frame(BAFANG_NODE_HMI,
                            BAFANG_CAN_OP_MULTIFRAME_START, 0x60u, sub, &len, 1u);
                        evist_ctrl_txn_on_frame(&t, &rx, now + 1u);
                        bafang_service_message_t m = block_message(stored);
                        m.sub = sub;
                        evist_ctrl_txn_on_service(&t, &m, now + 2u);
                    }
                    continue;
                }
                ++sent;
                if (path < 2u) {
                    bafang_can_frame_t rx = controller_frame(BAFANG_NODE_HMI,
                        BAFANG_CAN_OP_NORMAL_ACK, 0x32u, 0x03u, NULL, 0u);
                    evist_ctrl_txn_on_frame(&t, &rx, now + 1u);
                    continue;
                }
                if (revision == 0u) {
                    if (sent == 1u) {
                        bafang_can_frame_t rx = controller_frame(BAFANG_NODE_BESST,
                            BAFANG_CAN_OP_NORMAL_ACK, 0x60u, sub, NULL, 0u);
                        evist_ctrl_txn_on_frame(&t, &rx, now + 1u);
                    }
                    continue;
                }
                if (sent > 1u) memcpy(&stored[(sent - 2u) * 8u], tx.data, 8u);
                if (sent == 9u && path == 3u) stored[63] = 0u;
                bafang_can_frame_t rx = controller_frame(BAFANG_NODE_HMI,
                    BAFANG_CAN_OP_NORMAL_ACK, 0x60u, sub, NULL, 0u);
                evist_ctrl_txn_on_frame(&t, &rx, now + 1u);
            }
            evist_txn_kind_t kind; evist_txn_result_t result;
            check(evist_ctrl_txn_take_result(&t, &kind, &result, received),
                  "controller revision model returns a result");
            if (path < 2u) check(result == EVIST_TXN_OK && sent == 1u,
                                  "both revisions verify WHEEL and RIDE MODE");
            else if (revision == 0u) check(result == EVIST_TXN_MISMATCH && sent == 9u &&
                                              memcmp(stored, original, 63u) == 0,
                                              "0.649 source-3 parameter write remains unchanged on reread");
            else check(result == EVIST_TXN_OK && sent == 9u,
                       "0.654 parameter write ACK, apply and readback verify");
        }
    }
}

/* ---- router event semantics ------------------------------------------- */

static void test_router_event(void) {
    evist_can_router_t r; evist_can_router_init(&r);
    r.have_last_service = true; r.service_event = true;
    bafang_can_frame_t unrelated = controller_frame(BAFANG_NODE_BROADCAST, 0u, 0x32u, 0x01u, NULL, 8u);
    evist_can_router_rx(&r, &unrelated, 0u);
    check(!r.service_event, "unrelated RX clears the service event (old record is not fresh)");
}

/* ---- ADVANCED SETTINGS edit buffers ------------------------ */

typedef struct {
    evist_ctrl_txn_t txn;
    uint8_t wheel[6];   /* controller state */
    uint8_t assist[64];
    uint32_t now;
} sim_t;

/* Minimal controller model: ACKs writes, answers READs with its state and
 * broadcasts stale 0x3203 values every millisecond (continuous CAN). */
static void sim_step(sim_t *s, evist_advanced_t *a) {
    evist_adv_request_t req;
    if (evist_advanced_take_request(a, &req)) {
        bool started = false;
        if (req == EVIST_ADV_REQ_READ_WHEEL) started = evist_ctrl_txn_read_wheel(&s->txn, s->now);
        if (req == EVIST_ADV_REQ_READ_ASSIST) started = evist_ctrl_txn_read_assist(&s->txn, s->now);
        if (req == EVIST_ADV_REQ_WRITE_WHEEL) { evist_wheel_config_t c; evist_advanced_wheel_target(a, &c); started = evist_ctrl_txn_write_wheel(&s->txn, &c, s->now); }
        if (req == EVIST_ADV_REQ_WRITE_ASSIST) started = evist_ctrl_txn_write_assist(&s->txn, a->assist_block, a->committed.firmware_profile, s->now);
        if (!started) evist_advanced_request_failed(a, "REJECTED");
    }
    bafang_can_frame_t f, rx;
    rx = controller_frame(BAFANG_NODE_BROADCAST, 0u, 0x32u, 0x03u, s->wheel, 6u);
    evist_ctrl_txn_on_frame(&s->txn, &rx, s->now);
    if (pump(&s->txn, s->now, &f)) {
        bafang_can_id_t id = bafang_can_id_decode(f.id);
        if (id.type == BAFANG_CAN_OP_WRITE && id.cmd == 0x32u) {
            memcpy(s->wheel, f.data, 6);
            rx = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_NORMAL_ACK, 0x32u, 0x03u, NULL, 0u);
            evist_ctrl_txn_on_frame(&s->txn, &rx, s->now);
        } else if (id.type == BAFANG_CAN_OP_READ && id.cmd == 0x32u) {
            rx = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_NORMAL_ACK, 0x32u, 0x03u, s->wheel, 6u);
            evist_ctrl_txn_on_frame(&s->txn, &rx, s->now);
        } else if (id.type == BAFANG_CAN_OP_READ && id.cmd == 0x60u) {
            bafang_service_message_t m = block_message(s->assist);
            reply_block(&s->txn, &m, s->now);
        } else if (id.type == BAFANG_CAN_OP_WRITE && id.cmd == 0x60u && id.sub == 0x11u) {
            rx = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_NORMAL_ACK, 0x60u, 0x11u, NULL, 0u);
            evist_ctrl_txn_on_frame(&s->txn, &rx, s->now);
        } else if (id.type == BAFANG_CAN_OP_MULTIFRAME_START) {
            memcpy(s->assist, f.data, 8);
            rx = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_NORMAL_ACK, 0x60u, 0x11u, NULL, 0u);
            evist_ctrl_txn_on_frame(&s->txn, &rx, s->now);
        } else if (id.type == BAFANG_CAN_OP_MULTIFRAME) {
            memcpy(&s->assist[8u + id.sub * 8u], f.data, 8);
            rx = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_NORMAL_ACK, 0x60u, 0x11u, NULL, 0u);
            evist_ctrl_txn_on_frame(&s->txn, &rx, s->now);
        } else if (id.type == BAFANG_CAN_OP_MULTIFRAME_END) {
            memcpy(&s->assist[56], f.data, 8);
            rx = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_NORMAL_ACK, 0x60u, 0x11u, NULL, 0u);
            evist_ctrl_txn_on_frame(&s->txn, &rx, s->now);
        }
    }
    evist_txn_kind_t kind; evist_txn_result_t res; uint8_t data[64];
    if (evist_ctrl_txn_take_result(&s->txn, &kind, &res, data)) evist_advanced_on_result(a, kind, res, data);
    s->now++;
}
static void sim_run(sim_t *s, evist_advanced_t *a, uint32_t ms) { for (uint32_t i = 0u; i < ms; ++i) sim_step(s, a); }

static void test_advanced_buffers(void) {
    evist_preferences_t prefs; evist_preferences_defaults(&prefs);
    evist_advanced_t a; sim_t s;
    memset(&s, 0, sizeof s); evist_ctrl_txn_init(&s.txn);
    uint8_t w0[6] = {0xC4u, 0x09u, 0xD0u, 0x01u, 0xC0u, 0x08u}; /* 25.00 km/h, 29", 2240 mm */
    memcpy(s.wheel, w0, 6);
    block_fixture(s.assist);

    /* WHEEL: edit survives navigation and continuous CAN, then is verified. */
    evist_advanced_open(&a, &prefs, 5u);
    evist_ride_cfg_t ride=evist_ride_defaults();ride.mode=EVIST_RIDE_OFFROAD;
    evist_advanced_set_ride(&a,&ride);
    evist_advanced_key(&a, EVIST_ADV_KEY_POWER);               /* open WHEEL -> READ */
    sim_run(&s, &a, 1000u);
    check(a.wheel_valid && a.wheel_speed_centi_kph == 2500u, "WHEEL loaded by explicit READ");
    evist_advanced_key(&a, EVIST_ADV_KEY_POWER);               /* edit MAX SPEED */
    evist_advanced_key(&a, EVIST_ADV_KEY_UP);
    evist_advanced_key(&a, EVIST_ADV_KEY_UP);
    evist_advanced_key(&a, EVIST_ADV_KEY_POWER);               /* done, same row */
    sim_run(&s, &a, 500u);
    check(a.wheel_speed_centi_kph == 2600u && a.wheel_dirty, "WHEEL retains edit until SAVE (continuous CAN)");
    evist_advanced_key(&a, EVIST_ADV_KEY_DOWN);                /* CIRC */
    evist_advanced_key(&a, EVIST_ADV_KEY_DOWN);                /* DIAMETER */
    evist_advanced_key(&a, EVIST_ADV_KEY_DOWN);                /* SAVE */
    sim_run(&s, &a, 100u);
    check(a.wheel_speed_centi_kph == 2600u, "navigation keeps the edit");
    evist_advanced_key(&a, EVIST_ADV_KEY_POWER);               /* SAVE */
    sim_run(&s, &a, 2000u);
    check(s.wheel[0] == 0x28u && s.wheel[1] == 0x0Au, "controller received 26.00 km/h");
    check(strcmp(a.status, "VERIFY OK") == 0 && !a.wheel_dirty && a.wheel_speed_centi_kph == 2600u, "WHEEL verified by fresh readback");

    /* Speed limit is clamped to validated range. */
    a.cursor = EVIST_WHEEL_ROW_SPEED;
    evist_advanced_key(&a, EVIST_ADV_KEY_POWER);
    for (int i = 0; i < 200; ++i) evist_advanced_key(&a, EVIST_ADV_KEY_UP);
    check(a.wheel_speed_centi_kph == EVIST_WHEEL_SPEED_MAX, "speed stops at 70.00 km/h");
    evist_advanced_key(&a, EVIST_ADV_KEY_POWER);

    /* Diameter out of its circumference range is refused before any frame. */
    a.cursor = EVIST_WHEEL_ROW_DIAMETER;
    evist_advanced_key(&a, EVIST_ADV_KEY_POWER);
    evist_advanced_key(&a, EVIST_ADV_KEY_UP);                  /* 20": 1290..1880 */
    evist_advanced_key(&a, EVIST_ADV_KEY_POWER);
    a.cursor = EVIST_WHEEL_ROW_SAVE;
    evist_advanced_key(&a, EVIST_ADV_KEY_POWER);
    check(a.request == EVIST_ADV_REQ_NONE && strcmp(a.status, "CIRC OUT OF RANGE FOR DIAMETER") == 0, "diameter/circ check");

    /* BACK discards. */
    a.cursor = EVIST_WHEEL_ROW_BACK;
    evist_advanced_key(&a, EVIST_ADV_KEY_POWER);
    check(a.screen == EVIST_ADV_MENU, "BACK returns to menu");

    /* ASSIST lists enabled modes even while riding is OFF. */
    prefs.assist_level = 0u;
    a.working.assist_level = 0u;
    a.working.assist_mode_mask = (uint8_t)((1u << 0u) | (1u << 2u) | (1u << 4u));
    evist_advanced_key(&a, EVIST_ADV_KEY_DOWN);
    evist_advanced_key(&a, EVIST_ADV_KEY_POWER);               /* open ASSIST -> READ */
    sim_run(&s, &a, 1000u);
    check(a.assist_valid && evist_advanced_assist_cells(&a) == 6u, "ASSIST loaded, enabled modes x 2");
    check(evist_advanced_assist_cell_value(&a, 0u) == 25u, "L1 POWER = slot 1");
    check(evist_advanced_assist_cell_level(&a, 2u) == 3u && evist_advanced_assist_cell_level(&a, 4u) == 5u,
          "ASSIST E, S, B cell mapping");
    check(evist_advanced_assist_cell_value(&a, 2u) == 45u && evist_advanced_assist_cell_value(&a, 4u) == 60u,
          "ASSIST S and B read their own slots");
    a.cursor = 2u;
    evist_advanced_key(&a, EVIST_ADV_KEY_POWER);
    evist_advanced_key(&a, EVIST_ADV_KEY_UP);
    check(evist_advanced_assist_cell_value(&a, 2u) == 46u &&
          evist_advanced_assist_cell_value(&a, 0u) == 25u &&
          evist_advanced_assist_cell_value(&a, 4u) == 60u, "ASSIST S edit changes only S visible slot");
    evist_advanced_key(&a, EVIST_ADV_KEY_DOWN);
    evist_advanced_key(&a, EVIST_ADV_KEY_POWER);
    a.cursor = 0u;
    evist_advanced_key(&a, EVIST_ADV_KEY_POWER);               /* edit L1 POWER */
    evist_advanced_key(&a, EVIST_ADV_KEY_UP);
    evist_advanced_key(&a, EVIST_ADV_KEY_POWER);               /* done -> L1 SPEED */
    check(a.cursor == 1u, "POWER moves to the next cell");
    sim_run(&s, &a, 500u);
    check(evist_advanced_assist_cell_value(&a, 0u) == 26u && a.assist_dirty, "ASSIST retains edit until SAVE");
    a.cursor = evist_advanced_assist_cells(&a);                 /* SAVE */
    evist_advanced_key(&a, EVIST_ADV_KEY_POWER);
    sim_run(&s, &a, 3000u);
    check(s.assist[EVIST_ASSIST_POWER_OFF + 1u] == 26u && s.assist[63] == evist_assist_checksum(s.assist), "controller received edited block");
    check(strcmp(a.status, "VERIFY OK") == 0 && !a.assist_dirty, "ASSIST verified by fresh readback");

    /* SAVE without changes does nothing. */
    evist_advanced_key(&a, EVIST_ADV_KEY_POWER);
    check(a.request == EVIST_ADV_REQ_NONE && strcmp(a.status, "NO CHANGES") == 0, "SAVE without changes");

    /* Descending table read from the controller cannot be written as BAFANG. */
    a.assist_block[EVIST_ASSIST_POWER_OFF + 3u] = 5u; /* L2 below L1 */
    a.assist_dirty = true;
    evist_advanced_key(&a, EVIST_ADV_KEY_POWER);
    check(a.request == EVIST_ADV_REQ_NONE && strcmp(a.status, "LEVELS MUST NOT DECREASE") == 0, "BAFANG SAVE refuses descending levels");
}

/* ---- preference memory ------------------------- */

static uint8_t mem[0x10000];
static bool fail_reads, fail_writes;
static uint32_t lowest_write = 0xFFFFFFFFu, writes_in_ride;
static bool m_read(void *ctx, uint32_t addr, uint8_t *dst, size_t n) {
    (void)ctx;
    if (fail_reads || addr < EVIST_PREF_REGION_BASE || addr + n > EVIST_PREF_REGION_END) return false;
    memcpy(dst, mem + (addr - EVIST_PREF_REGION_BASE), n);
    return true;
}
static bool m_write(void *ctx, uint32_t addr, const uint8_t *src, size_t n) {
    (void)ctx;
    if (addr < EVIST_PREF_REGION_BASE || addr + n > EVIST_PREF_REGION_END) return false;
    if (addr < EVIST_PREF_RIDE_END && addr + n > EVIST_PREF_RIDE_BASE) ++writes_in_ride;
    if (addr < lowest_write) lowest_write = addr;
    if (fail_writes) return true; /* write "succeeds" but nothing is programmed */
    for (size_t i = 0; i < n; ++i) mem[addr - EVIST_PREF_REGION_BASE + i] &= src[i];
    return true;
}
static const uint8_t hdr[8] = {'E','V','P','1',0xBAu,0xA9u,0xAFu,0xCEu};
static void mem_reset(bool header) {
    memset(mem, 0xFF, sizeof mem);
    if (header) memcpy(mem, hdr, sizeof hdr);
    fail_reads = fail_writes = false;
    writes_in_ride = 0u;
}
static void put_pair(uint32_t idx, uint8_t field, uint8_t level) {
    uint8_t *r = mem + (EVIST_PREF_PAIR_BASE - EVIST_PREF_REGION_BASE) + idx * 2u;
    r[0] = (uint8_t)((field << 4) | level); r[1] = (uint8_t)(r[0] ^ 0x5Au);
}

static bool same_prefs(const evist_preferences_t *a, const evist_preferences_t *b);
static void put_code(uint32_t addr, uint8_t code);

static void test_preferences(void) {
    dpc245_persist_io_t io = {m_read, m_write, NULL, NULL};
    evist_preferences_t p0, p1, p2;
    evist_preferences_defaults(&p0);
    check(p0.temp_view == EVIST_TEMP_BOTH && (p0.panel_mask & (1u << 9)) == 0u,
          "TEMP defaults: BOTH and pair hidden");
    check(p0.graph_scale_w == 600u && evist_preferences_validate(&p0), "POWER GRAPH default 600 W");
    {
        evist_ride_cfg_t ride = evist_ride_defaults();
        check(ride.offroad_x100 == 4000u && ride.legal_x100 == 2500u && evist_ride_cfg_valid(&ride),
              "RIDE defaults: OFFROAD 40.0, LEGAL 25.0 km/h");
    }
    unsigned free_codes = 0u;
    for (unsigned code = 0u; code < 256u; ++code) {
        bool free = evist_pref_setting_slot_of_code((uint8_t)code) < 0;
        check(free == (code == 0x40u), "all 256 setting codes: only 0x40 is unclassified");
        if (free) ++free_codes;
    }
    check(free_codes == 1u, "one unused code for TEMP counter");
    mem_reset(true);
    put_code(EVIST_PREF_SET_BASE, 0x52u);
    check(evist_preferences_load(&io, &p1) && p1.temp_view == EVIST_TEMP_BOTH &&
          (p1.panel_mask & (1u << 9)) == 0u, "old settings log defaults TEMP state");

    mem_reset(true);
    p1 = p0; p1.temp_view = EVIST_TEMP_1;
    check(evist_preferences_save(&io, &p0, &p1) == EVIST_PREF_SAVE_OK &&
          evist_preferences_load(&io, &p2) && same_prefs(&p1, &p2), "TEMP 1 round trip");
    p2 = p1; p2.temp_view = EVIST_TEMP_2;
    check(evist_preferences_save(&io, &p1, &p2) == EVIST_PREF_SAVE_OK, "TEMP 2 round trip save");
    p1 = p2; p1.temp_view = EVIST_TEMP_ALTERNATE; p1.panel_mask |= 1u << 9;
    check(evist_preferences_save(&io, &p2, &p1) == EVIST_PREF_SAVE_OK &&
          evist_preferences_load(&io, &p2) && same_prefs(&p1, &p2), "ALTERNATE and TEMP pair enabled round trip");
    p2 = p1; p2.temp_view = EVIST_TEMP_BOTH; p2.panel_mask &= (uint16_t)~(1u << 9);
    check(evist_preferences_save(&io, &p1, &p2) == EVIST_PREF_SAVE_OK &&
          evist_preferences_load(&io, &p1) && same_prefs(&p1, &p2), "BOTH and TEMP pair hidden round trip");
    uint32_t tail = EVIST_PREF_SET_BASE;
    while (mem[tail - EVIST_PREF_REGION_BASE] != 0xFFu || mem[tail - EVIST_PREF_REGION_BASE + 1u] != 0xFFu) tail += 2u;
    mem[tail - EVIST_PREF_REGION_BASE] = 0x40u;
    mem[tail - EVIST_PREF_REGION_BASE + 1u] = 0x00u;
    check(evist_preferences_load(&io, &p1) && same_prefs(&p1, &p2), "torn TEMP increment ignored");
    p1 = p2; p1.temp_view = EVIST_TEMP_2; p1.panel_mask |= 1u << 9;
    check(evist_preferences_save(&io, NULL, &p1) == EVIST_PREF_SAVE_OK &&
          evist_preferences_load(&io, &p2) && same_prefs(&p1, &p2), "unknown prior snapshot writes TEMP state");
    p1 = p2; p1.temp_view = EVIST_TEMP_ALTERNATE; p1.panel_mask &= (uint16_t)~(1u << 9);
    check(evist_preferences_save(&io, NULL, &p1) == EVIST_PREF_SAVE_OK &&
          evist_preferences_load(&io, &p2) && same_prefs(&p1, &p2), "unknown prior snapshot advances from stored TEMP state");
    mem_reset(true);
    evist_pref_store_t store;
    evist_pref_store_init(&store, &io, &p1);
    p2 = p1; p2.temp_view = EVIST_TEMP_2; p2.panel_mask |= 1u << 9; p2.lower_panel = 9u;
    check(store.known && evist_pref_store_save(&store, &io, &p2) == EVIST_PREF_SAVE_OK &&
          evist_preferences_load(&io, &p1) && same_prefs(&p1, &p2),
          "cooperative TEMP view, enable bit and active pair round trip");

    /* record field codes map to the PANELS order. */
    check(evist_pref_panel_from_record_field(1u) == 3u, "field 1 CAD+TRQ -> panel 3");
    check(evist_pref_panel_from_record_field(12u) == 1u, "alternative pair code 12 MAX+AVG -> panel 1");
    for (uint8_t panel = 0u; panel < EVIST_PANEL_COUNT; ++panel)
        check(evist_pref_panel_from_record_field(evist_pref_record_field_from_panel(panel)) == panel, "panel <-> field round trip");

    mem_reset(true);
    put_pair(0u, 1u, 3u); /* stored record: CAD+TRQ, assist 3 */
    check(evist_preferences_load(&io, &p1) && p1.lower_panel == 3u && p1.assist_level == 3u, "reads pair record");
    p2 = p1; p2.lower_panel = 6u; /* PEDAL+MOTOR */
    check(evist_preferences_save(&io, &p1, &p2) == EVIST_PREF_SAVE_OK, "pair save");
    uint8_t *r = mem + (EVIST_PREF_PAIR_BASE - EVIST_PREF_REGION_BASE) + 2u;
    check((r[0] >> 4) == 3u, "writes field code 3 for PEDAL+MOTOR");

    /* Torn tail record: the previous valid record wins. */
    r[2] = 0x00u; r[3] = 0x00u;
    check(evist_preferences_load(&io, &p1) && p1.lower_panel == 6u, "torn tail tolerated");

    /* Full pair log stops before the RIDE reservation (the reservation is
     * still blank, so only the fence protects it). */
    mem_reset(true);
    for (uint32_t i = 0u; i < (EVIST_PREF_RIDE_BASE - EVIST_PREF_PAIR_BASE) / 2u; ++i) put_pair(i, 0u, 0u);
    p1 = p0; p1.assist_level = 1u;
    check(evist_preferences_save(&io, &p0, &p1) == EVIST_PREF_SAVE_LOG_FULL && writes_in_ride == 0u,
          "full pair log with blank RIDE area -> MEMORY FULL, no write at 0x1FDE00");
    memset(mem + (EVIST_PREF_RIDE_BASE - EVIST_PREF_REGION_BASE), 0xA5, EVIST_PREF_RIDE_END - EVIST_PREF_RIDE_BASE);
    p1 = p0; p1.assist_level = 1u;
    check(evist_preferences_save(&io, &p0, &p1) == EVIST_PREF_SAVE_LOG_FULL, "full pair log -> MEMORY FULL");
    check(writes_in_ride == 0u, "no write inside RIDE reservation 0x1FDE00..0x1FDFFF");
    bool ride_intact = true;
    for (uint32_t i = 0u; i < EVIST_PREF_RIDE_END - EVIST_PREF_RIDE_BASE; ++i)
        ride_intact = ride_intact && mem[EVIST_PREF_RIDE_BASE - EVIST_PREF_REGION_BASE + i] == 0xA5u;
    check(ride_intact, "RIDE records preserved");
    check(evist_preferences_load(&io, &p2) && p2.assist_level == 0u, "full log still loads");

    /* Failure reasons are distinguishable. */
    mem_reset(false);
    mem[0x100] = 0x00u; /* foreign data, no header */
    p1 = p0; p1.graph_scale_w = 1500u;
    check(evist_preferences_save(&io, &p0, &p1) == EVIST_PREF_SAVE_NO_MEMORY, "foreign data -> NO MEMORY, nothing written");
    mem_reset(true); fail_reads = true;
    check(evist_preferences_save(&io, &p0, &p1) == EVIST_PREF_SAVE_NO_MEMORY, "unreadable memory -> NO MEMORY");
    mem_reset(true); fail_writes = true;
    check(evist_preferences_save(&io, &p0, &p1) == EVIST_PREF_SAVE_WRITE_FAILED, "unprogrammed write -> WRITE ERROR");
    mem_reset(true);
    check(evist_preferences_save(&io, &p0, &p1) == EVIST_PREF_SAVE_OK && evist_preferences_load(&io, &p2) &&
          p2.graph_scale_w == 1500u, "settings code save/load");
    mem_reset(true);
    p1 = p0; p1.light_gain = 0u;
    check(evist_preferences_save(&io, &p0, &p1) == EVIST_PREF_SAVE_INVALID, "invalid preferences rejected");
    check(strcmp(evist_pref_save_text(EVIST_PREF_SAVE_LOG_FULL), "MEMORY FULL - NOT SAVED") == 0, "user text for full log");
}


/* ---- Regression: reply STARTed before our READ ----- */

/* main.c order: transaction sees the frame, the router reassembles, then a
 * completed message is offered to the transaction. */
static void deliver_rx(evist_can_router_t *r, evist_ctrl_txn_t *t, const bafang_can_frame_t *f, uint32_t now) {
    evist_ctrl_txn_on_frame(t, f, now);
    evist_can_router_rx(r, f, now);
    if (r->service_event) evist_ctrl_txn_on_service(t, &r->last_service, now);
}
/* main.c controller_config_service: tick, then at most one frame. */
static bool pump_main(evist_ctrl_txn_t *t, evist_can_router_t *r, uint32_t now, bafang_can_frame_t *out) {
    (void)r;
    return pump(t, now, out);
}
static void send_block(evist_can_router_t *r, evist_ctrl_txn_t *t, const uint8_t b[64], uint32_t now,
                       bool with_start, unsigned first_data, unsigned last_data) {
    bafang_can_frame_t f;
    if (with_start) {
        f = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_MULTIFRAME_START, 0x60u, 0x11u, NULL, 0u);
        deliver_rx(r, t, &f, now);
    }
    for (unsigned i = first_data; i <= last_data && i < 7u; ++i) {
        f = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_MULTIFRAME, 0u, (uint8_t)i, b + i * 8u, 8u);
        deliver_rx(r, t, &f, now);
    }
    if (last_data >= 7u) {
        f = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_MULTIFRAME_END, 0u, 7u, b + 56, 8u);
        deliver_rx(r, t, &f, now);
    }
}

static void accel_fragments(evist_can_router_t *r, evist_ctrl_txn_t *t,
                            uint8_t sub, const uint8_t block[64], uint32_t start_ms) {
    const uint8_t size = 64u;
    bafang_can_frame_t frame = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_MULTIFRAME_START,
                                                0x60u, sub, &size, 1u);
    for (uint8_t part = 0u; part < 9u; ++part) {
        if (part != 0u)
            frame = controller_frame(BAFANG_NODE_HMI,
                                     part == 8u ? BAFANG_CAN_OP_MULTIFRAME_END : BAFANG_CAN_OP_MULTIFRAME,
                                     0u, (uint8_t)(part - 1u), block + (part - 1u) * 8u, 8u);
        deliver_rx(r, t, &frame, start_ms + (uint32_t)part * 50u);
        bafang_can_frame_t ack;
        check(evist_can_router_peek_service_ack(r, &ack), "ACCEL ACK queued in receive pass");
        bafang_can_id_t id = bafang_can_id_decode(ack.id);
        check(id.src == BAFANG_NODE_HMI && id.dst == BAFANG_NODE_CONTROLLER &&
              id.type == BAFANG_CAN_OP_NORMAL_ACK && id.cmd == 0x60u && id.sub == sub,
              "ACCEL ACK retains parameter command");
        evist_can_router_pop_service_ack(r);
    }
}

static void test_accel_read(void) {
    evist_ctrl_txn_t t; evist_ctrl_txn_init(&t);
    evist_can_router_t r; evist_can_router_init(&r);
    bafang_can_frame_t tx;
    evist_txn_kind_t kind; evist_txn_result_t result; uint8_t data[64];
    check(evist_ctrl_txn_discover_accel(&t, 0u) && pump(&t, 0u, &tx) &&
          is_read_query(&tx, 0xF2u, 0x03u), "F203 discovery query");
    const uint8_t desc_size = 11u;
    tx = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_MULTIFRAME_START, 0xF2u, 0x03u,
                          &desc_size, 1u);
    deliver_rx(&r, &t, &tx, 599u);
    uint8_t arbitrary_desc[11] = {'n','o','t','-','e','V','D','!', '?','?','?'};
    tx = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_MULTIFRAME, 0u, 0u,
                          arbitrary_desc, 8u);
    deliver_rx(&r, &t, &tx, 650u);
    tx = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_MULTIFRAME_END, 0u, 1u,
                          arbitrary_desc + 8u, 3u);
    deliver_rx(&r, &t, &tx, 700u);
    check(evist_ctrl_txn_take_result(&t, &kind, &result, data) &&
          kind == EVIST_TXN_DISCOVER_ACCEL && result == EVIST_TXN_OK, "F203 START selects per-level");
    while (evist_can_router_take_service_ack(&r, &tx)) { }

    check(evist_ctrl_txn_discover_accel(&t, 1000u) && pump(&t, 1000u, &tx), "silent discovery begins");
    evist_ctrl_txn_tick(&t, 1600u);
    check(evist_ctrl_txn_take_result(&t, &kind, &result, data) && result == EVIST_TXN_NO_READBACK,
          "silent F203 selects global after 600 ms");
    check(evist_ctrl_txn_discover_accel(&t, 1700u) && pump(&t, 1700u, &tx), "START-only discovery begins");
    tx = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_MULTIFRAME_START, 0xF2u, 0x03u, NULL, 0u);
    evist_ctrl_txn_on_frame(&t, &tx, 1701u);
    evist_ctrl_txn_tick(&t, 2001u);
    check(evist_ctrl_txn_take_result(&t, &kind, &result, data) && result == EVIST_TXN_OK,
          "F203 START alone selects per-level after drain window");
    check(evist_ctrl_txn_discover_accel(&t, 2200u) && pump(&t, 2200u, &tx), "late discovery begins");
    tx = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_MULTIFRAME_START, 0xF2u, 0x03u, NULL, 0u);
    evist_ctrl_txn_on_frame(&t, &tx, 2800u);
    evist_ctrl_txn_tick(&t, 2800u);
    check(evist_ctrl_txn_take_result(&t, &kind, &result, data) && result == EVIST_TXN_NO_READBACK,
          "late F203 ignored");

    uint8_t p0[64] = {0};
    for (uint8_t i = 1u; i <= 9u; ++i) p0[i] = i <= 8u ? i : 8u;
    p0[63] = 0u; /* P0 flag, deliberately not sum8. */
    check(evist_ctrl_txn_read_accel_p0(&t, 3000u) && pump(&t, 3000u, &tx) &&
          is_read_query(&tx, 0x60u, 0x10u), "P0 READ query");
    accel_fragments(&r, &t, 0x10u, p0, 3001u);
    check(evist_ctrl_txn_take_result(&t, &kind, &result, data) && result == EVIST_TXN_OK &&
          data[2] == 2u && data[9] == 8u, "P0 full block, flag not checksum");

    uint8_t p2[64] = {0}; p2[54] = 7u; p2[63] = evist_assist_checksum(p2);
    check(evist_ctrl_txn_read_accel_p2(&t, 4000u) && pump(&t, 4000u, &tx) &&
          is_read_query(&tx, 0x60u, 0x12u), "P2 READ query");
    accel_fragments(&r, &t, 0x12u, p2, 4001u);
    check(evist_ctrl_txn_take_result(&t, &kind, &result, data) && result == EVIST_TXN_OK &&
          data[54] == 7u, "P2 global byte54");
    p2[63]++;
    check(evist_ctrl_txn_read_accel_p2(&t, 5000u) && pump(&t, 5000u, &tx), "bad checksum P2 query");
    accel_fragments(&r, &t, 0x12u, p2, 5001u);
    check(evist_ctrl_txn_take_result(&t, &kind, &result, data) && result == EVIST_TXN_CHECKSUM_BAD,
          "P2 bad checksum rejected");
    check(evist_ctrl_txn_read_accel_p0(&t, 6000u) && pump(&t, 6000u, &tx), "P0 NACK query");
    tx = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_ERROR_ACK, 0x60u, 0x10u, NULL, 0u);
    evist_ctrl_txn_on_frame(&t, &tx, 6001u);
    check(evist_ctrl_txn_busy(&t), "P0 ERROR_ACK waits for data");
    evist_ctrl_txn_tick(&t, 7050u);
    check(evist_ctrl_txn_take_result(&t, &kind, &result, data) && result == EVIST_TXN_NACK,
          "P0 ERROR_ACK without data fails at deadline");

    /* Two RX frames before a TX opportunity must retain two ordered ACKs. */
    evist_can_router_init(&r);
    const uint8_t size = 64u;
    bafang_can_frame_t start = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_MULTIFRAME_START,
                                                0x60u, 0x10u, &size, 1u);
    bafang_can_frame_t first = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_MULTIFRAME,
                                                0u, 0u, p0, 8u);
    evist_can_router_rx(&r, &start, 0u);
    evist_can_router_rx(&r, &first, 0u);
    check(r.service_ack_count == 2u, "back-to-back fragments retain both ACKs");
    check(evist_can_router_take_service_ack(&r, &tx) && r.service_ack_count == 1u,
          "first ACK consumed once");
    check(evist_can_router_take_service_ack(&r, &tx) && r.service_ack_count == 0u,
          "second ACK consumed once");
    evist_can_router_init(&r);
    evist_can_router_rx(&r, &start, 0u);
    first = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_MULTIFRAME,
                             0u, 1u, p0, 8u);
    evist_can_router_rx(&r, &first, 0u);
    check(!r.service_event && !r.service_rx.slot[BAFANG_NODE_CONTROLLER].active &&
          r.service_ack_count == 1u, "out-of-order fragment rejected without ACK");

    evist_preferences_t prefs; evist_preferences_defaults(&prefs);
    evist_advanced_t adv = {0}; evist_advanced_open(&adv, &prefs, 5u);
    evist_advanced_key(&adv, EVIST_ADV_KEY_DOWN);
    evist_advanced_key(&adv, EVIST_ADV_KEY_DOWN);
    evist_advanced_key(&adv, EVIST_ADV_KEY_POWER);
    evist_adv_request_t req;
    check(adv.screen == EVIST_ADV_ACCEL && evist_advanced_take_request(&adv, &req) &&
          req == EVIST_ADV_REQ_DISCOVER_ACCEL, "ACCEL opens with discovery");
    evist_advanced_on_result(&adv, EVIST_TXN_DISCOVER_ACCEL, EVIST_TXN_OK, NULL);
    check(adv.accel_mode == EVIST_ACCEL_PER_LEVEL && evist_advanced_take_request(&adv, &req) &&
          req == EVIST_ADV_REQ_READ_ACCEL_P0, "answered discovery requests P0");
    evist_advanced_on_result(&adv, EVIST_TXN_READ_ACCEL_P0, EVIST_TXN_OK, p0);
    check(adv.accel_valid && adv.accel_values[evist_assist_level_slot(5u, 5u)] == 8u,
          "visible level mapping and value");
    evist_advanced_open(&adv, &prefs, 5u);
    evist_advanced_open_page(&adv, EVIST_ADV_ACCEL);
    check(adv.accel_mode == EVIST_ACCEL_UNKNOWN && adv.request == EVIST_ADV_REQ_DISCOVER_ACCEL,
          "opening ADV resets controller mode until discovery");
    adv.screen = EVIST_ADV_ACCEL;
    evist_advanced_on_result(&adv, EVIST_TXN_READ_ACCEL_P0, EVIST_TXN_NO_READBACK, NULL);
    check(strcmp(adv.status, "P0 READ FAILED") == 0, "P0 explicit error");
    adv.accel_mode = EVIST_ACCEL_GLOBAL;
    evist_advanced_on_result(&adv, EVIST_TXN_READ_ACCEL_P2, EVIST_TXN_CHECKSUM_BAD, NULL);
    check(strcmp(adv.status, "CHECKSUM BAD") == 0, "P2 explicit checksum error");
}

static void test_stale_reassembly(void) {
    evist_ctrl_txn_t t; evist_ctrl_txn_init(&t);
    evist_can_router_t r; evist_can_router_init(&r);
    uint8_t b[64]; block_fixture(b);
    bafang_can_frame_t tx, f;
    evist_txn_kind_t kind; evist_txn_result_t res; uint8_t data[64];

    check(evist_ctrl_txn_write_assist(&t, b, EVIST_PROFILE_BAFANG, 0u), "stale: write starts");
    uint32_t now = 0u;
    for (unsigned sent = 0u; sent < 9u; ++now) if (pump_main(&t, &r, now, &tx)) {
        ++sent;
        f = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_NORMAL_ACK, 0x60u, 0x11u, NULL, 0u);
        deliver_rx(&r, &t, &f, now + 1u);
    }
    /* An old 0x6011 reply starts during the settle time and carries 56 bytes. */
    send_block(&r, &t, b, now + 40u, true, 0u, 6u);
    uint32_t read_at = 0u;
    for (uint32_t ms = now; ms < now + 2000u; ++ms)
        if (pump_main(&t, &r, ms, &tx) && is_read_query(&tx, 0x60u, 0x11u)) { read_at = ms; break; }
    check(read_at != 0u, "stale: own READ issued");
    send_block(&r, &t, b, read_at + 1u, false, 7u, 7u);  /* only its END arrives after READ */
    check(evist_ctrl_txn_busy(&t), "transfer STARTed before READ never verifies ");
    send_block(&r, &t, b, read_at + 5u, true, 0u, 7u);   /* fresh complete reply */
    check(evist_ctrl_txn_take_result(&t, &kind, &res, data) && res == EVIST_TXN_OK, "fresh full reply completes");

    /* Fresh reply with other content after a stale START: MISMATCH, not OK. */
    uint8_t other[64]; block_fixture(other); other[EVIST_ASSIST_SPEED_OFF + 8u] = 99u; other[63] = evist_assist_checksum(other);
    check(evist_ctrl_txn_write_assist(&t, b, EVIST_PROFILE_BAFANG, 10000u), "stale 2: write");
    now = 10000u;
    for (unsigned sent = 0u; sent < 9u; ++now) if (pump_main(&t, &r, now, &tx)) {
        ++sent;
        f = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_NORMAL_ACK, 0x60u, 0x11u, NULL, 0u);
        deliver_rx(&r, &t, &f, now + 1u);
    }
    send_block(&r, &t, b, now + 10u, true, 0u, 3u);
    for (uint32_t ms = now; ms < now + 2000u; ++ms)
        if (pump_main(&t, &r, ms, &tx) && is_read_query(&tx, 0x60u, 0x11u)) { read_at = ms; break; }
    send_block(&r, &t, other, read_at + 2u, true, 0u, 7u);
    check(evist_ctrl_txn_take_result(&t, &kind, &res, data) && res == EVIST_TXN_MISMATCH, "fresh different block -> MISMATCH");

    /* Retry boundary: a block STARTed before the second READ does not count. */
    check(evist_ctrl_txn_read_assist(&t, 20000u), "stale 3: read");
    uint32_t reads[3] = {0u, 0u, 0u}; unsigned nreads = 0u;
    for (uint32_t ms = 20000u; ms < 21000u && nreads < 2u; ++ms) {
        if (pump_main(&t, &r, ms, &tx) && is_read_query(&tx, 0x60u, 0x11u)) reads[nreads++] = ms;
        if (nreads == 1u && ms == reads[0] + 500u) send_block(&r, &t, b, ms, true, 0u, 6u); /* in the gap */
    }
    check(nreads == 2u, "second READ issued");
    send_block(&r, &t, b, reads[1] + 1u, false, 7u, 7u);
    check(evist_ctrl_txn_busy(&t), "START from the gap before the 2nd READ ignored");
}

/* ---- deadlines independent of RX/tick order ---------- */

static void test_deadlines(void) {
    uint8_t w[6] = {0xC4u, 0x09u, 0xD0u, 0x01u, 0xC0u, 0x08u};
    bafang_can_frame_t tx, f;
    evist_txn_kind_t kind; evist_txn_result_t res; uint8_t data[64];
    /* 0xFFFFFF80 and 0xFFFFFFF0: every window (220/450/600 ms) crosses zero. */
    const uint32_t starts[4] = {0u, 0xFFFFFF00u, 0xFFFFFF80u, 0xFFFFFFF0u};
    check((uint32_t)(starts[2] + 220u) < starts[2] && (uint32_t)(starts[3] + 220u) < starts[3],
          "deadline: WHEEL read window really wraps");
    for (unsigned k = 0u; k < 4u; ++k) {
        const uint32_t t0 = starts[k];
        evist_ctrl_txn_t t;
        /* READ WHEEL window 220 ms: due-1 accepted, due and due+1 rejected. */
        for (int delta = -1; delta <= 1; ++delta) {
            evist_ctrl_txn_init(&t);
            check(evist_ctrl_txn_read_wheel(&t, t0) && pump(&t, t0, &tx), "deadline: read sent");
            uint32_t at = t0 + 220u + (uint32_t)delta;
            f = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_NORMAL_ACK, 0x32u, 0x03u, w, 6u);
            evist_ctrl_txn_on_frame(&t, &f, at);      /* RX before tick, as in main */
            bool done = evist_ctrl_txn_take_result(&t, &kind, &res, data);
            if (delta < 0) check(done && res == EVIST_TXN_OK, "reply at due-1 accepted");
            else check(!done && evist_ctrl_txn_busy(&t), "reply at due / due+1 rejected (RX before tick)");
        }
        /* Long loop stall: a reply polled at +1000 ms is not OK. */
        evist_ctrl_txn_init(&t);
        check(evist_ctrl_txn_read_wheel(&t, t0) && pump(&t, t0, &tx), "stall: read sent");
        evist_ctrl_txn_on_frame(&t, &f, t0 + 1000u);
        check(!(t.step == EVIST_TXN_STEP_DONE && t.result == EVIST_TXN_OK), "reply after loop stall not accepted");
        /* WHEEL write ACK window 600 ms. */
        for (int delta = -1; delta <= 1; ++delta) {
            evist_wheel_config_t c = {2500u, 0x01D0u, 2240u};
            evist_ctrl_txn_init(&t);
            check(evist_ctrl_txn_write_wheel(&t, &c, t0) && pump(&t, t0, &tx), "ack window: write sent");
            f = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_NORMAL_ACK, 0x32u, 0x03u, NULL, 0u);
            evist_ctrl_txn_on_frame(&t, &f, t0 + 600u + (uint32_t)delta);
            evist_ctrl_txn_tick(&t, t0 + 600u + (uint32_t)delta);
            if (delta < 0) check(t.step == EVIST_TXN_STEP_SETTLE, "ACK at due-1 accepted");
            else check(evist_ctrl_txn_take_result(&t, &kind, &res, data) && res == EVIST_TXN_NO_ACK, "ACK at due / due+1 -> NO ACK");
        }
        /* ASSIST block (START and END together) at due / due+1 rejected, due-1 accepted. */
        uint8_t b[64]; block_fixture(b);
        for (int delta = -1; delta <= 1; ++delta) {
            evist_ctrl_txn_init(&t);
            check(evist_ctrl_txn_read_assist(&t, t0) && pump(&t, t0, &tx), "assist window: read sent");
            bafang_service_message_t m = block_message(b);
            reply_block(&t, &m, t0 + 450u + (uint32_t)delta);
            bool done = evist_ctrl_txn_take_result(&t, &kind, &res, data);
            if (delta < 0) check(done && res == EVIST_TXN_OK, "block at due-1 accepted");
            else check(!done, "block at due / due+1 rejected");
        }
        /* START in time (+1 ms), END at due-1 / due / due+1: only
         * the deadline of the completed block can reject the late END. */
        for (int delta = -1; delta <= 1; ++delta) {
            evist_can_router_t r; evist_can_router_init(&r);
            evist_ctrl_txn_init(&t);
            check(evist_ctrl_txn_read_assist(&t, t0) && pump(&t, t0, &tx), "split block: read sent");
            send_block(&r, &t, b, t0 + 1u, true, 0u, 6u);
            check(evist_ctrl_txn_busy(&t), "split block: START and data alone do not complete");
            send_block(&r, &t, b, t0 + 450u + (uint32_t)delta, false, 7u, 7u);
            if (delta < 0) check(t.step == EVIST_TXN_STEP_DONE && t.result == EVIST_TXN_OK, "split block: END at due-1 accepted");
            else check(evist_ctrl_txn_busy(&t), "split block: timely START cannot accept END at due / due+1");
        }
        /* Tick before RX: the window closes in tick, so a reply polled at due /
         * due+1 is dropped and is never counted for the READ that follows. */
        for (uint32_t late = 0u; late <= 1u; ++late) {
            const uint32_t at = t0 + 220u + late;
            evist_ctrl_txn_init(&t);
            check(evist_ctrl_txn_read_wheel(&t, t0) && pump(&t, t0, &tx), "tick-first wheel: read sent");
            evist_ctrl_txn_tick(&t, at);
            f = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_NORMAL_ACK, 0x32u, 0x03u, w, 6u);
            evist_ctrl_txn_on_frame(&t, &f, at);
            check(evist_ctrl_txn_busy(&t) && t.step == EVIST_TXN_STEP_SEND_READ, "tick-first wheel: late reply dropped");
            check(evist_ctrl_txn_next_tx(&t, at, &tx) && is_read_query(&tx, 0xF2u, 0x03u), "tick-first wheel: 0xF203 READ follows");

            const uint32_t at_a = t0 + 450u + late;
            evist_can_router_t r; evist_can_router_init(&r);
            evist_ctrl_txn_init(&t);
            check(evist_ctrl_txn_read_assist(&t, t0) && pump(&t, t0, &tx), "tick-first assist: read sent");
            send_block(&r, &t, b, t0 + 1u, true, 0u, 6u);
            evist_ctrl_txn_tick(&t, at_a);
            send_block(&r, &t, b, at_a, false, 7u, 7u);
            check(evist_ctrl_txn_busy(&t) && t.step == EVIST_TXN_STEP_READ_GAP, "tick-first assist: late END dropped");
            uint32_t again = run_until_read(&t, at_a, 0x60u, 0x11u);
            check(again == at_a + 150u, "tick-first assist: next READ after the 150 ms gap");
            send_block(&r, &t, b, again + 1u, false, 7u, 7u);
            check(evist_ctrl_txn_busy(&t), "tick-first assist: old transfer never completes the new READ");

            evist_wheel_config_t c = {2500u, 0x01D0u, 2240u};
            evist_ctrl_txn_init(&t);
            check(evist_ctrl_txn_write_wheel(&t, &c, t0) && pump(&t, t0, &tx), "tick-first ack: write sent");
            evist_ctrl_txn_tick(&t, t0 + 600u + late);
            f = controller_frame(BAFANG_NODE_HMI, BAFANG_CAN_OP_NORMAL_ACK, 0x32u, 0x03u, NULL, 0u);
            evist_ctrl_txn_on_frame(&t, &f, t0 + 600u + late);
            check(evist_ctrl_txn_take_result(&t, &kind, &res, data) && res == EVIST_TXN_NO_ACK, "tick-first ack: late ACK -> NO ACK");
        }
    }
}

/* ---- SAVED only for what a reload returns ------------ */

static unsigned write_calls, fail_write_at;
static bool counted_write(void *ctx, uint32_t addr, const uint8_t *src, size_t n) {
    if (++write_calls == fail_write_at) return false;
    return m_write(ctx, addr, src, n);
}

static void test_partial_save(void) {
    dpc245_persist_io_t io = {m_read, counted_write, NULL, NULL};
    evist_preferences_t original, edited, loaded;
    evist_preferences_defaults(&original);
    edited = original;
    edited.light_gain = 6u; edited.light_dwell = 4u; edited.graph_scale_w = 1500u; edited.assist_level = 2u;
    /* 3 setting records + 1 pair record; fail each one in turn. */
    for (unsigned k = 1u; k <= 4u; ++k) {
        mem_reset(true);
        evist_pref_store_t store;
        evist_pref_store_init(&store, &io, NULL);
        write_calls = 0u; fail_write_at = k;
        check(evist_pref_store_save(&store, &io, &edited) == EVIST_PREF_SAVE_WRITE_FAILED, "partial: failure reported");
        fail_write_at = 0u;
        /* The user goes back to the original values and saves again. */
        evist_pref_save_result_t r = evist_pref_store_save(&store, &io, &original);
        check(r == EVIST_PREF_SAVE_OK, "partial: revert saves");
        check(evist_preferences_load(&io, &loaded) && loaded.light_gain == original.light_gain &&
              loaded.light_dwell == original.light_dwell && loaded.graph_scale_w == original.graph_scale_w &&
              loaded.assist_level == original.assist_level, "partial: SAVED revert is what a reboot loads");
        /* Retrying the edit after a failure also ends in exactly the edit. */
        write_calls = 0u; fail_write_at = k;
        (void)evist_pref_store_save(&store, &io, &edited);
        fail_write_at = 0u;
        check(evist_pref_store_save(&store, &io, &edited) == EVIST_PREF_SAVE_OK &&
              evist_preferences_load(&io, &loaded) && loaded.light_gain == 6u && loaded.light_dwell == 4u &&
              loaded.graph_scale_w == 1500u && loaded.assist_level == 2u, "partial: retried edit loads exactly");
    }
    /* Raw API with a stale `before`: never OK. */
    mem_reset(true);
    write_calls = 0u; fail_write_at = 2u;
    evist_preferences_t two = original; two.light_gain = 6u; two.light_dwell = 4u;
    check(evist_preferences_save(&io, &original, &two) == EVIST_PREF_SAVE_WRITE_FAILED, "raw: second record fails");
    fail_write_at = 0u;
    check(evist_preferences_save(&io, &original, &original) == EVIST_PREF_SAVE_VERIFY_FAILED,
          "raw: stale snapshot cannot report SAVED");
    /* A blank area read in full is a known, empty state: the first save
     * writes the header and the changed fields only. */
    mem_reset(false);
    evist_pref_store_t fresh; evist_pref_store_init(&fresh, &io, &loaded);
    write_calls = 0u;
    check(fresh.known && evist_pref_store_save(&fresh, &io, &edited) == EVIST_PREF_SAVE_OK && write_calls == 5u &&
          evist_preferences_load(&io, &loaded) && loaded.light_gain == 6u && loaded.assist_level == 2u,
          "blank memory: first save writes header + changed fields, reload equal");
    /* Foreign data without header: unknown, never written. */
    mem_reset(false); mem[0x100] = 0x00u;
    evist_pref_store_init(&fresh, &io, &loaded);
    write_calls = 0u;
    check(!fresh.known && evist_pref_store_save(&fresh, &io, &edited) == EVIST_PREF_SAVE_NO_MEMORY && write_calls == 0u,
          "foreign area: store unknown, NO MEMORY, nothing written");
}

/* ---- read faults, bounded save work --------------- */

static unsigned f_reads, f_fail_at, f_fail_from, f_writes;
static size_t f_read_bytes, f_max_read, f_write_bytes;
static uint32_t f_fail_addr; static size_t f_fail_len;
static int f_spill;            /* 0 none; else programs one more record after the next write */
static bool f_read(void *ctx, uint32_t addr, uint8_t *dst, size_t n) {
    ++f_reads; f_read_bytes += n; if (n > f_max_read) f_max_read = n;
    if ((f_fail_at != 0u && f_reads == f_fail_at) || (f_fail_from != 0u && f_reads >= f_fail_from)) {
        f_fail_addr = addr; f_fail_len = n; return false;
    }
    return m_read(ctx, addr, dst, n);
}
static bool f_write(void *ctx, uint32_t addr, const uint8_t *src, size_t n) {
    ++f_writes; f_write_bytes += n;
    if (!m_write(ctx, addr, src, n)) return false;
    if (f_spill) {
        /* A valid record of another value lands in the next slot as well. */
        uint8_t junk[2];
        if (addr >= EVIST_PREF_SET_BASE) { junk[0] = 0x89u; junk[1] = (uint8_t)(0x89u ^ 0xC7u); } /* light level 15 */
        else { junk[0] = 0x09u; junk[1] = (uint8_t)(0x09u ^ 0x5Au); }                             /* assist 9, TRIP+ODO */
        f_spill = 0;
        (void)m_write(ctx, addr + (uint32_t)n, junk, 2u);
    }
    return true;
}
/* Fault case: the 2nd record fails and from then on every read fails. */
static unsigned r2_writes;
static bool r2_write(void *ctx, uint32_t addr, const uint8_t *src, size_t n) {
    if (++r2_writes == 2u) { f_fail_from = f_reads + 1u; return false; }
    return f_write(ctx, addr, src, n);
}
static void f_reset(void) {
    f_reads = f_fail_at = f_fail_from = f_writes = 0u;
    f_read_bytes = f_max_read = f_write_bytes = 0u; f_spill = 0;
}
/* SPI bytes on the wire (dpc245_spi_mem.c): read = 0x0B + 3 address + dummy
 * + data; program = WREN + 0x02 + 3 address + data + one status poll (2). */
static size_t f_spi_bytes(void) { return f_read_bytes + f_reads * 5u + f_write_bytes + f_writes * 7u; }

static void put_code(uint32_t addr, uint8_t code) {
    uint8_t *r = mem + (addr - EVIST_PREF_REGION_BASE);
    r[0] = code; r[1] = (uint8_t)(code ^ 0xC7u);
}
static bool same_prefs(const evist_preferences_t *a, const evist_preferences_t *b) {
    return a->assist_level == b->assist_level && a->lower_panel == b->lower_panel &&
           a->firmware_profile == b->firmware_profile && a->graph_scale_w == b->graph_scale_w &&
           a->battery_mask == b->battery_mask && a->light_mode == b->light_mode &&
           a->light_gain == b->light_gain && a->light_dwell == b->light_dwell &&
           a->light_threshold == b->light_threshold && a->light_level == b->light_level &&
           a->panel_mask == b->panel_mask && a->quick_walk_enabled == b->quick_walk_enabled &&
           a->quick_walk_hold_steps == b->quick_walk_hold_steps && a->temp_view == b->temp_view;
}
/* With healthy IO: a known snapshot is exactly what a fresh load returns. */
static bool store_truthful(const evist_pref_store_t *s, const dpc245_persist_io_t *io) {
    evist_preferences_t l;
    if (!s->known) return true;
    return evist_preferences_load(io, &l) && same_prefs(&l, &s->persisted);
}

/* Values in all three logs: settings, read-only settings log, pairs. */
static void populated_memory(void) {
    mem_reset(true);
    uint32_t a = EVIST_PREF_SET_BASE;
    put_code(a, 0x06u); a += 2u;                       /* older gain 2 */
    put_code(a, 0x0Bu); a += 2u;                       /* gain 7 */
    put_code(a, 0x19u); a += 2u;                       /* dwell 10 */
    put_code(a, 0x32u); a += 2u;                       /* panel 2 hidden */
    put_code(a, 0xBCu);                                /* graph 1500 W */
    put_code(0x001FFF00u, 0x6Fu);                      /* read-only log: threshold 4000 */
    put_code(0x001FFF02u, 0x83u);                      /* read-only log: level 9 */
    put_pair(0u, 0u, 1u);
    put_pair(1u, 3u, 4u);                              /* assist 4, PEDAL+MOTOR */
}

static void test_read_faults(void) {
    dpc245_persist_io_t io = {f_read, f_write, NULL, NULL};
    evist_preferences_t p, want;
    evist_pref_store_t s;
    static uint8_t snapshot[sizeof mem];

    populated_memory();
    f_reset();
    check(evist_preferences_load(&io, &want) && want.light_gain == 7u && want.light_dwell == 10u &&
          want.panel_mask == (((1u << EVIST_BASE_PANEL_COUNT) - 1u) & ~(1u << 2)) && want.graph_scale_w == 1500u &&
          want.light_threshold == 4000u && want.light_level == 9u && want.assist_level == 4u &&
          want.lower_panel == 6u, "fault fixture: all three logs load");
    const unsigned load_reads = f_reads;
    memcpy(snapshot, mem, sizeof mem);

    /* Every single read of a full load fails once in turn: never a load. */
    unsigned hit_header = 0u, hit_set_end = 0u, hit_set_scan = 0u, hit_ro = 0u, hit_pair = 0u, wrong = 0u;
    for (unsigned k = 1u; k <= load_reads; ++k) {
        f_reset(); f_fail_at = k;
        if (evist_preferences_load(&io, &p)) ++wrong;
        if (f_fail_addr == EVIST_PREF_REGION_BASE) ++hit_header;
        else if (f_fail_addr >= 0x001FFF00u) ++hit_ro;
        else if (f_fail_addr >= EVIST_PREF_SET_BASE) { if (f_fail_len == 2u) ++hit_set_end; else ++hit_set_scan; }
        else ++hit_pair;
        f_reset(); f_fail_at = k;
        evist_pref_store_init(&s, &io, &p);
        if (s.known) ++wrong;
    }
    check(wrong == 0u, "read fault anywhere in a load: load fails, store unknown");
    check(hit_header && hit_set_end && hit_set_scan && hit_ro && hit_pair,
          "read faults covered header, log-end search, record scan, read-only log and pair log");

    /* Every read of a save fails once (then IO works) or from then on. */
    evist_preferences_t edits[3];
    edits[0] = want; edits[0].light_gain = 3u;                       /* settings record */
    edits[1] = want; edits[1].assist_level = 5u;                     /* pair record */
    edits[2] = want; edits[2].light_gain = 3u; edits[2].lower_panel = 1u; edits[2].panel_mask |= 1u << 2;
    for (unsigned e = 0u; e < 3u; ++e) {
        memcpy(mem, snapshot, sizeof mem);
        f_reset(); evist_pref_store_init(&s, &io, &p);
        f_reset();
        check(evist_pref_store_save(&s, &io, &edits[e]) == EVIST_PREF_SAVE_OK, "fault sweep: healthy save");
        const unsigned save_reads = f_reads;
        unsigned saved = 0u, untruthful = 0u, known = 0u, stuck = 0u;
        for (unsigned mode = 0u; mode < 2u; ++mode) {
            for (unsigned k = 1u; k <= save_reads; ++k) {
                memcpy(mem, snapshot, sizeof mem);
                f_reset(); evist_pref_store_init(&s, &io, &p);
                f_reset();
                if (mode == 0u) f_fail_at = k; else f_fail_from = k;
                if (evist_pref_store_save(&s, &io, &edits[e]) == EVIST_PREF_SAVE_OK) ++saved;
                if (mode == 1u && s.known) ++known;
                f_reset();
                if (!store_truthful(&s, &io)) ++untruthful;
                /* IO works again: one SAVE reaches the edit, another reverts it. */
                if (evist_pref_store_save(&s, &io, &edits[e]) != EVIST_PREF_SAVE_OK ||
                    !evist_preferences_load(&io, &p) || !same_prefs(&p, &edits[e]) ||
                    evist_pref_store_save(&s, &io, &want) != EVIST_PREF_SAVE_OK ||
                    !evist_preferences_load(&io, &p) || !same_prefs(&p, &want)) ++stuck;
            }
        }
        check(saved == 0u, "read fault during SAVE: never SAVED");
        check(known == 0u, "unreadable memory after a failed SAVE: snapshot unknown");
        check(untruthful == 0u, "after a failed SAVE a known snapshot equals a fresh load");
        check(stuck == 0u, "IO back: one SAVE applies the edit, one SAVE reverts it, reload equal");
    }

    /* Fault case: partial write, then the settings log unreadable. */
    memcpy(mem, snapshot, sizeof mem);
    f_reset(); evist_pref_store_init(&s, &io, &p);
    evist_preferences_t two = want; two.light_gain = 6u; two.light_dwell = 4u;
    dpc245_persist_io_t partial = {f_read, r2_write, NULL, NULL};
    f_reset(); r2_writes = 0u;
    check(evist_pref_store_save(&s, &partial, &two) == EVIST_PREF_SAVE_WRITE_FAILED, "second record fails");
    check(!s.known, "unreadable after partial write -> snapshot unknown");
    check(evist_pref_store_save(&s, &io, &want) == EVIST_PREF_SAVE_NO_MEMORY, "revert while unreadable -> NO MEMORY, not SAVED");
    f_reset();
    check(evist_preferences_load(&io, &p) && p.light_gain == 6u, "the partial gain=6 is really stored");
    check(evist_pref_store_save(&s, &io, &want) == EVIST_PREF_SAVE_OK && evist_preferences_load(&io, &p) && same_prefs(&p, &want),
          "IO back -> one SAVE reverts, reload equal");

    /* A record appended behind the snapshot's back is noticed before writing. */
    memcpy(mem, snapshot, sizeof mem);
    f_reset(); evist_pref_store_init(&s, &io, &p);
    put_code(s.image.set_end, 0x89u);                 /* light level 15, not in the snapshot */
    f_reset();
    check(evist_pref_store_save(&s, &io, &edits[0]) == EVIST_PREF_SAVE_VERIFY_FAILED && store_truthful(&s, &io),
          "memory changed behind the snapshot -> VERIFY FAILED, re-learned");
    check(evist_pref_store_save(&s, &io, &edits[0]) == EVIST_PREF_SAVE_OK && evist_preferences_load(&io, &p) &&
          same_prefs(&p, &edits[0]), "after re-learning the same SAVE succeeds");

    /* Every PANELS record says "hidden": the load shows all panels (never
     * empty). Hiding one panel must write the stored bits, not the shown ones. */
    mem_reset(true);
    for (uint32_t i = 0u; i < EVIST_BASE_PANEL_COUNT; ++i) put_code(EVIST_PREF_SET_BASE + i * 2u, (uint8_t)(0x2Eu + i * 2u));
    f_reset(); evist_pref_store_init(&s, &io, &p);
    check(s.known && p.panel_mask == ((1u << EVIST_BASE_PANEL_COUNT) - 1u), "all-hidden records load with base panels");
    evist_preferences_t one_hidden = p; one_hidden.panel_mask &= (uint16_t)~1u;
    check(evist_pref_store_save(&s, &io, &one_hidden) == EVIST_PREF_SAVE_OK && evist_preferences_load(&io, &p) &&
          p.panel_mask == one_hidden.panel_mask, "all-hidden records: hiding panel 0 saves exactly");

    /* A record spilling into the next slot is caught by the end search. */
    for (unsigned e = 0u; e < 2u; ++e) {
        memcpy(mem, snapshot, sizeof mem);
        f_reset(); evist_pref_store_init(&s, &io, &p);
        f_reset(); f_spill = 1;
        check(evist_pref_store_save(&s, &io, &edits[e]) == EVIST_PREF_SAVE_VERIFY_FAILED, "spilled record -> VERIFY FAILED");
        f_reset();
        check(store_truthful(&s, &io), "spill: snapshot re-learned");
    }
}

/* Work of one SAVE, counted at the IO callbacks. */
#define SAVE_SPI_BUDGET 1024u     /* bytes; 8.7 ms at 937.5 kbit/s (dpc245_spi_mem.c) */
#define LOAD_SPI_BUDGET 9500u     /* full load of a full log; 81 ms */

static void test_save_workload(void) {
    dpc245_persist_io_t io = {f_read, f_write, NULL, NULL};
    evist_pref_store_t s;
    evist_preferences_t base, after;
    const char *logs[3] = {"empty", "long", "full"};
    size_t worst = 0u;
    bool bounded = true, small_reads = true;
    for (unsigned l = 0u; l < 3u; ++l) {
        mem_reset(true);
        const uint32_t records = l == 0u ? 0u : l == 1u ? 2000u : (EVIST_PREF_SET_END - EVIST_PREF_SET_BASE) / 2u;
        for (uint32_t i = 0u; i < records; ++i)   /* gain 5, the default: other settings absent */
            put_code(EVIST_PREF_SET_BASE + i * 2u, 0x09u);
        for (uint32_t i = 0u; i < records * 4u; ++i) put_pair(i, 0u, (uint8_t)(i % 3u));
        f_reset(); evist_pref_store_init(&s, &io, &base);
        printf("workload %-5s log: start-up load %u reads, %zu SPI bytes\n", logs[l], f_reads, f_spi_bytes());
        check(s.known && f_spi_bytes() <= LOAD_SPI_BUDGET, "full load stays within its budget");
        for (unsigned c = 0u; c < 5u; ++c) {
            after = s.persisted;
            const char *what = "unchanged";
            if (c == 1u) { after.assist_level = (uint8_t)((after.assist_level + 1u) % 10u); what = "assist"; }
            if (c == 2u) { after.lower_panel = (uint8_t)((after.lower_panel + 1u) % 9u); what = "panel"; }
            if (c == 3u) { after.light_dwell = (uint8_t)(after.light_dwell % 30u + 1u); what = "setting"; }
            if (c == 4u) {                                        /* every field at once */
                after.firmware_profile = EVIST_PROFILE_FAKE_TAXI; after.graph_scale_w = 2000u;
                after.battery_mask = EVIST_BATTERY_SOC; after.light_mode = EVIST_PREF_LIGHT_MANUAL;
                after.light_gain = 9u; after.light_dwell = 20u; after.light_threshold = 4500u;
                after.light_level = 7u; after.panel_mask = 0x0155u; after.quick_walk_enabled = true;
                after.quick_walk_hold_steps = 9u; after.assist_level = 9u; after.lower_panel = 8u; what = "all";
            }
            f_reset();
            evist_pref_save_result_t r = evist_pref_store_save(&s, &io, &after);
            const size_t bytes = f_spi_bytes();
            printf("workload %-5s log: %-9s -> %-23s %3u reads %2u writes %4zu SPI bytes (%.1f ms)\n",
                   logs[l], what, evist_pref_save_text(r), f_reads, f_writes, bytes, (double)bytes * 8.0 / 937.5);
            const bool full_settings = l == 2u && (c == 3u || c == 4u);
            check(full_settings ? r == EVIST_PREF_SAVE_LOG_FULL : r == EVIST_PREF_SAVE_OK, "workload: expected result");
            if (bytes > worst) worst = bytes;
            if (bytes > SAVE_SPI_BUDGET || f_writes > 20u) bounded = false;
            if (f_max_read > 8u) small_reads = false;
        }
    }
    printf("workload: worst SAVE %zu SPI bytes = %.1f ms (budget %u B)\n", worst, (double)worst * 8.0 / 937.5, SAVE_SPI_BUDGET);
    check(bounded, "every SAVE within the SPI budget and at most 20 records");
    check(small_reads, "SAVE never scans a log (reads of at most 8 bytes)");
}

/* Random saves under random faults: SAVED always survives a reload, and a
 * known snapshot always equals a fresh load. */
static uint32_t rng = 0x2545F491u;
static uint32_t rnd(uint32_t n) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng % n; }

static void test_store_property(void) {
    dpc245_persist_io_t io = {f_read, f_write, NULL, NULL};
    evist_pref_store_t s;
    evist_preferences_t p, after;
    unsigned false_saved = 0u, untruthful = 0u, ok = 0u;
    populated_memory();
    f_reset(); evist_pref_store_init(&s, &io, &p);
    for (unsigned it = 0u; it < 600u; ++it) {
        after = s.known ? s.persisted : p;
        switch (rnd(6u)) {
        case 0: after.assist_level = (uint8_t)rnd(10u); break;
        case 1: after.lower_panel = (uint8_t)rnd(9u); break;
        case 2: after.light_gain = (uint8_t)(1u + rnd(9u)); break;
        case 3: after.panel_mask = (uint16_t)(1u + rnd(EVIST_PANEL_MASK_ALL)); break;
        case 4: after.graph_scale_w = (uint16_t)(250u + 25u * rnd(111u)); break;
        default: after.quick_walk_hold_steps = (uint8_t)(1u + rnd(10u)); break;
        }
        f_reset();
        switch (rnd(8u)) {
        case 0: f_fail_at = 1u + rnd(40u); break;
        case 1: f_fail_from = 1u + rnd(40u); break;
        case 2: f_spill = 1; break;
        case 3: write_calls = 0u; fail_write_at = 1u + rnd(3u); break;
        default: break;
        }
        dpc245_persist_io_t used = io;
        if (fail_write_at != 0u) used.write = counted_write;
        evist_pref_save_result_t r = evist_pref_store_save(&s, &used, &after);
        fail_write_at = 0u;
        f_reset();
        if (r == EVIST_PREF_SAVE_OK) {
            ++ok;
            if (!evist_preferences_load(&io, &p) || !same_prefs(&p, &after)) ++false_saved;
        }
        if (!store_truthful(&s, &io)) ++untruthful;
        p = after;
    }
    printf("store property: 600 saves, %u SAVED\n", ok);
    check(ok > 200u, "property: most saves succeed");
    check(false_saved == 0u, "property: every SAVED is what a reload returns");
    check(untruthful == 0u, "property: a known snapshot always equals a fresh load");
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    test_assist_profile();
    test_wheel_write();
    test_assist_write();
    test_controller_revisions();
    test_router_event();
    test_advanced_buffers();
    test_preferences();
    test_stale_reassembly();
    test_accel_read();
    test_deadlines();
    test_partial_save();
    test_read_faults();
    test_save_workload();
    test_store_property();
    if (failures) { printf("config tests: %u failure(s)\n", failures); return 1; }
    printf("all config tests passed\n");
    return 0;
}
