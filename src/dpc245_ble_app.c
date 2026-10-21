/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_ble_app.h"
#include "bafang_can.h"
#include "dpc245_settings.h"
#include "app_config.h"
#include "assist_profile.h"
#include <stddef.h>

static void zero_bytes(uint8_t *p, size_t n) {
    for (size_t i = 0u; i < n; ++i) p[i] = 0u;
}

static void copy_bytes(uint8_t *dst, const uint8_t *src, size_t n) {
    for (size_t i = 0u; i < n; ++i) dst[i] = src[i];
}

static void copy_text_field(uint8_t *dst, size_t capacity, const char *src) {
    if (!dst || !src) return;
    size_t i = 0u;
    while (i < capacity && src[i] != '\0') {
        dst[i] = (uint8_t)src[i];
        ++i;
    }
}

static void copy_record_field(uint8_t *dst, size_t capacity,
                              const uint8_t *src, uint8_t len) {
    if (!dst || !src) return;
    size_t n = len;
    if (n > capacity) n = capacity;
    copy_bytes(dst, src, n);
}

static void put_u16le(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put_i16le(uint8_t *p, int16_t v) {
    put_u16le(p, (uint16_t)v);
}

static void put_u32le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint8_t temp_style(int16_t c) {
    int32_t v = (int32_t)c + 40;
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    return (uint8_t)v;
}

static size_t build_reply(dpc245_ble_app_result_t *r,
                          const dpc245_ble_frame_t *q,
                          uint8_t operation,
                          const uint8_t *payload,
                          uint8_t payload_len) {
    if (!r || !q) return 0u;
    size_t n = dpc245_ble_frame_build(r->tx, sizeof(r->tx),
                                      q->command_id, q->class_id,
                                      operation, q->selector,
                                      payload, payload_len);
    r->have_tx = n != 0u;
    r->tx_len = n;
    return n;
}

static uint8_t assist_wire(const dpc245_app_t *app) {
    if (!app) return 0u;
    uint8_t v = bafang_assist_code(app->assist_level_count, app->assist_level, app->walk);
    return v == 0xFFu ? 0u : v;
}

static void refresh_a3(dpc245_ble_app_t *b,
                       const dpc245_app_t *app,
                       const evist_can_router_t *router) {
    if (!b || !app || !router) return;
    uint8_t *p = b->a3;
    const bafang_telemetry_t *t = &router->telemetry;

    copy_text_field(&p[0], 24u, router->controller_identity.hardware);
    copy_text_field(&p[24], 24u, router->controller_identity.software);
    copy_text_field(&p[48], 24u, router->controller_identity.model);
    copy_text_field(&p[72], 40u, router->controller_identity.serial);

    if (t->have_battery_pct) put_u16le(&p[160], t->battery_pct);
    if (t->have_range) put_u16le(&p[166], t->range_centi_km);
    if (t->have_cadence) put_u16le(&p[168], t->cadence_rpm);
    if (t->have_torque_signal) put_u16le(&p[170], t->torque_signal_mv);
    if (t->have_speed) put_u16le(&p[172], t->speed_centi_kph);
    if (t->have_current) put_i16le(&p[174], t->current_centi_amp);
    if (t->have_voltage) put_u16le(&p[176], t->voltage_centi_volt);
    put_u16le(&p[178], t->have_controller_temp && bafang_temperature_raw_valid(t->controller_temp_raw) ?
                         temp_style(t->controller_temp_c) : 0u);
    if (t->have_controller_aux_temp_raw) put_u16le(&p[180], t->controller_aux_temp_raw);
    if (t->have_speed_limit) put_u16le(&p[184], t->speed_limit_centi_kph);
    if (t->have_wheel_size) put_u16le(&p[186], t->wheel_size_raw);
    if (t->have_circumference) put_u16le(&p[188], t->circumference_mm);
    put_u16le(&p[192], app->assist_level);
    put_u16le(&p[194], app->assist_level_count);
    p[213] = b->download_status[6] == 3u ? b->p2[54] : 0u;
    p[211] = b->download_status[7] == 3u ? b->start_angle[0] : 0u;
    p[212] = b->download_status[7] == 3u ? b->start_angle[1] : 0u;

    for (uint8_t i = 0u; i < 9u; ++i) {
        p[215u + i] = 0u;
        p[225u + i] = 0u;
    }
    if (b->download_status[5] == 3u) {
        for (uint8_t i = 0u; i < 9u; ++i) {
            p[215u + i] = b->p1[49u + i];
            p[225u + i] = b->p1[40u + i];
        }
    }
}

static void refresh_a5(dpc245_ble_app_t *b,
                       const dpc245_app_t *app,
                       const dpc245_hmi_identity_records_t *id,
                       bool light_on) {
    if (!b || !app) return;
    uint8_t *p = b->a5;
    static const char hw[] = "DP C245.C 3.0";
    static const char sw[] = "DPC245CF80301.2";
    static const char model[] = "DP C245.CAN";
    static const char maker[] = "BAFANG";

    copy_text_field(&p[0], 24u, hw);
    copy_text_field(&p[24], 24u, sw);
    copy_text_field(&p[48], 24u, model);
    if (id) {
        copy_record_field(&p[72], 40u, id->serial, id->serial_len);
        copy_record_field(&p[112], 16u, id->customer, id->customer_len);
    }
    copy_text_field(&p[136], 16u, maker);

    p[160] = 5u;
    p[161] = 0u;
    p[162] = 2u;
    p[163] = 1u;
    p[164] = assist_wire(app);
    p[166] = light_on ? 1u : 0u;
    put_u16le(&p[168], (uint16_t)((dpc245_get_u32_le(&app->runtime.settings,
                           DPC245_OFS_TOTAL_DISTANCE_RAW) + 50u) / 100u));
    put_u16le(&p[172], dpc245_get_u16_le(&app->runtime.settings, DPC245_OFS_MAX_SPEED_RAW));
    put_u16le(&p[174], dpc245_get_u16_le(&app->runtime.settings, DPC245_OFS_AVG_SPEED_RAW));
    put_u32le(&p[176], dpc245_get_u32_le(&app->runtime.settings, DPC245_OFS_SERVICE_DISTANCE_RAW));
    p[180] = dpc245_auto_off_min(&app->runtime.settings);
    p[182] = 10u;
    p[184] = dpc245_unit(&app->runtime.settings) ? 1u : 0u;
    put_u32le(&p[186], dpc245_get_u32_le(&app->runtime.settings,
                                         DPC245_OFS_TRIP_TIME_HALFSEC) / 120u);
    put_u32le(&p[194], dpc245_get_u32_le(&app->runtime.settings,
                                         DPC245_OFS_TRIP_DISTANCE_RAW) / 10u);
}

static void refresh_a9(dpc245_ble_app_t *b, const dpc245_app_t *app) {
    if (!b || !app) return;
    b->a9[64] = b->per_level ? 1u : 0u;
    for (uint8_t i = 0u; i < 10u; ++i) {
        b->a9[65u + i * 2u] = b->download_status[7] == 3u ? b->start_angle[0] : 0u;
        b->a9[66u + i * 2u] = b->download_status[7] == 3u ? b->start_angle[1] : 0u;
    }
    b->a9[85] = 0u;
    for (uint8_t i = 0u; i < 9u; ++i) {
        b->a9[86u + i] = 0u;
        b->a9[96u + i] = 0u;
        b->a9[106u + i] = 0u;
    }
    if (b->download_status[6] == 3u) b->a9[85] = b->p2[54];
    if (b->per_level && b->download_status[4] == 3u)
        for (uint8_t i = 0u; i < 9u; ++i) b->a9[86u + i] = b->p0[1u + i];
    /* The final two ten-element arrays mirror P1 speed, then current. */
    if (b->download_status[5] == 3u) {
        for (uint8_t i = 0u; i < 9u; ++i) {
            b->a9[96u + i] = b->p1[49u + i];
            b->a9[106u + i] = b->p1[40u + i];
        }
    }
}

#if EVIST_ENABLE_CONFIG_WRITES
static bool queue_motor_write(dpc245_ble_app_t *b, uint8_t which,
                              evist_firmware_profile_t profile) {
    if (!b || b->pending_write || b->controller_txn_active) return false;
    if (which == 1u) {
        if (b->download_status[5] != 3u) return false;
        copy_bytes(b->write_candidate, b->p1, 64u);
        for (uint8_t i = 0u; i < 9u; ++i) {
            b->write_candidate[49u + i] = b->a3[215u + i];
            b->write_candidate[40u + i] = b->a3[225u + i];
        }
        b->write_candidate[63] = evist_assist_checksum(b->write_candidate);
        if (!evist_assist_write_allowed(b->write_candidate, profile)) return false;
    } else {
        if (!b->per_level || b->download_status[4] != 3u) return false;
        copy_bytes(b->write_candidate, b->p0, 64u);
        for (uint8_t i = 0u; i < 9u; ++i) {
            uint8_t v = b->a9[86u + i];
            if (v < 1u || v > 8u) return false;
            b->write_candidate[1u + i] = v;
        }
        b->write_candidate[63] = evist_assist_checksum(b->write_candidate);
    }
    b->pending_write = which;
    return true;
}
#endif

bool dpc245_ble_app_start_controller(dpc245_ble_app_t *b, evist_ctrl_txn_t *txn,
                                     evist_firmware_profile_t profile, uint32_t now) {
#if !EVIST_ENABLE_CONFIG_WRITES
    (void)profile;
#endif
    if (!b || !txn || !b->controller_seen || b->controller_txn_active || evist_ctrl_txn_busy(txn)) return false;
    bool started = false;
    b->active_is_write = false;
    if (b->pending_write != 0u) {
#if EVIST_ENABLE_CONFIG_WRITES
        started = b->pending_write == 1u ?
            evist_ctrl_txn_write_app_p1(txn, b->write_candidate, profile, now) :
            evist_ctrl_txn_write_app_p0(txn, b->write_candidate, b->p0[63], now);
        b->active_is_write = started;
#endif
        if (!started) {
            b->download_status[b->pending_write == 1u ? 5u : 4u] = 4u;
            b->pending_write = 0u;
        }
    } else if (b->download_stage < 5u) {
        switch (b->download_stage) {
        case 0u: started = evist_ctrl_txn_discover_accel(txn, now); break;
        case 1u: b->download_status[5] = 1u; started = evist_ctrl_txn_read_assist(txn, now); break;
        case 2u: b->download_status[6] = 1u; started = evist_ctrl_txn_read_accel_p2(txn, now); break;
        default:
            if (b->download_stage == 3u) {
                if (!b->per_level) { b->download_stage = 4u; return false; }
                b->download_status[4] = 1u;
                started = evist_ctrl_txn_read_accel_p0(txn, now);
            } else {
                b->download_status[7] = 1u;
                started = evist_ctrl_txn_read_start_angle(txn, now);
            }
            break;
        }
    }
    b->controller_txn_active = started;
    return started;
}

static void cache_block(dpc245_ble_app_t *b, evist_txn_kind_t kind, const uint8_t data[64]) {
    if (kind == EVIST_TXN_READ_START_ANGLE) {
        b->start_angle[0] = data[0];
        b->start_angle[1] = data[1];
        b->download_status[7] = 3u;
        return;
    }
    uint8_t *dst = kind == EVIST_TXN_READ_ASSIST || kind == EVIST_TXN_WRITE_ASSIST ||
                   kind == EVIST_TXN_WRITE_APP_P1 ? b->p1 :
                   kind == EVIST_TXN_READ_ACCEL_P2 || kind == EVIST_TXN_WRITE_APP_P2 ? b->p2 : b->p0;
    copy_bytes(dst, data, 64u);
    if (dst == b->p1) {
        b->download_status[5] = 3u;
        for (uint8_t i = 0u; i < 9u; ++i) {
            b->a3[215u + i] = b->p1[49u + i];
            b->a3[225u + i] = b->p1[40u + i];
        }
    } else if (dst == b->p2) {
        b->download_status[6] = 3u;
    } else {
        b->download_status[4] = 3u;
        for (uint8_t i = 0u; i < 9u; ++i) b->a9[86u + i] = b->p0[1u + i];
    }
}

void dpc245_ble_app_on_controller_result(dpc245_ble_app_t *b, evist_txn_kind_t kind,
                                         evist_txn_result_t result, const uint8_t data[64]) {
    if (!b || !b->controller_txn_active) return;
    b->controller_txn_active = false;
    if (b->active_is_write) {
        uint8_t status_index = kind == EVIST_TXN_WRITE_APP_P1 ? 5u : 4u;
        b->pending_write = 0u;
        if ((result == EVIST_TXN_OK || result == EVIST_TXN_MISMATCH) && data)
            cache_block(b, kind, data);
        else if (result != EVIST_TXN_MISMATCH) b->download_status[status_index] = 4u;
        return;
    }
    if (kind == EVIST_TXN_DISCOVER_ACCEL) {
        if (result == EVIST_TXN_OK) b->per_level = true;
        if (result == EVIST_TXN_OK || ++b->stage_attempts >= 2u) {
            b->discovery_done = true;
            b->download_stage = 1u;
            b->stage_attempts = 0u;
        }
        return;
    }
    if (result == EVIST_TXN_OK && data) cache_block(b, kind, data);
    else b->download_status[b->download_stage == 1u ? 5u : b->download_stage == 2u ? 6u :
                            b->download_stage == 3u ? 4u : 7u] = 4u;
    if (result == EVIST_TXN_OK || ++b->stage_attempts >= 2u) {
        ++b->download_stage;
        b->stage_attempts = 0u;
    }
}

void dpc245_ble_app_capture_adv_result(dpc245_ble_app_t *b, evist_txn_kind_t kind,
                                       evist_txn_result_t result, const uint8_t data[64]) {
    if (!b) return;
    if (kind == EVIST_TXN_DISCOVER_ACCEL &&
        (result == EVIST_TXN_OK || result == EVIST_TXN_NO_READBACK)) {
        b->discovery_done = true;
        b->per_level = result == EVIST_TXN_OK;
    } else if (result == EVIST_TXN_OK &&
             (kind == EVIST_TXN_READ_ASSIST || kind == EVIST_TXN_WRITE_ASSIST ||
              kind == EVIST_TXN_READ_ACCEL_P0 || kind == EVIST_TXN_READ_ACCEL_P2 ||
              kind == EVIST_TXN_WRITE_APP_P0 || kind == EVIST_TXN_WRITE_APP_P2))
        cache_block(b, kind, data);
}

void dpc245_ble_app_apply_advanced_cache(const dpc245_ble_app_t *b, evist_advanced_t *adv) {
    if (!b || !adv || adv->busy) return;
    if (adv->screen == EVIST_ADV_ASSIST && !adv->edit && !adv->assist_dirty &&
        adv->request != EVIST_ADV_REQ_WRITE_ASSIST && b->download_status[5] == 3u) {
        copy_bytes(adv->assist_block, b->p1, 64u);
        copy_bytes(adv->assist_committed, b->p1, 64u);
        adv->assist_valid = true;
    }
    if (adv->screen == EVIST_ADV_ACCEL && !adv->edit && !adv->accel_dirty && !adv->accel_valid) {
        if (!b->discovery_done) return;
        adv->accel_mode = b->per_level ? EVIST_ACCEL_PER_LEVEL : EVIST_ACCEL_GLOBAL;
        if (b->per_level && b->download_status[4] == 3u) {
            copy_bytes(adv->accel_block, b->p0, 64u);
            copy_bytes(adv->accel_committed, b->p0, 64u);
            for (uint8_t i = 0u; i < 9u; ++i) adv->accel_values[i] = b->p0[1u + i];
            adv->accel_valid = true;
        } else if (!b->per_level && b->download_status[6] == 3u) {
            copy_bytes(adv->accel_block, b->p2, 64u);
            copy_bytes(adv->accel_committed, b->p2, 64u);
            adv->accel_global = b->p2[54];
            adv->accel_valid = true;
        }
    }
}

static uint8_t requested_chunk_len(const dpc245_ble_frame_t *q, size_t total) {
    if (!q || total == 0u) return 0u;
    size_t offset = q->selector;
    if (offset >= total) return 0u;
    size_t want = q->length != 0u ? q->payload[0] : 0u;
    if (want == 0u || want > DPC245_BLE_MAX_PAYLOAD) want = total - offset;
    if (want > total - offset) want = total - offset;
    if (want > DPC245_BLE_MAX_PAYLOAD) want = DPC245_BLE_MAX_PAYLOAD;
    return (uint8_t)want;
}

static bool read_block(dpc245_ble_app_result_t *r, const dpc245_ble_frame_t *q,
                       const uint8_t *block, size_t total) {
    if (!r || !q || !block) return false;
    uint8_t n = requested_chunk_len(q, total);
    if (n == 0u) return false;
    return build_reply(r, q, 0x04u, &block[q->selector], n) != 0u;
}

static bool write_block(dpc245_ble_app_result_t *r, const dpc245_ble_frame_t *q,
                        uint8_t *block, size_t total) {
    if (!r || !q || !block || q->length == 0u) return false;
    size_t offset = q->selector;
    if (offset >= total) return false;
    size_t n = q->length;
    if (n > total - offset) n = total - offset;
    copy_bytes(&block[offset], q->payload, n);
    uint8_t ok = 0u;
    return build_reply(r, q, 0x05u, &ok, 1u) != 0u;
}

static bool handle_10(dpc245_ble_app_t *b, const dpc245_ble_frame_t *q,
                      dpc245_app_t *app, dpc245_ble_app_result_t *r) {
    if (q->operation == 0x01u) {
        if (q->selector == 0x59u) {
            uint8_t v = app->runtime.settings.raw[0x2Eu];
            return build_reply(r, q, 0x04u, &v, 1u) != 0u;
        }
        if (q->selector == 0x80u) {
            uint8_t v = (uint8_t)((b->status_flags >> 3) & 1u);
            return build_reply(r, q, 0x04u, &v, 1u) != 0u;
        }
        if (q->selector < DPC245_SETTINGS_SIZE) {
            uint8_t v = app->runtime.settings.raw[q->selector];
            return build_reply(r, q, 0x04u, &v, 1u) != 0u;
        }
        return false;
    }
    if (q->operation == 0x02u) {
        if (q->selector == 0x59u && q->length >= 1u) {
            if (app->runtime.settings.raw[0x2Eu] != q->payload[0]) {
                app->runtime.settings.raw[0x2Eu] = q->payload[0];
                dpc245_settings_finalize(&app->runtime.settings);
                app->settings_dirty = true;
                r->settings_dirty = true;
            }
            uint8_t ok = 0u;
            return build_reply(r, q, 0x05u, &ok, 1u) != 0u;
        }
        if (q->selector == 0x80u && q->length >= 1u && q->payload[0] <= 1u) {
            if (q->payload[0]) b->status_flags |= 0x08u;
            else b->status_flags &= (uint8_t)~0x08u;
            uint8_t ok = 0u;
            return build_reply(r, q, 0x05u, &ok, 1u) != 0u;
        }
        return false;
    }
    if (q->operation == 0x20u && q->selector == 0u) {
        uint8_t v = 0u;
        return build_reply(r, q, 0x20u, &v, 1u) != 0u;
    }
    if (q->operation == 0x21u && q->selector == 2u) {
        b->status_flags = (uint8_t)((b->status_flags & 0x9Fu) | 0x20u);
        uint8_t ok = 0u;
        return build_reply(r, q, 0x05u, &ok, 1u) != 0u;
    }
    return false;
}

static bool handle_a5(dpc245_ble_app_t *b, const dpc245_ble_frame_t *q,
                      dpc245_app_t *app, const dpc245_hmi_identity_records_t *id,
                      bool light_on, dpc245_update_t *update,
                      dpc245_ble_app_result_t *r) {
    if (q->operation == 0x01u) {
        refresh_a5(b, app, id, light_on);
        return read_block(r, q, b->a5, sizeof(b->a5));
    }
    if (q->operation == 0x02u) {
        if (app->runtime.settings.raw[0x2Eu] != 0u) return false;
        if (q->selector == 0xA6u && q->length >= 1u) {
            if (q->payload[0] != 0u) b->status_flags |= 0x02u;
            else b->status_flags &= (uint8_t)~0x02u;
            if ((q->payload[0] & 1u) != 0u) b->status_flags |= 0x04u;
            else b->status_flags &= (uint8_t)~0x04u;
            b->pending_hmi_event_bits |= 0x08u;
        } else if (q->selector == 0xB8u && q->length >= 1u) {
            b->a5[165] = q->payload[0];
        } else if (q->selector == 0xB4u && q->length >= 1u) {
            b->a5[167] = q->payload[0] == 0xFFu ? 0u : q->payload[0];
        } else if (q->selector == 0xA4u && q->length >= 1u) {
            b->a5[164] = q->payload[0];
            if (q->payload[0] == 0xFFu) b->status_flags |= 0x08u;
            else b->status_flags &= (uint8_t)~0x08u;
        } else {
            return false;
        }
        uint8_t ok = 0u;
        return build_reply(r, q, 0x05u, &ok, 1u) != 0u;
    }
    if (q->operation == 0x30u && q->selector == 0u) {
        uint8_t ok = 0u;
        if (build_reply(r, q, 0x05u, &ok, 1u) == 0u) return false;
        r->update_requested = dpc245_update_request(update);
        return true;
    }
    return false;
}

void dpc245_ble_app_init(dpc245_ble_app_t *b) {
    if (!b) return;
    zero_bytes((uint8_t *)(void *)b, sizeof(*b));
}

bool dpc245_ble_app_handle(dpc245_ble_app_t *b,
                           const dpc245_ble_frame_t *request,
                           dpc245_app_t *app,
                           evist_can_router_t *router,
                           const dpc245_hmi_identity_records_t *hmi_identity,
                           bool light_on,
                           dpc245_update_t *update,
                           dpc245_ble_app_result_t *result) {
    if (!b || !request || !app || !router || !result) return false;
    zero_bytes((uint8_t *)(void *)result, sizeof(*result));
    if (request->class_id != 0x11u) {
        ++b->unsupported_frames;
        if (request->operation != 0x01u) {
            ++b->rejected_write_frames;
            b->rejected_class = request->class_id;
            b->rejected_command = request->command_id;
            b->rejected_operation = request->operation;
            b->rejected_selector = request->selector;
            b->rejected_reason = DPC245_BLE_REJECT_CLASS;
        }
        return false;
    }

    bool handled = false;
    dpc245_ble_reject_t reject = DPC245_BLE_REJECT_UNSUPPORTED;
    switch (request->command_id) {
    case 0x10u:
        handled = handle_10(b, request, app, result);
        break;
    case 0xA1u:
        if (request->operation == 0x01u && (request->selector == 0u || request->selector == 4u))
            handled = read_block(result, request, b->a1, sizeof(b->a1));
        break;
    case 0xA2u:
        if (request->operation == 0x01u && request->selector == 0u)
            handled = read_block(result, request, b->a2, sizeof(b->a2));
        break;
    case 0xA3u:
        if (request->operation == 0x01u) {
            refresh_a3(b, app, router);
            handled = read_block(result, request, b->a3, sizeof(b->a3));
        } else if (request->operation == 0x02u) {
#if EVIST_ENABLE_CONFIG_WRITES
            if (b->download_status[5] != 3u) {
                reject = DPC245_BLE_REJECT_READ_REQUIRED; break;
            }
            if (b->pending_write || b->controller_txn_active) {
                reject = DPC245_BLE_REJECT_BUSY; break;
            }
            if (b->download_status[6] != 3u)
                for (uint8_t i = 0u; i < 9u; ++i) {
                    b->a3[215u + i] = b->p1[49u + i];
                    b->a3[225u + i] = b->p1[40u + i];
                }
#endif
            reject = DPC245_BLE_REJECT_PAYLOAD;
            handled = write_block(result, request, b->a3, sizeof(b->a3));
            if (handled) {
#if EVIST_ENABLE_CONFIG_WRITES
                handled = queue_motor_write(b, 1u, app->advanced.committed.firmware_profile);
                if (!handled) {
                    result->have_tx = false;
                    reject = DPC245_BLE_REJECT_POLICY;
                }
#endif
            }
        }
        break;
    case 0xA4u:
        if (request->operation == 0x01u)
            handled = read_block(result, request, b->a4, sizeof(b->a4));
        else if (request->operation == 0x02u)
            handled = write_block(result, request, b->a4, sizeof(b->a4));
        break;
    case 0xA5u:
        handled = handle_a5(b, request, app, hmi_identity, light_on, update, result);
        break;
    case 0xA7u:
        if (request->operation == 0x01u)
            handled = read_block(result, request, b->a7, sizeof(b->a7));
        else if (request->operation == 0x02u)
            handled = write_block(result, request, b->a7, sizeof(b->a7));
        break;
    case 0xA9u:
        if (request->operation == 0x01u) {
            refresh_a9(b, app);
            handled = read_block(result, request, b->a9, sizeof(b->a9));
        } else if (request->operation == 0x02u) {
            if (!b->per_level) { reject = DPC245_BLE_REJECT_GLOBAL_MODE; break; }
#if EVIST_ENABLE_CONFIG_WRITES
            if (b->download_status[4] != 3u) {
                reject = DPC245_BLE_REJECT_READ_REQUIRED; break;
            }
            if (b->pending_write || b->controller_txn_active) {
                reject = DPC245_BLE_REJECT_BUSY; break;
            }
#endif
            reject = DPC245_BLE_REJECT_PAYLOAD;
            handled = write_block(result, request, b->a9, sizeof(b->a9));
            if (handled) {
#if EVIST_ENABLE_CONFIG_WRITES
                handled = queue_motor_write(b, 2u, app->advanced.committed.firmware_profile);
                if (!handled) {
                    result->have_tx = false;
                    reject = DPC245_BLE_REJECT_POLICY;
                }
#endif
            }
        }
        break;
    default:
        break;
    }

    result->handled = handled;
    if (handled) ++b->handled_frames;
    else {
        ++b->unsupported_frames;
        if (request->operation != 0x01u) {
            ++b->rejected_write_frames;
            b->rejected_class = request->class_id;
            b->rejected_command = request->command_id;
            b->rejected_operation = request->operation;
            b->rejected_selector = request->selector;
            b->rejected_reason = reject;
        }
    }
    return handled;
}

size_t dpc245_ble_app_build_periodic(uint8_t *out, size_t capacity,
                                     const dpc245_app_t *app,
                                     const evist_can_router_t *router,
                                     bool light_on) {
    if (!out || !app || !router) return 0u;
    uint8_t p[39];
    zero_bytes(p, sizeof(p));
    const dpc245_settings_t *s = &app->runtime.settings;
    const bafang_telemetry_t *t = &router->telemetry;

    p[3] = 1u;
    p[4] = light_on ? 1u : 0u;
    if (s->raw[0x2Eu] == 0u)
        p[5] = app->walk ? 0xFFu : app->assist_level;
    p[6] = 5u;
    p[7] = t->have_battery_pct ? t->battery_pct : 0u;
    if (t->have_speed) put_u16le(&p[9], (uint16_t)((t->speed_centi_kph / 10u) * 10u));
    put_u32le(&p[11], dpc245_get_u32_le(s, DPC245_OFS_TRIP_DISTANCE_RAW) / 10u);
    uint16_t calories = (uint16_t)dpc245_runtime_calories_kcal(&app->runtime);
    put_u16le(&p[15], calories);
    if (t->have_range) put_u16le(&p[17], t->range_centi_km);
    put_u16le(&p[19], (uint16_t)((dpc245_get_u32_le(s, DPC245_OFS_TOTAL_DISTANCE_RAW) + 50u) / 100u));
    uint32_t trip_minutes = dpc245_get_u32_le(s, DPC245_OFS_TRIP_TIME_HALFSEC) / 120u;
    p[21] = (uint8_t)trip_minutes;
    p[23] = s->raw[0x2Eu];
    put_u16le(&p[24], calories);
    put_u32le(&p[28], trip_minutes);
    p[32] = dpc245_unit(s) ? 1u : 0u;
    {
        uint16_t pedal;
        if (dpc245_runtime_pedal_avg(&app->runtime, &pedal))
            put_u16le(&p[26], (uint16_t)(0x8000u |
                                  (pedal > 0x7FFFu ? 0x7FFFu : pedal)));
    }
    put_u16le(&p[35], dpc245_get_u16_le(s, DPC245_OFS_AVG_SPEED_RAW));
    put_u16le(&p[37], dpc245_get_u16_le(s, DPC245_OFS_MAX_SPEED_RAW));

    return dpc245_ble_frame_build(out, capacity, 0x10u, 0x11u, 0x06u, 0x01u,
                                  p, (uint8_t)sizeof(p));
}

uint8_t dpc245_ble_app_take_hmi_event_bits(dpc245_ble_app_t *b) {
    if (!b) return 0u;
    uint8_t v = b->pending_hmi_event_bits;
    b->pending_hmi_event_bits = 0u;
    return v;
}
