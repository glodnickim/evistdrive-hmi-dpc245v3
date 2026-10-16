# Feature coverage

The application includes startup and update entry, the 320 × 170 display, keys and Walk, ambient-light sensing and backlight control, CAN telemetry and service transactions, BLE UART commands, controller settings, external SPI preferences, RTC application logic, ride statistics and information pages.

Some controller status bytes remain raw until a stable field definition is available. The external BLE module owns RF behavior and is not reprogrammed here. The board owns power removal. The exact MCU order-code suffix remains unconfirmed.

Host tests cover protocol and storage state machines, UI output equivalence and layout. New hardware or controller combinations need their own stand verification before riding.
