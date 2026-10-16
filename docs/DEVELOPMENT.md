# Development guide

Add product behavior to `src/`, interface definitions to `include/`, and MCU access to `platform/dpc245/`. Keep transport parsing separate from screen composition. Add focused host tests for protocol states, settings and safety behavior, and add each new screen to `sim/render_screens.c` for layout checks.

Controller configuration writes require a specified payload, input range validation, explicit user action, timeout, ACK if applicable and read-back comparison. Keep uncertain fields read-only. Preserve the application flash boundary and the update marker. Run `make check` and `make sim` before submitting a change.
