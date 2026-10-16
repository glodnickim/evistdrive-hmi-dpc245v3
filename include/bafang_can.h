/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef BAFANG_CAN_H
#define BAFANG_CAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BAFANG_CAN_MAX_DLC 8u
#define BAFANG_NODE_TORQUE_SENSOR 0x01u
#define BAFANG_NODE_CONTROLLER    0x02u
#define BAFANG_NODE_HMI           0x03u
#define BAFANG_NODE_BATTERY       0x04u
#define BAFANG_NODE_BESST         0x05u
#define BAFANG_NODE_BROADCAST     0x1Fu

typedef struct {
    uint8_t src;
    uint8_t dst;
    uint8_t type;
    uint8_t cmd;
    uint8_t sub;
} bafang_can_id_t;

typedef struct {
    uint32_t id; /* 29-bit extended CAN ID */
    uint8_t dlc;
    uint8_t data[BAFANG_CAN_MAX_DLC];
} bafang_can_frame_t;

typedef enum {
    BAFANG_FRAME_UNKNOWN = 0,
    BAFANG_FRAME_CONTROLLER_FAST_0,
    BAFANG_FRAME_CONTROLLER_FAST_1,
    BAFANG_FRAME_CONTROLLER_FAST_2_RAW,
    BAFANG_FRAME_CONTROLLER_FAST_5_RAW,
    BAFANG_FRAME_CONTROLLER_CONFIG,
    BAFANG_FRAME_CONTROLLER_STATE,
    BAFANG_FRAME_TORQUE_SENSOR,
    BAFANG_FRAME_BATTERY_FAST_0_RAW,
    BAFANG_FRAME_BATTERY_FAST_1_RAW,
    BAFANG_FRAME_BATTERY_FAST_2_RAW,
    BAFANG_FRAME_HMI_HEARTBEAT
} bafang_frame_kind_t;

typedef struct {
    bool have_speed;
    bool have_current;
    bool have_voltage;
    bool have_controller_temp;
    bool have_controller_aux_temp_raw;
    bool have_motor_temp;
    bool have_battery_pct;
    bool have_distance_counter; /* DPC245 field is 16-bit data[1..2] */
    bool have_cadence;
    bool have_torque_signal;
    bool have_range;
    bool have_speed_limit;
    bool have_wheel_size;
    bool have_circumference;
    bool have_controller_state;
    bool have_controller_status_bit0;
    bool have_controller_fast5_raw;
    bool have_torque_sensor;
    bool have_battery_capacity;
    bool have_battery_state;
    bool have_battery_cells;

    uint16_t speed_centi_kph;
    int16_t current_centi_amp;
    uint16_t voltage_centi_volt;
    int16_t controller_temp_c;
    uint8_t controller_temp_raw; /* 0x3201 byte6, retained for warning diagnostics */
    uint8_t controller_aux_temp_raw; /* 0x3201 byte7 raw; 0 and 0xFF mean no motor sensor */
    int16_t motor_temp_c; /* 0x3201 byte7, decoded with the same -40 C offset */
    uint32_t controller_fast1_rx_ms; /* last complete 0x3201 frame */
    uint8_t battery_pct;
    uint16_t distance_counter_raw; /* 02F83200 data[1..2]; scaling applied by the runtime */
    uint8_t distance_10m_counter; /* low byte of distance_counter_raw; not used for ride accounting */
    uint8_t cadence_rpm;
    uint16_t torque_signal_mv;
    uint16_t range_centi_km;
    uint16_t speed_limit_centi_kph;
    uint16_t wheel_size_tenth_in; /* decoded human value: 290 = 29.0 in */
    uint16_t wheel_size_raw;      /* on-wire Bafang nibble format: 29.0 in = 0x01D0 */
    uint16_t circumference_mm;
    uint16_t torque_sensor_raw;
    uint8_t torque_sensor_cadence_rpm;
    uint8_t torque_sensor_progressive;

    /* Battery node command 0x34. Capacity values are kept raw (no display
     * scaling). Relative/absolute charge state and the electrical units are
     * decoded as below; no state-of-health byte is decoded. */
    uint16_t battery_full_capacity_raw;
    uint16_t battery_remaining_capacity_raw;
    uint8_t battery_rsoc_percent;
    uint8_t battery_asoc_percent;
    int16_t battery_current_centi_amp;
    uint16_t battery_voltage_centi_volt;
    int16_t battery_temp_c;
    uint16_t battery_cell_mv[16];
    uint8_t battery_cell_count;

    uint8_t controller_state_raw; /* generic 0x1200-family byte, semantics intentionally raw */
    uint8_t controller_fast2_raw; /* 0x3202 byte0; only bit0 is consumed by this HMI profile */
    uint32_t controller_fast2_rx_ms; /* receive time for freshness-gated Walk indication */
    bool controller_status_bit0;  /* physical meaning intentionally unnamed */
    uint16_t controller_fast5_raw; /* 0x3205 first 16 bits; diagnostic/profile dependent */
} bafang_telemetry_t;

uint32_t bafang_can_id_encode(bafang_can_id_t f);
bafang_can_id_t bafang_can_id_decode(uint32_t id);
bafang_frame_kind_t bafang_frame_classify(uint32_t id);
/* A temperature byte is unavailable at either end of its encoded range. */
bool bafang_temperature_raw_valid(uint8_t raw);
bool bafang_decode_telemetry(const bafang_can_frame_t *f, bafang_telemetry_t *t);


typedef enum {
    BAFANG_ASSIST_LEVELS_3 = 3,
    BAFANG_ASSIST_LEVELS_5 = 5,
    BAFANG_ASSIST_LEVELS_9 = 9
} bafang_assist_level_count_t;

/* Exact Bafang assist-code translation used by the HMI heartbeat.
 * walk=true forces wire code 0x06. Returns 0xFF for an unsupported
 * level-count or out-of-range user level. */
uint8_t bafang_assist_code(uint8_t level_count, uint8_t user_level, bool walk);

/* DPC245 V3 HMI cyclic status, ID 0x03106300, DLC 4.
 *
 * HMI periodic frame set:
 *   byte0 = configured assist level count (3/5/9)
 *   byte1 = Bafang assist wire code
 *   byte2 bit0 = light state
 *   byte2 bits1..3 = one-shot HMI event bits
 *   byte3 = raw status byte (normally 1)
 *
 * IMPORTANT: physical DPC245 UP/DOWN/POWER keys are NOT parameters here.
 * Their motor-side signalling is on the separate analogue Button Signal
 * wire, so the keys are deliberately not mapped to CAN bits. event_bits_v3 is
 * supplied only by code that implements a specified one-shot event
 * (BLE A5/A6, see dpc245_ble_app.h).
 */
bool bafang_build_hmi_heartbeat_v3(bafang_can_frame_t *out,
                                    uint8_t level_count,
                                    uint8_t user_level,
                                    bool walk,
                                    bool light_on,
                                    uint8_t event_bits_v3,
                                    uint8_t status3_raw);

/* Safe V3 convenience wrapper: no one-shot event bits and the default
 * data[3] = 1. Physical keys are intentionally not represented here. */
bool bafang_build_hmi_heartbeat_v3_default(bafang_can_frame_t *out,
                                            uint8_t level_count,
                                            uint8_t user_level,
                                            bool walk,
                                            bool light_on);

/* Read-only service query. This never changes controller configuration. */
bool bafang_build_read_query(bafang_can_frame_t *out,
                             uint8_t dst,
                             uint8_t cmd,
                             uint8_t sub);

/* Controller configuration write in the service-tool form (05103203).
   Intended for expert configuration mode with read-back verification. */
bool bafang_build_besst_controller_config_write(bafang_can_frame_t *out,
                                                 uint16_t speed_limit_centi_kph,
                                                 uint16_t wheel_size_tenth_in,
                                                 uint16_t circumference_mm);

/* Power derived from voltage/current telemetry, in W. */
uint16_t bafang_wheel_tenths_to_raw(uint16_t wheel_size_tenth_in);
uint16_t bafang_wheel_raw_to_tenths(uint16_t wheel_size_raw);

int32_t bafang_power_w(const bafang_telemetry_t *t);
bool bafang_walk_bit_fresh(const bafang_telemetry_t *t, uint32_t now_ms);

#ifdef __cplusplus
}
#endif
#endif
