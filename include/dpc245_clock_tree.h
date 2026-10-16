/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_CLOCK_TREE_H
#define DPC245_CLOCK_TREE_H

/* Clock tree identification and the early return to the internal 8 MHz clock.
 *
 * The code after the clock setup (SysTick, CAN bit timing, UART baud rate)
 * is only correct for two trees, so the firmware never assumes which one is
 * running; it reads the RCU registers and classifies them:
 *
 *   HSI8          internal 8 MHz selected and confirmed, AHB/APB1/APB2 /1
 *   HXTAL_PLL     HXTAL 16 MHz / 2 x 15 = 120 MHz, AHB /1,
 *                 APB1 /2, APB2 /1 (CFG0 0x0037040A may be retained
 *                 by the resident updater)
 *   OTHER         anything else, including a requested switch that has not
 *                 completed (selected source != reported source)
 *
 * Pure logic on caller-supplied register pointers: usable before .data/.bss
 * exist and testable on the host with plain variables. */

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    DPC245_CLOCK_TREE_OTHER = 0,
    DPC245_CLOCK_TREE_HSI8,
    DPC245_CLOCK_TREE_HXTAL_PLL
} dpc245_clock_tree_t;

typedef struct {
    volatile uint32_t *ctl;   /* RCU_CTL  */
    volatile uint32_t *cfg0;  /* RCU_CFG0 */
    volatile uint32_t *cfg1;  /* RCU_CFG1 */
} dpc245_rcu_regs_t;

/* Polls allowed for HSI-ready and for the source switch; HSI is ready and the
 * switch completes within microseconds at any clock this core can run. */
#define DPC245_EARLY_CLOCK_SPIN_LIMIT 100000u

dpc245_clock_tree_t dpc245_clock_classify(uint32_t ctl, uint32_t cfg0, uint32_t cfg1);

/* Core clock of a supported tree, 0 for OTHER. */
uint32_t dpc245_clock_tree_core_hz(dpc245_clock_tree_t tree);

/* Return to HSI 8 MHz with all prescalers /1 and the PLL/HXTAL off. Each wait
 * is bounded by spin_limit. If HSI is not confirmed, nothing is changed and
 * the tree that is running is reported; if the switch does not confirm, the
 * result is OTHER. Returns the classification of the registers afterwards. */
dpc245_clock_tree_t dpc245_clock_force_hsi(const dpc245_rcu_regs_t *r, uint32_t spin_limit);

#endif
