# Technical Requirements

This document is the release checklist for EVistDrive HMI; the version is set by `VERSION` in the `Makefile`.

| ID | Requirement | Verification |
|---|---|---|
| HW-01 | APP starts at `0x08006000`. | linker + image checker |
| HW-02 | APP validity marker is `55 AA` at `0x0803FFFE`. | linker + image checker |
| HW-03 | Application RAM window is 64 KiB at `0x20000000` (96 KiB is inferred, not measured). | linker + image checker |
| CLK-01 | SYSCLK/AHB 120 MHz, APB2 120 MHz, APB1 60 MHz. | static requirement checker |
| CAN-01 | CAN0 runs at 250 kbit/s with extended 29-bit IDs. | source constants + host protocol tests |
| BLE-01 | USART1 uses PA9/PA10 at 230400 baud. | source constants |
| LCD-01 | UI target is RGB565 320x170. | display header + ARM build |
| SAFE-01 | Walk assist is dead-man; DOWN release clears the request immediately. | host test |
| SAFE-02 | Configuration writes require explicit APPLY and are followed by read-back verification. | transaction/UI logic + host tests |
| CFG-01 | Ten lower dashboard panels exist (including TEMP) and at least one remains enabled. | preference/advanced tests |
| CFG-02 | Power graph range is 250..3000 W in 25 W steps. | preference validation |
| CFG-03 | Quick Walk hold range is 0.5..5.0 s. | preference validation + host test |
| CFG-04 | Light settings validate gain 1..9, dwell 1..30, threshold 1800..5000 and level 0..15. | preference validation |
| IO-01 | Ride/settings and EVistDrive preferences persist through the SPI abstraction. | host round-trip tests |
| CAN-02 | WHEEL writes speed/wheel/circumference together and verifies read-back. | application flow |
| CAN-03 | ASSIST preserves the complete 64-byte profile, modifies level SPEED/POWER only, recomputes checksum, uses ACK-gated transfer and verifies read-back. | host transaction test + application flow |
| CAN-04 | ACCEL writes are gated, use a full validated block and require ACK plus read-back; P2 byte 54 remains unconfirmed across controller variants. | UI/application design + host tests |
| BLE-02 | BLE application dispatcher handles the supported 0x10/A1/A2/A3/A4/A5/A7/A9 families. | source contract + host tests |
| BLE-03 | Periodic phone status uses the 39-byte envelope and update requests use the shared handoff. | source contract + host tests |
| RTC-01 | RTC register map 0x80..0x8E, BCD conversion and write-protect handling are present. | host tests + source constants |
| CAN-05 | Controller 0x3202 bit0 and 0x3205 diagnostic value are retained without speculative labels. | host tests |
| INFO-01 | Controller/sensor/battery identity queries include 0x60/00..05; Battery Info has five pages. | source contract |
| COVER-01 | Platform and feature boundaries are explicitly documented. | repository checker |
| UPD-01 | Installation files are built from the application alone with fixed header fields and CRC16-XMODEM. Default short layout (`make update`): 237,568-byte payload with the application and its 55 AA marker. Full layout (`make update-full`): 456,540-byte payload with the application and a blank `0xFF` fill; inspection fails on any non-blank fill byte. Inspection accepts exactly these two payload lengths. | synthetic self-test and `make update` and `make update-full` inspection in `make check` |
| BUILD-01 | Release and debug compile with warnings as errors and 512-byte frame limit. | `make check` |
| BUILD-02 | Host test suite passes. | `make check` |
| REPO-01 | Repository contains no transient work artifacts or temporary work notes. | repository checker |

A release is accepted only when `make check` exits successfully.
