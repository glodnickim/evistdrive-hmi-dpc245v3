/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_ADC_H
#define DPC245_ADC_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t als_raw;                /* last completed temporal mean, PA5/ch5 */
    uint16_t button_signal_raw;      /* last completed temporal mean, PA1/ch1 */
    uint16_t als_instant;            /* most recent single PA5 conversion */
    uint16_t button_signal_instant;  /* most recent single PA1 conversion */
    uint16_t button_signal_min;      /* min inside last completed 32-sample window */
    uint16_t button_signal_max;      /* max inside last completed 32-sample window */
    uint16_t button_signal_seen_min; /* lifetime instantaneous min since boot */
    uint16_t button_signal_seen_max; /* lifetime instantaneous max since boot */
    uint8_t window_samples;           /* samples accumulated in current temporal window */
    bool valid;                      /* at least a diagnostic or temporal value exists */
    bool temporal_valid;         /* a full 32 x 40 ms temporal mean exists */
    bool button_signal_high;   /* defined PA1 threshold/debounce state */
} dpc245_adc_snapshot_t;

typedef struct {
    uint32_t als_sum;
    uint32_t button_sum;
    uint16_t button_min;
    uint16_t button_max;
    uint8_t count;
} dpc245_adc_window_filter_t;

bool dpc245_adc_init(void);
uint16_t dpc245_adc_read_channel(uint8_t channel);

/* Fast 32-conversion diagnostic snapshot. This is intentionally NOT the
 * time-domain filter: conversions are back-to-back so a value is available
 * immediately during first-flash bring-up. */
void dpc245_adc_sample32(dpc245_adc_snapshot_t *s);

/* Temporal sampler. Call once every 40 ms. It takes one PA5 and one PA1
 * conversion per call and publishes a new mean after 32 calls (~1.28 s).
 * Returns true exactly when a new temporal mean is published. */
void dpc245_adc_window_filter_init(dpc245_adc_window_filter_t *f);
bool dpc245_adc_window_filter_tick(dpc245_adc_window_filter_t *f, dpc245_adc_snapshot_t *s);

/* Ambient-light comparator: dark when
 *   avg(PA5) + 5 * AL_Sensitivity >= 3200
 * provided automatic-light mode is enabled by the caller. */
bool dpc245_als_is_dark(uint16_t als_raw, uint8_t al_sensitivity);

/* Defined PA1 digitalized state uses threshold 32 and a saturating
 * hysteresis counter. Kept separate from the raw analog value. */
bool dpc245_button_signal_filter(uint16_t raw, uint16_t *counter);

#endif
