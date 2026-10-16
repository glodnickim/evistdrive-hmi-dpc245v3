/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef EVIST_LIGHT_CONTROLLER_H
#define EVIST_LIGHT_CONTROLLER_H
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    EVIST_LIGHT_AUTO = 0,
    EVIST_LIGHT_MANUAL_OFF,
    EVIST_LIGHT_MANUAL_ON
} evist_light_mode_t;

typedef struct {
    evist_light_mode_t mode;
    uint8_t sensitivity;
    uint8_t gain;
    uint8_t dwell_limit;
    uint16_t threshold;
    uint8_t manual_level;
    bool light_on;
    uint16_t last_als_raw;
    uint16_t history0;
    uint16_t history1;
    uint16_t shaped_als;
    bool filter_valid;
    bool als_dark_known;
    bool als_dark;
    uint8_t dwell_count;
    uint8_t backlight_position;
} evist_light_controller_t;

void evist_light_init(evist_light_controller_t *c, uint8_t sensitivity);
void evist_light_configure(evist_light_controller_t *c, bool automatic,
                           uint8_t gain, uint8_t dwell, uint16_t threshold,
                           uint8_t manual_level);
void evist_light_set_mode(evist_light_controller_t *c, evist_light_mode_t mode);
bool evist_light_update_als(evist_light_controller_t *c, uint16_t als_raw);
bool evist_light_user_toggle(evist_light_controller_t *c);
uint8_t evist_light_backlight_position(const evist_light_controller_t *c);
#endif
