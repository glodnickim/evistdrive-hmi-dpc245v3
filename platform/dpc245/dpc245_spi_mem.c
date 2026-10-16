/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_spi_mem.h"
#include <stdint.h>

#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define REG16(a) (*(volatile uint16_t *)(uintptr_t)(a))
#define RCC_APB2ENR REG32(0x40021018u)
#define RCC_APB1ENR REG32(0x4002101Cu)
#define GPIOB_CRH   REG32(0x40010C04u)
#define GPIOB_BSRR  REG32(0x40010C10u)
#define GPIOB_BRR   REG32(0x40010C14u)
#define SPI2_BASE   0x40003800u
#define SPI_CR1     REG16(SPI2_BASE + 0x00u)
#define SPI_CR2     REG16(SPI2_BASE + 0x04u)
#define SPI_SR      REG16(SPI2_BASE + 0x08u)
#define SPI_DR      REG16(SPI2_BASE + 0x0Cu)

#define CS_PIN 12u

/* Limits are shared across every callback in one foreground store step.
 * Outside that scope each public operation gets the same finite defaults. */
static uint32_t flag_polls_left = DPC245_SPI_STEP_FLAG_POLLS;
static uint32_t status_polls_left = DPC245_SPI_STEP_STATUS_POLLS;
static bool step_limits_active;
void dpc245_spi_mem_set_limits(uint32_t flag_polls, uint32_t status_polls) {
    flag_polls_left = flag_polls;
    status_polls_left = status_polls;
    step_limits_active = true;
}
void dpc245_spi_mem_end_step(void) { step_limits_active = false; }
static void operation_begin(void) {
    if (!step_limits_active) {
        flag_polls_left = DPC245_SPI_STEP_FLAG_POLLS;
        status_polls_left = DPC245_SPI_STEP_STATUS_POLLS;
    }
}

static void cs_high(void) { GPIOB_BSRR = 1u << CS_PIN; }
static void cs_low(void)  { GPIOB_BRR  = 1u << CS_PIN; }

static bool wait_flag(uint16_t mask, bool set) {
    while (flag_polls_left != 0u) {
        --flag_polls_left;
        if (((SPI_SR & mask) != 0u) == set) return true;
    }
    return false;
}

/* Set by any transfer that timed out; a read or program that saw it fails,
 * so a dead bus is never taken for erased (0xFF) memory. */
static bool spi_fault;

static uint8_t xfer(uint8_t v) {
    if (!wait_flag(1u << 1, true)) { spi_fault = true; return 0xFFu; } /* TXE */
    SPI_DR = v;
    if (!wait_flag(1u << 0, true)) { spi_fault = true; return 0xFFu; } /* RXNE */
    return (uint8_t)SPI_DR;
}

static bool wait_not_busy(void) {
    while (status_polls_left != 0u) {
        --status_polls_left;
        cs_low();
        (void)xfer(0x05u);
        uint8_t s = xfer(0xFFu);
        cs_high();
        if (spi_fault) return false;
        if ((s & 0x01u) == 0u) return true;
    }
    return false;
}

static bool write_enable(void) {
    cs_low(); (void)xfer(0x06u); cs_high();
    /* Status is checked before program/erase. Only busy-ready is required;
     * the WEL bit is not checked. */
    return !spi_fault;
}

bool dpc245_spi_mem_init(void) {
    RCC_APB2ENR |= 1u << 3;  /* GPIOB */
    RCC_APB1ENR |= 1u << 14; /* SPI2 */

    uint32_t crh = GPIOB_CRH;
    /* PB12 CS: GP output PP 50MHz = 0x3
     * PB13 SCK: AF PP 50MHz       = 0xB
     * PB14 MISO: floating input   = 0x4
     * PB15 MOSI: AF PP 50MHz      = 0xB */
    crh &= ~((0xFu << 16) | (0xFu << 20) | (0xFu << 24) | (0xFu << 28));
    crh |=  (0x3u << 16) | (0xBu << 20) | (0x4u << 24) | (0xBu << 28);
    GPIOB_CRH = crh;
    cs_high();

    SPI_CR1 = 0u;
    SPI_CR2 = 0u;
    /* Master, software NSS, mode 0, APB1/64 = 937.5 kHz. */
    SPI_CR1 = (1u << 2) | (5u << 3) | (1u << 8) | (1u << 9);
    SPI_CR1 |= 1u << 6; /* SPE */
    return true;
}

bool dpc245_spi_mem_reset(void) {
    operation_begin();
    spi_fault = false;
    cs_low(); (void)xfer(0x66u); cs_high();
    cs_low(); (void)xfer(0x99u); cs_high();
    return wait_not_busy();
}

uint8_t dpc245_spi_mem_status(void) {
    operation_begin();
    spi_fault = false;
    cs_low(); (void)xfer(0x05u); uint8_t s = xfer(0xFFu); cs_high(); return s;
}

bool dpc245_spi_mem_read_jedec_id(uint8_t id[3]) {
    operation_begin();
    if (id == NULL) return false;
    spi_fault = false;
    cs_low();
    (void)xfer(0x9Fu); /* standard read-identification opcode; read-only */
    id[0] = xfer(0xFFu);
    id[1] = xfer(0xFFu);
    id[2] = xfer(0xFFu);
    cs_high();
    /* 00/FF-only responses are treated as unsupported/absent memory rather than
     * inventing a part number. The bytes are still returned to diagnostics. */
    return !spi_fault && !((id[0] == 0x00u && id[1] == 0x00u && id[2] == 0x00u) ||
             (id[0] == 0xFFu && id[1] == 0xFFu && id[2] == 0xFFu));
}

bool dpc245_spi_mem_read(uint32_t address, uint8_t *dst, size_t len) {
    operation_begin();
    if (!dst && len) return false;
    spi_fault = false;
    /* A busy memory must never masquerade as erased 0xFF data. */
    if (!wait_not_busy()) return false;
    cs_low();
    (void)xfer(0x0Bu);
    (void)xfer((uint8_t)(address >> 16));
    (void)xfer((uint8_t)(address >> 8));
    (void)xfer((uint8_t)address);
    (void)xfer(0xFFu); /* fast-read dummy byte */
    for (size_t i = 0u; i < len; ++i) dst[i] = xfer(0xFFu);
    cs_high();
    return !spi_fault;
}

static bool program_chunk(uint32_t address, const uint8_t *src, size_t len) {
    spi_fault = false;
    if (!wait_not_busy() || !write_enable()) return false;
    cs_low();
    (void)xfer(0x02u);
    (void)xfer((uint8_t)(address >> 16));
    (void)xfer((uint8_t)(address >> 8));
    (void)xfer((uint8_t)address);
    for (size_t i = 0u; i < len; ++i) (void)xfer(src[i]);
    cs_high();
    if (spi_fault) return false;
    return wait_not_busy();
}

bool dpc245_spi_mem_write(uint32_t address, const uint8_t *src, size_t len) {
    operation_begin();
    if (!src && len) return false;
    while (len) {
        size_t room = 0x100u - (size_t)(address & 0xFFu);
        size_t n = len < room ? len : room;
        if (!program_chunk(address, src, n)) return false;
        address += (uint32_t)n;
        src += n;
        len -= n;
    }
    return true;
}

bool dpc245_spi_mem_erase_page(uint32_t address) {
    if (!dpc245_spi_mem_erase_start(address)) return false;
    operation_begin();
    spi_fault = false;
    return wait_not_busy();
}

bool dpc245_spi_mem_erase_allowed(uint32_t address) {
    /* 0x81 erases one 256-byte page on the identified P25Q16H family.
     * This guard is the only path issuing an erase opcode. */
    return (address & 0xFFu) == 0u && address >= 0x001F0000u &&
           address <= 0x001FFE00u;
}

bool dpc245_spi_mem_erase_start(uint32_t address) {
    if (!dpc245_spi_mem_erase_allowed(address)) return false;
    uint8_t id[3];
    if (!dpc245_spi_mem_read_jedec_id(id) || id[0] != 0x85u ||
        id[1] != 0x60u || id[2] != 0x15u) return false;
    operation_begin();
    spi_fault = false;
    if (!wait_not_busy() || !write_enable()) return false;
    cs_low();
    (void)xfer(0x81u);
    (void)xfer((uint8_t)(address >> 16));
    (void)xfer((uint8_t)(address >> 8));
    (void)xfer((uint8_t)address);
    cs_high();
    return !spi_fault;
}

int dpc245_spi_mem_ready(void) {
    operation_begin();
    spi_fault = false;
    cs_low();
    (void)xfer(0x05u);
    uint8_t status = xfer(0xFFu);
    cs_high();
    if (spi_fault) return -1;
    return (status & 1u) == 0u ? 1 : 0;
}

static bool io_read(void *ctx, uint32_t a, uint8_t *d, size_t n) { (void)ctx; return dpc245_spi_mem_read(a,d,n); }
static bool io_write(void *ctx, uint32_t a, const uint8_t *s, size_t n) { (void)ctx; return dpc245_spi_mem_write(a,s,n); }
static bool io_erase(void *ctx, uint32_t a) { (void)ctx; return dpc245_spi_mem_erase_page(a); }

dpc245_persist_io_t dpc245_spi_mem_persist_io(void) {
    dpc245_persist_io_t io = {io_read, io_write, io_erase, (void *)0};
    return io;
}
