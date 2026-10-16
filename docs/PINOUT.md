# DPC245 LQFP100 Pin Contract

This table is the firmware-facing pin contract for the DPC245 board. `Unused by APP` means the application deliberately leaves the pin unclaimed; it does not mean the PCB pad is electrically disconnected.

| Pin | Signal | Application role |
|---:|---|---|
| 1 | PE2 | Unused by APP |
| 2 | PE3 | Unused by APP |
| 3 | PE4 | Unused by APP |
| 4 | PE5 | Unused by APP |
| 5 | PE6 | Unused by APP |
| 6 | VBAT | Backup/supply domain |
| 7 | PC13 | Unused by APP |
| 8 | PC14 | RTC board transport, bidirectional GPIO |
| 9 | PC15 | Unused by APP |
| 10 | VSS_5 | Ground |
| 11 | VDD_5 | Digital supply |
| 12 | OSCIN | 16 MHz HXTAL input |
| 13 | OSCOUT | 16 MHz HXTAL output |
| 14 | NRST | MCU reset |
| 15 | PC0 | Unused by APP |
| 16 | PC1 | Unused by APP |
| 17 | PC2 | Unused by APP |
| 18 | PC3 | Unused by APP |
| 19 | VSSA | Analog ground |
| 20 | VREF- | ADC negative reference |
| 21 | VREF+ | ADC positive reference |
| 22 | VDDA | Analog supply |
| 23 | PA0 | Pull-up input; reserved board input |
| 24 | PA1 | Button Signal ADC input; must remain high impedance |
| 25 | PA2 | POWER / ENTER, active-low pull-up |
| 26 | PA3 | UP / +, active-low pull-up |
| 27 | VSS_4 | Ground |
| 28 | VDD_4 | Digital supply |
| 29 | PA4 | DOWN / -, active-low pull-up |
| 30 | PA5 | Ambient-light sensor, ADC channel 5 |
| 31 | PA6 | LCD WR |
| 32 | PA7 | LCD RD, held high during normal drawing |
| 33 | PC4 | Unused by APP |
| 34 | PC5 | Unused by APP |
| 35 | PB0 | LCD D0 |
| 36 | PB1 | LCD D1 |
| 37 | PB2 | LCD D2 / BOOT1 strap |
| 38 | PE7 | Unused by APP |
| 39 | PE8 | Unused by APP |
| 40 | PE9 | Unused by APP |
| 41 | PE10 | Unused by APP |
| 42 | PE11 | Unused by APP |
| 43 | PE12 | Unused by APP |
| 44 | PE13 | Unused by APP |
| 45 | PE14 | Unused by APP |
| 46 | PE15 | Unused by APP |
| 47 | PB10 | Reserved board pin; no normal APP drive |
| 48 | PB11 | Backlight pulse controller |
| 49 | VSS_1 | Ground |
| 50 | VDD_1 | Digital supply |
| 51 | PB12 | External memory CS |
| 52 | PB13 | External memory SCK |
| 53 | PB14 | External memory MISO |
| 54 | PB15 | External memory MOSI |
| 55 | PD8 | Unused by APP |
| 56 | PD9 | Unused by APP |
| 57 | PD10 | Unused by APP |
| 58 | PD11 | Unused by APP |
| 59 | PD12 | Unused by APP |
| 60 | PD13 | Unused by APP |
| 61 | PD14 | Unused by APP |
| 62 | PD15 | Unused by APP |
| 63 | PC6 | Unused by APP |
| 64 | PC7 | Unused by APP |
| 65 | PC8 | Unused by APP |
| 66 | PC9 | Unused by APP |
| 67 | PA8 | LCD D/C |
| 68 | PA9 | BLE-module UART TX |
| 69 | PA10 | BLE-module UART RX |
| 70 | PA11 | CAN RX |
| 71 | PA12 | CAN TX |
| 72 | PA13 | SWDIO |
| 73 | NC | No MCU signal |
| 74 | VSS_2 | Ground |
| 75 | VDD_2 | Digital supply |
| 76 | PA14 | SWCLK |
| 77 | PA15 | JTAG released; unused by APP |
| 78 | PC10 | Unused by APP |
| 79 | PC11 | Unused by APP |
| 80 | PC12 | Unused by APP |
| 81 | PD0 | Unused by APP |
| 82 | PD1 | Unused by APP |
| 83 | PD2 | Unused by APP |
| 84 | PD3 | Unused by APP |
| 85 | PD4 | Unused by APP |
| 86 | PD5 | Unused by APP |
| 87 | PD6 | Unused by APP |
| 88 | PD7 | Unused by APP |
| 89 | PB3 | LCD D3; JTAG must be disabled while SWD remains enabled |
| 90 | PB4 | LCD D4 |
| 91 | PB5 | LCD D5 |
| 92 | PB6 | LCD D6 |
| 93 | PB7 | LCD D7 |
| 94 | BOOT0 | Boot strap |
| 95 | PB8 | LCD CS |
| 96 | PB9 | LCD RESET |
| 97 | PE0 | Unused by APP |
| 98 | PE1 | Unused by APP |
| 99 | VSS_3 | Ground |
| 100 | VDD_3 | Digital supply |

## Ownership rules

- PA1 is an **input only** in the application. It is part of the physical button/wake signaling network and must never be driven by firmware.
- PA13/PA14 remain available for SWD. JTAG is released so PB3/PB4 can serve the LCD data bus.
- PC14 is owned by the RTC board-transport layer. Other modules must not reconfigure it.
- PB11 is not PWM. Brightness changes are expressed as forward pulses in a 16-position external controller.
- P+/wake/hold is a board power-path signal, not an application-controlled GPIO. No pin in this table may be repurposed as a software power latch.
