/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_RECOVERY_H
#define DPC245_RECOVERY_H
#include <stdbool.h>
#include <stdint.h>

#define DPC245_APP_MARKER_ADDR 0x0803FFFEu
#define DPC245_APP_MARKER_VALID 0xAA55u

/* Every function here touches only peripheral registers and the stack, so
 * Reset_Handler may call them before .data/.bss are initialised. */

/* Programs 0x0000 over the AA55 application marker (1->0 transitions only,
 * no page erase). Returns true when the marker reads back as 0x0000. */
bool dpc245_boot_marker_clear(void);

/* Reset through the watchdog:
 * start FWDGT with prescaler /32 and reload 2 and wait for it to fire.
 * If FWDGT does not reset the core within a bounded time, SYSRESETREQ is used. */
void dpc245_system_reset(void) __attribute__((noreturn));

/* cpsid i -> clear marker (DPC245_MARKER_CLEAR_ATTEMPTS tries) -> reset. The
 * resident update environment then stays in programming mode because the
 * marker is no longer valid. If clearing fails, the reset comes back to this
 * application with the marker still valid. */
void dpc245_enter_resident_bootloader(void) __attribute__((noreturn));
#endif
