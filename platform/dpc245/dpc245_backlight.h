/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_BACKLIGHT_H
#define DPC245_BACKLIGHT_H
#include <stdint.h>
void dpc245_backlight_off(void);
void dpc245_backlight_init(void);
void dpc245_backlight_set_level(uint8_t level_0_to_4);
void dpc245_backlight_set_position(uint8_t position_1_to_16);
uint8_t dpc245_backlight_get_level(void);
uint8_t dpc245_backlight_get_position(void);
#endif
