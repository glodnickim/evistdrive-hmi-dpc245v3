/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Host stand-in for the ST7789 panel: the real drawing code in src/ui_dashboard.c
 * calls these functions, and here they write into a 320x170 RGB565 buffer instead
 * of the bus. Pixel placement matches the window model of platform/dpc245. */

#include "st7789_8080.h"
#include "ui_strip.h"
#include <stdint.h>

uint16_t sim_fb[DPC245_LCD_HEIGHT][DPC245_LCD_WIDTH];

static uint16_t win_x0, win_y0, win_x1, win_y1, cur_x, cur_y;

void dpc245_lcd_bus_init(void) {}
void dpc245_lcd_init(void (*delay_ms)(uint32_t ms)) { (void)delay_ms; }
void dpc245_lcd_command(uint8_t cmd) { (void)cmd; }
void dpc245_lcd_data8(uint8_t value) { (void)value; }

void dpc245_lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    if (ui_strip_capturing()) { ui_strip_set_window(x0, y0, x1, y1); return; }
    win_x0 = x0; win_y0 = y0; win_x1 = x1; win_y1 = y1;
    cur_x = x0; cur_y = y0;
}

void dpc245_lcd_data16(uint16_t value) {
    if (ui_strip_capturing()) { ui_strip_data16(value); return; }
    if (cur_x < DPC245_LCD_WIDTH && cur_y < DPC245_LCD_HEIGHT)
        sim_fb[cur_y][cur_x] = value;
    if (cur_x >= win_x1) {
        cur_x = win_x0;
        cur_y = (cur_y >= win_y1) ? win_y0 : (uint16_t)(cur_y + 1u);
    } else {
        cur_x++;
    }
}

void dpc245_lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t rgb565) {
    if (ui_strip_capturing()) { ui_strip_fill_rect(x, y, w, h, rgb565); return; }
    if (!w || !h || x >= DPC245_LCD_WIDTH || y >= DPC245_LCD_HEIGHT) return;
    if ((uint32_t)x + w > DPC245_LCD_WIDTH) w = (uint16_t)(DPC245_LCD_WIDTH - x);
    if ((uint32_t)y + h > DPC245_LCD_HEIGHT) h = (uint16_t)(DPC245_LCD_HEIGHT - y);
    for (uint16_t j = 0u; j < h; ++j)
        for (uint16_t i = 0u; i < w; ++i)
            sim_fb[y + j][x + i] = rgb565;
}

void dpc245_lcd_fill(uint16_t rgb565) {
    dpc245_lcd_fill_rect(0u, 0u, DPC245_LCD_WIDTH, DPC245_LCD_HEIGHT, rgb565);
}

void dpc245_lcd_blit_rgb565(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *pixels) {
    if (ui_strip_capturing()) { ui_strip_blit(x, y, w, h, pixels); return; }
    if (!pixels || !w || !h) return;
    for (uint16_t j = 0u; j < h; ++j)
        for (uint16_t i = 0u; i < w; ++i) {
            uint32_t px = (uint32_t)x + i, py = (uint32_t)y + j;
            if (px < DPC245_LCD_WIDTH && py < DPC245_LCD_HEIGHT)
                sim_fb[py][px] = pixels[(uint32_t)j * w + i];
        }
}
