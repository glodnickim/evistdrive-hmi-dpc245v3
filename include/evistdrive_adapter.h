/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef EVISTDRIVE_ADAPTER_H
#define EVISTDRIVE_ADAPTER_H

#include "bafang_can.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    EVIST_MOTOR_UNKNOWN = 0,
    EVIST_MOTOR_BAFANG_M820,
    EVIST_MOTOR_BAFANG_GENERIC_CAN
} evist_motor_family_t;

typedef struct {
    evist_motor_family_t family;
    const char *name;
    bool (*on_frame)(const bafang_can_frame_t *f, bafang_telemetry_t *telemetry);
} evist_motor_adapter_t;

const evist_motor_adapter_t *evist_adapter_bafang_m820(void);

#endif
