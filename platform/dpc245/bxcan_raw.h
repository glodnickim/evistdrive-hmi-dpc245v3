/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef EVIST_BXCAN_RAW_H
#define EVIST_BXCAN_RAW_H
#include "bafang_can.h"
#include <stdbool.h>

/* DPC245 V3: CAN1 is STM32F1-compatible bxCAN at 0x40006400.
   Pin mux and bit timing are set up in dpc245_can_init.c. */
void dpc245_can_filter_accept_all(void);
bool dpc245_can_receive_poll(bafang_can_frame_t *out);
bool dpc245_can_transmit(const bafang_can_frame_t *f);
/* True when all three bxCAN transmit mailboxes are empty. */
bool dpc245_can_tx_idle(void);
#endif
