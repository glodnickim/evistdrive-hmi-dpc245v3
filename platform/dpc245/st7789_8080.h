/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_ST7789_8080_H
#define DPC245_ST7789_8080_H
#include <stdint.h>
#include <stddef.h>

#define DPC245_LCD_WIDTH  320u
#define DPC245_LCD_HEIGHT 170u

void dpc245_lcd_bus_init(void);
void dpc245_lcd_init(void (*delay_ms)(uint32_t ms));
void dpc245_lcd_command(uint8_t cmd);
void dpc245_lcd_data8(uint8_t value);
void dpc245_lcd_data16(uint16_t value);
void dpc245_lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void dpc245_lcd_fill(uint16_t rgb565);
void dpc245_lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t rgb565);
void dpc245_lcd_blit_rgb565(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                            const uint16_t *pixels);
#endif
