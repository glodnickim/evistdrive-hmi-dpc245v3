/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_rtc.h"

static const uint8_t s_regs[7] = {
    DPC245_RTC_REG_SECONDS, DPC245_RTC_REG_MINUTES, DPC245_RTC_REG_HOURS,
    DPC245_RTC_REG_DATE, DPC245_RTC_REG_MONTH, DPC245_RTC_REG_WEEKDAY,
    DPC245_RTC_REG_YEAR
};

uint8_t dpc245_rtc_bcd_encode(uint8_t value) {
    return (uint8_t)(((value / 10u) << 4) | (value % 10u));
}

bool dpc245_rtc_bcd_decode(uint8_t bcd, uint8_t *value) {
    if (!value) return false;
    uint8_t lo = (uint8_t)(bcd & 0x0Fu);
    uint8_t hi = (uint8_t)((bcd >> 4) & 0x0Fu);
    if (lo > 9u || hi > 9u) return false;
    *value = (uint8_t)(hi * 10u + lo);
    return true;
}

bool dpc245_rtc_datetime_validate(const dpc245_rtc_datetime_t *dt) {
    if (!dt) return false;
    return dt->second <= 59u && dt->minute <= 59u && dt->hour <= 23u &&
           dt->date >= 1u && dt->date <= 31u &&
           dt->month >= 1u && dt->month <= 12u &&
           dt->weekday >= 1u && dt->weekday <= 7u && dt->year <= 99u;
}

static uint8_t *field_ptr(dpc245_rtc_datetime_t *dt, uint8_t index) {
    switch (index) {
    case 0u: return &dt->second;
    case 1u: return &dt->minute;
    case 2u: return &dt->hour;
    case 3u: return &dt->date;
    case 4u: return &dt->month;
    case 5u: return &dt->weekday;
    default: return &dt->year;
    }
}

bool dpc245_rtc_read(const dpc245_rtc_io_t *io, dpc245_rtc_datetime_t *dt) {
    if (!io || !io->read_register || !dt) return false;
    dpc245_rtc_datetime_t tmp = {0};
    for (uint8_t i = 0u; i < 7u; ++i) {
        uint8_t raw = 0u;
        uint8_t value = 0u;
        if (!io->read_register(io->ctx, s_regs[i], &raw)) return false;
        if (!dpc245_rtc_bcd_decode(raw, &value)) return false;
        *field_ptr(&tmp, i) = value;
    }
    if (!dpc245_rtc_datetime_validate(&tmp)) return false;
    *dt = tmp;
    return true;
}

bool dpc245_rtc_write(const dpc245_rtc_io_t *io, const dpc245_rtc_datetime_t *dt) {
    if (!io || !io->write_register || !dt || !dpc245_rtc_datetime_validate(dt)) return false;

    bool wp_was_enabled = true;
    if (io->read_register) {
        uint8_t wp = 0u;
        if (!io->read_register(io->ctx, DPC245_RTC_REG_WP, &wp)) return false;
        wp_was_enabled = (wp & DPC245_RTC_WP_ENABLE) != 0u;
    }
    if (wp_was_enabled && !io->write_register(io->ctx, DPC245_RTC_REG_WP, 0u)) return false;

    const uint8_t values[7] = {
        dt->second, dt->minute, dt->hour, dt->date,
        dt->month, dt->weekday, dt->year
    };
    bool ok = true;
    for (uint8_t i = 0u; i < 7u; ++i) {
        if (!io->write_register(io->ctx, s_regs[i], dpc245_rtc_bcd_encode(values[i]))) {
            ok = false;
            break;
        }
    }
    if (wp_was_enabled && !io->write_register(io->ctx, DPC245_RTC_REG_WP,
                                                DPC245_RTC_WP_ENABLE)) ok = false;
    return ok;
}

bool dpc245_rtc_set_time(const dpc245_rtc_io_t *io,
                         uint8_t hour, uint8_t minute, uint8_t second) {
    if (hour > 23u || minute > 59u || second > 59u) return false;
    dpc245_rtc_datetime_t dt = {
        .second = second, .minute = minute, .hour = hour,
        .date = 1u, .month = 1u, .weekday = 1u, .year = 0u
    };
    if (io && io->read_register) {
        dpc245_rtc_datetime_t current;
        if (dpc245_rtc_read(io, &current)) {
            dt = current;
            dt.hour = hour;
            dt.minute = minute;
            dt.second = second;
        }
    }
    return dpc245_rtc_write(io, &dt);
}
