/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "controller_write.h"
#include "assist_profile.h"
#include "app_config.h"
#include <stddef.h>

#define WHEEL_ACK_MS          600u
#define WHEEL_SETTLE_MS       400u
#define WHEEL_VERIFY_READ_MS  600u
#define WHEEL_READ_MS         220u
#define WHEEL_READ_ALT_MS     280u

#define ASSIST_FRAME_COUNT      9u
#define ASSIST_TX_TIMEOUT_MS  334u
#define ASSIST_SETTLE_MS      500u
#define ASSIST_READ_MS        450u
#define ASSIST_READ_GAP_MS    150u
#define ASSIST_READ_ATTEMPTS    3u
#define ACCEL_DISCOVERY_MS    600u
#define ACCEL_DISCOVERY_DRAIN_MS 300u
#define ACCEL_BLOCK_READ_MS  1050u

#define REQUESTER BAFANG_NODE_HMI

static bool expired(uint32_t now, uint32_t due) { return (int32_t)(now - due) >= 0; }

static void copy_bytes(uint8_t *d, const uint8_t *s, uint8_t n) {
    for (uint8_t i = 0u; i < n; ++i) d[i] = s[i];
}

static bool same_bytes(const uint8_t *a, const uint8_t *b, uint8_t n) {
    for (uint8_t i = 0u; i < n; ++i) if (a[i] != b[i]) return false;
    return true;
}

static bool is_wheel(const evist_ctrl_txn_t *t) {
    return t->kind == EVIST_TXN_READ_WHEEL || t->kind == EVIST_TXN_WRITE_WHEEL;
}

static bool is_start_angle(const evist_ctrl_txn_t *t) {
    return t->kind == EVIST_TXN_READ_START_ANGLE;
}

static bool is_accel_block(const evist_ctrl_txn_t *t) {
    return t->kind == EVIST_TXN_READ_ACCEL_P0 || t->kind == EVIST_TXN_READ_ACCEL_P2 ||
           t->kind == EVIST_TXN_WRITE_APP_P0 || t->kind == EVIST_TXN_WRITE_APP_P2;
}

static bool is_app_write(const evist_ctrl_txn_t *t) {
    return t->kind == EVIST_TXN_WRITE_APP_P1 || t->kind == EVIST_TXN_WRITE_APP_P0 ||
           t->kind == EVIST_TXN_WRITE_APP_P2;
}

static bool is_block_write(const evist_ctrl_txn_t *t) {
    return t->kind == EVIST_TXN_WRITE_ASSIST || is_app_write(t);
}

static uint8_t read_sub(const evist_ctrl_txn_t *t) {
    if (is_start_angle(t)) return 0xD9u;
    if (t->kind == EVIST_TXN_READ_ACCEL_P0 || t->kind == EVIST_TXN_WRITE_APP_P0) return 0x10u;
    if (t->kind == EVIST_TXN_READ_ACCEL_P2 || t->kind == EVIST_TXN_WRITE_APP_P2) return 0x12u;
    return 0x11u;
}

static bool is_write(const evist_ctrl_txn_t *t) {
    return t->kind == EVIST_TXN_WRITE_WHEEL || t->kind == EVIST_TXN_WRITE_ASSIST || is_app_write(t);
}

static uint8_t payload_len(const evist_ctrl_txn_t *t) {
    return is_start_angle(t) ? 2u : is_wheel(t) ? EVIST_WHEEL_PAYLOAD_LEN : EVIST_ASSIST_BLOCK_LEN;
}

static void finish(evist_ctrl_txn_t *t, evist_txn_result_t result) {
    t->result = result;
    t->step = EVIST_TXN_STEP_DONE;
}

static void begin(evist_ctrl_txn_t *t, evist_txn_kind_t kind, uint32_t now) {
    t->kind = kind;
    t->frame_index = 0u;
    t->write_attempts = 0u;
    t->read_attempts = 0u;
    t->ack_seen = false;
    t->ack_count = 0u;
    t->final_ack_seen = false;
    t->error_seen = false;
    t->error_len = 0u;
    t->read_error_seen = false;
    t->final_frame_ms = 0u;
    t->alternate_read = false;
    t->block_start_seen = false;
    t->p0_read_flag = 0u;
    t->due_ms = is_write(t) && !is_wheel(t) ? now + ASSIST_TX_TIMEOUT_MS : now;
    t->step = is_write(t)
            ? EVIST_TXN_STEP_SEND_WRITE : EVIST_TXN_STEP_SEND_READ;
}

void evist_wheel_config_encode(const evist_wheel_config_t *c, uint8_t out[EVIST_WHEEL_PAYLOAD_LEN]) {
    if (!c || !out) return;
    out[0] = (uint8_t)c->speed_limit_centi_kph; out[1] = (uint8_t)(c->speed_limit_centi_kph >> 8);
    out[2] = (uint8_t)c->wheel_code;            out[3] = (uint8_t)(c->wheel_code >> 8);
    out[4] = (uint8_t)c->circumference_mm;      out[5] = (uint8_t)(c->circumference_mm >> 8);
}

void evist_wheel_config_decode(const uint8_t in[EVIST_WHEEL_PAYLOAD_LEN], evist_wheel_config_t *c) {
    if (!in || !c) return;
    c->speed_limit_centi_kph = (uint16_t)(in[0] | (in[1] << 8));
    c->wheel_code = (uint16_t)(in[2] | (in[3] << 8));
    c->circumference_mm = (uint16_t)(in[4] | (in[5] << 8));
}

void evist_ctrl_txn_init(evist_ctrl_txn_t *t) {
    if (!t) return;
    t->kind = EVIST_TXN_READ_WHEEL;
    t->step = EVIST_TXN_STEP_IDLE;
    t->result = EVIST_TXN_OK;
    t->frame_index = t->write_attempts = t->read_attempts = 0u;
    t->ack_seen = t->alternate_read = t->block_start_seen = false;
    t->ack_count = t->error_len = 0u;
    t->final_ack_seen = t->error_seen = t->read_error_seen = false;
    t->final_frame_ms = 0u;
    t->due_ms = 0u;
    for (uint8_t i = 0u; i < 64u; ++i) t->expected[i] = t->readback[i] = 0u;
}

bool evist_ctrl_txn_busy(const evist_ctrl_txn_t *t) {
    return t && t->step != EVIST_TXN_STEP_IDLE && t->step != EVIST_TXN_STEP_DONE;
}

static bool can_start(const evist_ctrl_txn_t *t) {
    return t && !evist_ctrl_txn_busy(t);
}

bool evist_ctrl_txn_read_wheel(evist_ctrl_txn_t *t, uint32_t now) {
    if (!can_start(t)) return false;
    begin(t, EVIST_TXN_READ_WHEEL, now);
    return true;
}

bool evist_ctrl_txn_write_wheel(evist_ctrl_txn_t *t, const evist_wheel_config_t *c, uint32_t now) {
    if (!can_start(t) || !c) return false;
    evist_wheel_config_encode(c, t->expected);
    begin(t, EVIST_TXN_WRITE_WHEEL, now);
    return true;
}

bool evist_ctrl_txn_read_assist(evist_ctrl_txn_t *t, uint32_t now) {
    if (!can_start(t)) return false;
    begin(t, EVIST_TXN_READ_ASSIST, now);
    return true;
}

bool evist_ctrl_txn_discover_accel(evist_ctrl_txn_t *t, uint32_t now) {
    if (!can_start(t)) return false;
    begin(t, EVIST_TXN_DISCOVER_ACCEL, now);
    return true;
}

bool evist_ctrl_txn_read_accel_p0(evist_ctrl_txn_t *t, uint32_t now) {
    if (!can_start(t)) return false;
    begin(t, EVIST_TXN_READ_ACCEL_P0, now);
    return true;
}

bool evist_ctrl_txn_read_accel_p2(evist_ctrl_txn_t *t, uint32_t now) {
    if (!can_start(t)) return false;
    begin(t, EVIST_TXN_READ_ACCEL_P2, now);
    return true;
}

bool evist_ctrl_txn_read_start_angle(evist_ctrl_txn_t *t, uint32_t now) {
    if (!can_start(t)) return false;
    begin(t, EVIST_TXN_READ_START_ANGLE, now);
    return true;
}

bool evist_ctrl_txn_write_assist(evist_ctrl_txn_t *t, const uint8_t block[64],
                                 evist_firmware_profile_t profile, uint32_t now) {
    if (!can_start(t) || !evist_assist_write_allowed(block, profile)) return false;
    copy_bytes(t->expected, block, EVIST_ASSIST_BLOCK_LEN);
    begin(t, EVIST_TXN_WRITE_ASSIST, now);
    return true;
}

bool evist_ctrl_txn_write_app_p1(evist_ctrl_txn_t *t, const uint8_t block[64],
                                 evist_firmware_profile_t profile, uint32_t now) {
#if !EVIST_ENABLE_CONFIG_WRITES
    (void)t; (void)block; (void)profile; (void)now;
    return false;
#else
    if (!can_start(t) || !evist_assist_write_allowed(block, profile)) return false;
    copy_bytes(t->expected, block, 64u);
    begin(t, EVIST_TXN_WRITE_APP_P1, now);
    return true;
#endif
}

bool evist_ctrl_txn_write_app_p0(evist_ctrl_txn_t *t, const uint8_t block[64],
                                 uint8_t read_flag, uint32_t now) {
#if !EVIST_ENABLE_CONFIG_WRITES
    (void)t; (void)block; (void)read_flag; (void)now;
    return false;
#else
    if (!can_start(t) || !block) return false;
    for (uint8_t i = 1u; i <= 9u; ++i) if (block[i] < 1u || block[i] > 8u) return false;
    copy_bytes(t->expected, block, 64u);
    begin(t, EVIST_TXN_WRITE_APP_P0, now);
    t->p0_read_flag = read_flag;
    return true;
#endif
}

bool evist_ctrl_txn_write_app_p2(evist_ctrl_txn_t *t, const uint8_t block[64], uint32_t now) {
#if !EVIST_ENABLE_CONFIG_WRITES
    (void)t; (void)block; (void)now;
    return false;
#else
    if (!can_start(t) || !block || block[54] < 1u || block[54] > 8u ||
        !evist_assist_block_valid(block)) return false;
    copy_bytes(t->expected, block, 64u);
    begin(t, EVIST_TXN_WRITE_APP_P2, now);
    return true;
#endif
}

static uint32_t controller_id(uint8_t op, uint8_t cmd, uint8_t sub) {
    return bafang_can_id_encode((bafang_can_id_t){
        .src = REQUESTER, .dst = BAFANG_NODE_CONTROLLER, .type = op, .cmd = cmd, .sub = sub});
}

static void frame_clear(bafang_can_frame_t *f) {
    f->id = 0u;
    f->dlc = 0u;
    for (uint8_t i = 0u; i < 8u; ++i) f->data[i] = 0u;
}

/* Declaration, first data frame, six continuation frames, final frame. */
static void assist_write_frame(const evist_ctrl_txn_t *t, uint8_t index, bafang_can_frame_t *f) {
    if (index == 0u) {
        f->id = controller_id(BAFANG_CAN_OP_WRITE, 0x60u, read_sub(t));
        f->dlc = 1u;
        f->data[0] = EVIST_ASSIST_BLOCK_LEN;
        return;
    }
    uint8_t off = (uint8_t)((index - 1u) * 8u);
    if (index == 1u) f->id = controller_id(BAFANG_CAN_OP_MULTIFRAME_START, 0x60u, read_sub(t));
    else if (index < ASSIST_FRAME_COUNT - 1u) f->id = controller_id(BAFANG_CAN_OP_MULTIFRAME, 0u, (uint8_t)(index - 2u));
    else f->id = controller_id(BAFANG_CAN_OP_MULTIFRAME_END, 0u, 6u);
    f->dlc = 8u;
    copy_bytes(f->data, &t->expected[off], 8u);
}

static bool send_write(evist_ctrl_txn_t *t, uint32_t now, bafang_can_frame_t *f) {
    if (is_wheel(t)) {
        f->id = controller_id(BAFANG_CAN_OP_WRITE, 0x32u, 0x03u);
        f->dlc = EVIST_WHEEL_PAYLOAD_LEN;
        copy_bytes(f->data, t->expected, EVIST_WHEEL_PAYLOAD_LEN);
        t->ack_seen = false;
        t->step = EVIST_TXN_STEP_WAIT_ACK;
        t->due_ms = now + WHEEL_ACK_MS;
        return true;
    }
    assist_write_frame(t, t->frame_index, f);
    t->frame_index++;
    if (t->frame_index >= ASSIST_FRAME_COUNT) {
        t->step = EVIST_TXN_STEP_SETTLE;
        t->due_ms = now + ASSIST_SETTLE_MS;
        t->final_frame_ms = now;
    } else {
        t->due_ms = now + ASSIST_TX_TIMEOUT_MS;
    }
    return true;
}

static bool send_read(evist_ctrl_txn_t *t, uint32_t now, bafang_can_frame_t *f) {
    uint32_t window;
    if (is_wheel(t)) {
        uint8_t cmd = t->alternate_read ? 0xF2u : 0x32u;
        (void)bafang_build_read_query(f, BAFANG_NODE_CONTROLLER, cmd, 0x03u);
        if (t->kind == EVIST_TXN_WRITE_WHEEL) window = WHEEL_VERIFY_READ_MS;
        else window = t->alternate_read ? WHEEL_READ_ALT_MS : WHEEL_READ_MS;
    } else if (t->kind == EVIST_TXN_DISCOVER_ACCEL) {
        (void)bafang_build_read_query(f, BAFANG_NODE_CONTROLLER, 0xF2u, 0x03u);
        window = ACCEL_DISCOVERY_MS;
    } else if (is_start_angle(t)) {
        (void)bafang_build_read_query(f, BAFANG_NODE_CONTROLLER, 0x62u, 0xD9u);
        t->read_attempts++;
        window = 450u;
    } else {
        (void)bafang_build_read_query(f, BAFANG_NODE_CONTROLLER, 0x60u, read_sub(t));
        t->read_attempts++;
        t->block_start_seen = false;
        window = is_accel_block(t) ? ACCEL_BLOCK_READ_MS : ASSIST_READ_MS;
    }
    t->step = EVIST_TXN_STEP_WAIT_READ;
    t->read_error_seen = false;
    t->due_ms = now + window;
    return true;
}

bool evist_ctrl_txn_next_tx(evist_ctrl_txn_t *t, uint32_t now, bafang_can_frame_t *out) {
    if (!t || !out) return false;
    frame_clear(out);
    if (t->step == EVIST_TXN_STEP_SEND_WRITE) return send_write(t, now, out);
    if (t->step == EVIST_TXN_STEP_SEND_READ) return send_read(t, now, out);
    return false;
}

static void complete_read(evist_ctrl_txn_t *t, const uint8_t *data) {
    uint8_t n = payload_len(t);
    copy_bytes(t->readback, data, n);
    /* P0[63] is a flag in the independent-control contract, not a checksum. */
    if ((t->kind == EVIST_TXN_READ_ASSIST || t->kind == EVIST_TXN_WRITE_ASSIST ||
         t->kind == EVIST_TXN_WRITE_APP_P1 ||
         t->kind == EVIST_TXN_READ_ACCEL_P2 || t->kind == EVIST_TXN_WRITE_APP_P2) &&
        evist_assist_checksum(t->readback) != t->readback[EVIST_ASSIST_BLOCK_LEN - 1u]) {
        finish(t, EVIST_TXN_CHECKSUM_BAD);
        return;
    }
    if (t->kind == EVIST_TXN_WRITE_APP_P0) {
        /* Source-3 P0 ignores the write checksum and reads byte 63 as a flag. */
        if (!same_bytes(t->readback, t->expected, 63u) ||
            t->readback[63] != t->p0_read_flag) {
            finish(t, EVIST_TXN_MISMATCH);
            return;
        }
    } else if (is_write(t) && !same_bytes(t->readback, t->expected, n)) {
        finish(t, EVIST_TXN_MISMATCH);
        return;
    }
    finish(t, EVIST_TXN_OK);
}

static bool from_controller_to_us(const bafang_can_id_t *id) {
    return id->src == BAFANG_NODE_CONTROLLER &&
           (id->dst == BAFANG_NODE_HMI || id->dst == BAFANG_NODE_BESST);
}

static void record_error(evist_ctrl_txn_t *t, const bafang_can_frame_t *f) {
    t->error_seen = true;
    t->error_len = f->dlc > 4u ? 4u : f->dlc;
    for (uint8_t i = 0u; i < t->error_len; ++i) t->error_payload[i] = f->data[i];
}

static bool validator_reject(const bafang_can_frame_t *f) {
    return f->dlc == 4u && f->data[0] == 1u && f->data[1] == 0u &&
           f->data[2] == 4u && f->data[3] == 6u;
}

/* Receive windows: a deadline belongs to WAIT_ACK and WAIT_READ only. */
static bool window_closed(const evist_ctrl_txn_t *t, uint32_t now) {
    return (t->step == EVIST_TXN_STEP_WAIT_ACK || t->step == EVIST_TXN_STEP_WAIT_READ) &&
           expired(now, t->due_ms);
}

void evist_ctrl_txn_on_frame(evist_ctrl_txn_t *t, const bafang_can_frame_t *f, uint32_t now) {
    if (!evist_ctrl_txn_busy(t) || !f || window_closed(t, now)) return;
    bafang_can_id_t id = bafang_can_id_decode(f->id);
    if (!from_controller_to_us(&id)) return;

    if (is_start_angle(t)) {
        if (t->step == EVIST_TXN_STEP_WAIT_READ && id.dst == BAFANG_NODE_HMI &&
            id.cmd == 0x62u && id.sub == 0xD9u) {
            if (id.type == BAFANG_CAN_OP_ERROR_ACK) {
                t->read_error_seen = true;
                record_error(t, f);
            }
            else if (id.type == BAFANG_CAN_OP_NORMAL_ACK && f->dlc == 2u)
                complete_read(t, f->data);
        }
        return;
    }

    if (t->kind == EVIST_TXN_DISCOVER_ACCEL) {
        if (t->step == EVIST_TXN_STEP_WAIT_READ && id.dst == BAFANG_NODE_HMI &&
            id.type == BAFANG_CAN_OP_MULTIFRAME_START && id.cmd == 0xF2u && id.sub == 0x03u) {
            t->block_start_seen = true;
            /* Let the descriptor finish before issuing P0 on the same controller. */
            t->due_ms = now + ACCEL_DISCOVERY_DRAIN_MS;
        }
        return;
    }

    if (is_wheel(t)) {
        uint8_t expected_cmd = t->step == EVIST_TXN_STEP_WAIT_READ && t->alternate_read ? 0xF2u : 0x32u;
        bool wheel_field = id.dst == BAFANG_NODE_HMI && id.sub == 0x03u &&
                           id.cmd == expected_cmd;
        if (!wheel_field) return;
        if (id.type == BAFANG_CAN_OP_ERROR_ACK &&
            (t->step == EVIST_TXN_STEP_WAIT_ACK || t->step == EVIST_TXN_STEP_WAIT_READ)) {
            record_error(t, f);
            if (t->step == EVIST_TXN_STEP_WAIT_READ) t->read_error_seen = true;
            else if (validator_reject(f)) finish(t, EVIST_TXN_REJECTED);
            return;
        }
        if (t->step == EVIST_TXN_STEP_WAIT_ACK && id.type == BAFANG_CAN_OP_NORMAL_ACK &&
            id.cmd == 0x32u && f->dlc == 0u) {
            t->ack_seen = true;
            t->ack_count = 1u;
            t->final_ack_seen = true;
        } else if (t->step == EVIST_TXN_STEP_WAIT_READ &&
                   id.type == BAFANG_CAN_OP_NORMAL_ACK && f->dlc == EVIST_WHEEL_PAYLOAD_LEN) {
            complete_read(t, f->data);
        }
        return;
    }
    if (t->step == EVIST_TXN_STEP_WAIT_READ && id.dst == BAFANG_NODE_HMI &&
        id.type == BAFANG_CAN_OP_ERROR_ACK && id.cmd == 0x60u &&
        id.sub == read_sub(t)) {
        t->read_error_seen = true;
        record_error(t, f);
        return;
    }
    if (t->step == EVIST_TXN_STEP_WAIT_READ && id.dst == BAFANG_NODE_HMI &&
        id.type == BAFANG_CAN_OP_MULTIFRAME_START && id.cmd == 0x60u && id.sub == read_sub(t) &&
        (!is_accel_block(t) || (f->dlc == 1u && f->data[0] == 64u))) {
        t->block_start_seen = true;
        return;
    }
    if (is_block_write(t) && (t->step == EVIST_TXN_STEP_SEND_WRITE ||
                              t->step == EVIST_TXN_STEP_SETTLE) &&
        id.dst == BAFANG_NODE_HMI &&
        ((id.cmd == 0x60u && id.sub == read_sub(t)) ||
         (id.cmd == 0u && t->frame_index >= 3u &&
          id.sub <= (uint8_t)(t->frame_index - 3u)))) {
        if (id.type == BAFANG_CAN_OP_NORMAL_ACK && f->dlc == 0u && id.cmd == 0x60u) {
            if (t->ack_count < 255u) ++t->ack_count;
            if (t->ack_count >= 10u ||
                (t->step == EVIST_TXN_STEP_SETTLE &&
                 (uint32_t)(now - t->final_frame_ms) >= 10u))
                t->final_ack_seen = true;
        } else if (id.type == BAFANG_CAN_OP_ERROR_ACK) {
            record_error(t, f);
            if (validator_reject(f)) finish(t, EVIST_TXN_REJECTED);
        }
        return;
    }
}

void evist_ctrl_txn_on_service(evist_ctrl_txn_t *t, const bafang_service_message_t *m, uint32_t now) {
    if (!evist_ctrl_txn_busy(t) || !m || is_wheel(t) || t->step != EVIST_TXN_STEP_WAIT_READ) return;
    if (t->kind == EVIST_TXN_DISCOVER_ACCEL) {
        if (t->block_start_seen && !window_closed(t, now) && !m->error &&
            m->src == BAFANG_NODE_CONTROLLER && m->dst == BAFANG_NODE_HMI &&
            m->cmd == 0xF2u && m->sub == 0x03u) finish(t, EVIST_TXN_OK);
        return;
    }
    if (window_closed(t, now) || !t->block_start_seen) return;
    if (m->error || m->src != BAFANG_NODE_CONTROLLER || m->cmd != 0x60u || m->sub != read_sub(t)) return;
    if (m->dst != BAFANG_NODE_HMI && m->dst != BAFANG_NODE_BESST) return;
    if (is_app_write(t) && m->dst != BAFANG_NODE_HMI) return;
    if (m->len != EVIST_ASSIST_BLOCK_LEN) return;
    complete_read(t, m->data);
}

void evist_ctrl_txn_tick(evist_ctrl_txn_t *t, uint32_t now) {
    if (!evist_ctrl_txn_busy(t)) return;
    switch (t->step) {
    case EVIST_TXN_STEP_SEND_WRITE:
        if (is_block_write(t) && expired(now, t->due_ms)) finish(t, EVIST_TXN_TX_TIMEOUT);
        break;
    case EVIST_TXN_STEP_WAIT_ACK:
        if (t->ack_seen) {
            t->step = EVIST_TXN_STEP_SETTLE;
            t->due_ms = now + WHEEL_SETTLE_MS;
        } else if (expired(now, t->due_ms)) {
            if (t->error_seen) {
                t->step = EVIST_TXN_STEP_SETTLE;
                t->due_ms = now + WHEEL_SETTLE_MS;
            } else {
                t->write_attempts++;
                finish(t, EVIST_TXN_NO_ACK);
            }
        }
        break;
    case EVIST_TXN_STEP_SETTLE:
    case EVIST_TXN_STEP_READ_GAP:
        if (expired(now, t->due_ms)) t->step = EVIST_TXN_STEP_SEND_READ;
        break;
    case EVIST_TXN_STEP_WAIT_READ:
        if (!expired(now, t->due_ms)) break;
        if (t->kind == EVIST_TXN_READ_WHEEL && !t->alternate_read) {
            t->alternate_read = true;
            t->step = EVIST_TXN_STEP_SEND_READ;
        } else if (t->kind == EVIST_TXN_DISCOVER_ACCEL) {
            finish(t, t->block_start_seen ? EVIST_TXN_OK : EVIST_TXN_NO_READBACK);
        } else if (is_wheel(t)) {
            finish(t, t->read_error_seen ? EVIST_TXN_NACK : EVIST_TXN_NO_READBACK);
        } else if (is_accel_block(t)) {
            finish(t, t->read_error_seen ? EVIST_TXN_NACK :
                      is_block_write(t) && t->ack_count == 0u ? EVIST_TXN_NO_ACK : EVIST_TXN_NO_READBACK);
        } else if (!is_wheel(t) && t->read_attempts < ASSIST_READ_ATTEMPTS) {
            t->step = EVIST_TXN_STEP_READ_GAP;
            t->due_ms = now + ASSIST_READ_GAP_MS;
        } else {
            finish(t, t->read_error_seen ? EVIST_TXN_NACK :
                      is_block_write(t) && t->ack_count == 0u ? EVIST_TXN_NO_ACK : EVIST_TXN_NO_READBACK);
        }
        break;
    default:
        break;
    }
}

bool evist_ctrl_txn_take_result(evist_ctrl_txn_t *t, evist_txn_kind_t *kind,
                                evist_txn_result_t *result, uint8_t data[64]) {
    if (!t || t->step != EVIST_TXN_STEP_DONE) return false;
    if (kind) *kind = t->kind;
    if (result) *result = t->result;
    if (data && t->kind != EVIST_TXN_DISCOVER_ACCEL &&
        (t->result == EVIST_TXN_OK || t->result == EVIST_TXN_MISMATCH))
        copy_bytes(data, t->readback, payload_len(t));
    t->step = EVIST_TXN_STEP_IDLE;
    return true;
}

const char *evist_txn_result_text(evist_txn_result_t result) {
    switch (result) {
    case EVIST_TXN_OK:           return "VERIFY OK";
    case EVIST_TXN_NO_ACK:       return "NO CONTROLLER ACK";
    case EVIST_TXN_TX_TIMEOUT:   return "WRITE TX TIMEOUT";
    case EVIST_TXN_NO_READBACK:  return "NO READBACK";
    case EVIST_TXN_CHECKSUM_BAD: return "CHECKSUM BAD";
    case EVIST_TXN_MISMATCH:     return "VERIFY MISMATCH";
    case EVIST_TXN_NACK:        return "CONTROLLER NACK";
    case EVIST_TXN_REJECTED:    return "CONTROLLER REJECTED";
    default:                     return "UNKNOWN";
    }
}
