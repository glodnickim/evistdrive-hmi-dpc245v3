/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "boot_splash.h"

#include "boot_splash_rle.h"

bool boot_splash_decode(const uint8_t *rle, size_t len, boot_splash_emit_fn emit, void *ctx) {
    if (!rle || !emit || (len % 3u) != 0u) return false;

    uint32_t covered = 0u;
    for (size_t i = 0u; i < len; i += 3u) {
        uint8_t count = rle[i];
        uint16_t pixel = (uint16_t)(rle[i + 1u] | ((uint16_t)rle[i + 2u] << 8));
        if (count == 0u) return false;
        covered += count;
        if (covered > BOOT_SPLASH_W * BOOT_SPLASH_H) return false;
        emit(ctx, pixel, count);
    }
    return covered == BOOT_SPLASH_W * BOOT_SPLASH_H;
}

bool boot_splash_emit_all(boot_splash_emit_fn emit, void *ctx) {
    return boot_splash_decode(boot_splash_rle, BOOT_SPLASH_RLE_LEN, emit, ctx);
}
