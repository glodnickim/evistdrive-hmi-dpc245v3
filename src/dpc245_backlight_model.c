/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_backlight_model.h"

static uint8_t clamp_position(uint8_t position) {
    if (position < 1u) return 1u;
    if (position > 16u) return 16u;
    return position;
}

uint8_t dpc245_backlight_level_to_position(uint8_t level) {
    if (level >= 5u) return 1u;
    if (level == 0u) return 16u;
    return (uint8_t)(17u - 4u * level); /* 1=>13, 2=>9, 3=>5, 4=>1 */
}

uint8_t dpc245_backlight_forward_steps(uint8_t current, uint8_t target) {
    current = clamp_position(current);
    target = clamp_position(target);
    if (target >= current) return (uint8_t)(target - current);
    return (uint8_t)(target + 16u - current);
}
