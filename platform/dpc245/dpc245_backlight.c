/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_backlight.h"
#include "dpc245_backlight_model.h"
#include "dpc245_board.h"
#include <stdbool.h>
#include <stdint.h>

#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define GPIOB 0x40010C00u
#define RCC_APB2ENR REG32(0x40021018u)
#define GPIO_CRH REG32(GPIOB+0x04u)
#define GPIO_BSRR REG32(GPIOB+0x10u)
#define GPIO_BRR REG32(GPIOB+0x14u)

static uint8_t current_level;
static uint8_t current_position;
static bool initialized;

static void pin_high(void) { GPIO_BSRR = 1u << DPC245_BACKLIGHT_PIN; }
static void pin_low(void) { GPIO_BRR = 1u << DPC245_BACKLIGHT_PIN; }

/* Bounded delay between PB11 transitions for the pulse controller.
 * The timing is kept in the dedicated driver. */
#if defined(__arm__) || defined(__thumb__)
__attribute__((naked, noinline)) static void pulse_delay(uint32_t iterations __attribute__((unused))) {
    __asm__ volatile(
        "movs r1, #0\n"
        "b 2f\n"
        "1: adds r1, r1, #1\n"
        "2: cmp r1, r0\n"
        "bcc 1b\n"
        "bx lr\n");
}
#else
/* Host syntax check only; the target waveform is the ARM sequence above. */
static __attribute__((noinline)) void pulse_delay(uint32_t iterations) {
    for (uint32_t i = 0u; i < iterations; ++i) { __asm__ volatile("" ::: "memory"); }
}
#endif

static void emit_forward_pulse(void) {
    pin_low();
    pulse_delay(2u);
    pin_high();
    pulse_delay(2u);
}

static void set_position_internal(uint8_t target) {
    if (target < 1u) target = 1u;
    if (target > 16u) target = 16u;

    if (!initialized) {
        /* Power-up establishes position 1, then advances to the requested
         * position in the same call. */
        pin_high();
        pulse_delay(20u);
        current_position = 1u;
        for (uint8_t i = 1u; i < target; ++i) emit_forward_pulse();
        current_position = target;
        pulse_delay(2000u);
        initialized = true;
        return;
    }

    const uint8_t pulses = dpc245_backlight_forward_steps(current_position, target);
    if (pulses == 0u) return;
    for (uint8_t i = 0u; i < pulses; ++i) emit_forward_pulse();
    current_position = target;
    pulse_delay(2000u);
}

static void pin_output(void) {
    RCC_APB2ENR |= (1u << 3); /* IOPBEN */
    const unsigned shift = (DPC245_BACKLIGHT_PIN - 8u) * 4u;
    uint32_t v = GPIO_CRH;
    v &= ~(0xFu << shift);
    v |= (0x3u << shift);
    GPIO_CRH = v;
}

void dpc245_backlight_off(void) {
    RCC_APB2ENR |= (1u << 3); /* IOPBEN */
    pin_low(); /* Set the output latch before changing PB11 to output. */
    pin_output();
    pin_low();
    current_level = 0u;
    current_position = 0u;
    initialized = false;
}

void dpc245_backlight_init(void) {
    pin_output();
    pin_high();
    current_level = 0u;
    current_position = 1u;
    initialized = false;
}

void dpc245_backlight_set_level(uint8_t level) {
    const uint8_t target = dpc245_backlight_level_to_position(level);
    set_position_internal(target);
    current_level = level > 4u ? 4u : level;
}

void dpc245_backlight_set_position(uint8_t position) {
    set_position_internal(position);
}

uint8_t dpc245_backlight_get_level(void) { return current_level; }
uint8_t dpc245_backlight_get_position(void) { return current_position; }
