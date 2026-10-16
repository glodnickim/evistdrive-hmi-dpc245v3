/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef EVIST_RIDE_MODE_H
#define EVIST_RIDE_MODE_H

/* RIDE MODE model, import record decoder and nonblocking apply transaction.
 *
 * LEGAL and OFFROAD set the controller global speed limit (0x3203).
 * The six-byte formats are read during first-start import. New settings are
 * written by native_store. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "dpc245_persist.h"
#include "controller_write.h"

#define EVIST_RIDE_REC_SIZE       6u
#define EVIST_RIDE_REC_MAGIC      0xACu
#define EVIST_RIDE_REC_VERSION    0x01u
#define EVIST_RIDE_REC_VERSION_2  0x02u

#define EVIST_RIDE_LEGAL_X100     2500u
#define EVIST_RIDE_LEGAL_MIN      1500u
#define EVIST_RIDE_LEGAL_MAX      3200u
#define EVIST_RIDE_LEGAL_STEP     50u
#define EVIST_RIDE_OFFROAD_MIN    1000u
#define EVIST_RIDE_OFFROAD_MAX    7000u
#define EVIST_RIDE_OFFROAD_STEP   50u
#define EVIST_RIDE_OFFROAD_DEFAULT 4000u

typedef enum { EVIST_RIDE_LEGAL = 0, EVIST_RIDE_OFFROAD = 1 } evist_ride_mode_t;
typedef enum { EVIST_RIDE_START_LAST = 0, EVIST_RIDE_START_LEGAL = 1,
               EVIST_RIDE_START_OFFROAD = 2 } evist_ride_start_t;

typedef struct {
    evist_ride_mode_t mode;
    uint16_t offroad_x100;   /* user offroad speed; used only in OFFROAD */
    uint16_t legal_x100;
    evist_ride_start_t start;
    evist_ride_mode_t last_mode;
} evist_ride_cfg_t;

/* The speed limit the controller must hold for a given configuration. */
uint16_t evist_ride_target_x100(const evist_ride_cfg_t *cfg);
evist_ride_mode_t evist_ride_boot_mode(const evist_ride_cfg_t *cfg);
evist_ride_cfg_t evist_ride_defaults(void);
bool evist_ride_speed_valid(uint16_t x100);
bool evist_ride_legal_speed_valid(uint16_t x100);
bool evist_ride_cfg_valid(const evist_ride_cfg_t *cfg);

bool evist_ride_record_encode(const evist_ride_cfg_t *cfg, uint8_t out[EVIST_RIDE_REC_SIZE]);
bool evist_ride_record_decode(const uint8_t in[EVIST_RIDE_REC_SIZE], evist_ride_cfg_t *out);
bool evist_ride_record_v2_encode(const evist_ride_cfg_t *cfg, uint8_t out[EVIST_RIDE_REC_SIZE]);
bool evist_ride_record_v2_decode(const uint8_t in[EVIST_RIDE_REC_SIZE], evist_ride_cfg_t *out);
bool evist_ride_record_blank(const uint8_t in[EVIST_RIDE_REC_SIZE]);

/* Scans records oldest-first. A blank slot ends the written area; invalid or
 * torn records are skipped. Returns false (and keeps *out as the default LEGAL)
 * when no valid record exists. */
bool evist_ride_log_scan(const uint8_t *records, size_t count, evist_ride_cfg_t *out);

typedef enum { EVIST_RIDE_STORE_OK=0, EVIST_RIDE_STORE_NO_MEMORY,
               EVIST_RIDE_STORE_FULL, EVIST_RIDE_STORE_FAILED } evist_ride_store_result_t;
/* The two-byte tail of the reserved 512-byte area is never programmed. */
#define EVIST_RIDE_LOG_COUNT ((EVIST_PREF_RIDE_END-EVIST_PREF_RIDE_BASE)/EVIST_RIDE_REC_SIZE)
bool evist_ride_store_load(const dpc245_persist_io_t *io, evist_ride_cfg_t *cfg,
                           uint32_t *next_addr);
/* Returns false only when a required SPI read fails. No record is a valid
 * default configuration. */
bool evist_ride_store_load_checked(const dpc245_persist_io_t *io,
                                   evist_ride_cfg_t *cfg);
/* Boot always reads the newest valid record when SPI is available. The build
 * persistence flag gates appends only; no record still defaults to LEGAL. */
bool evist_ride_boot_load(const dpc245_persist_io_t *io, evist_ride_cfg_t *cfg,
                          uint32_t *next_addr);
evist_ride_store_result_t evist_ride_store_append(const dpc245_persist_io_t *io,
                           const evist_ride_cfg_t *before, const evist_ride_cfg_t *after,
                           uint32_t *next_addr);
const char *evist_ride_store_status(evist_ride_store_result_t result);

typedef enum { EVIST_RIDE_APPLY_IDLE=0, EVIST_RIDE_APPLY_READ,
               EVIST_RIDE_APPLY_WRITE, EVIST_RIDE_APPLY_OK,
               EVIST_RIDE_APPLY_FAILED } evist_ride_apply_phase_t;
typedef struct {
    evist_ride_apply_phase_t phase;
    uint16_t target_x100;
    uint16_t last_applied_x100;
    evist_txn_result_t last_result;
    uint8_t attempts;
    uint8_t original[EVIST_WHEEL_PAYLOAD_LEN];
} evist_ride_apply_t;
void evist_ride_apply_init(evist_ride_apply_t *a);
void evist_ride_apply_begin(evist_ride_apply_t *a, uint16_t target_x100);
bool evist_ride_apply_start_txn(evist_ride_apply_t *a, evist_ctrl_txn_t *txn, uint32_t now_ms);
void evist_ride_apply_on_result(evist_ride_apply_t *a, evist_txn_result_t result,
                                const uint8_t data[64]);

#endif
