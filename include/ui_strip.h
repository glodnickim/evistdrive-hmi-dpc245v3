/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef UI_STRIP_MODULE_H
#define UI_STRIP_MODULE_H

/* Minimal screen update without a full frame buffer (the panel image is 108 800 B,
 * the RAM window 64 KiB).
 *
 * The screen is drawn into RAM one 320 x UI_STRIP_H strip at a time by running the
 * normal drawing code while the LCD primitives are redirected here. Each strip is
 * split into UI_TILE_W x UI_STRIP_H tiles; a tile is sent to the panel only when its
 * CRC differs from the previous frame. Unchanged parts are never rewritten and the
 * panel is never cleared, so nothing flickers: when one digit changes, only the
 * tiles under that digit are sent. */

#include <stdbool.h>
#include <stdint.h>

#define UI_STRIP_W   320u
#define UI_STRIP_H   10u
#define UI_TILE_W    16u
#define UI_TILES_X   (UI_STRIP_W / UI_TILE_W)
#define UI_STRIPS    (170u / UI_STRIP_H)

/* Writes one tile to the panel. px points at the tile's top-left pixel; rows are
 * stride pixels apart. Called while no strip is being drawn. */
typedef void (*ui_strip_push_fn)(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                 const uint16_t *px, uint16_t stride);

/* Next render sends every tile (panel content unknown, e.g. after the splash). */
void ui_strip_invalidate(void);

/* Draws one frame with draw(ctx) and sends the changed tiles. Returns the number
 * of tiles sent. */
uint16_t ui_strip_render(void (*draw)(void *ctx), void *ctx, ui_strip_push_fn push);

/* Spread one frame over bounded main-loop passes. The draw context must remain
 * valid until active() becomes false. A new begin replaces no active frame. */
void ui_strip_render_begin(void (*draw)(void *ctx), void *ctx, ui_strip_push_fn push);
uint16_t ui_strip_render_step(uint16_t max_strips);
bool ui_strip_render_active(void);
/* Discard an in-progress frame so a changed input can start a current frame. */
void ui_strip_render_cancel(void);

/* While a strip is being drawn: its rows [*y0, *y1). Renderers use it to draw
 * only the rows of the current strip, so a frame costs about one screen of
 * pixel writes instead of one per strip. Returns false when no strip is
 * being drawn (draw everything). */
bool ui_strip_rows(uint16_t *y0, uint16_t *y1);

/* Pixel writes processed while computing the last frame (host measurement). */
uint32_t ui_strip_last_frame_ops(void);

/* LCD redirection used by the panel drivers while a strip is being drawn. */
bool ui_strip_capturing(void);
void ui_strip_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void ui_strip_data16(uint16_t v);
void ui_strip_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t v);
void ui_strip_blit(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *px);

#endif
