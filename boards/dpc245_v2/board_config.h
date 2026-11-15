/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef EVIST_BOARD_CONFIG_H
#define EVIST_BOARD_CONFIG_H

/* DPC245 V2 target: the values configured per display. src/ and
 * platform/dpc245/ are shared by all targets.
 *
 * Status: the shared MCU core (display, CAN, RTC, clock tree, SPI driver) and
 * the key pins match V3; the CAN flash/updater entry is identical to V3. The
 * radio is NOT V3's: it is a Bafang CAN-over-BLE gateway, so the V2 phone/Garmin
 * transport is a separate decision and is NOT required for the display, CAN or
 * the PROBE image this target builds first. The settings-memory id is a
 * hypothesis until the PROBE reads it on the device (see below). */

/* Identity strings answered for interoperability (protocol values only).
 * HW and MODEL are the V2 hardware's own values. SW is provisional: it mirrors
 * the V2 display's own SW id so the Bafang app recognizes it; a distinct
 * EVistDrive V2 release identity (as V3 uses DPC245CF80301.2) is an open product
 * decision and is not reported by the PROBE image, which this target builds first. */
#define EVIST_BOARD_ID_HW    "DP C245.C 2.0"       /* V2 hardware identity */
#define EVIST_BOARD_ID_SW    "DPC245CF80261.3"     /* provisional, app-compatible; distinct EVD V2 id is an open decision */
#define EVIST_BOARD_ID_MODEL "DP C245.CAN"         /* V2 BLE model name */

/* Radio module: the V2 gateway is not driven by AT commands from the MCU, so no
 * name command is sent. An empty command is a safe no-op:
 * dpc245_ble_uart_send_device_name_command() writes sizeof(cmd)-1 == 0 bytes. The
 * display never advertises its own name on V2; the gateway advertises itself. */
#define EVIST_BOARD_RADIO_NAME_CMD ""
/* V2 radio UART runs at 250000 baud; no AT+ handshake. */
#define EVIST_BOARD_BLE_BAUD 250000u
/* USART divider at 120 MHz APB2: 120e6 / (16 * 250000) = 30.0 -> BRR 0x1E0. */
#define EVIST_BOARD_BLE_BRR_PLL 0x1E0u
/* USART divider at 8 MHz HSI: 8e6 / (16 * 250000) = 2.0 -> BRR 0x20. */
#define EVIST_BOARD_BLE_BRR_HSI 0x20u

/* External settings memory identification, required before any erase.
 * The PROBE image reads this id on the device; the measured value 85 60 15 is
 * byte-identical to V3, so the V2 settings NOR is the same family and the shared
 * SPI driver (page-program opcode 0x81, 24-bit address, 256 B page) applies
 * unchanged. The part being equal to V3, V2 persistence is enabled exactly as
 * V3's. */
#define EVIST_BOARD_SPI_JEDEC_0 0x85u  /* = V3 */
#define EVIST_BOARD_SPI_JEDEC_1 0x60u
#define EVIST_BOARD_SPI_JEDEC_2 0x15u

#endif
