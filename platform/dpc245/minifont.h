/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef EVIST_MINIFONT_H
#define EVIST_MINIFONT_H
#include <stdint.h>

void ui_text_box(uint16_t x,uint16_t y,const char *s,uint16_t fg,uint16_t bg,
                 uint8_t glyph_w,uint8_t glyph_h,uint8_t advance);
void ui_text(uint16_t x,uint16_t y,const char *s,uint16_t fg,uint16_t bg,uint8_t scale);
void ui_u32(uint16_t x,uint16_t y,uint32_t value,uint16_t fg,uint16_t bg,uint8_t scale);
void ui_i32(uint16_t x,uint16_t y,int32_t value,uint16_t fg,uint16_t bg,uint8_t scale);
void ui_fixed_u32(uint16_t x,uint16_t y,uint32_t scaled_value,uint8_t decimals,uint16_t fg,uint16_t bg,uint8_t scale);
void ui_hex8(uint16_t x,uint16_t y,uint8_t value,uint16_t fg,uint16_t bg,uint8_t scale);
void ui_hex32(uint16_t x,uint16_t y,uint32_t value,uint8_t digits,uint16_t fg,uint16_t bg,uint8_t scale);
/* Any scale, no pixel buffer (block per font cell). Width is the advance sum. */
void ui_text_big(uint16_t x,uint16_t y,const char *s,uint16_t fg,uint16_t bg,uint8_t scale);
uint16_t ui_text_big_width(const char *s,uint8_t scale);
#endif
