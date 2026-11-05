/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef EVIST_BOARD_CONFIG_H
#define EVIST_BOARD_CONFIG_H

/* DPC245 V3 target: the values configured per display. src/ and
 * platform/dpc245/ are shared by all targets. */

/* Identity strings answered for interoperability (protocol values only). */
#define EVIST_BOARD_ID_HW    "DP C245.C 3.0"
#define EVIST_BOARD_ID_SW    "DPC245CF80301.2"
#define EVIST_BOARD_ID_MODEL "DP C245.CAN"

/* Radio module: name command and UART speed. */
#define EVIST_BOARD_RADIO_NAME_CMD "AT+NAMEDP C245-30.CAN\r\n"
#define EVIST_BOARD_BLE_BAUD 230400u
/* USART divider at 120 MHz APB2: 0x209 gives 230400 baud. */
#define EVIST_BOARD_BLE_BRR_PLL 0x209u
/* USART divider at 8 MHz HSI: 8e6 / (16 * 2.1875) = 228571 baud (-0.8 %). */
#define EVIST_BOARD_BLE_BRR_HSI 0x23u

/* External settings memory identification, required before any erase. */
#define EVIST_BOARD_SPI_JEDEC_0 0x85u
#define EVIST_BOARD_SPI_JEDEC_1 0x60u
#define EVIST_BOARD_SPI_JEDEC_2 0x15u

#endif
