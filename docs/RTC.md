# RTC Contract

## Register model

The clock/calendar register interface uses packed BCD:

| Register | Field |
|---:|---|
| `0x80` | seconds |
| `0x82` | minutes |
| `0x84` | hours |
| `0x86` | date |
| `0x88` | month |
| `0x8A` | weekday |
| `0x8C` | year |
| `0x8E` | write protect |

`dpc245_rtc.c` exposes binary values to the rest of the application, validates calendar ranges, converts BCD, clears write protect before a write when needed and restores it afterward.

## Board transport

PC14 is owned exclusively by `dpc245_rtc_board.c`. The transport is LSB-first and alternates PC14 between an output state and an input-with-pull-up state. A zero bit actively clears the line; a one bit leaves the line released by the transaction primitive. Other modules must not reconfigure PC14.

## Time setting

HMI control command `0x62/0x02` carries `HH MM SS`. The time-only setter preserves the existing calendar fields when they can be read. Invalid hours/minutes/seconds are rejected.
