/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_v3_profile.h"

/* Periodic HMI CAN transmit schedule. frame_slot indexes the DPC245 V3
 * periodic status-frame set; the values are listed with each frame below.
 *
 * 03106300, 4 bytes, 100 ms
 * 03106304, 4 bytes, 200 ms
 * 03106301, 8 bytes, 200 ms
 * 03106302, 5 bytes, 200 ms
 * 03106303, 1 byte, 1000 ms
 * 03986207, 1 byte, 1000 ms (listed in the table, not transmitted)
 *
 * BLE telemetry runs on a separate 500 ms task;
 * it is not a CAN periodic frame. */
const dpc245_v3_periodic_can_t dpc245_v3_periodic_can[] = {
    {0x03106300u,  100u, 4u, 1u, true},
    {0x03106304u,  200u, 4u, 2u, true},
    {0x03106301u,  200u, 8u, 5u, true},
    {0x03106302u,  200u, 5u, 6u, true},
    {0x03106303u, 1000u, 1u, 4u, true},
    {0x03986207u, 1000u, 1u, 7u, false},
};

const size_t dpc245_v3_periodic_can_count =
    sizeof(dpc245_v3_periodic_can) / sizeof(dpc245_v3_periodic_can[0]);

void dpc245_v3_tx_schedule_init(dpc245_v3_tx_schedule_t *s, uint32_t now_ms) {
    if (!s) return;
    s->started = true;
    s->last_job_ms = now_ms;
    for (size_t i = 0; i < DPC245_V3_PERIODIC_MAX; ++i) s->last_tx_ms[i] = now_ms;
}

uint32_t dpc245_v3_tx_schedule_poll(dpc245_v3_tx_schedule_t *s, uint32_t now_ms) {
    if (!s || !s->started) return 0u;
    if ((uint32_t)(now_ms - s->last_job_ms) < DPC245_V3_TX_JOB_MS) return 0u;
    s->last_job_ms = now_ms;
    uint32_t due = 0u;
    for (size_t i = 0; i < dpc245_v3_periodic_can_count && i < DPC245_V3_PERIODIC_MAX; ++i) {
        const dpc245_v3_periodic_can_t *p = &dpc245_v3_periodic_can[i];
        if (!p->transmitted) continue;
        if ((uint32_t)(now_ms - s->last_tx_ms[i]) >= p->period_ms) {
            s->last_tx_ms[i] = now_ms;
            due |= 1u << i;
        }
    }
    return due;
}

const dpc245_v3_periodic_can_t *dpc245_v3_periodic_find(uint32_t can_id) {
    for (size_t i = 0; i < dpc245_v3_periodic_can_count; ++i) {
        if (dpc245_v3_periodic_can[i].can_id == can_id) return &dpc245_v3_periodic_can[i];
    }
    return NULL;
}

static void frame_clear(bafang_can_frame_t *out, uint32_t id, uint8_t dlc) {
    out->id = id;
    out->dlc = dlc;
    for (uint8_t i = 0; i < BAFANG_CAN_MAX_DLC; ++i) out->data[i] = 0u;
}

static void put_u24_le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
}

bool dpc245_v3_build_status_frame_4(bafang_can_frame_t *out,
                                  const dpc245_v3_hmi_stats_raw_t *s) {
    if (!out || !s) return false;
    frame_clear(out, 0x03106304u, 4u);
    out->data[0] = 0x05u;
    out->data[1] = s->raw0a;
    out->data[2] = 0x04u;
    out->data[3] = s->raw06;
    return true;
}

bool dpc245_v3_build_status_frame_1(bafang_can_frame_t *out,
                                  const dpc245_v3_hmi_stats_raw_t *s) {
    if (!out || !s) return false;
    frame_clear(out, 0x03106301u, 8u);
    const uint32_t a = (s->raw10 + 50u) / 100u;
    const uint32_t b = s->raw14 / 10u;
    put_u24_le(&out->data[0], a);
    put_u24_le(&out->data[3], b);
    out->data[6] = (uint8_t)s->raw2f;
    out->data[7] = (uint8_t)(s->raw2f >> 8);
    return true;
}

bool dpc245_v3_build_status_frame_2(bafang_can_frame_t *out,
                                  const dpc245_v3_hmi_stats_raw_t *s) {
    if (!out || !s) return false;
    frame_clear(out, 0x03106302u, 5u);
    out->data[0] = (uint8_t)s->raw2c;
    out->data[1] = (uint8_t)(s->raw2c >> 8);
    put_u24_le(&out->data[2], s->raw18 / 10u);
    return true;
}

bool dpc245_v3_build_status_frame_3(bafang_can_frame_t *out,
                                  const dpc245_v3_hmi_stats_raw_t *s) {
    if (!out || !s) return false;
    frame_clear(out, 0x03106303u, 1u);
    out->data[0] = s->raw07 ? s->raw07 : 0xFFu;
    return true;
}
