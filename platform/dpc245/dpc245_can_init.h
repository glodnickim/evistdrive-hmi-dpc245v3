/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_CAN_INIT_H
#define DPC245_CAN_INIT_H
#include <stdbool.h>
/* CAN0: PA11 RX / PA12 TX, 250 kbit/s from 60 MHz APB1. */
bool dpc245_can_init_250k(void);
/* Same bus rate with APB1 = 8 MHz HSI (safe mode only). */
bool dpc245_can_init_250k_hsi(void);
#endif
