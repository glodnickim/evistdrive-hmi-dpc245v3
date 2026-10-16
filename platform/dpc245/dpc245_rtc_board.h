/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_RTC_BOARD_H
#define DPC245_RTC_BOARD_H

#include "dpc245_rtc.h"

/* Board RTC transport exposed as the generic RTC register interface. */
dpc245_rtc_io_t dpc245_rtc_board_io(void);

#endif
