/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_RTC_H
#define DPC245_RTC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DPC245_RTC_REG_SECONDS 0x80u
#define DPC245_RTC_REG_MINUTES 0x82u
#define DPC245_RTC_REG_HOURS   0x84u
#define DPC245_RTC_REG_DATE    0x86u
#define DPC245_RTC_REG_MONTH   0x88u
#define DPC245_RTC_REG_WEEKDAY 0x8Au
#define DPC245_RTC_REG_YEAR    0x8Cu
#define DPC245_RTC_REG_WP      0x8Eu
#define DPC245_RTC_WP_ENABLE   0x80u

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t date;
    uint8_t month;
    uint8_t weekday;
    uint8_t year;
} dpc245_rtc_datetime_t;

typedef struct {
    void *ctx;
    bool (*read_register)(void *ctx, uint8_t reg, uint8_t *value);
    bool (*write_register)(void *ctx, uint8_t reg, uint8_t value);
} dpc245_rtc_io_t;

uint8_t dpc245_rtc_bcd_encode(uint8_t value);
bool dpc245_rtc_bcd_decode(uint8_t bcd, uint8_t *value);
bool dpc245_rtc_datetime_validate(const dpc245_rtc_datetime_t *dt);

/* Read/write the seven clock/calendar registers. Values exposed by this API are
 * binary; the board clock stores packed BCD. The write sequence temporarily
 * disables register write-protect when necessary and restores it afterwards. */
bool dpc245_rtc_read(const dpc245_rtc_io_t *io, dpc245_rtc_datetime_t *dt);
bool dpc245_rtc_write(const dpc245_rtc_io_t *io, const dpc245_rtc_datetime_t *dt);

/* Time-only setter used by the HMI control protocol. Existing calendar fields
 * are retained when they can be read. If a read is unavailable, conservative
 * calendar defaults are used while the requested HH:MM:SS is still written. */
bool dpc245_rtc_set_time(const dpc245_rtc_io_t *io,
                         uint8_t hour, uint8_t minute, uint8_t second);

#ifdef __cplusplus
}
#endif
#endif
