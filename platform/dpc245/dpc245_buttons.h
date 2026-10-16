/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_BUTTONS_H
#define DPC245_BUTTONS_H
#include <stdint.h>
#include "dpc245_key_sampler.h"

#define DPC245_KEY_DOWN  0x01u /* PA4, DOWN */
#define DPC245_KEY_UP    0x02u /* PA3, UP */
#define DPC245_KEY_POWER 0x04u /* PA2, POWER/ENTER */

/* Positional aliases. */
#define DPC245_KEY_A DPC245_KEY_DOWN
#define DPC245_KEY_B DPC245_KEY_UP
#define DPC245_KEY_C DPC245_KEY_POWER

void dpc245_buttons_init(void);
uint8_t dpc245_buttons_raw(void);

/* 1 ms sampler used from SysTick. It retains press and release edges across
 * foreground passes. */
void dpc245_buttons_tick_1ms(void);
uint8_t dpc245_buttons_state(void);
uint8_t dpc245_buttons_take_pressed(void);
uint8_t dpc245_buttons_take_released(void);
void dpc245_buttons_key_sampler_start(dpc245_key_sampler_t *sampler, uint32_t now_ms);
bool dpc245_buttons_key_event(dpc245_key_sample_event_t *event);
#ifdef DPC245_BUTTONS_HOST_TEST
void dpc245_buttons_host_raw_set(uint8_t raw);
#endif

#endif
