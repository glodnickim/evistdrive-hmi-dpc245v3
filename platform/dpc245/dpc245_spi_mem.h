/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DPC245_SPI_MEM_H
#define DPC245_SPI_MEM_H

#include "dpc245_persist.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* External persistent-memory interface:
 * SPI2 PB12=CS, PB13=SCK, PB14=MISO, PB15=MOSI.
 * Wire commands: 0x06 WREN, 0x05 RDSR, 0x0B FAST READ,
 * 0x02 PAGE PROGRAM, 0x81 address erase, 0x66/0x99 reset sequence. */
/* Finite software work bound, not a hardware timing qualification.
 * 256 two-byte RDSR polls = <=512 status bytes per foreground step.
 * TXE/RXNE polls, including successful ones, share a separate CPU-work bound. */
#define DPC245_SPI_STEP_FLAG_POLLS 131072u
#define DPC245_SPI_STEP_STATUS_POLLS 256u
void dpc245_spi_mem_set_limits(uint32_t flag_polls, uint32_t status_polls);
void dpc245_spi_mem_end_step(void);
bool dpc245_spi_mem_init(void);
bool dpc245_spi_mem_reset(void);
bool dpc245_spi_mem_read(uint32_t address, uint8_t *dst, size_t len);
bool dpc245_spi_mem_write(uint32_t address, const uint8_t *src, size_t len);
bool dpc245_spi_mem_erase_page(uint32_t address);
/* Starts a 256-byte page erase in the native settings range. Completion is
 * checked by later foreground steps: 1 ready, 0 busy, -1 transfer failure. */
bool dpc245_spi_mem_erase_start(uint32_t address);
int dpc245_spi_mem_ready(void);
bool dpc245_spi_mem_erase_allowed(uint32_t address);
uint8_t dpc245_spi_mem_status(void);
bool dpc245_spi_mem_read_jedec_id(uint8_t id[3]);

dpc245_persist_io_t dpc245_spi_mem_persist_io(void);

#endif
