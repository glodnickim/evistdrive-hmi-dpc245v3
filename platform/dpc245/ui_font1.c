/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui_font1.h"
#include "st7789_8080.h"
#include "ui_strip.h"

#include "ui_font1_speed_data.h"
#include "ui_font1_speed_small_data.h"
#include "ui_font1_badge_data.h"
#include "ui_font1_badge_plus_data.h"
#include "ui_font1_value_data.h"
#include "ui_font1_small_data.h"
#include "ui_font1_menu_data.h"
#include "ui_font1_dashboard_data.h"
#include "ui_font1_title_data.h"

static const ui_font1_glyph_t *find(const ui_font1_t *f, char c) {
    for (uint8_t i = 0u; f && i < f->count; ++i)
        if (f->codes[i] == c) return &f->glyphs[i];
    return 0;
}

uint16_t ui_font1_width(const ui_font1_t *f, const char *s) {
    uint16_t w = 0u;
    while (s && *s) {
        const ui_font1_glyph_t *g = find(f, *s++);
        if (g) w = (uint16_t)(w + g->adv);
    }
    return w;
}

#ifdef UI_FONT1_TRACE
/* Host-only layout check (sim): ink box of each drawn string and skipped glyphs. */
void (*ui_font1_trace)(int x0, int y0, int x1, int y1, unsigned skipped) = 0;
#endif

/* One LCD window per glyph, pixels streamed straight from the bitmap: no RAM buffer. */
static uint16_t text_clipped(const ui_font1_t *f, uint16_t x, int baseline, const char *s,
                             uint16_t fg, uint16_t bg, int clip_x0, int clip_x1) {
#ifdef UI_FONT1_TRACE
    int bx0 = 10000, by0 = 10000, bx1 = -1, by1 = -1; unsigned skipped = 0u;
#endif
    uint16_t strip_lo = 0u, strip_hi = DPC245_LCD_HEIGHT;
    (void)ui_strip_rows(&strip_lo, &strip_hi);
    while (s && *s) {
        const ui_font1_glyph_t *g = find(f, *s++);
        if (!g) {
#ifdef UI_FONT1_TRACE
            skipped++;   /* a missing glyph is a layout error */
            if (bx1 < 0) { bx0 = (int)x; by0 = baseline - 1; bx1 = (int)x; by1 = baseline; }
#endif
            continue;
        }
        int gx = (int)x + g->ox, gy = baseline + g->oy;
#ifdef UI_FONT1_TRACE
        if (g->w && g->h) {
            int tx0 = gx < clip_x0 ? clip_x0 : gx;
            int tx1 = gx + g->w - 1 > clip_x1 ? clip_x1 : gx + g->w - 1;
            if (tx0 <= tx1) {
                if (tx0 < bx0) bx0 = tx0; if (gy < by0) by0 = gy;
                if (tx1 > bx1) bx1 = tx1; if (gy + g->h - 1 > by1) by1 = gy + g->h - 1;
            }
            if (gx < 0 || gy < 0 || gx + g->w > (int)DPC245_LCD_WIDTH || gy + g->h > (int)DPC245_LCD_HEIGHT) skipped++;
        }
#endif
        int draw_x0 = gx < clip_x0 ? clip_x0 : gx;
        int draw_x1 = gx + g->w - 1 > clip_x1 ? clip_x1 : gx + g->w - 1;
        if (g->w && g->h && draw_x0 <= draw_x1 && draw_x0 >= 0 && gy >= 0 &&
            draw_x1 < (int)DPC245_LCD_WIDTH && gy + g->h <= (int)DPC245_LCD_HEIGHT) {
            /* Only the glyph rows inside the current strip (all rows when drawing
             * straight to the panel). */
            int r0 = (int)strip_lo - gy, r1 = (int)strip_hi - gy;
            if (r0 < 0) r0 = 0;
            if (r1 > g->h) r1 = g->h;
            if (r0 < r1) {
                const uint16_t rb = (uint16_t)((g->w + 7u) / 8u);
                const uint8_t *row = f->bitmap + g->off + (uint32_t)r0 * rb;
                dpc245_lcd_set_window((uint16_t)draw_x0, (uint16_t)(gy + r0),
                                      (uint16_t)draw_x1, (uint16_t)(gy + r1 - 1));
                for (int r = r0; r < r1; ++r, row += rb)
                    for (int px = draw_x0; px <= draw_x1; ++px) {
                        uint8_t c = (uint8_t)(px - gx);
                        dpc245_lcd_data16((row[c >> 3] & (0x80u >> (c & 7u))) ? fg : bg);
                    }
            }
        }
        x = (uint16_t)(x + g->adv);
    }
#ifdef UI_FONT1_TRACE
    if (ui_font1_trace && bx1 >= 0) ui_font1_trace(bx0, by0, bx1, by1, skipped);
#endif
    return x;
}

uint16_t ui_font1_text(const ui_font1_t *f, uint16_t x, int baseline, const char *s,
                       uint16_t fg, uint16_t bg) {
    return text_clipped(f,x,baseline,s,fg,bg,0,(int)DPC245_LCD_WIDTH-1);
}

uint16_t ui_font1_text_clipped(const ui_font1_t *f, uint16_t x, int baseline, const char *s,
                               uint16_t fg, uint16_t bg, uint16_t clip_x0, uint16_t clip_x1) {
    return text_clipped(f,x,baseline,s,fg,bg,clip_x0,clip_x1);
}
