/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_HMI_CONTROL_H
#define DPC245_HMI_CONTROL_H

#include "bafang_can.h"
#include "dpc245_rtc.h"
#include "dpc245_runtime.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool handled;
    bool settings_dirty;
    bool rtc_updated;
    bool board_event_65_00;
    bafang_can_frame_t reply;
    bool have_reply;
} dpc245_hmi_control_result_t;

/* Process write/control requests addressed directly to the HMI. This module
 * owns only commands that mutate HMI-local state; controller configuration
 * writes remain in controller_write.c. */

/* Build direct NORMAL_ACK payloads for the HMI-local 0x63 READ group.
 * Supported records are the ride summary (01), average/service counters (02)
 * and service-target record (0B). */
bool dpc245_hmi_control_build_read_reply(const bafang_can_frame_t *request,
                                          const dpc245_runtime_t *runtime,
                                          bafang_can_frame_t *reply);

bool dpc245_hmi_control_process(const bafang_can_frame_t *request,
                                dpc245_runtime_t *runtime,
                                const dpc245_rtc_io_t *rtc_io,
                                dpc245_hmi_control_result_t *result);

#ifdef __cplusplus
}
#endif
#endif
