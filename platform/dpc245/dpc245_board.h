/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_BOARD_H
#define DPC245_BOARD_H

#include <stdbool.h>
#include <stdint.h>

/* DPC245 V3 hardware map.
 * MCU family: GD32F303, LQFP100, 1024 KiB Flash (MCU density register);
 * GD32F303VGT6 is the leading capacity/package candidate, but the exact order-code
 * suffix remains unconfirmed. Physical pin numbers below are package positions and
 * are kept next to logical GPIO names to make continuity checks unambiguous. */

#define DPC245_LCD_WIDTH     320u
#define DPC245_LCD_HEIGHT    170u
#define DPC245_LCD_Y_OFFSET   35u

#define DPC245_GPIOA_BASE 0x40010800u
#define DPC245_GPIOB_BASE 0x40010C00u
#define DPC245_GPIOC_BASE 0x40011000u
#define DPC245_AFIO_BASE  0x40010000u

/* LCD: 8-bit 8080 write bus. */
#define DPC245_LCD_DATA_PORT_BASE DPC245_GPIOB_BASE
#define DPC245_LCD_CTRL_PORT_BASE DPC245_GPIOA_BASE
#define DPC245_LCD_CS_PIN     8u  /* PB8, physical 95 */
#define DPC245_LCD_RST_PIN    9u  /* PB9, physical 96 */
#define DPC245_BACKLIGHT_PIN 11u  /* PB11, physical 48; pulse-coded brightness */
#define DPC245_LCD_WR_PIN     6u  /* PA6, physical 31 */
#define DPC245_LCD_RD_PIN     7u  /* PA7, physical 32; kept inactive HIGH */
#define DPC245_LCD_DC_PIN     8u  /* PA8, physical 67 */

/* Keys: active-low pull-up inputs. */
#define DPC245_BTN_DOWN_PIN   4u  /* PA4, physical 29 */
#define DPC245_BTN_POWER_PIN  2u  /* PA2, physical 25 */
#define DPC245_BTN_UP_PIN     3u  /* PA3, physical 26 */

/* Analog inputs. GD naming calls the first ADC ADC0; source retains the
 * STM32-compatible register layout/address for clarity. */
#define DPC245_ALS_ADC_PIN        5u /* PA5, physical 30, ADC channel 5 */
#define DPC245_BUTTON_SIGNAL_PIN  1u /* PA1, physical 24, ADC channel 1 */

/* PA0 is an input pull-up with no assigned function. Keep it read-only. */
#define DPC245_UNKNOWN_PA0_PIN    0u /* PA0, physical 23 */

/* BLE UART: GD32 USART0 at 0x40013800 (STM32-style USART1 location). */
#define DPC245_BLE_TX_PIN     9u  /* PA9, physical 68 */
#define DPC245_BLE_RX_PIN    10u  /* PA10, physical 69 */
#define DPC245_USART0_BASE 0x40013800u
#define DPC245_USART1_BASE DPC245_USART0_BASE /* alias */
#define DPC245_BLE_BAUD 230400u

/* CAN0, default pin mapping PA11/PA12. */
#define DPC245_CAN_RX_PIN    11u  /* PA11, physical 70 */
#define DPC245_CAN_TX_PIN    12u  /* PA12, physical 71 */
#define DPC245_CAN0_BASE 0x40006400u
#define DPC245_CAN1_BASE DPC245_CAN0_BASE /* alias */
#define DPC245_CAN_BITRATE 250000u

/* SWD is deliberately retained while JTAG is released,
 * because PB3/PB4 are LCD D3/D4. */
#define DPC245_SWDIO_PIN     13u /* PA13, physical 72 */
#define DPC245_SWCLK_PIN     14u /* PA14, physical 76 */

/* External serial memory: GD SPI1 at 0x40003800 (STM32-style SPI2 location). */
#define DPC245_SPI_MEM_CS_PIN   12u /* PB12, physical 51 */
#define DPC245_SPI_MEM_SCK_PIN  13u /* PB13, physical 52 */
#define DPC245_SPI_MEM_MISO_PIN 14u /* PB14, physical 53 */
#define DPC245_SPI_MEM_MOSI_PIN 15u /* PB15, physical 54 */
#define DPC245_SPI1_BASE 0x40003800u
#define DPC245_SPI2_BASE DPC245_SPI1_BASE /* alias */

/* RTC: PC14 (physical 8) is the bidirectional RTC transport line.
 * No other RTC line is driven by the application. */
#define DPC245_RTC_IO_PIN 14u

/* Boot/debug package pins. */
#define DPC245_PHYS_PIN_NRST   14u
#define DPC245_PHYS_PIN_BOOT0  94u
#define DPC245_PHYS_PIN_SWDIO  72u
#define DPC245_PHYS_PIN_SWCLK  76u

/* Release JTAG-DP while keeping SW-DP. This is REQUIRED before PB3/PB4 can
 * function as LCD data bits. The function preserves all unrelated AFIO remaps. */
void dpc245_debug_keep_swd_release_jtag(void);

/* Configure PA0 as input pull-up without assigning a function. */
void dpc245_unknown_pa0_init_pullup(void);
bool dpc245_unknown_pa0_high(void);

#endif
