/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef EVIST_LOOP_STATS_H
#define EVIST_LOOP_STATS_H

/* Main-loop iteration time: the longest iteration since start
 * and the longest one in the last completed window, in microseconds. Pure logic;
 * the caller supplies the iteration time and the current millisecond.
 *
 * Window semantics: a window closes on the first pass that
 * ends at least EVIST_LOOP_WINDOW_MS after the window started, and that pass counts
 * into the closing window. A pass longer than the window therefore closes one window
 * whose maximum is that pass (LOOP 1S then shows the long pass, not 0); the next
 * window starts at the end of that pass. Windows with no pass are not reported. */

#include <stdbool.h>
#include <stdint.h>

#define EVIST_LOOP_WINDOW_MS 1000u

typedef struct {
    uint32_t max_us;              /* longest iteration since start */
    uint32_t window_max_us;       /* longest in the window being filled */
    uint32_t last_window_max_us;  /* longest in the last completed window */
    uint32_t window_start_ms;
} evist_loop_stats_t;

/* Microseconds from a 1 ms SysTick sample: ms = interrupt
 * counter, cvr/rvr = SysTick current/reload value, pending = SysTick exception
 * pending when CVR was read. A pending tick with CVR in the upper half means the
 * counter already reloaded for the next millisecond. 32-bit arithmetic. */
uint32_t evist_micros_compose(uint32_t ms, uint32_t cvr, uint32_t rvr, bool pending);

void evist_loop_stats_init(evist_loop_stats_t *s, uint32_t now_ms);
void evist_loop_stats_add(evist_loop_stats_t *s, uint32_t iteration_us, uint32_t now_ms);

#endif
