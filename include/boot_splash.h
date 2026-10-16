/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef EVIST_BOOT_SPLASH_H
#define EVIST_BOOT_SPLASH_H

/* Streaming decoder for the boot splash (320x170 RGB565, RLE records of
 * [count:u8][pixel:u16 LE]). It never builds a frame buffer: the caller gets
 * each run and writes it straight to the panel, which keeps RAM use at zero. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void (*boot_splash_emit_fn)(void *ctx, uint16_t pixel, uint8_t count);

/* Returns true only when the stream is well formed and covers the whole canvas
 * exactly once. Partial output may already have been emitted on failure. */
bool boot_splash_decode(const uint8_t *rle, size_t len, boot_splash_emit_fn emit, void *ctx);

/* Decodes the built-in splash asset. */
bool boot_splash_emit_all(boot_splash_emit_fn emit, void *ctx);

#endif
