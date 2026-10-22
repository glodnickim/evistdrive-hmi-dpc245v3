/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef UI_FONT1_H
#define UI_FONT1_H

/* Crisp 1-bit subset fonts for large dashboard text, drawn at their real size
 * (no scaling, no smoothing). Data is generated from tools/fonts/InterDisplay-Bold.ttf
 * (SIL OFL 1.1) by tools/gen_font1.ps1; see docs/BUILD_AND_RELEASE.md. */

#include <stdint.h>

typedef struct { uint8_t w, h; int8_t ox, oy; uint8_t adv; uint16_t off; } ui_font1_glyph_t;

typedef struct {
    const char *codes;               /* characters present, in glyph order */
    const ui_font1_glyph_t *glyphs;
    const uint8_t *bitmap;           /* rows MSB first, ceil(w/8) bytes per row */
    uint8_t count;
} ui_font1_t;

extern const ui_font1_t ui_font1_speed;   /* main speed digits */
extern const ui_font1_t ui_font1_speed_small; /* speed tenths */
extern const ui_font1_t ui_font1_arc_integer; /* ARC integer digits */
extern const ui_font1_t ui_font1_arc_tenth; /* ARC decimal and tenth */
extern const ui_font1_t ui_font1_badge;   /* mode letters */
extern const ui_font1_t ui_font1_badge_plus; /* reduced plus glyph */
extern const ui_font1_t ui_font1_value;   /* lower values, units, km/h */
extern const ui_font1_t ui_font1_small;   /* battery values */
extern const ui_font1_t ui_font1_menu;    /* menu rows, labels, values, hints (full ASCII) */
extern const ui_font1_t ui_font1_dashboard; /* dashboard labels and messages */
extern const ui_font1_t ui_font1_title;   /* screen titles, edit values (upper case) */

/* Sum of advances; characters missing from the font count as 0. */
uint16_t ui_font1_width(const ui_font1_t *f, const char *s);

/* Draws s with the pen at x and the baseline at y. Only glyph boxes are written;
 * the caller clears the background. Returns the pen position after the text. */
uint16_t ui_font1_text(const ui_font1_t *f, uint16_t x, int baseline, const char *s,
                       uint16_t fg, uint16_t bg);
uint16_t ui_font1_text_clipped(const ui_font1_t *f, uint16_t x, int baseline, const char *s,
                               uint16_t fg, uint16_t bg, uint16_t clip_x0, uint16_t clip_x1);

#endif
