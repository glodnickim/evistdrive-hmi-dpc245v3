/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui_strip.h"

static uint16_t strip[UI_STRIP_H][UI_STRIP_W];
static uint32_t tile_crc[UI_STRIPS][UI_TILES_X];
static bool force_all = true;
static bool capturing;
static uint16_t strip_y0;
static uint16_t win_x0, win_y0, win_x1, win_y1, cur_x, cur_y;
static void (*frame_draw)(void *ctx);
static void *frame_ctx;
static ui_strip_push_fn frame_push;
static uint16_t next_strip;
static uint32_t invalidate_epoch;

bool ui_strip_capturing(void) { return capturing; }

static uint32_t frame_ops, last_frame_ops;
uint32_t ui_strip_last_frame_ops(void) { return last_frame_ops; }

bool ui_strip_rows(uint16_t *y0, uint16_t *y1) {
    if (!capturing) return false;
    *y0 = strip_y0;
    *y1 = (uint16_t)(strip_y0 + UI_STRIP_H);
    return true;
}

void ui_strip_invalidate(void) {
    force_all = true;
    ++invalidate_epoch;
    if (ui_strip_render_active()) next_strip = 0u;
}

static void put(uint16_t x, uint16_t y, uint16_t v) {
    frame_ops++;
    if (x < UI_STRIP_W && y >= strip_y0 && y < strip_y0 + UI_STRIP_H)
        strip[y - strip_y0][x] = v;
}

void ui_strip_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    win_x0 = x0; win_y0 = y0; win_x1 = x1; win_y1 = y1;
    cur_x = x0; cur_y = y0;
}

void ui_strip_data16(uint16_t v) {
    put(cur_x, cur_y, v);
    if (cur_x >= win_x1) {
        cur_x = win_x0;
        cur_y = (cur_y >= win_y1) ? win_y0 : (uint16_t)(cur_y + 1u);
    } else {
        cur_x++;
    }
}

void ui_strip_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t v) {
    uint32_t y_end = (uint32_t)y + h, x_end = (uint32_t)x + w;
    uint32_t lo = y > strip_y0 ? y : strip_y0;
    uint32_t hi = y_end < (uint32_t)strip_y0 + UI_STRIP_H ? y_end : (uint32_t)strip_y0 + UI_STRIP_H;
    if (x_end > UI_STRIP_W) x_end = UI_STRIP_W;
    for (uint32_t yy = lo; yy < hi; ++yy)
        for (uint32_t xx = x; xx < x_end; ++xx) {
            strip[yy - strip_y0][xx] = v;
            frame_ops++;
        }
}

void ui_strip_blit(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *px) {
    if (!px) return;
    for (uint16_t j = 0u; j < h; ++j) {
        uint32_t yy = (uint32_t)y + j;
        if (yy < strip_y0 || yy >= (uint32_t)strip_y0 + UI_STRIP_H) continue;
        for (uint16_t i = 0u; i < w; ++i) put((uint16_t)(x + i), (uint16_t)yy, px[(uint32_t)j * w + i]);
    }
}

/* FNV-1a over the tile's pixels: one multiply per pixel keeps a whole frame
 * (340 tiles) well under a millisecond at 120 MHz. */
static uint32_t tile_crc32(uint16_t tx) {
    uint32_t h = 2166136261u;
    for (uint16_t r = 0u; r < UI_STRIP_H; ++r)
        for (uint16_t c = 0u; c < UI_TILE_W; ++c) {
            h ^= strip[r][tx * UI_TILE_W + c];
            h *= 16777619u;
        }
    return h;
}

bool ui_strip_render_active(void) { return frame_draw != 0; }

void ui_strip_render_cancel(void) {
    frame_draw = 0;
    next_strip = 0u;
}

void ui_strip_render_begin(void (*draw)(void *ctx), void *ctx, ui_strip_push_fn push) {
    if (ui_strip_render_active() || !draw || !push) return;
    frame_draw = draw;
    frame_ctx = ctx;
    frame_push = push;
    next_strip = 0u;
    frame_ops = 0u;
}

uint16_t ui_strip_render_step(uint16_t max_strips) {
    uint16_t sent = 0u;
    if (!ui_strip_render_active() || max_strips == 0u) return 0u;
    uint32_t epoch = invalidate_epoch;
    uint16_t end = (uint16_t)(next_strip + max_strips);
    if (end < next_strip || end > UI_STRIPS) end = UI_STRIPS;
    for (uint16_t s = next_strip; s < end; ++s) {
        strip_y0 = (uint16_t)(s * UI_STRIP_H);
        capturing = true;
        frame_draw(frame_ctx);
        capturing = false;
        for (uint16_t tx = 0u; tx < UI_TILES_X; ++tx) {
            uint32_t crc = tile_crc32(tx);
            if (!force_all && crc == tile_crc[s][tx]) continue;
            tile_crc[s][tx] = crc;
            frame_push((uint16_t)(tx * UI_TILE_W), strip_y0, UI_TILE_W, UI_STRIP_H,
                 &strip[0][tx * UI_TILE_W], UI_STRIP_W);
            sent++;
        }
        if (epoch != invalidate_epoch) return sent;
    }
    next_strip = end;
    if (next_strip == UI_STRIPS) {
        force_all = false;
        last_frame_ops = frame_ops;
        frame_draw = 0;
    }
    return sent;
}

uint16_t ui_strip_render(void (*draw)(void *ctx), void *ctx, ui_strip_push_fn push) {
    if (ui_strip_render_active()) return 0u;
    ui_strip_render_begin(draw, ctx, push);
    return ui_strip_render_step(UI_STRIPS);
}
