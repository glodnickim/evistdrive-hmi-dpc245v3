/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_tx.h"
#include "dpc245_v3_profile.h"
#include <stddef.h>

bool dpc245_build_periodic_frame(uint32_t can_id,
                                       const dpc245_app_t *app,
                                       bool light_on,
                                       uint8_t event_bits_v3,
                                       uint8_t status3_raw,
                                       bafang_can_frame_t *out) {
    if (!app || !out) return false;
    if (can_id == 0x03106300u) {
        return bafang_build_hmi_heartbeat_v3(out,
                                             app->assist_level_count,
                                             app->assist_level,
                                             app->walk,
                                             light_on,
                                             event_bits_v3,
                                             status3_raw);
    }

    dpc245_v3_hmi_stats_raw_t s;
    dpc245_runtime_hmi_stats(&app->runtime, &s);
    if (can_id == 0x03106304u) return dpc245_v3_build_status_frame_4(out, &s);
    if (can_id == 0x03106301u) return dpc245_v3_build_status_frame_1(out, &s);
    if (can_id == 0x03106302u) return dpc245_v3_build_status_frame_2(out, &s);
    if (can_id == 0x03106303u) return dpc245_v3_build_status_frame_3(out, &s);
    if (can_id == 0x03986207u) {
        out->id = 0x03986207u;
        out->dlc = 1u;
        out->data[0] = 0x0Au;
        for (uint8_t i = 1u; i < BAFANG_CAN_MAX_DLC; ++i) out->data[i] = 0u;
        return true;
    }
    return false;
}


typedef struct {
    uint8_t dst;
    uint8_t cmd;
    uint8_t sub;
} dpc245_query_spec_t;

/* Information-page READ sets.  H/W, S/W, Model and S/N are the four
 * identity fields shown on the Information pages.  Battery service
 * additionally requests charging data, the first eight cell voltages and the
 * characteristics block.  All are READ operations (type=1). */
static const dpc245_query_spec_t controller_queries[] = {
    {BAFANG_NODE_CONTROLLER, 0x60u, 0x00u},
    {BAFANG_NODE_CONTROLLER, 0x60u, 0x01u},
    {BAFANG_NODE_CONTROLLER, 0x60u, 0x02u},
    {BAFANG_NODE_CONTROLLER, 0x60u, 0x03u},
    {BAFANG_NODE_CONTROLLER, 0x60u, 0x04u},
    {BAFANG_NODE_CONTROLLER, 0x60u, 0x05u},
};
static const dpc245_query_spec_t sensor_queries[] = {
    {BAFANG_NODE_TORQUE_SENSOR, 0x60u, 0x00u},
    {BAFANG_NODE_TORQUE_SENSOR, 0x60u, 0x01u},
    {BAFANG_NODE_TORQUE_SENSOR, 0x60u, 0x02u},
    {BAFANG_NODE_TORQUE_SENSOR, 0x60u, 0x03u},
    {BAFANG_NODE_TORQUE_SENSOR, 0x60u, 0x04u},
    {BAFANG_NODE_TORQUE_SENSOR, 0x60u, 0x05u},
};
static const dpc245_query_spec_t battery_queries[] = {
    {BAFANG_NODE_BATTERY, 0x60u, 0x00u},
    {BAFANG_NODE_BATTERY, 0x60u, 0x01u},
    {BAFANG_NODE_BATTERY, 0x60u, 0x02u},
    {BAFANG_NODE_BATTERY, 0x60u, 0x03u},
    {BAFANG_NODE_BATTERY, 0x60u, 0x04u},
    {BAFANG_NODE_BATTERY, 0x60u, 0x05u},
    {BAFANG_NODE_BATTERY, 0x64u, 0x01u},
    {BAFANG_NODE_BATTERY, 0x64u, 0x02u}, /* cells 1..4 */
    {BAFANG_NODE_BATTERY, 0x64u, 0x03u}, /* cells 5..8 */
    {BAFANG_NODE_BATTERY, 0x64u, 0x15u},
};

static const dpc245_query_spec_t *query_set(dpc245_app_screen_t screen,
                                                   size_t *count) {
    if (count) *count = 0u;
    switch (screen) {
    case DPC245_APP_SCREEN_INFO_CONTROLLER:
        if (count) *count = sizeof(controller_queries) / sizeof(controller_queries[0]);
        return controller_queries;
    case DPC245_APP_SCREEN_INFO_SENSOR:
        if (count) *count = sizeof(sensor_queries) / sizeof(sensor_queries[0]);
        return sensor_queries;
    case DPC245_APP_SCREEN_INFO_BATTERY:
        if (count) *count = sizeof(battery_queries) / sizeof(battery_queries[0]);
        return battery_queries;
    default:
        return NULL;
    }
}

size_t dpc245_service_query_count(dpc245_app_screen_t screen) {
    size_t count = 0u;
    (void)query_set(screen, &count);
    return count;
}

bool dpc245_build_service_query(dpc245_app_screen_t screen,
                                      size_t ordinal,
                                      bafang_can_frame_t *out) {
    size_t count = 0u;
    const dpc245_query_spec_t *set = query_set(screen, &count);
    if (!out || !set || ordinal >= count) return false;
    return bafang_build_read_query(out, set[ordinal].dst, set[ordinal].cmd, set[ordinal].sub);
}
