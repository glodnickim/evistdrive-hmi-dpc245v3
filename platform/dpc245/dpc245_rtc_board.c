/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_rtc_board.h"
#include "dpc245_board.h"
#include <stdbool.h>
#include <stdint.h>

#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define GPIOC_CRH  REG32(DPC245_GPIOC_BASE + 0x04u)
#define GPIOC_IDR  REG32(DPC245_GPIOC_BASE + 0x08u)
#define GPIOC_BSRR REG32(DPC245_GPIOC_BASE + 0x10u)
#define GPIOC_BRR  REG32(DPC245_GPIOC_BASE + 0x14u)
#define RTC_MASK (1u << DPC245_RTC_IO_PIN)

static void pc14_mode(uint32_t nibble) {
    uint32_t shift = (DPC245_RTC_IO_PIN - 8u) * 4u;
    uint32_t v = GPIOC_CRH;
    v &= ~(0xFu << shift);
    v |= (nibble & 0xFu) << shift;
    GPIOC_CRH = v;
}

static void pc14_output(void) {
    /* 2 MHz push-pull output. */
    pc14_mode(0x2u);
}

static void pc14_input_pullup(void) {
    /* Input with pull-up; on this GPIO generation the output latch selects
     * pull-up versus pull-down. */
    GPIOC_BSRR = RTC_MASK;
    pc14_mode(0x8u);
}

static void shift_byte(uint8_t value) {
    for (uint16_t mask = 1u; mask <= 0x80u; mask <<= 1u) {
        if ((value & (uint8_t)mask) == 0u) GPIOC_BRR = RTC_MASK;
    }
}

static bool board_write(void *ctx, uint8_t reg, uint8_t value) {
    (void)ctx;
    pc14_output();
    shift_byte(reg);
    shift_byte(value);
    return true;
}

static bool board_read(void *ctx, uint8_t reg, uint8_t *value) {
    (void)ctx;
    if (!value) return false;
    pc14_output();
    shift_byte(reg);
    pc14_input_pullup();
    uint8_t out = 0u;
    uint8_t mask = 1u;
    do {
        if ((GPIOC_IDR & RTC_MASK) != 0u) out |= mask;
        mask = (uint8_t)(mask << 1u);
    } while (mask != 0u);
    pc14_output();
    GPIOC_BRR = RTC_MASK;
    *value = out;
    return true;
}

dpc245_rtc_io_t dpc245_rtc_board_io(void) {
    dpc245_rtc_io_t io = {
        .ctx = 0,
        .read_register = board_read,
        .write_register = board_write
    };
    return io;
}
