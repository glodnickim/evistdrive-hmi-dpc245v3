/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_CLOCK_H
#define DPC245_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

/* HXTAL 16 MHz / 2 x 15 (120 MHz system target). */
#define DPC245_HXTAL_HZ   16000000u
#define DPC245_SYSCLK_HZ 120000000u
#define DPC245_AHB_HZ    120000000u
#define DPC245_APB2_HZ   120000000u
#define DPC245_APB1_HZ    60000000u

#include "dpc245_clock_tree.h"

bool dpc245_clock_init(void);

/* Classification of the live RCU registers (dpc245_clock_tree.h). */
dpc245_clock_tree_t dpc245_clock_tree_now(void);

/* After a failed dpc245_clock_init(): back to the clean HSI 8 MHz tree
 * (prescalers /1, PLL/HXTAL off), bounded; returns the tree now running. */
dpc245_clock_tree_t dpc245_clock_fallback_hsi(void);

/* Tree confirmed by Reset_Handler before main() (dpc245_clock_tree_t). */
extern uint8_t dpc245_early_clock_tree;

#endif
