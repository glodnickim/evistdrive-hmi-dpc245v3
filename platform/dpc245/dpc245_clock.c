/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_clock.h"
#include <stdint.h>

#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))

#define RCU_CTL      REG32(0x40021000u)
#define RCU_CFG0     REG32(0x40021004u)
#define RCU_APB1EN   REG32(0x4002101Cu)
#define RCU_CFG1     REG32(0x4002102Cu)

#define PMU_CTL      REG32(0x40007000u)
#define PMU_CS       REG32(0x40007004u)


#define RCU_CTL_HXTALEN  (1u << 16)
#define RCU_CTL_HXTALSTB (1u << 17)
#define RCU_CTL_PLLEN    (1u << 24)
#define RCU_CTL_PLLSTB   (1u << 25)
#define RCU_APB1EN_PMUEN (1u << 28)

#define RCU_CFG0_SCS_MASK      (3u << 0)
#define RCU_CFG0_SCSS_MASK     (3u << 2)
#define RCU_CFG0_APB1PSC_MASK  (7u << 8)
#define RCU_CFG0_APB2PSC_MASK  (7u << 11)
#define RCU_CFG0_PLLSEL        (1u << 16)
#define RCU_CFG0_PREDV0        (1u << 17)
/* PLLMF[3:0] at bits 21..18, PLLMF[4] at bit 27, PLLMF[5] at bit 30; the
 * multiplier is PLLMF + 2 below 15. The application configures
 * PLLMF = 13 (bits 27/30 clear) = x15 from HXTAL / 2. The board crystal is
 * 16 MHz (HXTAL at 16 MHz), so 8 MHz x 15 = 120 MHz. Setting
 * bit 27 as well (x30) would run the core at 240 MHz.
 * The configured multiplier keeps the core at 120 MHz. */
#define RCU_CFG0_PLLMF_MASK    ((0xFu << 18) | (1u << 27) | (1u << 30))
#define RCU_CFG0_PLL_MUL15     (13u << 18)
#define RCU_CFG0_APB1_DIV2     (4u << 8)
#define RCU_CFG0_SYS_PLL       2u
#define RCU_CFG0_SCSS_PLL      (2u << 2)

#define PMU_CTL_LDOVS          (3u << 14)
#define PMU_CTL_HDEN           (1u << 16)
#define PMU_CTL_HDS            (1u << 17)
#define PMU_CS_HDRF            (1u << 16)
#define PMU_CS_HDSRF           (1u << 17)


static bool wait_mask(volatile uint32_t *reg, uint32_t mask, uint32_t expected) {
    for (uint32_t n = 0u; n < 4000000u; ++n) {
        if ((*reg & mask) == expected) return true;
    }
    return false;
}

dpc245_clock_tree_t dpc245_clock_tree_now(void) {
    return dpc245_clock_classify(RCU_CTL, RCU_CFG0, RCU_CFG1);
}

dpc245_clock_tree_t dpc245_clock_fallback_hsi(void) {
    const dpc245_rcu_regs_t rcu = {&RCU_CTL, &RCU_CFG0, &RCU_CFG1};
    return dpc245_clock_force_hsi(&rcu, DPC245_EARLY_CLOCK_SPIN_LIMIT);
}

bool dpc245_clock_init(void) {
    RCU_CTL |= RCU_CTL_HXTALEN;
    if (!wait_mask((volatile uint32_t *)(uintptr_t)0x40021000u,
                   RCU_CTL_HXTALSTB, RCU_CTL_HXTALSTB)) return false;

    /* Clock initialization leaves the flash wait-state registers at their
     * reset values; so does this one. */

    RCU_APB1EN |= RCU_APB1EN_PMUEN;
    PMU_CTL |= PMU_CTL_LDOVS;

    uint32_t cfg = RCU_CFG0;
    cfg &= ~(0xFu << 4);            /* AHB /1 */
    cfg &= ~RCU_CFG0_APB1PSC_MASK;
    cfg |= RCU_CFG0_APB1_DIV2;      /* APB1 /2 */
    cfg &= ~RCU_CFG0_APB2PSC_MASK;  /* APB2 /1 */
    cfg &= ~(RCU_CFG0_PLLSEL | RCU_CFG0_PREDV0 | RCU_CFG0_PLLMF_MASK);
    cfg |= RCU_CFG0_PLLSEL | RCU_CFG0_PREDV0 | RCU_CFG0_PLL_MUL15;
    RCU_CFG0 = cfg;

    RCU_CTL |= RCU_CTL_PLLEN;
    if (!wait_mask((volatile uint32_t *)(uintptr_t)0x40021000u,
                   RCU_CTL_PLLSTB, RCU_CTL_PLLSTB)) return false;

    PMU_CTL |= PMU_CTL_HDEN;
    if (!wait_mask((volatile uint32_t *)(uintptr_t)0x40007004u,
                   PMU_CS_HDRF, PMU_CS_HDRF)) return false;
    PMU_CTL |= PMU_CTL_HDS;
    if (!wait_mask((volatile uint32_t *)(uintptr_t)0x40007004u,
                   PMU_CS_HDSRF, PMU_CS_HDSRF)) return false;

    RCU_CFG0 = (RCU_CFG0 & ~RCU_CFG0_SCS_MASK) | RCU_CFG0_SYS_PLL;
    return wait_mask((volatile uint32_t *)(uintptr_t)0x40021004u,
                     RCU_CFG0_SCSS_MASK, RCU_CFG0_SCSS_PLL);
}
