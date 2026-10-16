# Hardware specification

## MCU identity

- GD32F303 family, LQFP100.
- 1024 KiB Flash, as reported by the MCU density register.
- 64 KiB SRAM application window. 96 KiB is inferred for the GD32F303VGT6 candidate, not measured.
- GD32F303VGT6 is the leading capacity/package candidate; the exact order-code suffix is unconfirmed.
- Cortex-M4 target, 16 MHz external clock, 120 MHz system/AHB clock, 60 MHz APB1 and 120 MHz APB2.

## Memory map

| Region | Address | Purpose |
|---|---:|---|
| Resident update area | `0x08000000..0x08005FFF` | retained update environment; application workflow never overwrites it |
| Application | `0x08006000..0x0803FFFD` | vectors, code and data |
| Valid marker | `0x0803FFFE..0x0803FFFF` | little-endian `55 AA` |
| Area after the application | `0x08040000..0x0807575B` | written as `0xFF` only by the full-layout file (`make update-full`); after the default short file its content depends on the display's updater; not used by the application |

Never mass erase. Keep unowned GPIOs in input/high-impedance mode and preserve SWD during development.

## Interfaces

- Display: 320 × 170 RGB565, parallel 8080 bus, `platform/dpc245/st7789_8080.*`.
- CAN0: PA11 RX, PA12 TX, 250 kbit/s, 29-bit identifiers.
- Keys: POWER PA2, UP PA3, DOWN PA4. Board circuitry owns power removal; the MCU does not drive a power latch.
- Ambient-light input: PA5 / ADC channel 5.
- BLE radio: USART1 PA9/PA10 at 230400 baud; the radio firmware is separate.
- External settings memory: SPI2 PB12..PB15.
- Backlight controller: PB11, 16-position forward pulse interface.
