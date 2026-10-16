/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_adc.h"
#include <stddef.h>
#include <stdint.h>

#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define RCC_APB2ENR REG32(0x40021018u)
#define RCC_CFGR    REG32(0x40021004u)
#define GPIOA_CRL   REG32(0x40010800u)
#define ADC1_BASE   0x40012400u
#define ADC_SR      REG32(ADC1_BASE + 0x00u)
#define ADC_CR1     REG32(ADC1_BASE + 0x04u)
#define ADC_CR2     REG32(ADC1_BASE + 0x08u)
#define ADC_SMPR2   REG32(ADC1_BASE + 0x10u)
#define ADC_SQR1    REG32(ADC1_BASE + 0x2Cu)
#define ADC_SQR3    REG32(ADC1_BASE + 0x34u)
#define ADC_DR      REG32(ADC1_BASE + 0x4Cu)

static void short_delay(void) {
    for (volatile uint32_t n = 0; n < 256u; ++n) { __asm volatile ("nop"); }
}

bool dpc245_adc_init(void) {
    RCC_APB2ENR |= (1u << 2) | (1u << 9); /* GPIOA + ADC1 */

    /* PA1 and PA5: MODE=00, CNF=00 = analog input. */
    uint32_t crl = GPIOA_CRL;
    crl &= ~((0xFu << (1u * 4u)) | (0xFu << (5u * 4u)));
    GPIOA_CRL = crl;

    /* APB2 is 120 MHz. Divide by 8 for a 15 MHz ADC clock. */
    RCC_CFGR = (RCC_CFGR & ~(3u << 14)) | (3u << 14);

    ADC_CR1 = 0u;
    ADC_CR2 = 0u;
    ADC_SQR1 = 0u; /* one regular conversion */

    /* 55.5 cycle sample time for ch1 and ch5 (SMPR2 code 5). */
    ADC_SMPR2 = (ADC_SMPR2 & ~((7u << 3) | (7u << 15))) |
                (5u << 3) | (5u << 15);

    ADC_CR2 |= 1u << 0; /* ADON */
    short_delay();

    /* F1/GD32F30x compatible calibration sequence. */
    ADC_CR2 |= 1u << 3; /* RSTCAL */
    for (uint32_t n = 0; (ADC_CR2 & (1u << 3)) != 0u && n < 1000000u; ++n) {}
    if ((ADC_CR2 & (1u << 3)) != 0u) return false;
    ADC_CR2 |= 1u << 2; /* CAL */
    for (uint32_t n = 0; (ADC_CR2 & (1u << 2)) != 0u && n < 1000000u; ++n) {}
    if ((ADC_CR2 & (1u << 2)) != 0u) return false;

    return true;
}

uint16_t dpc245_adc_read_channel(uint8_t channel) {
    if (channel > 17u) return 0u;
    ADC_SQR3 = channel;
    ADC_SR = 0u;

    /* ADON is already set; on STM32F1 a second ADON starts a SW conversion
     * when EXTTRIG is not used. GD32F30x retains the compatible behavior. */
    ADC_CR2 |= 1u << 0;
    for (uint32_t n = 0; (ADC_SR & (1u << 1)) == 0u && n < 1000000u; ++n) {}
    return (uint16_t)(ADC_DR & 0x0FFFu);
}

void dpc245_adc_sample32(dpc245_adc_snapshot_t *s) {
    if (s == NULL) return;
    uint32_t als = 0u, btn = 0u;
    uint16_t mn = 0x0FFFu, mx = 0u;
    for (uint32_t i = 0; i < 32u; ++i) {
        uint16_t a = dpc245_adc_read_channel(5u);
        uint16_t b = dpc245_adc_read_channel(1u);
        als += a; btn += b;
        s->als_instant = a;
        s->button_signal_instant = b;
        if (b < mn) mn = b;
        if (b > mx) mx = b;
    }
    bool had_previous = s->valid;
    uint16_t seen_min = s->button_signal_seen_min;
    uint16_t seen_max = s->button_signal_seen_max;
    s->als_raw = (uint16_t)(als >> 5);
    s->button_signal_raw = (uint16_t)(btn >> 5);
    s->button_signal_min = mn;
    s->button_signal_max = mx;
    if (!had_previous) {
        s->button_signal_seen_min = mn;
        s->button_signal_seen_max = mx;
    } else {
        s->button_signal_seen_min = mn < seen_min ? mn : seen_min;
        s->button_signal_seen_max = mx > seen_max ? mx : seen_max;
    }
    s->valid = true;
    /* Deliberately leave temporal_valid unchanged/false: this helper is
     * a rapid diagnostic snapshot, not the 32 x 40 ms temporal filter. */
}

void dpc245_adc_window_filter_init(dpc245_adc_window_filter_t *f) {
    if (f == NULL) return;
    f->als_sum = 0u;
    f->button_sum = 0u;
    f->button_min = 0x0FFFu;
    f->button_max = 0u;
    f->count = 0u;
}

bool dpc245_adc_window_filter_tick(dpc245_adc_window_filter_t *f, dpc245_adc_snapshot_t *s) {
    if (f == NULL || s == NULL) return false;
    uint16_t a = dpc245_adc_read_channel(5u);
    uint16_t b = dpc245_adc_read_channel(1u);
    s->als_instant = a;
    s->button_signal_instant = b;

    if (!s->valid) {
        s->button_signal_seen_min = b;
        s->button_signal_seen_max = b;
        s->valid = true;
    } else {
        if (b < s->button_signal_seen_min) s->button_signal_seen_min = b;
        if (b > s->button_signal_seen_max) s->button_signal_seen_max = b;
    }

    f->als_sum += a;
    f->button_sum += b;
    if (b < f->button_min) f->button_min = b;
    if (b > f->button_max) f->button_max = b;
    ++f->count;
    s->window_samples = f->count;

    if (f->count < 32u) return false;

    s->als_raw = (uint16_t)(f->als_sum >> 5);
    s->button_signal_raw = (uint16_t)(f->button_sum >> 5);
    s->button_signal_min = f->button_min;
    s->button_signal_max = f->button_max;
    s->temporal_valid = true;
    s->window_samples = 0u;

    dpc245_adc_window_filter_init(f);
    return true;
}

bool dpc245_als_is_dark(uint16_t als_raw, uint8_t al_sensitivity) {
    uint32_t v = (uint32_t)als_raw + (uint32_t)al_sensitivity * 5u;
    return v >= 3200u;
}

bool dpc245_button_signal_filter(uint16_t raw, uint16_t *counter) {
    if (counter == NULL) return false;
    uint16_t c = *counter;
    if (raw > 32u) {
        if (c < 10u) ++c;
    } else {
        /* Decrement only once c >= 2. */
        if (c >= 2u) --c;
    }
    *counter = c;
    return c >= 10u;
}
