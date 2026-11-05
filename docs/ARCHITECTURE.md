# Architecture

EVistDrive separates application state, transport parsing, screen composition and MCU access. `src/` contains behavior and protocol modules; `platform/dpc245/` owns startup, clocks, GPIO, LCD, CAN, ADC, SPI, BLE UART and update entry.

## Display targets

One source tree builds every supported display. `boards/<target>/` holds what differs between displays: identity strings, radio settings, memory identification (`board_config.h`) and build settings (`board.mk`). Select a target with `make TARGET=<target>`; the default is `dpc245_v3`, and output goes to `build/<target>/`. `src/` and `include/` must not contain per-display values or conditions, and `tools/check_repository.py` enforces this, so a feature behaves the same on every target.

`dpc245_app`, `advanced_settings`, `evist_preferences`, `light_controller`, `dpc245_runtime` and `menu_model` own user behavior. `bafang_can`, `bafang_service`, `can_router`, `controller_write`, `dpc245_tx`, `dpc245_hmi_service` and `dpc245_ble_proto` own frames and transactions. `ui_dashboard` composes screens; `ui_strip_render()` sends only changed 16 × 10 tiles to the panel.

The clock interrupt samples keys and queues timestamped gestures. The cooperative loop drains CAN and BLE input, samples ADC, advances application and controller transactions, services persistence, sends due frames, and draws changed UI content. A navigation gesture discards any in-progress screen snapshot. Its new frame renders at most three strips per pass; all later key events stay queued in timestamp order until all 17 strips complete in six passes. CAN, BLE, persistence and the debounced physical DOWN release check continue between three-strip chunks; that physical check clears held Walk while gesture delivery is gated. At a 13.3 ms pass interval, the next queued navigation can run about 79.8 ms after the previous action; hardware pass timing after navigation requires measurement. Hardware setup and short peripheral pulses use bounded waits. No application module writes peripheral registers directly. Controller writes use one in-flight transaction with validation, ACK, timeout and read-back.
