/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "evist_preferences.h"
#include <stddef.h>

#define PREF_BASE       EVIST_PREF_REGION_BASE
#define PREF_SET_BASE   EVIST_PREF_SET_BASE
#define PREF_SET_END    EVIST_PREF_SET_END
#define PREF_OLD_BASE   0x001FFF00u
#define PREF_OLD_END    0x00200000u
#define PREF_XOR_PAIR   0x5Au
#define PREF_XOR_SET    0xC7u
#define PREF_GRAPH_V1_BASE 0x40u
#define PREF_GRAPH_V2_BASE 0x80u
#define PREF_TEMP_STEP  0x40u
#define PREF_CHUNK      64u    /* bytes per read when scanning or blank-checking */

static const uint8_t header[8] = {'E','V','P','1',0xBAu,0xA9u,0xAFu,0xCEu};

void evist_preferences_defaults(evist_preferences_t *p) {
    if (!p) return;
    p->assist_level = 0u;
    p->assist_mode_mask = EVIST_ASSIST_MODE_MASK_ALL;
    p->lower_panel = 0u;
    p->temp_view = EVIST_TEMP_BOTH;
    p->motor_temp_limit_c = 0u;
    p->ctrl_temp_limit_c = 0u;
    p->firmware_profile = EVIST_PROFILE_BAFANG;
    p->graph_scale_w = EVIST_PREF_GRAPH_DEFAULT_W;
    p->battery_mask = (uint8_t)(EVIST_BATTERY_ALL & (uint8_t)~EVIST_BATTERY_BAR);
    p->light_mode = EVIST_PREF_LIGHT_AUTO;
    p->light_gain = 5u;
    p->light_dwell = 3u;
    p->light_threshold = 3200u;
    p->light_level = 2u;
    p->panel_mask = (1u << EVIST_BASE_PANEL_COUNT) - 1u;
    p->quick_walk_enabled = true;
    p->quick_walk_hold_steps = 1u;
}

bool evist_preferences_validate(const evist_preferences_t *p) {
    if (!p || p->assist_level > 9u || p->lower_panel >= EVIST_PANEL_COUNT) return false;
    if ((p->assist_mode_mask & (uint8_t)~EVIST_ASSIST_MODE_MASK_ALL) != 0u) return false;
    if (p->temp_view > EVIST_TEMP_2) return false;
    if ((p->motor_temp_limit_c && (p->motor_temp_limit_c < 40u || p->motor_temp_limit_c > 150u)) ||
        (p->ctrl_temp_limit_c && (p->ctrl_temp_limit_c < 40u || p->ctrl_temp_limit_c > 150u))) return false;
    if (p->firmware_profile > EVIST_PROFILE_FAKE_TAXI) return false;
    if (p->graph_scale_w < 250u || p->graph_scale_w > 3000u || (p->graph_scale_w % 25u) != 0u) return false;
    if ((p->battery_mask & (uint8_t)~EVIST_BATTERY_ALL) != 0u) return false;
    if (p->light_mode > EVIST_PREF_LIGHT_MANUAL || p->light_gain < 1u || p->light_gain > 9u) return false;
    if (p->light_dwell < 1u || p->light_dwell > 30u) return false;
    if (p->light_threshold < 1800u || p->light_threshold > 5000u || ((p->light_threshold - 1800u) % 100u) != 0u) return false;
    if (p->light_level > 15u) return false;
    if ((p->panel_mask & EVIST_PANEL_MASK_ALL) == 0u || (p->panel_mask & (uint16_t)~EVIST_PANEL_MASK_ALL) != 0u) return false;
    if (p->quick_walk_hold_steps < 1u || p->quick_walk_hold_steps > 10u) return false;
    return true;
}

/* A read either returns data, finds nothing, or fails. A failed read is never
 * taken as "nothing stored": defaults must not pass for the memory content. */
typedef enum { PREF_READ_OK = 0, PREF_READ_ABSENT, PREF_READ_IO_ERROR } pref_read_t;

static pref_read_t read_header(const dpc245_persist_io_t *io) {
    uint8_t b[8];
    if (!io || !io->read || !io->read(io->ctx, PREF_BASE, b, sizeof(b))) return PREF_READ_IO_ERROR;
    for (size_t i=0;i<sizeof(b);++i) if (b[i] != header[i]) return PREF_READ_ABSENT;
    return PREF_READ_OK;
}

static bool region_blank(const dpc245_persist_io_t *io) {
    uint8_t b[PREF_CHUNK];
    if (!io || !io->read) return false;
    for (uint32_t a=PREF_BASE; a<EVIST_PREF_REGION_END; a+=(uint32_t)sizeof(b)) {
        if (!io->read(io->ctx,a,b,sizeof(b))) return false;
        for(size_t i=0;i<sizeof(b);++i) if(b[i]!=0xFFu) return false;
    }
    return true;
}

static bool write_verified(const dpc245_persist_io_t *io, uint32_t addr, const uint8_t *src, size_t n) {
    uint8_t check[8];
    if (n > sizeof(check) || !io->write(io->ctx, addr, src, n)) return false;
    if (!io->read(io->ctx, addr, check, n)) return false;
    for (size_t i = 0; i < n; ++i) if (check[i] != src[i]) return false;
    return true;
}

static bool valid_record(uint8_t code,uint8_t check,uint8_t xorv){ return check==(uint8_t)(code^xorv); }

/* ---- settings code log ------------------------------------------------------
 * Record: byte0 = code, byte1 = code ^ 0xC7. Each setting owns a code range;
 * the newest valid record in its range wins. The read-only settings log at
 * 0x1FFF00 is used only for settings the main settings log does not hold. */
enum {
    SLOT_PROFILE = 0, SLOT_BATTERY, SLOT_LIGHT_MODE, SLOT_GAIN, SLOT_DWELL,
    SLOT_THRESHOLD, SLOT_LEVEL, SLOT_WALK, SLOT_HOLD, SLOT_GRAPH, SLOT_BATTERY_BAR,
    SLOT_PANEL0 /* .. SLOT_PANEL0 + 8 */
};
#define PREF_ALL_SLOTS ((1u << EVIST_PREF_SETTING_SLOTS) - 1u)

typedef struct { uint8_t lo1, hi1, lo2, hi2; } code_range_t; /* lo2 > hi2: no second range */
static const code_range_t slot_range[SLOT_PANEL0] = {
    {0x00u,0x01u,1u,0u}, {0x50u,0x57u,1u,0u}, {0x02u,0x03u,1u,0u}, {0x05u,0x0Du,1u,0u},
    {0x10u,0x2Du,1u,0u}, {0x59u,0x79u,1u,0u}, {0x7Au,0x89u,1u,0u}, {0x0Eu,0x0Fu,1u,0u},
    {0x4Du,0x4Fu,0xF9u,0xFFu}, {0x8Au,0xF8u,0x41u,0x4Cu}, {0x04u,0x04u,0x58u,0x58u}
};

int evist_pref_setting_slot_of_code(uint8_t c) {
    for (unsigned s = 0u; s < SLOT_PANEL0; ++s) {
        const code_range_t *r = &slot_range[s];
        if ((c >= r->lo1 && c <= r->hi1) || (r->lo2 <= r->hi2 && c >= r->lo2 && c <= r->hi2)) return (int)s;
    }
    if (c >= 0x2Eu && c <= (uint8_t)(0x2Eu + EVIST_BASE_PANEL_COUNT * 2u - 1u)) return (int)(SLOT_PANEL0 + (c - 0x2Eu) / 2u);
    return -1;
}

static uint8_t hold_from_code(uint8_t c){
    if(c>=0x4Du&&c<=0x4Fu)return (uint8_t)(1u+c-0x4Du);
    if(c>=0xF9u)return (uint8_t)(4u+c-0xF9u);
    return 4u;
}
static uint8_t hold_code(uint8_t v){return v<=3u?(uint8_t)(0x4Du+v-1u):(uint8_t)(0xF9u+v-4u);}

/* The code a save writes for `slot`; equal values always give equal codes. */
static uint8_t setting_code(const evist_preferences_t *p, unsigned slot) {
    switch (slot) {
    case SLOT_PROFILE:    return (uint8_t)p->firmware_profile;
    case SLOT_BATTERY:    return (uint8_t)(0x50u + (p->battery_mask & (uint8_t)~EVIST_BATTERY_BAR));
    case SLOT_LIGHT_MODE: return (uint8_t)(0x02u + (uint8_t)p->light_mode);
    case SLOT_GAIN:       return (uint8_t)(0x05u + p->light_gain - 1u);
    case SLOT_DWELL:      return (uint8_t)(0x10u + p->light_dwell - 1u);
    case SLOT_THRESHOLD:  return (uint8_t)(0x59u + (p->light_threshold - 1800u) / 100u);
    case SLOT_LEVEL:      return (uint8_t)(0x7Au + p->light_level);
    case SLOT_WALK:       return p->quick_walk_enabled ? 0x0Fu : 0x0Eu;
    case SLOT_HOLD:       return hold_code(p->quick_walk_hold_steps);
    case SLOT_GRAPH:      return (uint8_t)(PREF_GRAPH_V2_BASE + p->graph_scale_w / 25u);
    case SLOT_BATTERY_BAR:return (p->battery_mask & EVIST_BATTERY_BAR) ? 0x58u : 0x04u;
    default: {
        unsigned i = slot - SLOT_PANEL0;
        return (uint8_t)(0x2Eu + i * 2u + ((p->panel_mask & (1u << i)) ? 1u : 0u));
    }
    }
}

/* Settings logs are append-only with a blank tail; any non-blank record (torn
 * ones included) is used, so the end is found by binary search. */
static bool log_used_end(const dpc245_persist_io_t *io, uint32_t begin, uint32_t end, uint32_t *out) {
    uint32_t lo = 0u, hi = (end - begin) / 2u;
    uint8_t r[2];
    while (lo < hi) {
        uint32_t mid = lo + ((hi - lo) >> 1);
        if (!io->read(io->ctx, begin + mid * 2u, r, 2u)) return false;
        if (r[0] == 0xFFu && r[1] == 0xFFu) hi = mid;
        else lo = mid + 1u;
    }
    *out = begin + lo * 2u;
    return true;
}

/* One backwards pass over the used part of a log, PREF_CHUNK bytes per read,
 * keeping the newest valid code of every slot still missing. Stops as soon as
 * every slot is known. False only when a read fails. */
static bool scan_log(const dpc245_persist_io_t *io, uint32_t begin, uint32_t end,
                     evist_pref_image_t *img, uint32_t *used) {
    uint8_t b[PREF_CHUNK];
    uint32_t a;
    if (!log_used_end(io, begin, end, &a)) return false;
    if (used) *used = a;
    while (a > begin && (begin == PREF_SET_BASE || img->found != PREF_ALL_SLOTS)) {
        uint32_t n = (a - begin) < PREF_CHUNK ? (a - begin) : PREF_CHUNK;
        a -= n;
        if (!io->read(io->ctx, a, b, n)) return false;
        for (uint32_t i = n; i >= 2u; i -= 2u) {
            if (!valid_record(b[i - 2u], b[i - 1u], PREF_XOR_SET)) continue;
            if (begin == PREF_SET_BASE && b[i - 2u] == PREF_TEMP_STEP) {
                img->temp_state = (uint8_t)((img->temp_state + 1u) & 7u);
                continue;
            }
            int s = evist_pref_setting_slot_of_code(b[i - 2u]);
            if (s < 0 || (img->found & (1u << s)) != 0u) continue;
            img->found |= 1u << s;
            img->code[s] = b[i - 2u];
        }
    }
    return true;
}

/* ---- assist/panel pair log ------------------------------------------------
 * Record: byte0 = field code << 4 | assist level, byte1 = byte0 ^ 0x5A.
 * Field codes 0..8 are display-field numbers; 9..14 are alternative pair
 * codes that are accepted on read. The panel index used by
 * this firmware (order of the PANELS page) is different, so it is mapped.
 * The log ends at the RIDE MODE reservation, at the reserved boundary. */
#define PAIR_RECORD_COUNT ((EVIST_PREF_RIDE_BASE - EVIST_PREF_PAIR_BASE) / 2u)
#define PAIR_FIELD_CODE_MAX 15u
#define PAIR_TAIL_TOLERANCE 8u

static const uint8_t panel_of_field[PAIR_FIELD_CODE_MAX + 1u] = {
    /* 0 TRIP+ODO */ 0u, /* 1 CAD+TRQ */ 3u, /* 2 MAX+AVG */ 1u, /* 3 PEDAL+MOTOR */ 6u,
    /* 4 W+TEMP */ 5u, /* 5 RANGE+VOLT */ 7u, /* 6 CAD+CAL */ 2u, /* 7 VOLT+TEMP */ 8u,
    /* 8 TIME+TRIP */ 4u,
    /* alternative pair codes: 9 W+TEMP, 10 RANGE+VOLT, 11 TRIP+ODO, 12 MAX+AVG, 13 CAD+CAL, 14 TIME+TRIP */
    5u, 7u, 0u, 1u, 2u, 4u, 9u /* TEMP panel */
};
static const uint8_t field_of_panel[EVIST_PANEL_COUNT] = {0u, 2u, 6u, 1u, 8u, 4u, 3u, 5u, 7u, 15u};

uint8_t evist_pref_panel_from_record_field(uint8_t field) {
    return field <= PAIR_FIELD_CODE_MAX ? panel_of_field[field] : 0u;
}

uint8_t evist_pref_record_field_from_panel(uint8_t panel) {
    return panel < EVIST_PANEL_COUNT ? field_of_panel[panel] : 0u;
}

static uint32_t pair_addr(uint32_t idx) { return EVIST_PREF_PAIR_BASE + idx * 2u; }

static bool pair_read(const dpc245_persist_io_t *io, uint32_t idx, uint8_t r[2]) {
    return idx < PAIR_RECORD_COUNT && io->read(io->ctx, pair_addr(idx), r, 2u);
}

/* Append-only: any non-blank record (torn ones included) is occupied, so
 * occupied -> blank is monotonic and a binary search finds the end. */
static bool pair_first_blank(const dpc245_persist_io_t *io, uint32_t *out) {
    uint32_t lo = 0u, hi = PAIR_RECORD_COUNT;
    uint8_t r[2];
    while (lo < hi) {
        uint32_t mid = lo + ((hi - lo) >> 1);
        if (!pair_read(io, mid, r)) return false;
        if (r[0] == 0xFFu && r[1] == 0xFFu) hi = mid;
        else lo = mid + 1u;
    }
    *out = lo;
    return true;
}

static bool pair_usable(uint8_t b0) {
    return (b0 & 0x0Fu) <= 9u && (b0 >> 4) <= PAIR_FIELD_CODE_MAX;
}

/* The newest valid record among the last eight. */
static pref_read_t latest_pair(const dpc245_persist_io_t *io, uint32_t *end, uint8_t *pair) {
    uint32_t pos;
    uint8_t r[2];
    if (!pair_first_blank(io, &pos)) return PREF_READ_IO_ERROR;
    *end = pos;
    for (uint32_t back = 0u; back < PAIR_TAIL_TOLERANCE && pos > 0u; ++back) {
        --pos;
        if (!pair_read(io, pos, r)) return PREF_READ_IO_ERROR;
        if (!valid_record(r[0], r[1], PREF_XOR_PAIR) || !pair_usable(r[0])) continue;
        *pair = r[0];
        return PREF_READ_OK;
    }
    return PREF_READ_ABSENT;
}

/* ---- image: what is stored, and how it decodes ----------------------------- */

static void image_clear(evist_pref_image_t *img) {
    img->found = 0u;
    for (unsigned s = 0u; s < EVIST_PREF_SETTING_SLOTS; ++s) img->code[s] = 0u;
    img->header = false;
    img->have_pair = false;
    img->pair = 0u;
    img->set_end = PREF_SET_BASE;
    img->pair_end = 0u;
    img->temp_state = 0u;
}

/* Full load: every read must succeed. ABSENT = readable area without header. */
static pref_read_t image_scan(const dpc245_persist_io_t *io, evist_pref_image_t *img) {
    image_clear(img);
    pref_read_t r = read_header(io);
    if (r != PREF_READ_OK) return r;
    img->header = true;
    if (!scan_log(io, PREF_SET_BASE, PREF_SET_END, img, &img->set_end)) return PREF_READ_IO_ERROR;
    if (img->found != PREF_ALL_SLOTS && !scan_log(io, PREF_OLD_BASE, PREF_OLD_END, img, NULL))
        return PREF_READ_IO_ERROR;
    r = latest_pair(io, &img->pair_end, &img->pair);
    if (r == PREF_READ_IO_ERROR) return r;
    img->have_pair = r == PREF_READ_OK;
    return PREF_READ_OK;
}

static bool have(const evist_pref_image_t *img, unsigned slot) { return (img->found & (1u << slot)) != 0u; }

/* PANELS bits as stored (a missing record means "shown"), before the
 * never-empty rule of the decoded mask. */
static uint16_t stored_panel_bits(const evist_pref_image_t *img) {
    uint16_t m = 0u;
    for (unsigned i = 0u; i < EVIST_BASE_PANEL_COUNT; ++i) {
        unsigned s = SLOT_PANEL0 + i;
        if (!have(img, s) || img->code[s] == (uint8_t)(0x2Fu + i * 2u)) m |= (uint16_t)(1u << i);
    }
    if ((img->temp_state & 4u) != 0u) m |= 1u << EVIST_BASE_PANEL_COUNT;
    return m;
}

static bool image_decode(const evist_pref_image_t *img, evist_preferences_t *p) {
    evist_preferences_defaults(p);
    p->temp_view = (evist_temp_view_t)(img->temp_state & 3u);
    const uint8_t *c = img->code;
    if (have(img, SLOT_PROFILE))    p->firmware_profile = (evist_firmware_profile_t)c[SLOT_PROFILE];
    if (have(img, SLOT_BATTERY))    p->battery_mask = (uint8_t)(c[SLOT_BATTERY] - 0x50u);
    if (have(img, SLOT_BATTERY_BAR) && c[SLOT_BATTERY_BAR] == 0x58u) p->battery_mask |= EVIST_BATTERY_BAR;
    if (have(img, SLOT_LIGHT_MODE)) p->light_mode = (evist_pref_light_mode_t)(c[SLOT_LIGHT_MODE] - 0x02u);
    if (have(img, SLOT_GAIN))       p->light_gain = (uint8_t)(1u + c[SLOT_GAIN] - 0x05u);
    if (have(img, SLOT_DWELL))      p->light_dwell = (uint8_t)(1u + c[SLOT_DWELL] - 0x10u);
    if (have(img, SLOT_THRESHOLD))  p->light_threshold = (uint16_t)(1800u + (uint16_t)(c[SLOT_THRESHOLD] - 0x59u) * 100u);
    if (have(img, SLOT_LEVEL))      p->light_level = (uint8_t)(c[SLOT_LEVEL] - 0x7Au);
    if (have(img, SLOT_WALK))       p->quick_walk_enabled = c[SLOT_WALK] == 0x0Fu;
    if (have(img, SLOT_HOLD))       p->quick_walk_hold_steps = hold_from_code(c[SLOT_HOLD]);
    if (have(img, SLOT_GRAPH)) {
        uint8_t g = c[SLOT_GRAPH];
        p->graph_scale_w = (g >= 0x8Au) ? (uint16_t)((g - PREF_GRAPH_V2_BASE) * 25u) : (uint16_t)((g - PREF_GRAPH_V1_BASE) * 250u);
    }
    if (img->have_pair) {
        p->assist_level = (uint8_t)(img->pair & 0x0Fu);
        p->lower_panel = evist_pref_panel_from_record_field((uint8_t)(img->pair >> 4));
    }
    p->panel_mask = stored_panel_bits(img);
    if ((p->panel_mask & EVIST_PANEL_MASK_ALL) == 0u)
        p->panel_mask = (1u << EVIST_BASE_PANEL_COUNT) - 1u;
    return evist_preferences_validate(p);
}

bool evist_preferences_load(const dpc245_persist_io_t *io, evist_preferences_t *p) {
    evist_pref_image_t img;
    if (!p) return false;
    if (image_scan(io, &img) != PREF_READ_OK) { evist_preferences_defaults(p); return false; }
    return image_decode(&img, p);
}

/* ---- saving ------------------------------------------------------------------ */

static bool same_preferences(const evist_preferences_t *a, const evist_preferences_t *b) {
    return a->assist_level == b->assist_level && a->lower_panel == b->lower_panel &&
           a->assist_mode_mask == b->assist_mode_mask &&
           a->firmware_profile == b->firmware_profile && a->graph_scale_w == b->graph_scale_w &&
           a->battery_mask == b->battery_mask && a->light_mode == b->light_mode &&
           a->light_gain == b->light_gain && a->light_dwell == b->light_dwell &&
           a->light_threshold == b->light_threshold && a->light_level == b->light_level &&
           a->panel_mask == b->panel_mask && a->quick_walk_enabled == b->quick_walk_enabled &&
           a->quick_walk_hold_steps == b->quick_walk_hold_steps && a->temp_view == b->temp_view &&
           a->motor_temp_limit_c == b->motor_temp_limit_c && a->ctrl_temp_limit_c == b->ctrl_temp_limit_c;
}

/* Codes for the settings of `after` that differ from `before` (NULL: all).
 * `before_panels` are the PANELS bits as stored. */
static uint8_t temp_state(const evist_preferences_t *p) {
    return (uint8_t)((uint8_t)p->temp_view |
           (((p->panel_mask & (1u << EVIST_BASE_PANEL_COUNT)) != 0u) ? 4u : 0u));
}

static size_t plan_codes(const evist_preferences_t *before, uint16_t before_panels, uint8_t prior_temp,
                         const evist_preferences_t *after, uint8_t codes[EVIST_PREF_MAX_CODES]) {
    size_t n = 0u;
    for (unsigned s = 0u; s < SLOT_PANEL0; ++s)
        if (!before || setting_code(before, s) != setting_code(after, s)) codes[n++] = setting_code(after, s);
    for (unsigned i = 0u; i < EVIST_BASE_PANEL_COUNT; ++i)
        if (!before || ((before_panels ^ after->panel_mask) & (1u << i)) != 0u)
            codes[n++] = setting_code(after, SLOT_PANEL0 + i);
    uint8_t steps = (uint8_t)((temp_state(after) - prior_temp) & 7u);
    for (uint8_t i = 0u; i < steps; ++i) codes[n++] = PREF_TEMP_STEP;
    return n;
}

static bool pair_differs(const evist_preferences_t *before, const evist_preferences_t *after) {
    return !before || before->assist_level != after->assist_level || before->lower_panel != after->lower_panel;
}

/* Appends at the ends recorded in `img` and keeps `img` equal to what was
 * really programmed (each record is read back before it is counted). Then
 * both ends are searched again: they must sit right after the new records,
 * so a load sees them as the newest. */
static evist_pref_save_result_t append_records(const dpc245_persist_io_t *io, evist_pref_image_t *img,
                                               const uint8_t *codes, size_t n,
                                               bool write_pair, const evist_preferences_t *after) {
    uint8_t r[2];
    uint32_t end;
    if (n == 0u && !write_pair) return EVIST_PREF_SAVE_OK;
    if (n != 0u && PREF_SET_END - img->set_end < (uint32_t)n * 2u) return EVIST_PREF_SAVE_LOG_FULL;
    if (write_pair && img->pair_end >= PAIR_RECORD_COUNT) return EVIST_PREF_SAVE_LOG_FULL; /* the RIDE reservation is never used */
    if (!img->header) {
        if (!write_verified(io, PREF_BASE, header, sizeof(header))) return EVIST_PREF_SAVE_WRITE_FAILED;
        img->header = true;
    }
    for (size_t i = 0u; i < n; ++i) {
        r[0] = codes[i]; r[1] = (uint8_t)(codes[i] ^ PREF_XOR_SET);
        if (!write_verified(io, img->set_end, r, 2u)) return EVIST_PREF_SAVE_WRITE_FAILED;
        if (codes[i] == PREF_TEMP_STEP) img->temp_state = (uint8_t)((img->temp_state + 1u) & 7u);
        else {
            int s = evist_pref_setting_slot_of_code(codes[i]);
            img->found |= 1u << s;
            img->code[s] = codes[i];
        }
        img->set_end += 2u;
    }
    if (n != 0u && (!log_used_end(io, PREF_SET_BASE, PREF_SET_END, &end) || end != img->set_end))
        return EVIST_PREF_SAVE_VERIFY_FAILED;
    if (write_pair) {
        r[0] = (uint8_t)((evist_pref_record_field_from_panel(after->lower_panel) << 4) | (after->assist_level & 0x0Fu));
        r[1] = (uint8_t)(r[0] ^ PREF_XOR_PAIR);
        if (!write_verified(io, pair_addr(img->pair_end), r, 2u)) return EVIST_PREF_SAVE_WRITE_FAILED;
        img->pair = r[0];
        img->have_pair = true;
        img->pair_end++;
        if (!pair_first_blank(io, &end) || end != img->pair_end) return EVIST_PREF_SAVE_VERIFY_FAILED;
    }
    return EVIST_PREF_SAVE_OK;
}

/* SAVED is reported only when a fresh full load returns exactly `after`. A
 * readable area without header loads as the defaults only if it is blank. */
static evist_pref_save_result_t verify_full(const dpc245_persist_io_t *io, const evist_preferences_t *after) {
    evist_pref_image_t img;
    evist_preferences_t stored;
    pref_read_t r = image_scan(io, &img);
    if (r == PREF_READ_IO_ERROR) return EVIST_PREF_SAVE_VERIFY_FAILED;
    if (r == PREF_READ_ABSENT) {
        if (!region_blank(io)) return EVIST_PREF_SAVE_NO_MEMORY;
        evist_preferences_defaults(&stored);
    } else if (!image_decode(&img, &stored)) {
        return EVIST_PREF_SAVE_VERIFY_FAILED;
    }
    return same_preferences(&stored, after) ? EVIST_PREF_SAVE_OK : EVIST_PREF_SAVE_VERIFY_FAILED;
}

evist_pref_save_result_t evist_preferences_save(const dpc245_persist_io_t *io,const evist_preferences_t *before,const evist_preferences_t *after){
    uint8_t codes[EVIST_PREF_MAX_CODES];
    if(!after||!evist_preferences_validate(after))return EVIST_PREF_SAVE_INVALID;
    if(!io||!io->read||!io->write)return EVIST_PREF_SAVE_NO_MEMORY;
    uint8_t prior_temp = before ? temp_state(before) : 0u;
    if (!before) {
        pref_read_t h = read_header(io);
        if (h == PREF_READ_IO_ERROR) return EVIST_PREF_SAVE_NO_MEMORY;
        if (h == PREF_READ_OK) {
            evist_preferences_t stored;
            if (!evist_preferences_load(io, &stored)) return EVIST_PREF_SAVE_NO_MEMORY;
            prior_temp = temp_state(&stored);
        }
    }
    const size_t n=plan_codes(before,before?before->panel_mask:0u,prior_temp,after,codes);
    const bool pair=pair_differs(before,after);
    if(n!=0u||pair){
        evist_pref_image_t img;
        image_clear(&img);
        pref_read_t h=read_header(io);
        if(h==PREF_READ_IO_ERROR)return EVIST_PREF_SAVE_NO_MEMORY;
        if(h==PREF_READ_ABSENT&&!region_blank(io))return EVIST_PREF_SAVE_NO_MEMORY; /* never write over foreign data */
        img.header=h==PREF_READ_OK;
        if(!log_used_end(io,PREF_SET_BASE,PREF_SET_END,&img.set_end)||!pair_first_blank(io,&img.pair_end))
            return EVIST_PREF_SAVE_NO_MEMORY;
        evist_pref_save_result_t res=append_records(io,&img,codes,n,pair,after);
        if(res!=EVIST_PREF_SAVE_OK)return res;
    }
    return verify_full(io,after);
}

/* The memory still has the structure the snapshot recorded. */
static evist_pref_save_result_t store_check(const dpc245_persist_io_t *io, const evist_pref_image_t *img) {
    uint8_t b[8];
    uint32_t end;
    if (!io->read(io->ctx, PREF_BASE, b, sizeof(b))) return EVIST_PREF_SAVE_NO_MEMORY;
    for (size_t i = 0u; i < sizeof(b); ++i)
        if (b[i] != (img->header ? header[i] : 0xFFu)) return EVIST_PREF_SAVE_VERIFY_FAILED;
    if (!log_used_end(io, PREF_SET_BASE, PREF_SET_END, &end)) return EVIST_PREF_SAVE_NO_MEMORY;
    if (end != img->set_end) return EVIST_PREF_SAVE_VERIFY_FAILED;
    if (!pair_first_blank(io, &end)) return EVIST_PREF_SAVE_NO_MEMORY;
    if (end != img->pair_end) return EVIST_PREF_SAVE_VERIFY_FAILED;
    return EVIST_PREF_SAVE_OK;
}

/* ---- store: cooperative relearn and verified save ------------------------- */

enum {
    STORE_IDLE = 0, STORE_HEADER, STORE_BLANK, STORE_SET_END, STORE_SET_SCAN,
    STORE_OLD_END, STORE_OLD_SCAN, STORE_PAIR, STORE_DECODE, STORE_SAVE,
    STORE_WRITE_HEADER, STORE_WRITE_SET, STORE_VERIFY_SET, STORE_WRITE_PAIR,
    STORE_VERIFY_PAIR, STORE_VERIFY_TARGET
};

static evist_pref_save_result_t store_finish(evist_pref_store_t *s, evist_pref_save_result_t result) {
    s->saving = false;
    s->state = STORE_IDLE;
    if (result != EVIST_PREF_SAVE_OK && result != EVIST_PREF_SAVE_LOG_FULL) s->known = false;
    return result;
}

/* Exactly one backward read, using the same precedence/decoder as scan_log. */
static bool store_scan_chunk(evist_pref_store_t *s, const dpc245_persist_io_t *io, uint32_t begin) {
    uint8_t b[PREF_CHUNK];
    uint32_t n = s->scan_addr - begin;
    if (n > PREF_CHUNK) n = PREF_CHUNK;
    s->scan_addr -= n;
    if (!io->read(io->ctx, s->scan_addr, b, n)) return false;
    for (uint32_t i = n; i >= 2u; i -= 2u) {
        if (!valid_record(b[i - 2u], b[i - 1u], PREF_XOR_SET)) continue;
        if (begin == PREF_SET_BASE && b[i - 2u] == PREF_TEMP_STEP) {
            s->image.temp_state = (uint8_t)((s->image.temp_state + 1u) & 7u);
            continue;
        }
        int slot = evist_pref_setting_slot_of_code(b[i - 2u]);
        if (slot < 0 || (s->image.found & (1u << slot)) != 0u) continue;
        s->image.found |= 1u << slot;
        s->image.code[slot] = b[i - 2u];
    }
    return true;
}

void evist_pref_store_init(evist_pref_store_t *s, const dpc245_persist_io_t *io, evist_preferences_t *loaded) {
    if (!s) return;
    s->saving = false;
    s->state = STORE_IDLE;
    s->scan_addr = 0u;
    evist_preferences_defaults(&s->target);
    pref_read_t r = image_scan(io, &s->image);
    if (r == PREF_READ_OK) s->known = image_decode(&s->image, &s->persisted);
    else {
        s->known = r == PREF_READ_ABSENT && region_blank(io);
        image_clear(&s->image);
        evist_preferences_defaults(&s->persisted);
    }
    if (!s->known) evist_preferences_defaults(&s->persisted);
    if (loaded) *loaded = s->persisted;
}

bool evist_pref_store_busy(const evist_pref_store_t *s) { return s && s->saving; }

bool evist_pref_store_begin(evist_pref_store_t *s, const evist_preferences_t *after) {
    if (!s || s->saving || !after || !evist_preferences_validate(after)) return false;
    s->target = *after;
    s->saving = true;
    s->state = s->known ? STORE_SAVE : STORE_HEADER;
    return true;
}

/* Binary end searches and the eight-record pair tail are already bounded.
 * Long/blank scans consume at most one 64-byte read per application loop.
 * Errors end this request, leaving UNKNOWN; a later explicit request relearns.
 * No post-failure full rescan runs inside the SAVE step. */
evist_pref_save_result_t evist_pref_store_save_step(evist_pref_store_t *s, const dpc245_persist_io_t *io) {
    if (!s || !s->saving) return EVIST_PREF_SAVE_INVALID; /* idle is not SAVED */
    if (!io || !io->read || !io->write) return store_finish(s, EVIST_PREF_SAVE_NO_MEMORY);
    switch (s->state) {
    case STORE_HEADER: {
        image_clear(&s->image);
        pref_read_t r = read_header(io);
        if (r == PREF_READ_IO_ERROR) return store_finish(s, EVIST_PREF_SAVE_NO_MEMORY);
        if (r == PREF_READ_ABSENT) { s->scan_addr = PREF_BASE; s->state = STORE_BLANK; }
        else { s->image.header = true; s->state = STORE_SET_END; }
        break;
    }
    case STORE_BLANK: {
        uint8_t b[PREF_CHUNK];
        if (!io->read(io->ctx, s->scan_addr, b, sizeof(b))) return store_finish(s, EVIST_PREF_SAVE_NO_MEMORY);
        for (size_t i = 0u; i < sizeof(b); ++i)
            if (b[i] != 0xFFu) return store_finish(s, EVIST_PREF_SAVE_NO_MEMORY);
        s->scan_addr += (uint32_t)sizeof(b);
        if (s->scan_addr == EVIST_PREF_REGION_END) s->state = STORE_DECODE;
        break;
    }
    case STORE_SET_END:
        if (!log_used_end(io, PREF_SET_BASE, PREF_SET_END, &s->image.set_end)) return store_finish(s, EVIST_PREF_SAVE_NO_MEMORY);
        s->scan_addr = s->image.set_end;
        s->state = STORE_SET_SCAN;
        break;
    case STORE_SET_SCAN:
        if (s->scan_addr > PREF_SET_BASE) {
            if (!store_scan_chunk(s, io, PREF_SET_BASE)) return store_finish(s, EVIST_PREF_SAVE_NO_MEMORY);
        } else s->state = s->image.found == PREF_ALL_SLOTS ? STORE_PAIR : STORE_OLD_END;
        break;
    case STORE_OLD_END:
        if (!log_used_end(io, PREF_OLD_BASE, PREF_OLD_END, &s->scan_addr)) return store_finish(s, EVIST_PREF_SAVE_NO_MEMORY);
        s->state = STORE_OLD_SCAN;
        break;
    case STORE_OLD_SCAN:
        if (s->scan_addr > PREF_OLD_BASE && s->image.found != PREF_ALL_SLOTS) {
            if (!store_scan_chunk(s, io, PREF_OLD_BASE)) return store_finish(s, EVIST_PREF_SAVE_NO_MEMORY);
        } else s->state = STORE_PAIR;
        break;
    case STORE_PAIR: {
        pref_read_t r = latest_pair(io, &s->image.pair_end, &s->image.pair);
        if (r == PREF_READ_IO_ERROR) return store_finish(s, EVIST_PREF_SAVE_NO_MEMORY);
        s->image.have_pair = r == PREF_READ_OK;
        s->state = STORE_DECODE;
        break;
    }
    case STORE_DECODE:
        s->known = image_decode(&s->image, &s->persisted);
        if (!s->known) return store_finish(s, EVIST_PREF_SAVE_NO_MEMORY);
        s->state = STORE_SAVE;
        break;
    case STORE_SAVE: {
        evist_preferences_t current;
        if (!s->known || !image_decode(&s->image, &current)) return store_finish(s, EVIST_PREF_SAVE_NO_MEMORY);
        evist_pref_save_result_t res = store_check(io, &s->image);
        if (res != EVIST_PREF_SAVE_OK) return store_finish(s, res);
        s->code_count = plan_codes(&current, stored_panel_bits(&s->image), temp_state(&current), &s->target, s->codes);
        s->code_pos = 0u;
        s->write_pair = pair_differs(&current, &s->target);
        if ((s->code_count != 0u && PREF_SET_END - s->image.set_end < s->code_count * 2u) ||
            (s->write_pair && s->image.pair_end >= PAIR_RECORD_COUNT)) return store_finish(s, EVIST_PREF_SAVE_LOG_FULL);
        s->state = !s->image.header && (s->code_count != 0u || s->write_pair)
            ? STORE_WRITE_HEADER : STORE_WRITE_SET;
        break;
    }
    case STORE_WRITE_HEADER:
        s->known = false; /* committed snapshot is unknown throughout programming */
        if (!write_verified(io, PREF_BASE, header, sizeof(header))) return store_finish(s, EVIST_PREF_SAVE_WRITE_FAILED);
        s->image.header = true;
        s->state = STORE_WRITE_SET;
        break;
    case STORE_WRITE_SET: {
        if (s->code_pos == s->code_count) {
            s->state = s->code_count != 0u ? STORE_VERIFY_SET : STORE_WRITE_PAIR;
            break;
        }
        uint8_t r[2] = {s->codes[s->code_pos], (uint8_t)(s->codes[s->code_pos] ^ PREF_XOR_SET)};
        int slot = r[0] == PREF_TEMP_STEP ? -1 : evist_pref_setting_slot_of_code(r[0]);
        if (slot < 0 && r[0] != PREF_TEMP_STEP) return store_finish(s, EVIST_PREF_SAVE_INVALID);
        s->known = false;
        if (!write_verified(io, s->image.set_end, r, sizeof(r))) return store_finish(s, EVIST_PREF_SAVE_WRITE_FAILED);
        if (r[0] == PREF_TEMP_STEP) s->image.temp_state = (uint8_t)((s->image.temp_state + 1u) & 7u);
        else { s->image.found |= 1u << slot; s->image.code[slot] = r[0]; }
        s->image.set_end += 2u;
        ++s->code_pos;
        break;
    }
    case STORE_VERIFY_SET: {
        uint32_t end;
        if (!log_used_end(io, PREF_SET_BASE, PREF_SET_END, &end) || end != s->image.set_end)
            return store_finish(s, EVIST_PREF_SAVE_VERIFY_FAILED);
        s->state = STORE_WRITE_PAIR;
        break;
    }
    case STORE_WRITE_PAIR: {
        if (!s->write_pair) { s->state = STORE_VERIFY_TARGET; break; }
        uint8_t code = (uint8_t)((evist_pref_record_field_from_panel(s->target.lower_panel) << 4) | s->target.assist_level);
        uint8_t r[2] = {code, (uint8_t)(code ^ PREF_XOR_PAIR)};
        s->known = false;
        if (!write_verified(io, pair_addr(s->image.pair_end), r, sizeof(r))) return store_finish(s, EVIST_PREF_SAVE_WRITE_FAILED);
        s->image.pair = code;
        s->image.have_pair = true;
        ++s->image.pair_end;
        s->state = STORE_VERIFY_PAIR;
        break;
    }
    case STORE_VERIFY_PAIR: {
        uint32_t end;
        if (!pair_first_blank(io, &end) || end != s->image.pair_end)
            return store_finish(s, EVIST_PREF_SAVE_VERIFY_FAILED);
        s->state = STORE_VERIFY_TARGET;
        break;
    }
    case STORE_VERIFY_TARGET: {
        evist_preferences_t decoded;
        if (!image_decode(&s->image, &decoded) || !same_preferences(&decoded, &s->target))
            return store_finish(s, EVIST_PREF_SAVE_VERIFY_FAILED);
        s->persisted = s->target;
        s->known = true;
        return store_finish(s, EVIST_PREF_SAVE_OK);
    }
    default: return store_finish(s, EVIST_PREF_SAVE_VERIFY_FAILED);
    }
    return EVIST_PREF_SAVE_IN_PROGRESS;
}

/* Compatibility helper for host callers. The application uses begin +
 * one step per service loop; total synchronous relearn is intentionally not a
 * foreground-time guarantee. An active target cannot be overwritten. */
evist_pref_save_result_t evist_pref_store_save(evist_pref_store_t *s, const dpc245_persist_io_t *io, const evist_preferences_t *after) {
    if (!s || !after || !evist_preferences_validate(after)) return EVIST_PREF_SAVE_INVALID;
    if (s->saving) return EVIST_PREF_SAVE_IN_PROGRESS;
    if (!io || !io->read || !io->write) return EVIST_PREF_SAVE_NO_MEMORY;
    if (!evist_pref_store_begin(s, after)) return EVIST_PREF_SAVE_INVALID;
    evist_pref_save_result_t result;
    do { result = evist_pref_store_save_step(s, io); } while (result == EVIST_PREF_SAVE_IN_PROGRESS);
    return result;
}

const char *evist_pref_save_text(evist_pref_save_result_t result){
    switch(result){
    case EVIST_PREF_SAVE_OK:           return "SAVED";
    case EVIST_PREF_SAVE_INVALID:      return "INVALID - NOT SAVED";
    case EVIST_PREF_SAVE_NO_MEMORY:    return "NO MEMORY - NOT SAVED";
    case EVIST_PREF_SAVE_LOG_FULL:     return "MEMORY FULL - NOT SAVED";
    case EVIST_PREF_SAVE_WRITE_FAILED: return "WRITE ERROR - NOT SAVED";
    case EVIST_PREF_SAVE_VERIFY_FAILED: return "VERIFY FAILED - NOT SAVED";
    case EVIST_PREF_SAVE_IN_PROGRESS:  return "SAVE IN PROGRESS";
    default:                           return "NOT SAVED";
    }
}
