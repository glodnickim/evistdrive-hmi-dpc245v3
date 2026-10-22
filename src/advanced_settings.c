/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "advanced_settings.h"
#include "assist_profile.h"
#include "app_config.h"

#define MENU_ITEMS 13u
#define MENU_EXIT  12u

static const char *const menu_items[MENU_ITEMS] = {
    "WHEEL", "ASSIST", "ACCEL", "POWER GRAPH", "FIRMWARE",
    "BATTERY", "LIGHT", "PANELS", "QUICK WALK", "MODE", "DIAG", "ASSIST MODES", "EXIT"
};
static const evist_adv_screen_t menu_targets[MENU_EXIT] = {
    EVIST_ADV_WHEEL, EVIST_ADV_ASSIST, EVIST_ADV_ACCEL, EVIST_ADV_GRAPH, EVIST_ADV_FIRMWARE,
    EVIST_ADV_BATTERY, EVIST_ADV_LIGHT, EVIST_ADV_PANELS, EVIST_ADV_QUICK_WALK, EVIST_ADV_MODE, EVIST_ADV_DIAG,
    EVIST_ADV_MODES
};

/* Diameter codes accepted by the controller and the circumference range each
 * allows (validated table). */
typedef struct { uint16_t code; uint16_t circ_min; uint16_t circ_max; const char *label; } wheel_diameter_t;
static const wheel_diameter_t diameters[EVIST_WHEEL_DIAMETERS] = {
    {0x0000u,    0u,    0u, "KEEP"},
    {0x0140u, 1290u, 1880u, "20\""},
    {0x0180u, 1290u, 2200u, "24\""},
    {0x01A0u, 1880u, 2510u, "26\""},
    {0x01B5u, 1880u, 2510u, "27.5\""},
    {0x01C0u, 1880u, 2510u, "28\""},
    {0x01D0u, 1880u, 2510u, "29\""},
};

/* ---- small helpers ------------------------------------------------------ */

static uint8_t wrap_up(uint8_t v, size_t n) { return n == 0u ? 0u : (v == 0u ? (uint8_t)(n - 1u) : (uint8_t)(v - 1u)); }
static uint8_t wrap_down(uint8_t v, size_t n) { return n == 0u ? 0u : (uint8_t)(((size_t)v + 1u) % n); }

static void cycle_u8(uint8_t *v, uint8_t lo, uint8_t hi, bool up, bool repeat) {
    if (up) { if (*v < hi) ++*v; else if (!repeat) *v = lo; }
    else { if (*v > lo) --*v; else if (!repeat) *v = hi; }
}
static void cycle_u16(uint16_t *v, uint16_t lo, uint16_t hi, uint16_t step, bool up, bool repeat) {
    if (up) { if (*v <= hi - step) *v = (uint16_t)(*v + step); else if (!repeat) *v = lo; }
    else { if (*v >= lo + step) *v = (uint16_t)(*v - step); else if (!repeat) *v = hi; }
}
/* WHEEL values stop at the limits, they never wrap. */
static bool clamp_step_u16(uint16_t *v, uint16_t lo, uint16_t hi, uint16_t step, bool up) {
    if (up && *v + step <= hi) { *v = (uint16_t)(*v + step); return true; }
    if (!up && *v >= lo + step) { *v = (uint16_t)(*v - step); return true; }
    return false;
}

static void copy64(uint8_t *d, const uint8_t *s) { for (uint8_t i = 0u; i < 64u; ++i) d[i] = s[i]; }
static void accel_values_from_block(evist_advanced_t *a) {
    for (uint8_t i = 0u; i < 9u; ++i) a->accel_values[i] = a->accel_block[i + 1u];
    a->accel_global = a->accel_block[54];
}

static void enter(evist_advanced_t *a, evist_adv_screen_t s) {
    a->screen = s;
    /* Returning to the menu restores the row the user left; other pages start at 0. */
    a->cursor = (s == EVIST_ADV_MENU) ? a->menu_cursor : 0u;
    a->edit = false;
    a->repeat_active = false;
    a->status = "";
}

static void request(evist_advanced_t *a, evist_adv_request_t r, const char *status) {
    a->request = r;
    a->status = status;
}

static void open_wheel(evist_advanced_t *a) {
    enter(a, EVIST_ADV_WHEEL);
    a->wheel_valid = false;
    a->wheel_dirty = false;
    a->wheel_diameter_index = 0u;
    request(a, EVIST_ADV_REQ_READ_WHEEL, "READING");
}

static void open_assist(evist_advanced_t *a) {
    enter(a, EVIST_ADV_ASSIST);
    a->assist_valid = false;
    a->assist_dirty = false;
    request(a, EVIST_ADV_REQ_READ_ASSIST, "READING");
}

static void open_accel(evist_advanced_t *a) {
    enter(a, EVIST_ADV_ACCEL);
    a->accel_valid = false;
    a->accel_dirty = false;
    a->accel_mode = EVIST_ACCEL_UNKNOWN;
    request(a, EVIST_ADV_REQ_DISCOVER_ACCEL, "DETECTING");
}

void evist_advanced_open_page(evist_advanced_t *a, evist_adv_screen_t screen) {
    if (!a) return;
    if (screen == EVIST_ADV_WHEEL) open_wheel(a);
    else if (screen == EVIST_ADV_ASSIST) open_assist(a);
    else if (screen == EVIST_ADV_ACCEL) open_accel(a);
    else if (screen == EVIST_ADV_DIAG) { a->diag_page = 0u; enter(a, screen); }
    else if (screen != EVIST_ADV_MENU) enter(a, screen);
}

static void back_to_menu(evist_advanced_t *a) {
    a->working = a->committed;
    a->ride_working = a->ride_committed;
    if (a->wheel_valid) {
        a->wheel_speed_centi_kph = a->wheel_read.speed_limit_centi_kph;
        a->wheel_circumference_mm = a->wheel_read.circumference_mm;
        a->wheel_diameter_index = 0u;
    }
    a->wheel_dirty = false;
    copy64(a->assist_block, a->assist_committed);
    a->assist_dirty = false;
    copy64(a->accel_block, a->accel_committed);
    a->accel_dirty = false;
    accel_values_from_block(a);
    a->request = EVIST_ADV_REQ_NONE;
    enter(a, EVIST_ADV_MENU);
}

void evist_advanced_abandon(evist_advanced_t *a) {
    if (!a) return;
    a->working = a->committed;
    a->ride_working = a->ride_committed;
    if (a->wheel_valid) {
        a->wheel_speed_centi_kph = a->wheel_read.speed_limit_centi_kph;
        a->wheel_circumference_mm = a->wheel_read.circumference_mm;
        a->wheel_diameter_index = 0u;
    }
    a->wheel_dirty = false;
    copy64(a->assist_block, a->assist_committed);
    a->assist_dirty = false;
    copy64(a->accel_block, a->accel_committed);
    a->accel_dirty = false;
    accel_values_from_block(a);
    a->edit = false;
    a->request = EVIST_ADV_REQ_NONE;
    a->preferences_changed = false;
    a->ride_changed = false;
    if (a->screen == EVIST_ADV_LIGHT) a->light_runtime_dirty = true;
}

static void commit_preferences(evist_advanced_t *a) {
    if (!evist_preferences_validate(&a->working)) { a->status = "INVALID"; return; }
    a->committed = a->working;
    a->preferences_changed = true;
    a->status = "SAVING";
}

/* ---- public model --------------------------------------------------------- */

void evist_advanced_open(evist_advanced_t *a, const evist_preferences_t *p, uint8_t assist_level_count) {
    if (!a) return;
    if (p) a->working = *p;
    else evist_preferences_defaults(&a->working);
    a->committed = a->working;
    a->screen = EVIST_ADV_MENU;
    a->cursor = 0u;
    a->edit = false;
    a->preferences_changed = false;
    a->exit_requested = false;
    a->status = "";
    a->assist_level_count = evist_assist_level_count_normalize(assist_level_count);
    a->request = EVIST_ADV_REQ_NONE;
    a->active_request = EVIST_ADV_REQ_NONE;
    a->busy = false;
    a->wheel_valid = false;
    a->wheel_dirty = false;
    a->wheel_diameter_index = 0u;
    a->wheel_speed_centi_kph = a->wheel_circumference_mm = 0u;
    a->wheel_read.speed_limit_centi_kph = a->wheel_read.wheel_code = a->wheel_read.circumference_mm = 0u;
    a->assist_valid = false;
    a->assist_dirty = false;
    a->accel_valid = false;
    a->accel_dirty = false;
    a->diag_page = 0u;
    a->menu_cursor = 0u;
    a->light_runtime_dirty = false;
    a->ride_changed = false;
    a->ride_committed=evist_ride_defaults();
    a->ride_effective_mode=EVIST_RIDE_LEGAL;
    a->ride_working = a->ride_committed;
    for (uint8_t i = 0u; i < 64u; ++i) {
        a->assist_block[i] = a->assist_committed[i] = 0u;
        a->accel_block[i] = a->accel_committed[i] = 0u;
    }
}

uint8_t evist_advanced_assist_cells(const evist_advanced_t *a) {
    if (!a) return 0u;
    uint8_t count = 0u;
    for (uint8_t level = 1u; level <= evist_assist_visible_levels(a->assist_level_count); ++level)
        if (a->working.assist_mode_mask & (uint8_t)(1u << (level - 1u))) count += 2u;
    return count;
}

uint8_t evist_advanced_assist_cell_level(const evist_advanced_t *a, uint8_t cell) {
    if (!a) return 0u;
    for (uint8_t level = 1u; level <= evist_assist_visible_levels(a->assist_level_count); ++level) {
        if ((a->working.assist_mode_mask & (uint8_t)(1u << (level - 1u))) == 0u) continue;
        if (cell < 2u) return level;
        cell -= 2u;
    }
    return 0u;
}

size_t evist_advanced_row_count(const evist_advanced_t *a) {
    if (!a) return 0u;
    switch (a->screen) {
    case EVIST_ADV_MENU:       return MENU_ITEMS;
    case EVIST_ADV_WHEEL:      return EVIST_WHEEL_ROWS;
    case EVIST_ADV_ASSIST:     return (size_t)evist_advanced_assist_cells(a) + 2u;
    case EVIST_ADV_ACCEL:      return (a->accel_mode == EVIST_ACCEL_PER_LEVEL ?
                                        evist_assist_visible_levels(a->assist_level_count) : 1u) + 2u;
    case EVIST_ADV_GRAPH:      return 3u;
    case EVIST_ADV_FIRMWARE:   return 3u;
    case EVIST_ADV_BATTERY:    return 6u;
    case EVIST_ADV_LIGHT:      return 7u;
    case EVIST_ADV_PANELS:     return EVIST_PANEL_COUNT + 2u;
    case EVIST_ADV_TEMP_VIEW:  return 5u;
    case EVIST_ADV_MODES:      return EVIST_ASSIST_MODE_COUNT + 2u;
    case EVIST_ADV_QUICK_WALK: return 4u;
    case EVIST_ADV_MODE:       return 6u;
    case EVIST_ADV_DIAG:       return 0u; /* page-based: drawn by the renderer, no rows */
    default:                   return 0u;
    }
}

const char *evist_advanced_menu_label(size_t i) { return i < MENU_ITEMS ? menu_items[i] : ""; }

const char *evist_advanced_screen_title(evist_adv_screen_t s) {
    switch (s) {
    case EVIST_ADV_WHEEL:      return "WHEEL";
    case EVIST_ADV_ASSIST:     return "ASSIST LEVELS";
    case EVIST_ADV_ACCEL:      return "ACCELERATION";
    case EVIST_ADV_GRAPH:      return "POWER GRAPH";
    case EVIST_ADV_FIRMWARE:   return "ASSIST PROFILE";
    case EVIST_ADV_BATTERY:    return "BATTERY VIEW";
    case EVIST_ADV_LIGHT:      return "BACKLIGHT";
    case EVIST_ADV_PANELS:     return "LOWER FIELDS";
    case EVIST_ADV_TEMP_VIEW:  return "TEMP VIEW";
    case EVIST_ADV_MODES:      return "ASSIST MODES";
    case EVIST_ADV_QUICK_WALK: return "QUICK WALK";
    case EVIST_ADV_MODE:       return "RIDE MODE";
    case EVIST_ADV_DIAG:       return "DIAGNOSTICS";
    default:                   return "ADVANCED SETTINGS";
    }
}

const char *evist_advanced_wheel_diameter_label(const evist_advanced_t *a) {
    if (!a) return "";
    if (a->wheel_diameter_index != 0u && a->wheel_diameter_index < EVIST_WHEEL_DIAMETERS)
        return diameters[a->wheel_diameter_index].label;
    for (uint8_t i = 1u; i < EVIST_WHEEL_DIAMETERS; ++i)
        if (diameters[i].code == a->wheel_read.wheel_code) return diameters[i].label;
    return "KEEP";
}

uint8_t evist_advanced_assist_cell_value(const evist_advanced_t *a, uint8_t cell) {
    uint8_t level = evist_advanced_assist_cell_level(a, cell);
    if (level == 0u) return 0u;
    evist_assist_column_t column = (cell & 1u) ? EVIST_ASSIST_COLUMN_SPEED : EVIST_ASSIST_COLUMN_POWER;
    return a->assist_block[evist_assist_column_offset(column) + evist_assist_level_slot(a->assist_level_count, level)];
}

/* ---- WHEEL page --------------------------------------------------------- */

static void wheel_edit(evist_advanced_t *a, bool up) {
    bool changed = false;
    if (a->cursor == EVIST_WHEEL_ROW_SPEED) {
        if (a->ride_effective_mode == EVIST_RIDE_OFFROAD)
            changed = clamp_step_u16(&a->wheel_speed_centi_kph, a->ride_committed.legal_x100,
                                     EVIST_WHEEL_SPEED_MAX, EVIST_WHEEL_SPEED_STEP, up);
        else a->status = "LEGAL MODE - MAX LOCKED";
    }
    else if (a->cursor == EVIST_WHEEL_ROW_CIRC)
        changed = clamp_step_u16(&a->wheel_circumference_mm, EVIST_WHEEL_CIRC_MIN, EVIST_WHEEL_CIRC_MAX, EVIST_WHEEL_CIRC_STEP, up);
    else if (a->cursor == EVIST_WHEEL_ROW_DIAMETER) {
        if (up && a->wheel_diameter_index + 1u < EVIST_WHEEL_DIAMETERS) { a->wheel_diameter_index++; changed = true; }
        else if (!up && a->wheel_diameter_index > 0u) { a->wheel_diameter_index--; changed = true; }
    }
    if (changed) a->wheel_dirty = true;
}

static void wheel_save(evist_advanced_t *a) {
    if (!a->wheel_valid) { a->status = "READ FIRST"; return; }
    if (a->ride_effective_mode == EVIST_RIDE_LEGAL) a->wheel_speed_centi_kph = a->ride_committed.legal_x100;
    const wheel_diameter_t *d = &diameters[a->wheel_diameter_index];
    if (a->wheel_diameter_index != 0u &&
        (a->wheel_circumference_mm < d->circ_min || a->wheel_circumference_mm > d->circ_max)) {
        a->status = "CIRC OUT OF RANGE FOR DIAMETER";
        return;
    }
    if (a->wheel_speed_centi_kph < (a->ride_effective_mode == EVIST_RIDE_OFFROAD ?
                                     a->ride_committed.legal_x100 : EVIST_WHEEL_SPEED_MIN) ||
        a->wheel_speed_centi_kph > EVIST_WHEEL_SPEED_MAX ||
        a->wheel_circumference_mm < EVIST_WHEEL_CIRC_MIN || a->wheel_circumference_mm > EVIST_WHEEL_CIRC_MAX) {
        a->status = "RANGE 10-70 KM/H, CIRC 500-3000";
        return;
    }
    request(a, EVIST_ADV_REQ_WRITE_WHEEL, "WRITING");
}

static void wheel_key(evist_advanced_t *a, evist_adv_key_t k) {
    if (a->edit) {
        if (k == EVIST_ADV_KEY_POWER) {
            a->edit = false;
            /* POWER exits edit on the same row, like the other ADV SET pages. */
        } else {
            wheel_edit(a, k == EVIST_ADV_KEY_UP);
        }
        return;
    }
    if (k == EVIST_ADV_KEY_UP) { a->cursor = wrap_up(a->cursor, EVIST_WHEEL_ROWS); return; }
    if (k == EVIST_ADV_KEY_DOWN) { a->cursor = wrap_down(a->cursor, EVIST_WHEEL_ROWS); return; }
    if (a->cursor == EVIST_WHEEL_ROW_BACK) { back_to_menu(a); return; }
    if (a->busy || a->request != EVIST_ADV_REQ_NONE) return;
    if (a->cursor == EVIST_WHEEL_ROW_SAVE) { wheel_save(a); return; }
    if (!a->wheel_valid) { a->status = "READ FIRST"; return; }
    if (a->cursor == EVIST_WHEEL_ROW_SPEED && a->ride_effective_mode == EVIST_RIDE_LEGAL) {
        a->status = "LEGAL MODE - MAX LOCKED"; return;
    }
    a->edit = true;
}

void evist_advanced_wheel_target(const evist_advanced_t *a, evist_wheel_config_t *out) {
    if (!a || !out) return;
    out->speed_limit_centi_kph = a->ride_effective_mode == EVIST_RIDE_LEGAL
        ? a->ride_committed.legal_x100 : a->wheel_speed_centi_kph;
    out->circumference_mm = a->wheel_circumference_mm;
    out->wheel_code = a->wheel_diameter_index != 0u ? diameters[a->wheel_diameter_index].code
                                                    : a->wheel_read.wheel_code;
}

static void wheel_load(evist_advanced_t *a, const uint8_t data[64]) {
    evist_wheel_config_decode(data, &a->wheel_read);
    a->wheel_speed_centi_kph = a->wheel_read.speed_limit_centi_kph;
    a->wheel_circumference_mm = a->wheel_read.circumference_mm;
    a->wheel_diameter_index = 0u;
    a->wheel_dirty = false;
    a->wheel_valid = true;
}

/* ---- ASSIST page -------------------------------------------------------- */

static void assist_save(evist_advanced_t *a) {
    if (!a->assist_valid) { a->status = "READ FIRST"; return; }
    if (!a->assist_dirty) { a->status = "NO CHANGES"; return; }
    uint8_t candidate[64];
    copy64(candidate, a->assist_block);
    if (!evist_assist_prepare_write(candidate, a->committed.firmware_profile, a->assist_level_count)) {
        a->status = "LEVELS MUST NOT DECREASE";
        return;
    }
    copy64(a->assist_block, candidate);
    request(a, EVIST_ADV_REQ_WRITE_ASSIST, "WRITING");
}

static void assist_key(evist_advanced_t *a, evist_adv_key_t k) {
    if (a->active_request == EVIST_ADV_REQ_WRITE_ASSIST ||
        a->request == EVIST_ADV_REQ_WRITE_ASSIST) return;
    uint8_t cells = evist_advanced_assist_cells(a);
    uint8_t rows = (uint8_t)(cells + 2u), save = cells, back = (uint8_t)(cells + 1u);
    if (a->edit) {
        if (k == EVIST_ADV_KEY_POWER) {
            a->edit = false;
            if ((uint8_t)(a->cursor + 1u) < save) a->cursor++; /* next value cell */
            return;
        }
        uint8_t level = evist_advanced_assist_cell_level(a, a->cursor);
        if (level == 0u) return;
        evist_assist_column_t column = (a->cursor & 1u) ? EVIST_ASSIST_COLUMN_SPEED : EVIST_ASSIST_COLUMN_POWER;
        if (evist_assist_edit_step(a->assist_block, a->committed.firmware_profile, a->assist_level_count,
                                   column, level, k == EVIST_ADV_KEY_UP))
            a->assist_dirty = true;
        return;
    }
    if (k == EVIST_ADV_KEY_UP) { a->cursor = wrap_up(a->cursor, rows); return; }
    if (k == EVIST_ADV_KEY_DOWN) { a->cursor = wrap_down(a->cursor, rows); return; }
    if (a->cursor == back) { back_to_menu(a); return; }
    if (a->cursor == save) {
        if (a->busy && a->assist_valid) {
            assist_save(a);
            if (a->request == EVIST_ADV_REQ_WRITE_ASSIST) a->status = "WAITING TO WRITE";
            return;
        }
        assist_save(a); return;
    }
    if (!a->assist_valid) { a->status = "READ FIRST"; return; }
    if (!a->busy && a->request == EVIST_ADV_REQ_NONE) a->status = "";
    a->edit = true;
}

/* ---- ACCEL page --------------------------------------------------------- */

static void accel_save(evist_advanced_t *a) {
#if !EVIST_ENABLE_CONFIG_WRITES
    a->status = "WRITES DISABLED IN THIS BUILD";
    return;
#endif
    if (!a->accel_valid) { a->status = "READ FIRST"; return; }
    if (!a->accel_dirty) { a->status = "NO CHANGES"; return; }
    if (a->accel_mode == EVIST_ACCEL_GLOBAL) a->accel_block[54] = a->accel_global;
    else for (uint8_t i = 0u; i < 9u; ++i) a->accel_block[i + 1u] = a->accel_values[i];
    a->accel_block[63] = evist_assist_checksum(a->accel_block);
    request(a, a->accel_mode == EVIST_ACCEL_PER_LEVEL ?
            EVIST_ADV_REQ_WRITE_ACCEL_P0 : EVIST_ADV_REQ_WRITE_ACCEL_P2,
            a->busy ? "WAITING TO WRITE" : "WRITING");
}

static void accel_key(evist_advanced_t *a, evist_adv_key_t k) {
    if (a->active_request == EVIST_ADV_REQ_WRITE_ACCEL_P0 ||
        a->active_request == EVIST_ADV_REQ_WRITE_ACCEL_P2 ||
        a->request == EVIST_ADV_REQ_WRITE_ACCEL_P0 ||
        a->request == EVIST_ADV_REQ_WRITE_ACCEL_P2) return;
    uint8_t rows = (uint8_t)evist_advanced_row_count(a);
    uint8_t save = (uint8_t)(rows - 2u), back = (uint8_t)(rows - 1u);
    if (a->edit) {
        if (k == EVIST_ADV_KEY_POWER) { a->edit = false; return; }
        uint8_t *v = a->accel_mode == EVIST_ACCEL_GLOBAL ? &a->accel_global :
            &a->accel_values[evist_assist_level_slot(a->assist_level_count, (uint8_t)(a->cursor + 1u))];
        uint8_t old = *v;
        cycle_u8(v, 1u, 8u, k == EVIST_ADV_KEY_UP, a->repeat_active);
        if (*v != old) a->accel_dirty = true;
        return;
    }
    if (k == EVIST_ADV_KEY_UP) { a->cursor = wrap_up(a->cursor, rows); return; }
    if (k == EVIST_ADV_KEY_DOWN) { a->cursor = wrap_down(a->cursor, rows); return; }
    if (a->cursor == back) { back_to_menu(a); return; }
    if (a->cursor == save) { accel_save(a); return; }
    if (!a->accel_valid) { a->status = "READ FIRST"; return; }
    if (!a->busy && a->request == EVIST_ADV_REQ_NONE) a->status = "";
    a->edit = true;
}

/* ---- preference pages --------------------------------------------------- */

static void preference_edit(evist_advanced_t *a, bool up) {
    evist_preferences_t *w = &a->working;
    switch (a->screen) {
    case EVIST_ADV_GRAPH:
        cycle_u16(&w->graph_scale_w, 250u, 3000u, 25u, up, a->repeat_active);
        break;
    case EVIST_ADV_FIRMWARE:
        w->firmware_profile = w->firmware_profile == EVIST_PROFILE_BAFANG ? EVIST_PROFILE_FAKE_TAXI : EVIST_PROFILE_BAFANG;
        break;
    case EVIST_ADV_BATTERY: {
        static const uint8_t bits[4] = {EVIST_BATTERY_ICON, EVIST_BATTERY_SOC, EVIST_BATTERY_VOLT, EVIST_BATTERY_BAR};
        w->battery_mask ^= bits[a->cursor < 4u ? a->cursor : 3u];
        break;
    }
    case EVIST_ADV_LIGHT:
        if (a->cursor == 0u) w->light_mode = w->light_mode == EVIST_PREF_LIGHT_AUTO ? EVIST_PREF_LIGHT_MANUAL : EVIST_PREF_LIGHT_AUTO;
        else if (a->cursor == 1u) cycle_u8(&w->light_gain, 1u, 9u, up, a->repeat_active);
        else if (a->cursor == 2u) cycle_u8(&w->light_dwell, 1u, 30u, up, a->repeat_active);
        else if (a->cursor == 3u) cycle_u16(&w->light_threshold, 1800u, 5000u, 100u, up, a->repeat_active);
        else if (a->cursor == 4u) cycle_u8(&w->light_level, 0u, 15u, up, a->repeat_active);
        break;
    case EVIST_ADV_PANELS: {
        uint16_t next = (uint16_t)(w->panel_mask ^ (uint16_t)(1u << a->cursor));
        if ((next & EVIST_PANEL_MASK_ALL) != 0u) w->panel_mask = next;
        else a->status = "AT LEAST ONE PANEL";
        break;
    }
    case EVIST_ADV_TEMP_VIEW: {
        if(a->cursor==0u){
            uint8_t view=(uint8_t)w->temp_view;
            cycle_u8(&view,EVIST_TEMP_BOTH,EVIST_TEMP_2,up,a->repeat_active);
            w->temp_view=(evist_temp_view_t)view;
        }else{
            uint8_t *limit=a->cursor==1u?&w->motor_temp_limit_c:&w->ctrl_temp_limit_c;
            if(up){if(*limit==0u)*limit=40u;else if(*limit<150u)++*limit;}
            else if(*limit==40u)*limit=0u;else if(*limit>40u)--*limit;
        }
        break;
    }
    case EVIST_ADV_QUICK_WALK:
        if (a->cursor == 0u) w->quick_walk_enabled = !w->quick_walk_enabled;
        else if (a->cursor == 1u) cycle_u8(&w->quick_walk_hold_steps, 1u, 10u, up, a->repeat_active);
        break;
    case EVIST_ADV_MODES:
        if (a->cursor < EVIST_ASSIST_MODE_COUNT)
            w->assist_mode_mask ^= (uint8_t)(1u << a->cursor);
        break;
    case EVIST_ADV_MODE:
        if (a->cursor == 0u) a->ride_working.mode = up ? EVIST_RIDE_OFFROAD : EVIST_RIDE_LEGAL;
        else if (a->cursor == 1u) {
            uint8_t start = (uint8_t)a->ride_working.start;
            cycle_u8(&start, EVIST_RIDE_START_LAST, EVIST_RIDE_START_OFFROAD, up, a->repeat_active);
            a->ride_working.start = (evist_ride_start_t)start;
        } else if (a->cursor == 2u)
            (void)clamp_step_u16(&a->ride_working.legal_x100,EVIST_RIDE_LEGAL_MIN,
                                  a->ride_working.offroad_x100 < EVIST_RIDE_LEGAL_MAX ?
                                  a->ride_working.offroad_x100 : EVIST_RIDE_LEGAL_MAX,
                                  EVIST_RIDE_LEGAL_STEP,up);
        else if (a->cursor == 3u)
            (void)clamp_step_u16(&a->ride_working.offroad_x100,a->ride_working.legal_x100,
                                  EVIST_RIDE_OFFROAD_MAX,EVIST_RIDE_OFFROAD_STEP,up);
        break;
    default:
        break;
    }
}

static void preference_key(evist_advanced_t *a, evist_adv_key_t k) {
    size_t rows = evist_advanced_row_count(a);
    uint8_t save = (uint8_t)(rows - 2u), back = (uint8_t)(rows - 1u);
    if (a->screen == EVIST_ADV_LIGHT) a->light_runtime_dirty = true;
    if (a->edit) {
        if (k == EVIST_ADV_KEY_POWER) a->edit = false;
        else preference_edit(a, k == EVIST_ADV_KEY_UP);
        return;
    }
    if (k == EVIST_ADV_KEY_UP) { a->cursor = wrap_up(a->cursor, rows); return; }
    if (k == EVIST_ADV_KEY_DOWN) { a->cursor = wrap_down(a->cursor, rows); return; }
    if (a->cursor == save) {
        if (a->screen == EVIST_ADV_MODE) {
            if (!evist_ride_cfg_valid(&a->ride_working)) { a->status="INVALID"; return; }
            if (a->ride_working.mode==a->ride_committed.mode &&
                a->ride_working.offroad_x100==a->ride_committed.offroad_x100 &&
                a->ride_working.legal_x100==a->ride_committed.legal_x100 &&
                a->ride_working.start==a->ride_committed.start) {
                a->status="NO CHANGES"; return;
            }
            if (a->ride_working.mode != a->ride_committed.mode)
                a->ride_effective_mode=a->ride_working.mode;
            if (a->ride_working.start == EVIST_RIDE_START_LAST)
                a->ride_working.last_mode = a->ride_effective_mode;
            a->ride_committed=a->ride_working;a->ride_changed=true;a->status="SAVING";
        } else commit_preferences(a);
        return;
    }
    if (a->cursor == back) { back_to_menu(a); return; }
    a->edit = true;
}

void evist_advanced_key(evist_advanced_t *a, evist_adv_key_t k) {
    if (!a || k == EVIST_ADV_KEY_NONE) return;
    switch (a->screen) {
    case EVIST_ADV_MENU:
        if (k == EVIST_ADV_KEY_UP) a->cursor = wrap_up(a->cursor, MENU_ITEMS);
        else if (k == EVIST_ADV_KEY_DOWN) a->cursor = wrap_down(a->cursor, MENU_ITEMS);
        else if (a->cursor >= MENU_EXIT) a->exit_requested = true;
        else {
            a->menu_cursor = a->cursor; /* remembered so the page can return to this row */
            evist_advanced_open_page(a, menu_targets[a->cursor]);
        }
        return;
    case EVIST_ADV_ACCEL:
        accel_key(a, k);
        return;
    case EVIST_ADV_DIAG:
        if (k == EVIST_ADV_KEY_UP) a->diag_page = wrap_up(a->diag_page, EVIST_DIAG_PAGES);
        else if (k == EVIST_ADV_KEY_DOWN) a->diag_page = wrap_down(a->diag_page, EVIST_DIAG_PAGES);
        else enter(a, EVIST_ADV_MENU);
        return;
    case EVIST_ADV_WHEEL:
        wheel_key(a, k);
        return;
    case EVIST_ADV_ASSIST:
        assist_key(a, k);
        return;
    default:
        preference_key(a, k);
        return;
    }
}

/* ---- main loop interface ------------------------------------------------ */

bool evist_advanced_take_request(evist_advanced_t *a, evist_adv_request_t *out) {
    if (!a || a->request == EVIST_ADV_REQ_NONE || a->busy) return false;
    if (out) *out = a->request;
    a->active_request = a->request;
    if (a->active_request == EVIST_ADV_REQ_WRITE_ASSIST ||
        a->active_request == EVIST_ADV_REQ_WRITE_ACCEL_P0 ||
        a->active_request == EVIST_ADV_REQ_WRITE_ACCEL_P2) a->status = "WRITING";
    a->request = EVIST_ADV_REQ_NONE;
    a->busy = true;
    return true;
}

void evist_advanced_request_failed(evist_advanced_t *a, const char *status) {
    if (!a) return;
    a->busy = false;
    a->active_request = EVIST_ADV_REQ_NONE;
    a->status = status ? status : "";
}

void evist_advanced_on_result(evist_advanced_t *a, evist_txn_kind_t kind,
                              evist_txn_result_t result, const uint8_t data[64]) {
    if (!a) return;
    a->busy = false;
    a->active_request = EVIST_ADV_REQ_NONE;
    bool ok = result == EVIST_TXN_OK && data;
    switch (kind) {
    case EVIST_TXN_READ_WHEEL:
        if (a->screen != EVIST_ADV_WHEEL) return;
        if (ok) { wheel_load(a, data); a->status = ""; }
        else a->status = "NO 3203 READBACK";
        return;
    case EVIST_TXN_WRITE_WHEEL:
        if (a->screen != EVIST_ADV_WHEEL) return;
        if (ok) {
            wheel_load(a, data);
            if (a->ride_effective_mode==EVIST_RIDE_OFFROAD &&
                a->ride_committed.offroad_x100!=a->wheel_speed_centi_kph) {
                a->ride_committed.offroad_x100=a->wheel_speed_centi_kph;
                a->ride_working=a->ride_committed;a->ride_changed=true;
            }
        } /* edits are kept on failure: retry with SAVE */
        a->status = evist_txn_result_text(result);
        return;
    case EVIST_TXN_READ_ASSIST:
        if (a->screen != EVIST_ADV_ASSIST) return;
        if (a->assist_dirty || a->edit || a->request == EVIST_ADV_REQ_WRITE_ASSIST) {
            if (a->request == EVIST_ADV_REQ_NONE) a->status = "";
            return;
        }
        if (ok && evist_assist_block_valid(data)) {
            copy64(a->assist_block, data);
            copy64(a->assist_committed, data);
            a->assist_valid = true;
            a->assist_dirty = false;
            a->status = "";
        } else {
            a->status = ok ? "6011 BLOCK INVALID" : evist_txn_result_text(result);
        }
        return;
    case EVIST_TXN_WRITE_ASSIST:
        if (a->screen != EVIST_ADV_ASSIST) return;
        if (ok) { copy64(a->assist_block, data); copy64(a->assist_committed, data); a->assist_dirty = false; }
        a->status = evist_txn_result_text(result);
        return;
    case EVIST_TXN_DISCOVER_ACCEL:
        if (result != EVIST_TXN_OK && result != EVIST_TXN_NO_READBACK) {
            if (a->screen == EVIST_ADV_ACCEL) a->status = evist_txn_result_text(result);
            return;
        }
        a->accel_mode = result == EVIST_TXN_OK ? EVIST_ACCEL_PER_LEVEL : EVIST_ACCEL_GLOBAL;
        if (a->screen == EVIST_ADV_ACCEL && a->accel_valid) { a->status = ""; return; }
        if (a->screen == EVIST_ADV_ACCEL && !a->accel_valid && !a->accel_dirty)
            request(a, a->accel_mode == EVIST_ACCEL_PER_LEVEL ?
                    EVIST_ADV_REQ_READ_ACCEL_P0 : EVIST_ADV_REQ_READ_ACCEL_P2, "READING");
        return;
    case EVIST_TXN_READ_ACCEL_P0:
    case EVIST_TXN_READ_ACCEL_P2:
        if (a->screen != EVIST_ADV_ACCEL) return;
        if (a->accel_dirty || a->edit || a->request == EVIST_ADV_REQ_WRITE_ACCEL_P0 ||
            a->request == EVIST_ADV_REQ_WRITE_ACCEL_P2) {
            if (a->request == EVIST_ADV_REQ_NONE) a->status = "";
            return;
        }
        if (!ok) {
            if (result == EVIST_TXN_CHECKSUM_BAD) a->status = "CHECKSUM BAD";
            else if (result == EVIST_TXN_NACK)
                a->status = kind == EVIST_TXN_READ_ACCEL_P0 ? "P0 NACK" : "P2 NACK";
            else a->status = kind == EVIST_TXN_READ_ACCEL_P0 ? "P0 READ FAILED" : "P2 NO RESPONSE";
            return;
        }
        copy64(a->accel_block, data);
        accel_values_from_block(a);
        if (kind == EVIST_TXN_READ_ACCEL_P0) {
            uint8_t count = evist_assist_visible_levels(a->assist_level_count);
            for (uint8_t level = 1u; level <= count; ++level) {
                uint8_t v = a->accel_values[evist_assist_level_slot(a->assist_level_count, level)];
                if (v < 1u || v > 8u) { a->status = "P0 VALUE INVALID"; return; }
            }
        } else {
            a->accel_global = data[54];
            if (a->accel_global < 1u || a->accel_global > 8u) {
                a->status = "P2 VALUE INVALID"; return;
            }
        }
        copy64(a->accel_committed, data);
        a->accel_valid = true;
        a->status = "";
        return;
    case EVIST_TXN_WRITE_APP_P0:
    case EVIST_TXN_WRITE_APP_P2:
        if (a->screen != EVIST_ADV_ACCEL) return;
        if (ok) {
            copy64(a->accel_block, data);
            copy64(a->accel_committed, data);
            accel_values_from_block(a);
            a->accel_dirty = false;
        }
        a->status = evist_txn_result_text(result);
        return;
    default:
        return;
    }
}

bool evist_advanced_take_preferences(evist_advanced_t *a, evist_preferences_t *out) {
    if (!a || !a->preferences_changed) return false;
    if (out) *out = a->committed;
    a->preferences_changed = false;
    return true;
}

bool evist_advanced_take_light_runtime(evist_advanced_t *a, evist_preferences_t *out) {
    if (!a || !a->light_runtime_dirty) return false;
    if (out) *out = a->working;
    a->light_runtime_dirty = false;
    return true;
}

bool evist_advanced_take_exit(evist_advanced_t *a) {
    if (!a) return false;
    bool v = a->exit_requested;
    a->exit_requested = false;
    return v;
}

void evist_advanced_set_status(evist_advanced_t *a, const char *s) {
    if (a) a->status = s ? s : "";
}

void evist_advanced_set_ride(evist_advanced_t *a,const evist_ride_cfg_t *cfg) {
    if (!a || !evist_ride_cfg_valid(cfg)) return;
    a->ride_committed=*cfg;a->ride_working=*cfg;a->ride_changed=false;
    a->ride_effective_mode=cfg->mode;
}

bool evist_advanced_take_ride(evist_advanced_t *a,evist_ride_cfg_t *out) {
    if (!a || !a->ride_changed) return false;
    if (out) *out=a->ride_committed;
    a->ride_changed=false;
    return true;
}
