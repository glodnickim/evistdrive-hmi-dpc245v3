/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_V3_PROFILE_H
#define DPC245_V3_PROFILE_H

#include "bafang_can.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Periodic transmit profile for the DPC245 C 3.0 protocol.
 * These entries describe when each status frame is due. They do not
 * enable TX by themselves and do not imply that every payload field is named. */
typedef struct {
    uint32_t can_id;
    uint16_t period_ms;
    uint8_t dlc;
    uint8_t frame_slot; /* Index within the DPC245 V3 periodic status-frame set. */
    bool transmitted; /* false: listed, never transmitted */
} dpc245_v3_periodic_can_t;

extern const dpc245_v3_periodic_can_t dpc245_v3_periodic_can[];
extern const size_t dpc245_v3_periodic_can_count;

#define DPC245_V3_HEARTBEAT_PERIOD_MS 100u
#define DPC245_V3_BLE_TELEMETRY_PERIOD_MS 500u

/* Periodic frames are checked from a 33 ms job, so a nominal period P is
 * emitted every ceil(P / 33) * 33 ms: 100 -> 132, 200 -> 231, 1000 -> 1023. */
#define DPC245_V3_TX_JOB_MS 33u
#define DPC245_V3_PERIODIC_MAX 8u

const dpc245_v3_periodic_can_t *dpc245_v3_periodic_find(uint32_t can_id);

typedef struct {
    bool started;
    uint32_t last_job_ms;
    uint32_t last_tx_ms[DPC245_V3_PERIODIC_MAX];
} dpc245_v3_tx_schedule_t;

void dpc245_v3_tx_schedule_init(dpc245_v3_tx_schedule_t *s, uint32_t now_ms);
/* Returns a bit mask of dpc245_v3_periodic_can[] entries due now (bit i =
 * entry i). Only transmitted entries are ever reported, and only on 33 ms job
 * boundaries. */
uint32_t dpc245_v3_tx_schedule_poll(dpc245_v3_tx_schedule_t *s, uint32_t now_ms);

/* Raw inputs of the periodic status frames. Each name gives the byte offset
 * of the value in the 73-byte settings record (dpc245_settings_t.raw), so the
 * serializers stay byte-exact without assigning UI labels.
 *
 * raw20_halfsec counts 500 ms steps while riding (divide by 120 for minutes).
 */
typedef struct {
    uint8_t raw06;
    uint8_t raw07;
    uint8_t raw0a;
    uint32_t raw10;
    uint32_t raw14;
    uint32_t raw18;
    uint32_t raw20_halfsec;
    uint32_t raw28_halfsec;
    uint16_t raw2c;
    uint16_t raw2f;
} dpc245_v3_hmi_stats_raw_t;

/* Periodic-frame serialization.
 * They only build frames; they never transmit them. */
bool dpc245_v3_build_status_frame_4(bafang_can_frame_t *out,
                                  const dpc245_v3_hmi_stats_raw_t *s); /* 03106304 */
bool dpc245_v3_build_status_frame_1(bafang_can_frame_t *out,
                                  const dpc245_v3_hmi_stats_raw_t *s); /* 03106301 */
bool dpc245_v3_build_status_frame_2(bafang_can_frame_t *out,
                                  const dpc245_v3_hmi_stats_raw_t *s); /* 03106302 */
bool dpc245_v3_build_status_frame_3(bafang_can_frame_t *out,
                                  const dpc245_v3_hmi_stats_raw_t *s); /* 03106303 */

#endif
