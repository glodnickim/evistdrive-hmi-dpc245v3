/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_BACKLIGHT_MODEL_H
#define DPC245_BACKLIGHT_MODEL_H

#include <stdint.h>

/* User-visible levels 0..4 use a five-position profile.  Values
 * >=5 deliberately clamp to the brightest hardware position. */
uint8_t dpc245_backlight_level_to_position(uint8_t level);

/* The external controller advances only forward through positions 1..16 and
 * wraps 16 -> 1.  This returns the number of pulses required to reach target. */
uint8_t dpc245_backlight_forward_steps(uint8_t current, uint8_t target);

#endif
