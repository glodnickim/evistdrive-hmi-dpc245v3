/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef EVIST_BAFANG_CONFIG_H
#define EVIST_BAFANG_CONFIG_H
#include "bafang_can.h"
#include "can_router.h"
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    uint16_t speed_limit_centi_kph;
    uint16_t wheel_size_tenth_in;
    uint16_t circumference_mm;
    bool have_readback;
    bool dirty;
} evist_bafang_config_t;
void evist_bafang_config_from_telemetry(evist_bafang_config_t *c,const bafang_telemetry_t *t);
bool evist_bafang_config_validate(const evist_bafang_config_t *c);
bool evist_bafang_config_build_write(const evist_bafang_config_t *c,
                                      const evist_can_router_t *router,
                                      bool explicit_confirm,
                                      bafang_can_frame_t *out);
#endif
