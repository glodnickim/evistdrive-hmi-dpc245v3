/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_BLE_UART_H
#define DPC245_BLE_UART_H

#include <stdbool.h>
#include <stdint.h>

#define DPC245_BLE_TRACE_BYTES 16u
#define DPC245_BLE_RX_QUEUE_BYTES 128u

typedef struct {
    uint32_t rx_count;
    uint32_t tx_count;
    uint32_t overrun_count;
    uint8_t last_rx[DPC245_BLE_TRACE_BYTES];
    uint8_t last_rx_len;
} dpc245_ble_uart_stats_t;

/* USART1: PA9 TX / PA10 RX, 230400 baud from 120 MHz APB2. */
void dpc245_ble_uart_init(void);
/* Same link with APB2 = 8 MHz HSI (safe mode only). */
void dpc245_ble_uart_init_hsi(void);

/* Poll and preserve all currently pending module bytes. No bytes are sent by
 * init or poll. */
void dpc245_ble_uart_poll(void);
void dpc245_ble_uart_get_stats(dpc245_ble_uart_stats_t *out);
/* Bytes preserved by poll() can be consumed by the DPC245 frame parser. */
bool dpc245_ble_uart_read_byte(uint8_t *out);

/* Low-level TX; each byte waits for TXE at most spin_limit polls. */
bool dpc245_ble_uart_write(const uint8_t *data, uint16_t len, uint32_t spin_limit);
bool dpc245_ble_uart_send_device_name_command(uint32_t spin_limit);

#endif
