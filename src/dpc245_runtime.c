/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_runtime.h"
#include <stddef.h>

static void add_u32(dpc245_settings_t *s, uint8_t off, uint32_t delta) {
    uint32_t v = dpc245_get_u32_le(s, off);
    dpc245_set_u32_le(s, off, v + delta);
}

static void update_service_due(dpc245_runtime_t *rt);

static void pedal_window_clear(dpc245_runtime_t *rt) {
    rt->pedal_sample_count = 0u;
    rt->pedal_sample_next = 0u;
    rt->pedal_avg_w = 0u;
}

/* Zero samples with cadence above zero are valid samples and stay in the
 * window; only a stopped crank clears it. */
static void pedal_window_push(dpc245_runtime_t *rt, uint16_t sample) {
    uint32_t sum = 0u;
    uint32_t avg;
    rt->pedal_samples[rt->pedal_sample_next] = sample;
    rt->pedal_sample_next =
        (uint8_t)((rt->pedal_sample_next + 1u) % DPC245_PEDAL_WINDOW);
    if (rt->pedal_sample_count < DPC245_PEDAL_WINDOW)
        rt->pedal_sample_count++;
    for (uint8_t i = 0u; i < rt->pedal_sample_count; i++)
        sum += rt->pedal_samples[i];
    avg = (sum + rt->pedal_sample_count / 2u) / rt->pedal_sample_count;
    if (avg > 9999u) avg = 9999u;
    rt->pedal_avg_w = (uint16_t)avg;
}

void dpc245_runtime_init(dpc245_runtime_t *rt,
                               const dpc245_settings_t *settings) {
    if (!rt) return;
    if (settings) rt->settings = *settings;
    else dpc245_settings_defaults(&rt->settings);
    rt->previous_distance_counter = 0u;
    rt->distance_counter_initialized = false;
    rt->previous_avg_distance_aux = 0u;
    rt->service_due = false;
    rt->controller_error = rt->sensor_error = rt->battery_error = 0u;
    rt->have_controller_error = rt->have_sensor_error = rt->have_battery_error = false;
    rt->pedal_power_w = 0u;
    pedal_window_clear(rt);
    rt->pedal_avg_valid = false;
    rt->pedal_seen = false;
    rt->pedal_last_ms = 0u;
    rt->now_ms = 0u;
    rt->rider_energy_half_joule = 0u;
    update_service_due(rt);
}

static void update_service_due(dpc245_runtime_t *rt) {
    uint32_t distance = dpc245_get_u32_le(&rt->settings, DPC245_OFS_SERVICE_DISTANCE_RAW);
    uint32_t target = dpc245_get_u32_le(&rt->settings, DPC245_OFS_SERVICE_TARGET_RAW);
    rt->service_due = distance >= target;
    if (rt->service_due) {
        dpc245_set_u32_le(&rt->settings, DPC245_OFS_SERVICE_DISTANCE_RAW, target);
    }
}

bool dpc245_runtime_on_can(dpc245_runtime_t *rt,
                                  const bafang_can_frame_t *frame) {
    if (!rt || !frame) return false;
    uint32_t id = frame->id & 0x1FFFFFFFu;

    /* Error-history acceptance gate:
     * source is torque sensor/controller/battery, type=7, sub=0. The latest
     * source error is retained transiently; persistent history filters trivial
     * codes and consecutive duplicates in dpc245_error_push(). */
    {
        bafang_can_id_t cid = bafang_can_id_decode(id);
        if (frame->dlc >= 1u && cid.type == 7u && cid.sub == 0u &&
            (cid.src == BAFANG_NODE_TORQUE_SENSOR || cid.src == BAFANG_NODE_CONTROLLER ||
             cid.src == BAFANG_NODE_BATTERY)) {
            if (cid.src == BAFANG_NODE_CONTROLLER) { rt->controller_error = frame->data[0]; rt->have_controller_error = true; }
            else if (cid.src == BAFANG_NODE_TORQUE_SENSOR) { rt->sensor_error = frame->data[0]; rt->have_sensor_error = true; }
            else { rt->battery_error = frame->data[0]; rt->have_battery_error = true; }
            (void)dpc245_error_push(&rt->settings, frame->data[0]);
            return true;
        }
    }
    if (id == 0x02F83200u && frame->dlc >= 8u) {
        {
            uint16_t torque=(uint16_t)frame->data[4]|((uint16_t)frame->data[5]<<8);
            uint8_t cadence=frame->data[3];
            uint16_t sample=0u;
            if(torque>750u && cadence>0u){
                uint32_t n=(uint32_t)(torque-750u)*(uint32_t)cadence*88u;
                uint32_t watts=n/23396u;
                sample=(uint16_t)(watts>65535u?65535u:watts);
            }
            rt->pedal_power_w=sample;
            /* A gap longer than the freshness limit also clears the window,
             * even if no poll ran while the main loop was busy. */
            if(rt->pedal_seen&&(uint32_t)(rt->now_ms-rt->pedal_last_ms)>DPC245_PEDAL_STALE_MS)
                pedal_window_clear(rt);
            rt->pedal_last_ms=rt->now_ms;
            rt->pedal_seen=true;
            if(cadence==0u){
                pedal_window_clear(rt);
            }else{
                pedal_window_push(rt,sample);
            }
            rt->pedal_avg_valid=true;
        }
        uint16_t current = (uint16_t)((uint16_t)frame->data[1] |
                                      ((uint16_t)frame->data[2] << 8));

        /* If the received counter moved backwards, or the local
         * initialization flag is clear, set the baseline and do not accumulate. */
        if (!rt->distance_counter_initialized || current < rt->previous_distance_counter) {
            rt->previous_distance_counter = current;
            rt->distance_counter_initialized = true;
            return true;
        }

        uint16_t delta = (uint16_t)(current - rt->previous_distance_counter);
        if (delta != 0u) {
            rt->previous_distance_counter = current;
            add_u32(&rt->settings, DPC245_OFS_TRIP_DISTANCE_RAW, delta);
            add_u32(&rt->settings, DPC245_OFS_DISTANCE_AUX_RAW, delta);
            add_u32(&rt->settings, DPC245_OFS_SERVICE_DISTANCE_RAW, delta);
            add_u32(&rt->settings, DPC245_OFS_TOTAL_DISTANCE_RAW, delta);
        }
        return true;
    }

    if (id == 0x02F83201u && frame->dlc >= 2u) {
        /* Ride-statistics update. Current speed drives max/average accumulation;
         * settings +0x2F is MAX and +0x2C is AVG. */
        uint16_t speed_centi_kph = (uint16_t)((uint16_t)frame->data[0] |
                                              ((uint16_t)frame->data[1] << 8));
        /* The controller realtime frame is 0.01 km/h, but raw[0x2F] and
         * raw[0x2C] are 0.1 km/h, the unit the 03106301/02 status frames carry
         * unchanged. The arithmetic is integer, so centi-km/h are truncated to
         * tenths here. */
        uint16_t speed = (uint16_t)(speed_centi_kph / 10u);
        uint16_t max_speed = dpc245_get_u16_le(&rt->settings, DPC245_OFS_MAX_SPEED_RAW);
        if (speed > max_speed) {
            max_speed = speed;
            dpc245_set_u16_le(&rt->settings, DPC245_OFS_MAX_SPEED_RAW, max_speed);
        }
        if (speed == 0u) {
            update_service_due(rt);
            return true;
        }

        uint32_t time_halfsec = dpc245_get_u32_le(&rt->settings, DPC245_OFS_TIME_AUX_HALFSEC);
        if (time_halfsec == 0u) {
            dpc245_set_u16_le(&rt->settings, DPC245_OFS_AVG_SPEED_RAW, 0u);
            update_service_due(rt);
            return true;
        }
        uint32_t distance_aux = dpc245_get_u32_le(&rt->settings, DPC245_OFS_DISTANCE_AUX_RAW);
        if (distance_aux == rt->previous_avg_distance_aux) {
            update_service_due(rt);
            return true;
        }
        rt->previous_avg_distance_aux = distance_aux;

        uint32_t avg32 = (720u * distance_aux) / time_halfsec;
        uint16_t avg = avg32 > 0xFFFFu ? 0xFFFFu : (uint16_t)avg32;
        if (avg > max_speed) avg = max_speed;
        dpc245_set_u16_le(&rt->settings, DPC245_OFS_AVG_SPEED_RAW, avg);
        update_service_due(rt);
        return true;
    }
    return false;
}

bool dpc245_runtime_service_due(const dpc245_runtime_t *rt) {
    return rt ? rt->service_due : false;
}

bool dpc245_runtime_service_tip_active(const dpc245_runtime_t *rt) {
    return rt && dpc245_service_tips(&rt->settings) != 0u && rt->service_due;
}

void dpc245_runtime_trip_reset(dpc245_runtime_t *rt) {
    if (!rt) return;
    dpc245_set_u32_le(&rt->settings, DPC245_OFS_TRIP_DISTANCE_RAW, 0u);
    dpc245_set_u32_le(&rt->settings, DPC245_OFS_TRIP_TIME_HALFSEC, 0u);
    dpc245_set_u32_le(&rt->settings, DPC245_OFS_DISTANCE_AUX_RAW, 0u);
    dpc245_set_u32_le(&rt->settings, DPC245_OFS_TIME_AUX_HALFSEC, 0u);
    dpc245_set_u16_le(&rt->settings, DPC245_OFS_MAX_SPEED_RAW, 0u);
    dpc245_set_u16_le(&rt->settings, DPC245_OFS_AVG_SPEED_RAW, 0u);
    rt->rider_energy_half_joule = 0u;
    rt->pedal_power_w = 0u;
    /* Validity follows sample freshness, not the trip reset. */
    pedal_window_clear(rt);
}

bool dpc245_runtime_pedal_avg(const dpc245_runtime_t *rt, uint16_t *watts) {
    if (!rt || !rt->pedal_avg_valid) return false;
    if (watts) *watts = rt->pedal_avg_w;
    return true;
}

void dpc245_runtime_pedal_poll(dpc245_runtime_t *rt, uint32_t now_ms) {
    if (!rt) return;
    if (!rt->pedal_seen ||
        (uint32_t)(now_ms - rt->pedal_last_ms) > DPC245_PEDAL_STALE_MS) {
        rt->pedal_avg_valid = false;
        pedal_window_clear(rt);
    }
}

void dpc245_runtime_tick_500ms(dpc245_runtime_t *rt,
                                      bool activity_gate) {
    if (!rt || !activity_gate) return;
    add_u32(&rt->settings, DPC245_OFS_TRIP_TIME_HALFSEC, 1u);
    add_u32(&rt->settings, DPC245_OFS_TIME_AUX_HALFSEC, 1u);
    rt->rider_energy_half_joule += rt->pedal_power_w;
}

void dpc245_runtime_hmi_stats(const dpc245_runtime_t *rt,
                                    dpc245_v3_hmi_stats_raw_t *out) {
    if (!rt || !out) return;
    out->raw06 = rt->settings.raw[0x06];
    out->raw07 = rt->settings.raw[0x07];
    out->raw0a = rt->settings.raw[0x0A];
    out->raw10 = dpc245_get_u32_le(&rt->settings, DPC245_OFS_TOTAL_DISTANCE_RAW);
    out->raw14 = dpc245_get_u32_le(&rt->settings, DPC245_OFS_TRIP_DISTANCE_RAW);
    out->raw18 = dpc245_get_u32_le(&rt->settings, DPC245_OFS_SERVICE_DISTANCE_RAW);
    out->raw20_halfsec = dpc245_get_u32_le(&rt->settings, DPC245_OFS_TRIP_TIME_HALFSEC);
    out->raw28_halfsec = dpc245_get_u32_le(&rt->settings, DPC245_OFS_TIME_AUX_HALFSEC);
    out->raw2c = dpc245_get_u16_le(&rt->settings, DPC245_OFS_AVG_SPEED_RAW);
    out->raw2f = dpc245_get_u16_le(&rt->settings, DPC245_OFS_MAX_SPEED_RAW);
}

uint32_t dpc245_runtime_calories_kcal(const dpc245_runtime_t *rt) {
    return rt ? rt->rider_energy_half_joule / 8368u : 0u;
}
