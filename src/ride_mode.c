/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ride_mode.h"
#include "app_config.h"
#include <stddef.h>

uint16_t evist_ride_target_x100(const evist_ride_cfg_t *cfg) {
    if (!cfg) return EVIST_RIDE_LEGAL_X100;
    if (cfg->mode != EVIST_RIDE_OFFROAD) return cfg->legal_x100;
    return cfg->offroad_x100;
}

evist_ride_cfg_t evist_ride_defaults(void) {
    evist_ride_cfg_t cfg = { EVIST_RIDE_LEGAL, EVIST_RIDE_OFFROAD_DEFAULT,
                             EVIST_RIDE_LEGAL_X100, EVIST_RIDE_START_LEGAL,
                             EVIST_RIDE_LEGAL };
    return cfg;
}

evist_ride_mode_t evist_ride_boot_mode(const evist_ride_cfg_t *cfg) {
    if (!cfg) return EVIST_RIDE_LEGAL;
    if (cfg->start == EVIST_RIDE_START_LAST) return cfg->last_mode;
    return cfg->start == EVIST_RIDE_START_OFFROAD ? EVIST_RIDE_OFFROAD : EVIST_RIDE_LEGAL;
}

bool evist_ride_speed_valid(uint16_t x100) {
    return x100 >= EVIST_RIDE_OFFROAD_MIN && x100 <= EVIST_RIDE_OFFROAD_MAX &&
           (x100 % EVIST_RIDE_OFFROAD_STEP) == 0u;
}

bool evist_ride_legal_speed_valid(uint16_t x100) {
    return x100 >= EVIST_RIDE_LEGAL_MIN && x100 <= EVIST_RIDE_LEGAL_MAX &&
           (x100 % EVIST_RIDE_LEGAL_STEP) == 0u;
}

bool evist_ride_cfg_valid(const evist_ride_cfg_t *cfg) {
    if (!cfg) return false;
    if (cfg->mode != EVIST_RIDE_LEGAL && cfg->mode != EVIST_RIDE_OFFROAD) return false;
    if (cfg->last_mode != EVIST_RIDE_LEGAL && cfg->last_mode != EVIST_RIDE_OFFROAD) return false;
    if (cfg->start > EVIST_RIDE_START_OFFROAD) return false;
    return evist_ride_legal_speed_valid(cfg->legal_x100) &&
           evist_ride_speed_valid(cfg->offroad_x100) &&
           cfg->offroad_x100 >= cfg->legal_x100;
}

static uint8_t record_crc(const uint8_t *r) {
    return (uint8_t)(r[0] ^ r[1] ^ r[2] ^ r[3] ^ r[4]);
}

bool evist_ride_record_encode(const evist_ride_cfg_t *cfg, uint8_t out[EVIST_RIDE_REC_SIZE]) {
    if (!evist_ride_cfg_valid(cfg) || !out) return false;
    out[0] = EVIST_RIDE_REC_MAGIC;
    out[1] = EVIST_RIDE_REC_VERSION;
    out[2] = (uint8_t)cfg->mode;
    out[3] = (uint8_t)(cfg->offroad_x100 & 0xFFu);
    out[4] = (uint8_t)(cfg->offroad_x100 >> 8);
    out[5] = record_crc(out);
    return true;
}

bool evist_ride_record_decode(const uint8_t in[EVIST_RIDE_REC_SIZE], evist_ride_cfg_t *out) {
    if (!in || !out) return false;
    if (in[0] != EVIST_RIDE_REC_MAGIC || in[1] != EVIST_RIDE_REC_VERSION) return false;
    if (in[5] != record_crc(in)) return false;
    evist_ride_cfg_t cfg = evist_ride_defaults();
    cfg.mode = (evist_ride_mode_t)in[2];
    cfg.offroad_x100 = (uint16_t)(in[3] | ((uint16_t)in[4] << 8));
    /* A record may hold OFFROAD below 25.0; the UI requires
     * OFFROAD to be at least LEGAL, so raise it on load. */
    if (cfg.offroad_x100 < cfg.legal_x100 && evist_ride_speed_valid(cfg.offroad_x100))
        cfg.offroad_x100 = cfg.legal_x100;
    if (!evist_ride_cfg_valid(&cfg)) return false;
    *out = cfg;
    return true;
}

bool evist_ride_record_v2_encode(const evist_ride_cfg_t *cfg, uint8_t out[EVIST_RIDE_REC_SIZE]) {
    if (!evist_ride_cfg_valid(cfg) || !out) return false;
    out[0] = EVIST_RIDE_REC_MAGIC;
    out[1] = EVIST_RIDE_REC_VERSION_2;
    out[2] = (uint8_t)((uint8_t)cfg->mode | ((uint8_t)cfg->start << 1) |
                       ((uint8_t)cfg->last_mode << 3));
    out[3] = (uint8_t)((cfg->offroad_x100 - EVIST_RIDE_OFFROAD_MIN) / EVIST_RIDE_OFFROAD_STEP);
    out[4] = (uint8_t)((cfg->legal_x100 - EVIST_RIDE_LEGAL_MIN) / EVIST_RIDE_LEGAL_STEP);
    out[5] = record_crc(out);
    return true;
}

bool evist_ride_record_v2_decode(const uint8_t in[EVIST_RIDE_REC_SIZE], evist_ride_cfg_t *out) {
    if (!in || !out || in[0] != EVIST_RIDE_REC_MAGIC ||
        in[1] != EVIST_RIDE_REC_VERSION_2 || in[5] != record_crc(in) ||
        (in[2] & 0xE0u) != 0u || in[3] > 120u || in[4] > 34u) return false;
    evist_ride_cfg_t cfg;
    cfg.mode = (evist_ride_mode_t)(in[2] & 1u);
    cfg.start = (evist_ride_start_t)((in[2] >> 1) & 3u);
    cfg.last_mode = (evist_ride_mode_t)((in[2] >> 3) & 1u);
    cfg.offroad_x100 = (uint16_t)(EVIST_RIDE_OFFROAD_MIN + in[3] * EVIST_RIDE_OFFROAD_STEP);
    cfg.legal_x100 = (uint16_t)(EVIST_RIDE_LEGAL_MIN + in[4] * EVIST_RIDE_LEGAL_STEP);
    if (!evist_ride_cfg_valid(&cfg)) return false;
    *out = cfg;
    return true;
}

static bool record_any_decode(const uint8_t in[EVIST_RIDE_REC_SIZE], evist_ride_cfg_t *out) {
    return evist_ride_record_v2_decode(in, out) || evist_ride_record_decode(in, out);
}

static uint16_t record_v1_speed(const uint8_t in[EVIST_RIDE_REC_SIZE]) {
    return (uint16_t)(in[3] | ((uint16_t)in[4] << 8));
}

static evist_ride_cfg_t merge_unpaired_v1(const uint8_t in[EVIST_RIDE_REC_SIZE],
                                           const evist_ride_cfg_t *v2,
                                           const evist_ride_cfg_t *v1) {
    evist_ride_cfg_t cfg = v2 ? *v2 : evist_ride_defaults();
    uint16_t speed = record_v1_speed(in);
    cfg.mode = v1->mode;
    cfg.offroad_x100 = speed < cfg.legal_x100 ? cfg.legal_x100 : speed;
    if (cfg.start == EVIST_RIDE_START_LAST) cfg.last_mode = cfg.mode;
    return cfg;
}

bool evist_ride_record_blank(const uint8_t in[EVIST_RIDE_REC_SIZE]) {
    for (size_t i = 0u; i < EVIST_RIDE_REC_SIZE; ++i)
        if (in[i] != 0xFFu) return false;
    return true;
}

bool evist_ride_log_scan(const uint8_t *records, size_t count, evist_ride_cfg_t *out) {
    evist_ride_cfg_t found = evist_ride_defaults();
    evist_ride_cfg_t last_v2 = found;
    bool any = false;
    bool have_v2 = false;
    for (size_t i = 0u; records && i < count; ++i) {
        const uint8_t *r = records + i * EVIST_RIDE_REC_SIZE;
        if (evist_ride_record_blank(r)) break;
        evist_ride_cfg_t cfg;
        if (!record_any_decode(r, &cfg)) continue;
        if (r[1] == EVIST_RIDE_REC_VERSION_2) {
            last_v2 = cfg;
            have_v2 = true;
        }
        if (r[1] == EVIST_RIDE_REC_VERSION && i > 0u) {
            const uint8_t *prev = r - EVIST_RIDE_REC_SIZE;
            evist_ride_cfg_t paired;
            if (prev[1] == EVIST_RIDE_REC_VERSION_2 && (prev[2] & 0x10u) &&
                evist_ride_record_v2_decode(prev, &paired) &&
                paired.mode == cfg.mode && paired.offroad_x100 == record_v1_speed(r))
                continue;
        }
        if (r[1] == EVIST_RIDE_REC_VERSION)
            cfg = merge_unpaired_v1(r, have_v2 ? &last_v2 : NULL, &cfg);
        found = cfg;
        any = true;
    }
    if (out) *out = found;
    return any;
}

bool evist_ride_store_load(const dpc245_persist_io_t *io, evist_ride_cfg_t *cfg,
                           uint32_t *next_addr) {
    evist_ride_cfg_t latest = evist_ride_defaults();
    evist_ride_cfg_t last_v2 = latest;
    bool found = false;
    bool have_v2 = false;
    uint32_t next = EVIST_PREF_RIDE_END;
    if (!io || !io->read) { if (cfg) *cfg=latest; if(next_addr)*next_addr=next; return false; }
    uint8_t rec[EVIST_RIDE_REC_SIZE];
    for (uint32_t i=0u;i<EVIST_RIDE_LOG_COUNT;i++) {
        uint32_t addr=EVIST_PREF_RIDE_BASE+i*EVIST_RIDE_REC_SIZE;
        if (!io->read(io->ctx,addr,rec,sizeof(rec))) {
            if (cfg) *cfg=latest;
            if (next_addr) *next_addr=EVIST_PREF_RIDE_END;
            return false;
        }
        if (evist_ride_record_blank(rec)) { next=addr; break; }
        evist_ride_cfg_t candidate;
        if (record_any_decode(rec,&candidate)) {
            if (rec[1] == EVIST_RIDE_REC_VERSION_2) {
                last_v2 = candidate;
                have_v2 = true;
            }
            bool paired_v1 = false;
            if (rec[1] == EVIST_RIDE_REC_VERSION && i > 0u) {
                uint8_t prev[EVIST_RIDE_REC_SIZE];
                evist_ride_cfg_t paired;
                if (!io->read(io->ctx, addr-EVIST_RIDE_REC_SIZE, prev, sizeof(prev))) {
                    if (cfg) *cfg=latest;
                    if (next_addr) *next_addr=EVIST_PREF_RIDE_END;
                    return false;
                }
                paired_v1 = prev[1] == EVIST_RIDE_REC_VERSION_2 && (prev[2] & 0x10u) &&
                    evist_ride_record_v2_decode(prev, &paired) &&
                    paired.mode == candidate.mode && paired.offroad_x100 == record_v1_speed(rec);
            }
            if (!paired_v1) {
                latest=rec[1] == EVIST_RIDE_REC_VERSION
                    ? merge_unpaired_v1(rec, have_v2 ? &last_v2 : NULL, &candidate)
                    : candidate;
                found=true;
            }
        }
    }
    if (cfg) *cfg=latest;
    if (next_addr) *next_addr=next;
    return found;
}

bool evist_ride_boot_load(const dpc245_persist_io_t *io, evist_ride_cfg_t *cfg,
                          uint32_t *next_addr) {
    return evist_ride_store_load(io,cfg,next_addr);
}

bool evist_ride_store_load_checked(const dpc245_persist_io_t *io,
                                   evist_ride_cfg_t *cfg) {
    if (!io || !io->read || !cfg) return false;
    uint32_t next;
    bool found=evist_ride_store_load(io,cfg,&next);
    if (found || next != EVIST_PREF_RIDE_END) return true;
    uint8_t record[EVIST_RIDE_REC_SIZE];
    for (uint32_t i=0u;i<EVIST_RIDE_LOG_COUNT;++i)
        if (!io->read(io->ctx,EVIST_PREF_RIDE_BASE+i*EVIST_RIDE_REC_SIZE,
                      record,sizeof record)) return false;
    return true;
}

evist_ride_store_result_t evist_ride_store_append(const dpc245_persist_io_t *io,
                           const evist_ride_cfg_t *before, const evist_ride_cfg_t *after,
                           uint32_t *next_addr) {
#if !EVIST_ENABLE_PERSISTENCE
    (void)io;(void)before;(void)after;(void)next_addr;
    return EVIST_RIDE_STORE_NO_MEMORY;
#else
    if (!io || !io->read || !io->write || !next_addr ||
        !evist_ride_cfg_valid(after)) return EVIST_RIDE_STORE_NO_MEMORY;
    if (before && before->mode==after->mode && before->offroad_x100==after->offroad_x100 &&
        before->legal_x100==after->legal_x100 && before->start==after->start &&
        before->last_mode==after->last_mode)
        return EVIST_RIDE_STORE_OK;
    if (*next_addr<EVIST_PREF_RIDE_BASE)
        return EVIST_RIDE_STORE_FULL;
    uint8_t blank[EVIST_RIDE_REC_SIZE],rec[EVIST_RIDE_REC_SIZE],verify[EVIST_RIDE_REC_SIZE];
    /* A failed program may leave a torn slot; append after it on the next SAVE. */
    while (*next_addr<=EVIST_PREF_RIDE_END-EVIST_RIDE_REC_SIZE) {
        if (!io->read(io->ctx,*next_addr,blank,sizeof(blank))) return EVIST_RIDE_STORE_NO_MEMORY;
        if (evist_ride_record_blank(blank)) break;
        *next_addr+=EVIST_RIDE_REC_SIZE;
    }
    uint32_t required = EVIST_RIDE_REC_SIZE;
    if (*next_addr > EVIST_PREF_RIDE_END-required) return EVIST_RIDE_STORE_FULL;
    if (!evist_ride_record_v2_encode(after,rec)) return EVIST_RIDE_STORE_FAILED;
    if (!io->write(io->ctx,*next_addr,rec,sizeof(rec))) return EVIST_RIDE_STORE_FAILED;
    if (!io->read(io->ctx,*next_addr,verify,sizeof(verify))) return EVIST_RIDE_STORE_FAILED;
    for (size_t i=0u;i<sizeof(rec);i++) if (verify[i]!=rec[i]) return EVIST_RIDE_STORE_FAILED;
    *next_addr+=EVIST_RIDE_REC_SIZE;
    return EVIST_RIDE_STORE_OK;
#endif
}

const char *evist_ride_store_status(evist_ride_store_result_t result) {
#if !EVIST_ENABLE_PERSISTENCE
    (void)result;
    return "SAVING DISABLED IN THIS BUILD";
#else
    switch(result) {
    case EVIST_RIDE_STORE_OK: return "SAVED";
    case EVIST_RIDE_STORE_FULL: return "NOT SAVED - LOG FULL";
    case EVIST_RIDE_STORE_NO_MEMORY: return "SAVED RAM ONLY (NO SPI)";
    default: return "SAVE FAILED";
    }
#endif
}

void evist_ride_apply_init(evist_ride_apply_t *a) {
    if (!a) return;
    a->phase=EVIST_RIDE_APPLY_IDLE;a->target_x100=EVIST_RIDE_LEGAL_X100;
    a->last_applied_x100=0u;a->last_result=EVIST_TXN_NO_READBACK;a->attempts=0u;
    for (unsigned i=0u;i<EVIST_WHEEL_PAYLOAD_LEN;i++)a->original[i]=0u;
}

void evist_ride_apply_begin(evist_ride_apply_t *a,uint16_t target_x100) {
    if (!a || !evist_ride_speed_valid(target_x100)) return;
    a->target_x100=target_x100;a->attempts=0u;a->phase=EVIST_RIDE_APPLY_READ;
    a->last_result=EVIST_TXN_NO_READBACK;
}

bool evist_ride_apply_start_txn(evist_ride_apply_t *a,evist_ctrl_txn_t *txn,uint32_t now_ms) {
    if (!a || !txn || evist_ctrl_txn_busy(txn)) return false;
    if (a->phase==EVIST_RIDE_APPLY_READ) {
        if (a->attempts>=3u) return false;
        if (!evist_ctrl_txn_read_wheel(txn,now_ms)) return false;
        a->attempts++;
        return true;
    }
#if EVIST_ENABLE_RIDE_SPEED_WRITE
    if (a->phase==EVIST_RIDE_APPLY_WRITE) {
        /* The third successful read must still be allowed to reach its write. */
        evist_wheel_config_t wheel;
        evist_wheel_config_decode(a->original,&wheel);
        wheel.speed_limit_centi_kph=a->target_x100;
        return evist_ctrl_txn_write_wheel(txn,&wheel,now_ms);
    }
#endif
    return false;
}

void evist_ride_apply_on_result(evist_ride_apply_t *a,evist_txn_result_t result,
                                const uint8_t data[64]) {
    if (!a) return;
    a->last_result=result;
    if (a->phase==EVIST_RIDE_APPLY_READ && result==EVIST_TXN_OK && data) {
        uint16_t speed=(uint16_t)(data[0]|((uint16_t)data[1]<<8));
        if (speed==a->target_x100) {
            a->last_applied_x100=speed;a->phase=EVIST_RIDE_APPLY_OK;return;
        }
        for(unsigned i=0u;i<EVIST_WHEEL_PAYLOAD_LEN;i++)a->original[i]=data[i];
#if EVIST_ENABLE_RIDE_SPEED_WRITE
        a->phase=EVIST_RIDE_APPLY_WRITE;
#else
        a->phase=EVIST_RIDE_APPLY_FAILED;
#endif
        return;
    }
    if (a->phase==EVIST_RIDE_APPLY_WRITE && result==EVIST_TXN_OK && data) {
        a->last_applied_x100=a->target_x100;a->phase=EVIST_RIDE_APPLY_OK;return;
    }
    a->phase=a->attempts<3u?EVIST_RIDE_APPLY_READ:EVIST_RIDE_APPLY_FAILED;
}
