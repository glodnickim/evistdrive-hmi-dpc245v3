/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_RUNTIME_H
#define DPC245_RUNTIME_H

#include "bafang_can.h"
#include "dpc245_settings.h"
#include "dpc245_v3_profile.h"
#include <stdbool.h>
#include <stdint.h>

/* Ride-statistics state machine.
 * Persistent bytes remain canonical in dpc245_settings_t so serialization
 * uses the 73-byte persistent settings record. */
typedef struct {
    dpc245_settings_t settings;
    uint16_t previous_distance_counter;
    bool distance_counter_initialized;
    uint32_t previous_avg_distance_aux; /* distance_aux of the last AVG update */
    bool service_due; /* status bit2: raw18 >= raw1C */
    uint8_t controller_error;
    uint8_t sensor_error;
    uint8_t battery_error;
    bool have_controller_error;
    bool have_sensor_error;
    bool have_battery_error;
    uint16_t pedal_power_w;
    uint32_t rider_energy_half_joule;
} dpc245_runtime_t;

/* Persistent ride-statistic offsets. */
#define DPC245_OFS_TOTAL_DISTANCE_RAW   0x10u
#define DPC245_OFS_TRIP_DISTANCE_RAW    0x14u
#define DPC245_OFS_SERVICE_DISTANCE_RAW 0x18u
#define DPC245_OFS_SERVICE_TARGET_RAW   0x1Cu
#define DPC245_OFS_TRIP_TIME_HALFSEC    0x20u
#define DPC245_OFS_DISTANCE_AUX_RAW     0x24u
#define DPC245_OFS_TIME_AUX_HALFSEC     0x28u
#define DPC245_OFS_AVG_SPEED_RAW        0x2Cu
#define DPC245_OFS_MAX_SPEED_RAW        0x2Fu

void dpc245_runtime_init(dpc245_runtime_t *rt,
                               const dpc245_settings_t *settings);

/* Ride-stat update behavior. Distance counters use 0.01 km; raw2F/raw2C use 0.1 km/h.
 * FAST0 updates distance accumulators; FAST1 converts realtime 0.01 km/h to
 * 0.1 km/h, updates MAX, and computes
 * AVG = min(MAX, 720*distance_aux/time_halfsec) when distance_aux changes. */
bool dpc245_runtime_on_can(dpc245_runtime_t *rt,
                                  const bafang_can_frame_t *frame);

/* Display Setting -> Trip reset:
 * raw14/raw20/raw24/raw28/raw2F/raw2C are cleared; total/service raw10/raw18
 * remain intact. */
void dpc245_runtime_trip_reset(dpc245_runtime_t *rt);

/* The ride-time counters advance once per 500 ms while the activity gate is
 * true. The caller decides the gate (main.c: speed is non-zero). */
void dpc245_runtime_tick_500ms(dpc245_runtime_t *rt,
                                      bool activity_gate);

/* Map the persistent record to the raw inputs of the
 * 03106301/02/03/04 status frames. */
void dpc245_runtime_hmi_stats(const dpc245_runtime_t *rt,
                                    dpc245_v3_hmi_stats_raw_t *out);

/* service_due is set when accumulated service
 * distance reaches the configured target; raw18 is then clamped to raw1C. */
bool dpc245_runtime_service_due(const dpc245_runtime_t *rt);

/* The service reminder is shown only when
 * Display Setting -> Service tips (raw[0x09]) is enabled.  The accumulated
 * service counter is still maintained and clamped independently. */
bool dpc245_runtime_service_tip_active(const dpc245_runtime_t *rt);
uint32_t dpc245_runtime_calories_kcal(const dpc245_runtime_t *rt);

#endif
