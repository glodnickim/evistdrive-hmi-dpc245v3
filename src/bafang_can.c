/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "bafang_can.h"
#include <stddef.h>
void *memset(void *s, int c, size_t n);

static uint16_t u16le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

bool bafang_temperature_raw_valid(uint8_t raw) {
    return raw != 0u && raw != 0xFFu;
}

uint16_t bafang_wheel_tenths_to_raw(uint16_t wheel_size_tenth_in) {
    uint16_t whole = (uint16_t)(wheel_size_tenth_in / 10u);
    uint16_t tenth = (uint16_t)(wheel_size_tenth_in % 10u);
    if (whole > 0x0FFFu) whole = 0x0FFFu;
    return (uint16_t)((whole << 4) | tenth);
}

uint16_t bafang_wheel_raw_to_tenths(uint16_t wheel_size_raw) {
    uint16_t tenth = (uint16_t)(wheel_size_raw & 0x0Fu);
    if (tenth > 9u) return 0u;
    return (uint16_t)(((wheel_size_raw >> 4) * 10u) + tenth);
}

uint32_t bafang_can_id_encode(bafang_can_id_t f) {
    return (((uint32_t)f.src  & 0x1Fu) << 24) |
           (((uint32_t)f.dst  & 0x1Fu) << 19) |
           (((uint32_t)f.type & 0x07u) << 16) |
           (((uint32_t)f.cmd) << 8) |
           ((uint32_t)f.sub);
}

bafang_can_id_t bafang_can_id_decode(uint32_t id) {
    bafang_can_id_t f;
    f.src  = (uint8_t)((id >> 24) & 0x1Fu);
    f.dst  = (uint8_t)((id >> 19) & 0x1Fu);
    f.type = (uint8_t)((id >> 16) & 0x07u);
    f.cmd  = (uint8_t)((id >> 8) & 0xFFu);
    f.sub  = (uint8_t)(id & 0xFFu);
    return f;
}

bafang_frame_kind_t bafang_frame_classify(uint32_t id) {
    switch (id & 0x1FFFFFFFu) {
    case 0x02F83200u: return BAFANG_FRAME_CONTROLLER_FAST_0;
    case 0x02F83201u: return BAFANG_FRAME_CONTROLLER_FAST_1;
    case 0x02F83202u: return BAFANG_FRAME_CONTROLLER_FAST_2_RAW;
    case 0x02F83205u: return BAFANG_FRAME_CONTROLLER_FAST_5_RAW;
    case 0x02F83203u: return BAFANG_FRAME_CONTROLLER_CONFIG;
    case 0x02FF1200u: return BAFANG_FRAME_CONTROLLER_STATE;
    case 0x01F83100u: return BAFANG_FRAME_TORQUE_SENSOR;
    case 0x04F83400u: return BAFANG_FRAME_BATTERY_FAST_0_RAW;
    case 0x04F83401u: return BAFANG_FRAME_BATTERY_FAST_1_RAW;
    case 0x04F83402u: return BAFANG_FRAME_BATTERY_FAST_2_RAW;
    case 0x03106300u: return BAFANG_FRAME_HMI_HEARTBEAT;
    default: return BAFANG_FRAME_UNKNOWN;
    }
}

bool bafang_decode_telemetry(const bafang_can_frame_t *f, bafang_telemetry_t *t) {
    if (!f || !t || f->dlc > 8u) return false;

    /* Battery-node command 0x34.
     * The HMI consumes sub 0 (capacity/SOC) and sub 1 (electrical state)
     * here. Cell voltages belong to on-demand command 0x64/0x02..0x09 and are
     * handled by bafang_service.c, not by the broadcast decoder. */
    {
        bafang_can_id_t cid = bafang_can_id_decode(f->id);
        if (cid.src == BAFANG_NODE_BATTERY && cid.dst == BAFANG_NODE_BROADCAST &&
            cid.type == 0u && cid.cmd == 0x34u) {
            if (cid.sub == 0u) {
                if (f->dlc < 6u) return false;
                t->battery_full_capacity_raw = u16le(&f->data[0]);
                t->battery_remaining_capacity_raw = u16le(&f->data[2]);
                t->battery_rsoc_percent = f->data[4];
                t->battery_asoc_percent = f->data[5];
                t->have_battery_capacity = true;
                return true;
            }
            if (cid.sub == 1u) {
                if (f->dlc < 5u) return false;
                t->battery_current_centi_amp = (int16_t)u16le(&f->data[0]);
                t->battery_voltage_centi_volt = u16le(&f->data[2]);
                t->battery_temp_c = (int16_t)f->data[4] - 40;
                t->have_battery_state = true;
                return true;
            }
        }
    }

    switch (bafang_frame_classify(f->id)) {
    case BAFANG_FRAME_CONTROLLER_FAST_0:
        if (f->dlc < 8u) return false;
        t->battery_pct = f->data[0];
        /* The HMI parser consumes
         * data[1..2] as one little-endian rolling distance counter. */
        t->distance_counter_raw = u16le(&f->data[1]);
        t->distance_10m_counter = (uint8_t)t->distance_counter_raw; /* low-byte alias */
        t->cadence_rpm = f->data[3];
        t->torque_signal_mv = u16le(&f->data[4]);
        t->range_centi_km = u16le(&f->data[6]);
        t->have_battery_pct = true;
        t->have_distance_counter = true;
        t->have_cadence = true;
        t->have_torque_signal = true;
        t->have_range = true;
        return true;

    case BAFANG_FRAME_CONTROLLER_FAST_1:
        if (f->dlc < 8u) return false;
        t->speed_centi_kph = u16le(&f->data[0]);
        t->current_centi_amp = (int16_t)u16le(&f->data[2]);
        t->voltage_centi_volt = u16le(&f->data[4]);
        t->controller_temp_c = (int16_t)f->data[6] - 40;
        t->controller_temp_raw = f->data[6];
        t->controller_aux_temp_raw = f->data[7];
        t->motor_temp_c = (int16_t)f->data[7] - 40;
        t->have_speed = true;
        t->have_current = true;
        t->have_voltage = true;
        t->have_controller_temp = bafang_temperature_raw_valid(f->data[6]);
        t->have_controller_aux_temp_raw = true;
        t->have_motor_temp = bafang_temperature_raw_valid(f->data[7]);
        return true;

    case BAFANG_FRAME_CONTROLLER_CONFIG:
        if (f->dlc < 6u) return false;
        t->speed_limit_centi_kph = u16le(&f->data[0]);
        t->wheel_size_raw = u16le(&f->data[2]);
        t->wheel_size_tenth_in = bafang_wheel_raw_to_tenths(t->wheel_size_raw);
        t->circumference_mm = u16le(&f->data[4]);
        t->have_speed_limit = true;
        t->have_wheel_size = true;
        t->have_circumference = true;
        return true;

    case BAFANG_FRAME_CONTROLLER_STATE:
        if (f->dlc < 1u) return false;
        t->controller_state_raw = f->data[0];
        t->have_controller_state = true;
        return true;

    case BAFANG_FRAME_TORQUE_SENSOR:
        if (f->dlc < 4u) return false;
        t->torque_sensor_raw = u16le(&f->data[0]);
        t->torque_sensor_cadence_rpm = f->data[2];
        t->torque_sensor_progressive = f->data[3];
        t->have_torque_sensor = true;
        return true;

    case BAFANG_FRAME_CONTROLLER_FAST_2_RAW:
        if (f->dlc < 1u) return false;
        t->controller_fast2_raw = f->data[0];
        t->controller_status_bit0 = (f->data[0] & 0x01u) != 0u;
        t->have_controller_status_bit0 = true;
        return true;

    case BAFANG_FRAME_CONTROLLER_FAST_5_RAW:
        if (f->dlc < 2u) return false;
        t->controller_fast5_raw = u16le(&f->data[0]);
        t->have_controller_fast5_raw = true;
        return true;

    case BAFANG_FRAME_BATTERY_FAST_0_RAW:
    case BAFANG_FRAME_BATTERY_FAST_1_RAW:
    case BAFANG_FRAME_BATTERY_FAST_2_RAW:
        /* Battery cmd 0x34 is decoded by the generic source/subcode path above. */
        return false;

    default:
        return false;
    }
}

uint8_t bafang_assist_code(uint8_t level_count, uint8_t user_level, bool walk) {
    static const uint8_t lut3[4] = {0x00u, 0x0Cu, 0x02u, 0x03u};
    static const uint8_t lut5[6] = {0x00u, 0x0Bu, 0x0Du, 0x15u, 0x17u, 0x03u};
    static const uint8_t lut9[10] = {
        0x00u, 0x01u, 0x0Bu, 0x0Cu, 0x0Du,
        0x02u, 0x15u, 0x16u, 0x17u, 0x03u
    };
    if (walk) return 0x06u;
    if (level_count == 3u && user_level <= 3u) return lut3[user_level];
    if (level_count == 5u && user_level <= 5u) return lut5[user_level];
    if (level_count == 9u && user_level <= 9u) return lut9[user_level];
    return 0xFFu;
}

bool bafang_build_hmi_heartbeat_v3(bafang_can_frame_t *out,
                                    uint8_t level_count,
                                    uint8_t user_level,
                                    bool walk,
                                    bool light_on,
                                    uint8_t event_bits_v3,
                                    uint8_t status3_raw) {
    uint8_t code;
    if (!out) return false;
    if ((event_bits_v3 & (uint8_t)~0x0Eu) != 0u) return false;
    if (status3_raw > 1u) return false;
    code = bafang_assist_code(level_count, user_level, walk);
    if (code == 0xFFu) return false;
    memset(out, 0, sizeof(*out));
    out->id = 0x03106300u;
    out->dlc = 4u;
    out->data[0] = level_count;
    out->data[1] = code;
    out->data[2] = (uint8_t)((light_on ? 0x01u : 0u) | event_bits_v3);
    out->data[3] = status3_raw;
    return true;
}

bool bafang_build_hmi_heartbeat_v3_default(bafang_can_frame_t *out,
                                            uint8_t level_count,
                                            uint8_t user_level,
                                            bool walk,
                                            bool light_on) {
    return bafang_build_hmi_heartbeat_v3(out, level_count, user_level, walk,
                                         light_on, 0u, 1u);
}

bool bafang_build_read_query(bafang_can_frame_t *out,
                             uint8_t dst,
                             uint8_t cmd,
                             uint8_t sub) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    out->id = bafang_can_id_encode((bafang_can_id_t){
        .src = BAFANG_NODE_HMI,
        .dst = dst,
        .type = 1u,
        .cmd = cmd,
        .sub = sub,
    });
    out->dlc = 0u;
    return true;
}

bool bafang_build_besst_controller_config_write(bafang_can_frame_t *out,
                                                 uint16_t speed_limit_centi_kph,
                                                 uint16_t wheel_size_tenth_in,
                                                 uint16_t circumference_mm) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    out->id = 0x05103203u;
    out->dlc = 6u;
    out->data[0] = (uint8_t)speed_limit_centi_kph;
    out->data[1] = (uint8_t)(speed_limit_centi_kph >> 8);
    uint16_t wheel_raw = bafang_wheel_tenths_to_raw(wheel_size_tenth_in);
    out->data[2] = (uint8_t)wheel_raw;
    out->data[3] = (uint8_t)(wheel_raw >> 8);
    out->data[4] = (uint8_t)circumference_mm;
    out->data[5] = (uint8_t)(circumference_mm >> 8);
    return true;
}

int32_t bafang_power_w(const bafang_telemetry_t *t) {
    if (!t || !t->have_voltage || !t->have_current) return 0;
    /* centivolt * centiamp = 1e-4 watt */
    /* Values on known Bafang systems stay comfortably inside int32 here. */
    int32_t p = (int32_t)t->voltage_centi_volt * (int32_t)t->current_centi_amp;
    if (p >= 0) return (p + 5000) / 10000;
    return (p - 5000) / 10000;
}

bool bafang_walk_bit_fresh(const bafang_telemetry_t *t, uint32_t now_ms) {
    return t && t->have_controller_status_bit0 && t->controller_status_bit0 &&
           (uint32_t)(now_ms - t->controller_fast2_rx_ms) <= 500u;
}
