/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_KEY_SAMPLER_H
#define DPC245_KEY_SAMPLER_H
#include "dpc245_keys.h"

#define DPC245_KEY_SAMPLER_CAPACITY 64u

typedef struct {
    uint32_t events;
    uint32_t time_ms;
} dpc245_key_sample_event_t;

/* Keep a sampled event no later than the foreground pass that handles it. */
uint32_t dpc245_key_time_for_pass(uint32_t sample_ms, uint32_t pass_ms);

typedef struct {
    dpc245_keys_t keys;
    dpc245_key_sample_event_t queue[DPC245_KEY_SAMPLER_CAPACITY];
    volatile uint8_t read;
    volatile uint8_t write;
    volatile bool overflow;
    volatile bool navigation_suppressed;
    volatile bool pending_down_release;
    volatile uint32_t pending_release_time_ms;
} dpc245_key_sampler_t;

void dpc245_key_sampler_init(dpc245_key_sampler_t *s, uint8_t raw, uint32_t now_ms);
/* Called every millisecond from the clock interrupt. */
void dpc245_key_sampler_tick(dpc245_key_sampler_t *s, uint8_t raw, uint32_t now_ms);
/* The consumer runs in the main loop. Interrupts must be masked while popping. */
bool dpc245_key_sampler_pop(dpc245_key_sampler_t *s, dpc245_key_sample_event_t *event);

#endif
