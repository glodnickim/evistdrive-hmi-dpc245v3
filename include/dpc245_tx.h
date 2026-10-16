/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_TX_H
#define DPC245_TX_H

#include "bafang_can.h"
#include "dpc245_app.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Pure frame serializer. It never touches CAN hardware. */
bool dpc245_build_periodic_frame(uint32_t can_id,
                                       const dpc245_app_t *app,
                                       bool light_on,
                                       uint8_t event_bits_v3,
                                       uint8_t status3_raw,
                                       bafang_can_frame_t *out);

/* Read-only Information-page query plan.  These frames cannot alter controller
 * configuration; active transmission is still target-gated by
 * DPC245_APP_SERVICE_TX.  The plan intentionally contains only
 * protocol fields with a defined meaning: identity and
 * battery service data/cells. */
size_t dpc245_service_query_count(dpc245_app_screen_t screen);
bool dpc245_build_service_query(dpc245_app_screen_t screen,
                                      size_t ordinal,
                                      bafang_can_frame_t *out);

#endif
