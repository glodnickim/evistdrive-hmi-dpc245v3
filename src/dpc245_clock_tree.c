/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_clock_tree.h"

#define CTL_HSIEN      (1u << 0)
#define CTL_HSISTB     (1u << 1)
#define CTL_HXTALEN    (1u << 16)
#define CTL_HXTALSTB   (1u << 17)
#define CTL_CKMEN      (1u << 19)
#define CTL_PLLEN      (1u << 24)
#define CTL_PLLSTB     (1u << 25)

#define CFG0_SCS(v)    ((v) & 3u)
#define CFG0_SCSS(v)   (((v) >> 2) & 3u)
#define CFG0_AHB(v)    (((v) >> 4) & 0xFu)
#define CFG0_APB1(v)   (((v) >> 8) & 7u)
#define CFG0_APB2(v)   (((v) >> 11) & 7u)
#define CFG0_PLLSEL    (1u << 16)
#define CFG0_PREDV0    (1u << 17)
#define CFG0_PLLMF     ((0xFu << 18) | (1u << 27) | (1u << 30))
#define CFG0_PLL_MUL15 (13u << 18)
#define CFG1_PLLPRESEL (1u << 30)

#define SRC_HSI 0u
#define SRC_PLL 2u
#define APB_DIV1 0u
#define APB_DIV2 4u

dpc245_clock_tree_t dpc245_clock_classify(uint32_t ctl, uint32_t cfg0, uint32_t cfg1) {
    if (CFG0_SCS(cfg0) != CFG0_SCSS(cfg0)) return DPC245_CLOCK_TREE_OTHER; /* switch pending */
    if (CFG0_AHB(cfg0) != 0u || CFG0_APB2(cfg0) != APB_DIV1) return DPC245_CLOCK_TREE_OTHER;

    if (CFG0_SCSS(cfg0) == SRC_HSI && CFG0_APB1(cfg0) == APB_DIV1 && (ctl & CTL_HSISTB) != 0u)
        return DPC245_CLOCK_TREE_HSI8;

    const uint32_t pll = cfg0 & (CFG0_PLLSEL | CFG0_PREDV0 | CFG0_PLLMF);
    if (CFG0_SCSS(cfg0) == SRC_PLL && CFG0_APB1(cfg0) == APB_DIV2 &&
        pll == (CFG0_PLLSEL | CFG0_PREDV0 | CFG0_PLL_MUL15) &&
        (cfg1 & CFG1_PLLPRESEL) == 0u &&
        (ctl & (CTL_HXTALSTB | CTL_PLLSTB)) == (CTL_HXTALSTB | CTL_PLLSTB))
        return DPC245_CLOCK_TREE_HXTAL_PLL;

    return DPC245_CLOCK_TREE_OTHER;
}

uint32_t dpc245_clock_tree_core_hz(dpc245_clock_tree_t tree) {
    if (tree == DPC245_CLOCK_TREE_HSI8) return 8000000u;
    if (tree == DPC245_CLOCK_TREE_HXTAL_PLL) return 120000000u;
    return 0u;
}

static dpc245_clock_tree_t classify_now(const dpc245_rcu_regs_t *r) {
    return dpc245_clock_classify(*r->ctl, *r->cfg0, *r->cfg1);
}

dpc245_clock_tree_t dpc245_clock_force_hsi(const dpc245_rcu_regs_t *r, uint32_t spin_limit) {
    *r->ctl |= CTL_HSIEN;
    uint32_t n = 0u;
    while ((*r->ctl & CTL_HSISTB) == 0u) {
        if (++n >= spin_limit) return classify_now(r); /* untouched: report what runs */
    }

    *r->cfg0 &= ~3u;                                  /* select HSI */
    n = 0u;
    while (CFG0_SCSS(*r->cfg0) != SRC_HSI) {
        if (++n >= spin_limit) return classify_now(r); /* pending switch -> OTHER */
    }

    *r->cfg0 = 0u;                                    /* all prescalers /1 */
    *r->cfg1 = 0u;                                    /* PLL pre-source HXTAL, for the 120 MHz tree */
    *r->ctl &= ~(CTL_HXTALEN | CTL_CKMEN | CTL_PLLEN);
    return classify_now(r);
}
