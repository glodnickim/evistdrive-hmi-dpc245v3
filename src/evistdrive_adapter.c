/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "evistdrive_adapter.h"

static bool m820_on_frame(const bafang_can_frame_t *f, bafang_telemetry_t *t) {
    /* Start from the common Bafang CAN dialect. M820-specific messages can be
       added here without contaminating the UI/router with motor-specific code. */
    return bafang_decode_telemetry(f, t);
}

const evist_motor_adapter_t *evist_adapter_bafang_m820(void) {
    static const evist_motor_adapter_t a = {
        .family = EVIST_MOTOR_BAFANG_M820,
        .name = "Bafang M820",
        .on_frame = m820_on_frame,
    };
    return &a;
}
