/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "loop_stats.h"

uint32_t evist_micros_compose(uint32_t ms, uint32_t cvr, uint32_t rvr, bool pending) {
    if (pending && cvr > (rvr >> 1)) ms++;
    return ms * 1000u + ((rvr - cvr) * 1000u) / (rvr + 1u);
}

void evist_loop_stats_init(evist_loop_stats_t *s, uint32_t now_ms) {
    if (!s) return;
    s->max_us = 0u;
    s->window_max_us = 0u;
    s->last_window_max_us = 0u;
    s->window_start_ms = now_ms;
}

void evist_loop_stats_add(evist_loop_stats_t *s, uint32_t iteration_us, uint32_t now_ms) {
    if (!s) return;
    if (iteration_us > s->max_us) s->max_us = iteration_us;
    if (iteration_us > s->window_max_us) s->window_max_us = iteration_us;
    if ((uint32_t)(now_ms - s->window_start_ms) >= EVIST_LOOP_WINDOW_MS) {
        s->last_window_max_us = s->window_max_us;
        s->window_max_us = 0u;
        s->window_start_ms = now_ms;
    }
}
