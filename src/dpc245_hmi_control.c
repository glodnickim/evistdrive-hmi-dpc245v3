/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_hmi_control.h"
#include "bafang_service.h"
#include <stddef.h>
void *memset(void *s, int c, size_t n);

static uint32_t u24le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

static void make_ack(const bafang_can_id_t *q, const bafang_can_frame_t *request,
                     bafang_can_frame_t *reply, bool echo_payload) {
    memset(reply, 0, sizeof(*reply));
    reply->id = bafang_can_id_encode((bafang_can_id_t){
        .src = BAFANG_NODE_HMI, .dst = q->src,
        .type = BAFANG_CAN_OP_NORMAL_ACK, .cmd = q->cmd, .sub = q->sub
    });
    if (echo_payload) {
        reply->dlc = request->dlc;
        for (uint8_t i = 0u; i < request->dlc; ++i) reply->data[i] = request->data[i];
    }
}


static void put_u24le(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
}

bool dpc245_hmi_control_build_read_reply(const bafang_can_frame_t *request,
                                          const dpc245_runtime_t *runtime,
                                          bafang_can_frame_t *reply) {
    if (!request || !runtime || !reply || request->dlc != 0u) return false;
    bafang_can_id_t q = bafang_can_id_decode(request->id);
    if (q.dst != BAFANG_NODE_HMI || q.type != BAFANG_CAN_OP_READ ||
        q.cmd != 0x63u || q.src >= BAFANG_NODE_BROADCAST || q.src == BAFANG_NODE_HMI) return false;
    memset(reply, 0, sizeof(*reply));
    reply->id = bafang_can_id_encode((bafang_can_id_t){
        .src = BAFANG_NODE_HMI, .dst = q.src,
        .type = BAFANG_CAN_OP_NORMAL_ACK, .cmd = q.cmd, .sub = q.sub
    });

    if (q.sub == 0x01u) {
        uint32_t total = (dpc245_get_u32_le(&runtime->settings, DPC245_OFS_TOTAL_DISTANCE_RAW) + 50u) / 100u;
        uint32_t trip = dpc245_get_u32_le(&runtime->settings, DPC245_OFS_TRIP_DISTANCE_RAW) / 10u;
        uint16_t max_speed = dpc245_get_u16_le(&runtime->settings, DPC245_OFS_MAX_SPEED_RAW);
        put_u24le(&reply->data[0], total);
        put_u24le(&reply->data[3], trip);
        reply->data[6] = (uint8_t)max_speed;
        reply->data[7] = (uint8_t)(max_speed >> 8);
        reply->dlc = 8u;
        return true;
    }
    if (q.sub == 0x02u) {
        uint16_t average = dpc245_get_u16_le(&runtime->settings, DPC245_OFS_AVG_SPEED_RAW);
        uint32_t service = dpc245_get_u32_le(&runtime->settings, DPC245_OFS_SERVICE_DISTANCE_RAW) / 10u;
        reply->data[0] = (uint8_t)average;
        reply->data[1] = (uint8_t)(average >> 8);
        put_u24le(&reply->data[2], service);
        reply->dlc = 5u;
        return true;
    }
    if (q.sub == 0x0Bu) {
        uint32_t target = dpc245_get_u32_le(&runtime->settings, DPC245_OFS_SERVICE_TARGET_RAW) / 10u;
        reply->data[0] = 4u;
        put_u24le(&reply->data[1], target);
        reply->dlc = 4u;
        return true;
    }
    return false;
}

bool dpc245_hmi_control_process(const bafang_can_frame_t *request,
                                dpc245_runtime_t *runtime,
                                const dpc245_rtc_io_t *rtc_io,
                                dpc245_hmi_control_result_t *result) {
    if (!request || !runtime || !result || request->dlc > BAFANG_CAN_MAX_DLC) return false;
    memset(result, 0, sizeof(*result));
    bafang_can_id_t q = bafang_can_id_decode(request->id);
    if (q.dst != BAFANG_NODE_HMI || q.type != BAFANG_CAN_OP_WRITE ||
        q.src >= BAFANG_NODE_BROADCAST || q.src == BAFANG_NODE_HMI) return false;

    if (q.cmd == 0x62u && q.sub == 0x01u && request->dlc >= 3u) {
        uint32_t value = u24le(request->data) * 100u;
        dpc245_set_u32_le(&runtime->settings, DPC245_OFS_TOTAL_DISTANCE_RAW, value);
        if (value == 0u) dpc245_set_u32_le(&runtime->settings, DPC245_OFS_TRIP_DISTANCE_RAW, 0u);
        dpc245_settings_finalize(&runtime->settings);
        result->settings_dirty = true;
        make_ack(&q, request, &result->reply, true);
        result->have_reply = result->handled = true;
        return true;
    }
    if (q.cmd == 0x62u && q.sub == 0x02u && request->dlc >= 3u) {
        /* Payload order is HH, MM, SS. */
        result->rtc_updated = dpc245_rtc_set_time(rtc_io,
                                                   request->data[0],
                                                   request->data[1],
                                                   request->data[2]);
        make_ack(&q, request, &result->reply, true);
        result->have_reply = result->handled = true;
        return true;
    }
    if (q.cmd == 0x62u && q.sub == 0x03u && request->dlc >= 3u) {
        uint32_t value = u24le(request->data) * 100u;
        dpc245_set_u32_le(&runtime->settings, DPC245_OFS_TRIP_DISTANCE_RAW, value);
        if (value == 0u) {
            dpc245_set_u32_le(&runtime->settings, DPC245_OFS_TRIP_TIME_HALFSEC, 0u);
            dpc245_set_u32_le(&runtime->settings, DPC245_OFS_DISTANCE_AUX_RAW, 0u);
            dpc245_set_u32_le(&runtime->settings, DPC245_OFS_TIME_AUX_HALFSEC, 0u);
            dpc245_set_u16_le(&runtime->settings, DPC245_OFS_AVG_SPEED_RAW, 0u);
            dpc245_set_u16_le(&runtime->settings, DPC245_OFS_MAX_SPEED_RAW, 0u);
        }
        dpc245_settings_finalize(&runtime->settings);
        result->settings_dirty = true;
        make_ack(&q, request, &result->reply, true);
        result->have_reply = result->handled = true;
        return true;
    }
    if (q.cmd == 0x63u && q.sub == 0x02u && request->dlc >= 3u) {
        if (u24le(request->data) != 0u) return false;
        dpc245_set_u32_le(&runtime->settings, DPC245_OFS_SERVICE_DISTANCE_RAW, 0u);
        runtime->service_due = false;
        dpc245_settings_finalize(&runtime->settings);
        result->settings_dirty = true;
        make_ack(&q, request, &result->reply, false);
        result->have_reply = result->handled = true;
        return true;
    }
    if (q.cmd == 0x63u && q.sub == 0x0Bu && request->dlc >= 3u) {
        uint32_t value = u24le(request->data);
        /* The accepted input range is 10000..50000,
         * converted to the persistent service-target domain by x10. */
        if (value < 10000u || value > 50000u) return false;
        dpc245_set_u32_le(&runtime->settings, DPC245_OFS_SERVICE_TARGET_RAW, value * 10u);
        dpc245_settings_finalize(&runtime->settings);
        result->settings_dirty = true;
        make_ack(&q, request, &result->reply, false);
        result->have_reply = result->handled = true;
        return true;
    }
    if (q.cmd == 0x65u && q.sub == 0x00u && request->dlc >= 1u && request->data[0] == 1u) {
        /* The board-side event is real, but no stronger user-facing name is
         * assigned here. Keeping it explicit prevents conflating it with LIGHT. */
        result->board_event_65_00 = true;
        make_ack(&q, request, &result->reply, false);
        result->have_reply = result->handled = true;
        return true;
    }
    return false;
}
