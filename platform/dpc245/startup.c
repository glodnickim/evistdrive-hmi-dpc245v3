/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdint.h>
#include "app_config.h"
#include "dpc245_buttons.h"
#include "dpc245_clock.h"
#include "dpc245_clock_tree.h"
#include "dpc245_probe.h"
#include "dpc245_recovery.h"
#include "dpc245_recovery_trigger.h"

extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;
int main(void);

#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define SCB_VTOR REG32(0xE000ED08u)
#define RCC_CR REG32(0x40021000u)
#define RCC_CFGR REG32(0x40021004u)
#define RCC_APB2ENR REG32(0x40021018u)
#define RCC_CFG1 REG32(0x4002102Cu)
#define GPIOA_CRL REG32(0x40010800u)
#define GPIOA_IDR REG32(0x40010808u)
#define GPIOA_ODR REG32(0x4001080Cu)
#define SYST_CSR REG32(0xE000E010u)
#define SYST_RVR REG32(0xE000E014u)
#define SYST_CVR REG32(0xE000E018u)
#define DPC245_APP_BASE 0x08006000u
volatile uint32_t evist_ms_ticks;
uint8_t dpc245_early_clock_tree;

/* Every wait before main() is bounded: a register that never changes must not
 * be able to stall start-up ahead of the recovery code. One SysTick
 * period is 1 ms; 100000 polls is far above that at any supported clock. */
#define EARLY_MS_SPIN_LIMIT 100000u

/* Required by the resident DPC245 bootloader: a bootable application
 * carries 0xAA55 at absolute 0x0803FFFE. The linker pins this object to that
 * exact address. */
__attribute__((section(".app_boot_marker"), used, aligned(2)))
const uint16_t dpc245_app_boot_marker = DPC245_APP_MARKER_VALID;

/* Faults and unexpected interrupts reset the MCU instead of hanging: every
 * boot passes the early chord and the preflight recovery window again, so a
 * crashing application with a valid marker stays recoverable. */
void Default_Handler(void) { dpc245_system_reset(); }
void SysTick_Handler(void) { evist_ms_ticks++; dpc245_buttons_tick_1ms(); }

/* Resident updater may leave PLL/prescalers configured. Return to the
 * deterministic HSI/IRC8M 8 MHz tree, with every wait bounded, and report
 * which tree is actually running afterwards (dpc245_clock_tree.h). */
static dpc245_clock_tree_t clock_force_hsi8(void) {
    const dpc245_rcu_regs_t rcu = {&RCC_CR, &RCC_CFGR, &RCC_CFG1};
    return dpc245_clock_force_hsi(&rcu, DPC245_EARLY_CLOCK_SPIN_LIMIT);
}

/* NO-SWD recovery guard. This runs before .data/.bss initialization and before
 * LCD/CAN/BLE. Holding POWER+UP+DOWN through reset for about 1.5 s clears only
 * the APP-valid marker and resets into the resident bootloader. */
static void early_cfg_key_pullup(unsigned pin) {
    const unsigned sh = pin * 4u;
    uint32_t v = GPIOA_CRL;
    v &= ~(0xFu << sh);
    v |= 0x8u << sh; /* MODE=00, CNF=10: input pull-up/down */
    GPIOA_CRL = v;
    GPIOA_ODR |= 1u << pin; /* select pull-up */
}

static int early_all_keys_held(void) {
    const uint32_t mask = (1u << 2) | (1u << 3) | (1u << 4);
    return (GPIOA_IDR & mask) == 0u; /* PA2/PA3/PA4 are active-low */
}

/* core_hz is the confirmed clock of a supported tree (8 or 120 MHz). */
static void early_delay_ms(uint32_t ms, uint32_t core_hz) {
    SYST_RVR = core_hz / 1000u - 1u;
    SYST_CVR = 0u;
    SYST_CSR = 5u; /* ENABLE + processor clock, IRQ disabled */
    for (uint32_t i = 0; i < ms; ++i) {
        for (uint32_t n = 0u; n < EARLY_MS_SPIN_LIMIT && (SYST_CSR & (1u << 16)) == 0u; ++n) {}
    }
    SYST_CSR = 0u;
}

static void early_recovery_guard(uint32_t core_hz) {
    RCC_APB2ENR |= 1u << 2; /* IOPA / GPIOA clock */
    early_cfg_key_pullup(2u);
    early_cfg_key_pullup(3u);
    early_cfg_key_pullup(4u);

    if (!early_all_keys_held()) return;
    early_delay_ms(DPC245_RECOVERY_CHORD_MS, core_hz);
    if (early_all_keys_held() &&
        dpc245_marker_clear_retry(dpc245_boot_marker_clear, DPC245_MARKER_CLEAR_ATTEMPTS) != 0u) {
        dpc245_system_reset();
    }
}

void Reset_Handler(void) {
    /* Relocate VTOR to the application vector table before enabling interrupts. */
    SCB_VTOR = DPC245_APP_BASE;
#if EVIST_BUILD_PROFILE == EVIST_PROFILE_PROBE
    /* One-shot image: invalidate the marker before any wait, clock change or
     * peripheral access, so nothing it runs afterwards can keep it selected. */
    const uint8_t disarm_attempt = dpc245_marker_clear_retry(dpc245_boot_marker_clear,
                                                             DPC245_MARKER_CLEAR_ATTEMPTS);
#endif
    /* Only the two supported trees have known timings for SysTick, CAN and
     * the UART. Any other state (unconfirmed switch, foreign prescalers) is
     * left through a hardware reset, the one way to bring the RCU back to its
     * defined reset tree; every reset passes the resident updater first. */
    const dpc245_clock_tree_t clock_tree = clock_force_hsi8();
    const uint32_t core_hz = dpc245_clock_tree_core_hz(clock_tree);
    if (core_hz == 0u) dpc245_system_reset();
    early_recovery_guard(core_hz);

    uint32_t *s = &_sidata, *d = &_sdata;
    while (d < &_edata) *d++ = *s++;
    for (d = &_sbss; d < &_ebss;) *d++ = 0u;
    dpc245_early_clock_tree = (uint8_t)clock_tree;
#if EVIST_BUILD_PROFILE == EVIST_PROFILE_PROBE
    dpc245_probe_early_disarm_attempt = disarm_attempt;
#endif

    (void)main();
    for (;;) {}
}

#define D Default_Handler
__attribute__((section(".isr_vector"), used))
void (*const vectors[])(void) = {
    (void (*)(void))(&_estack), Reset_Handler,
    D,D,D,D,D,0,0,0,0,D,D,0,D,SysTick_Handler,
    D,D,D,D,D,D,D,D,D,D,D,D,D,D,D,D,
    D,D,D,D,D,D,D,D,D,D,D,D,D,D,D,D,
    D,D,D,D,D,D,D,D,D,D,D,D,D,D,D,D,
    D,D,D,D,D,D,D,D
};
