/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef UI_SURFACE_H
#define UI_SURFACE_H

#include <stdint.h>

/* Drawing target for UI code. The firmware binds it to the LCD; host tests bind
 * it to an in-memory framebuffer so rendering can be compared pixel by pixel. */
typedef struct {
    void (*blit)(void *ctx, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                 const uint16_t *rgb565);
    void *ctx;
    uint16_t width;
    uint16_t height;
} ui_surface_t;

#endif
