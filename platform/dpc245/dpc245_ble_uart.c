/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_ble_uart.h"
#include "dpc245_board.h"
#include <stddef.h>
#include <stdint.h>

#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define RCC_APB2ENR REG32(0x40021018u)
#define GPIOA_CRH   REG32(0x40010804u)
#define USART1_SR   REG32(DPC245_USART1_BASE + 0x00u)
#define USART1_DR   REG32(DPC245_USART1_BASE + 0x04u)
#define USART1_BRR  REG32(DPC245_USART1_BASE + 0x08u)
#define USART1_CR1  REG32(DPC245_USART1_BASE + 0x0Cu)
#define USART1_CR2  REG32(DPC245_USART1_BASE + 0x10u)
#define USART1_CR3  REG32(DPC245_USART1_BASE + 0x14u)

#define USART_SR_ORE  (1u << 3)
#define USART_SR_RXNE (1u << 5)
#define USART_SR_TXE  (1u << 7)
#define USART_CR1_RE  (1u << 2)
#define USART_CR1_TE  (1u << 3)
#define USART_CR1_UE  (1u << 13)

static dpc245_ble_uart_stats_t s_stats;
static uint8_t s_rxq[DPC245_BLE_RX_QUEUE_BYTES];
static uint8_t s_rxq_head;
static uint8_t s_rxq_tail;

static void queue_rx(uint8_t b) {
    uint8_t next=(uint8_t)((s_rxq_head + 1u) % DPC245_BLE_RX_QUEUE_BYTES);
    if(next==s_rxq_tail) {
        /* Preserve the newest stream bytes; the counter makes loss visible. */
        s_rxq_tail=(uint8_t)((s_rxq_tail + 1u) % DPC245_BLE_RX_QUEUE_BYTES);
        ++s_stats.overrun_count;
    }
    s_rxq[s_rxq_head]=b;
    s_rxq_head=next;
}

static void trace_rx(uint8_t b) {
    if (s_stats.last_rx_len < DPC245_BLE_TRACE_BYTES) {
        s_stats.last_rx[s_stats.last_rx_len++] = b;
    } else {
        for (uint8_t i = 1u; i < DPC245_BLE_TRACE_BYTES; ++i)
            s_stats.last_rx[i - 1u] = s_stats.last_rx[i];
        s_stats.last_rx[DPC245_BLE_TRACE_BYTES - 1u] = b;
    }
    ++s_stats.rx_count;
    queue_rx(b);
}

static void ble_uart_init_brr(uint32_t brr) {
    /* AFIO + GPIOA + USART1 on APB2. */
    RCC_APB2ENR |= (1u << 0) | (1u << 2) | (1u << 14);

    /* PA9: 50 MHz alternate-function push-pull (0xB).
     * PA10: floating input (0x4). */
    uint32_t crh = GPIOA_CRH;
    crh &= ~((0xFu << 4u) | (0xFu << 8u));
    crh |=  (0xBu << 4u) | (0x4u << 8u);
    GPIOA_CRH = crh;

    USART1_CR1 = 0u;
    USART1_CR2 = 0u;
    USART1_CR3 = 0u;

    USART1_BRR = brr;
    USART1_CR1 = USART_CR1_UE | USART_CR1_RE | USART_CR1_TE;

    s_stats.rx_count = 0u;
    s_stats.tx_count = 0u;
    s_stats.overrun_count = 0u;
    s_stats.last_rx_len = 0u;
    s_rxq_head = 0u;
    s_rxq_tail = 0u;
}

void dpc245_ble_uart_init(void) {
    /* USART1 is clocked from APB2 at 120 MHz. 0x209 gives 230400 baud. */
    ble_uart_init_brr(0x209u);
}

void dpc245_ble_uart_init_hsi(void) {
    /* APB2 = 8 MHz HSI: 8e6 / (16 * 2.1875) = 228571 baud (-0.8 %). */
    ble_uart_init_brr(0x23u);
}

void dpc245_ble_uart_poll(void) {
    for (unsigned guard = 0u; guard < 64u; ++guard) {
        uint32_t sr = USART1_SR;
        if ((sr & USART_SR_ORE) != 0u) {
            /* Reading SR then DR clears ORE on STM32F1-compatible USART. */
            uint8_t b = (uint8_t)USART1_DR;
            ++s_stats.overrun_count;
            trace_rx(b);
            continue;
        }
        if ((sr & USART_SR_RXNE) == 0u) break;
        trace_rx((uint8_t)USART1_DR);
    }
}

bool dpc245_ble_uart_read_byte(uint8_t *out) {
    if(!out || s_rxq_head==s_rxq_tail) return false;
    *out=s_rxq[s_rxq_tail];
    s_rxq_tail=(uint8_t)((s_rxq_tail + 1u) % DPC245_BLE_RX_QUEUE_BYTES);
    return true;
}

void dpc245_ble_uart_get_stats(dpc245_ble_uart_stats_t *out) {
    if (!out) return;
    *out = s_stats;
}

bool dpc245_ble_uart_write(const uint8_t *data, uint16_t len, uint32_t spin_limit) {
    if (!data && len != 0u) return false;
    for (uint16_t i = 0u; i < len; ++i) {
        uint32_t spins = 0u;
        while ((USART1_SR & USART_SR_TXE) == 0u) {
            if (++spins >= spin_limit) return false;
        }
        USART1_DR = data[i];
        ++s_stats.tx_count;
    }
    return true;
}

bool dpc245_ble_uart_send_device_name_command(uint32_t spin_limit) {
    static const uint8_t cmd[] = "AT+NAMEDP C245-30.CAN\r\n";
    return dpc245_ble_uart_write(cmd, (uint16_t)(sizeof(cmd) - 1u), spin_limit);
}
