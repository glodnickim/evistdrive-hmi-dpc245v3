/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_board.h"
#include <stdint.h>

#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define RCC_APB2ENR REG32(0x40021018u)
#define AFIO_PCF0   REG32(DPC245_AFIO_BASE + 0x04u)
#define GPIOA_CTL0  REG32(DPC245_GPIOA_BASE + 0x00u)
#define GPIOA_ISTAT REG32(DPC245_GPIOA_BASE + 0x08u)
#define GPIOA_OCTL  REG32(DPC245_GPIOA_BASE + 0x0Cu)

void dpc245_debug_keep_swd_release_jtag(void) {
    /* RCU/RCC APB2 bit0 = AFIO clock. AFIO_PCF0[26:24] = 0b010 means
     * JTAG-DP disabled, SW-DP enabled on GD32F30x. */
    RCC_APB2ENR |= 1u << 0;
    uint32_t v = AFIO_PCF0;
    v &= ~(7u << 24);
    v |=  (2u << 24);
    AFIO_PCF0 = v;
}

void dpc245_unknown_pa0_init_pullup(void) {
    RCC_APB2ENR |= 1u << 2; /* GPIOA */
    uint32_t v = GPIOA_CTL0;
    v &= ~0xFu;
    v |=  0x8u; /* input with pull-up/down */
    GPIOA_CTL0 = v;
    GPIOA_OCTL |= 1u << DPC245_UNKNOWN_PA0_PIN; /* select pull-up */
}

bool dpc245_unknown_pa0_high(void) {
    return (GPIOA_ISTAT & (1u << DPC245_UNKNOWN_PA0_PIN)) != 0u;
}
