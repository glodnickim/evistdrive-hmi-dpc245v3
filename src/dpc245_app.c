/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_app.h"
#include <stddef.h>

static const char *const root_menu[] = {
    "DISPLAY", "BIKE", "INFORMATION", "EXIT"
};
static const char *const display_menu[] = {
    "UNIT", "BACKLIGHT", "BATTERY VIEW", "LOWER FIELDS", "TEMP VIEW", "POWER GRAPH",
    "THEMES", "SERVICE TIPS", "AUTO OFF", "BACK"
};
static const char *const bike_menu[] = {
    "ASSIST LEVELS", "ACCELERATION", "ASSIST PROFILE", "WHEEL", "RIDE MODE", "QUICK WALK",
    "ASSIST MODES", "BACK"
};
static const char *const info_menu[] = {
    "HMI", "BATTERY", "CONTROLLER", "SENSOR", "ERRORS", "DIAGNOSTICS", "TRIP RESET", "BACK"
};
static const char *const theme_menu[] = {
    "STANDARD", "YELLOW", "ARC", "WEDGE", "SAVE", "BACK"
};

static uint8_t wrap_up(uint8_t v, size_t n) {
    if (n == 0u) return 0u;
    return v == 0u ? (uint8_t)(n - 1u) : (uint8_t)(v - 1u);
}
static uint8_t wrap_down(uint8_t v, size_t n) {
    if (n == 0u) return 0u;
    return (uint8_t)(((size_t)v + 1u) % n);
}
static void enter(dpc245_app_t *c, dpc245_app_screen_t s) {
    c->screen = s;
    c->selection = 0u;
    c->detail_page = 0u;
}
/* Returning to a list restores the row the user left, so browsing continues there. */
static void return_to(dpc245_app_t *c, dpc245_app_screen_t s) {
    c->screen = s;
    c->selection = c->list_selection;
    c->detail_page = 0u;
}
static void dirty(dpc245_app_t *c) {
    dpc245_settings_finalize(&c->runtime.settings);
    c->settings_dirty = true;
}
static void leave_themes(dpc245_app_t *c) {
    if (!c->theme_save_pending) {
        if (c->theme_save_failed || dpc245_theme(&c->runtime.settings) != c->stored_theme) {
            dpc245_set_theme(&c->runtime.settings, c->stored_theme);
            dpc245_settings_finalize(&c->runtime.settings);
        }
        c->theme_save_failed = false;
    }
    c->pending_theme = c->stored_theme;
    c->theme_status = 0;
}
static void open_advanced(dpc245_app_t *c, evist_adv_screen_t page) {
    c->advanced_parent = c->screen;
    c->advanced_parent_selection = c->selection;
    evist_advanced_open(&c->advanced, &c->preferences, c->assist_level_count);
    evist_advanced_set_ride(&c->advanced, &c->ride_cfg);
    c->advanced.ride_effective_mode=c->ride_session==DPC245_RIDE_SESSION_OFFROAD?
        EVIST_RIDE_OFFROAD:c->ride_session==DPC245_RIDE_SESSION_LEGAL?
        EVIST_RIDE_LEGAL:c->ride_cfg.mode;
    evist_advanced_open_page(&c->advanced, page);
    enter(c, DPC245_APP_SCREEN_ADVANCED);
}

static void return_from_advanced(dpc245_app_t *c) {
    c->screen = c->advanced_parent;
    c->selection = c->advanced_parent_selection;
    c->detail_page = 0u;
}

static uint8_t next_enabled_panel(const dpc245_app_t *c,uint8_t current){
    uint16_t mask=(uint16_t)(c->preferences.panel_mask&EVIST_PANEL_MASK_ALL);
    if(mask==0u)mask=EVIST_PANEL_MASK_ALL;
    for(uint8_t n=0u;n<EVIST_PANEL_COUNT;++n){
        current=(uint8_t)((current+1u)%EVIST_PANEL_COUNT);
        if((mask&(uint16_t)(1u<<current))!=0u)return current;
    }
    return 0u;
}

static bool assist_enabled(const dpc245_app_t *c,uint8_t level){
    return level>=1u&&level<=EVIST_ASSIST_MODE_COUNT&&level<=c->assist_level_count&&
        (c->preferences.assist_mode_mask&(uint8_t)(1u<<(level-1u)))!=0u;
}

static uint8_t cycle_enabled_assist(const dpc245_app_t *c, bool up){
    uint8_t limit=c->assist_level_count<EVIST_ASSIST_MODE_COUNT?
        c->assist_level_count:EVIST_ASSIST_MODE_COUNT;
    if(limit==0u)return 0u;
    uint8_t level=c->assist_level;
    if(!up&&level==0u)return 0u;
    for(uint8_t n=0u;n<limit;++n){
        level=up?(uint8_t)(level>=limit?1u:level+1u):
            (uint8_t)(level<=1u?0u:level-1u);
        if(level==0u)return 0u;
        if(assist_enabled(c,level))return level;
    }
    return up?0u:level;
}

static void reconcile_selected_assist(dpc245_app_t *c){
    if(c->assist_level>c->assist_level_count||
       (c->assist_level!=0u&&!assist_enabled(c,c->assist_level))){
        c->assist_level=0u;
        c->walk_armed=c->walk_active=c->walk=false;
        c->walk_quick=false;
        c->walk_hold_latched=false;
        c->walk_deadline_ms=0u;
    }
    c->preferences.assist_level=c->assist_level;
}

static bool value_edit_active(const dpc245_app_t *c) {
    if (c->screen == DPC245_APP_SCREEN_ADVANCED) return c->advanced.edit;
    return c->screen == DPC245_APP_SCREEN_EDIT_UNIT ||
        c->screen == DPC245_APP_SCREEN_EDIT_SERVICE_TIPS ||
        c->screen == DPC245_APP_SCREEN_EDIT_AUTO_OFF ||
        c->screen == DPC245_APP_SCREEN_EDIT_BRIGHTNESS ||
        c->screen == DPC245_APP_SCREEN_EDIT_AL_SENS;
}

static bool value_edit_repeatable(const dpc245_app_t *c) {
    if (c->screen != DPC245_APP_SCREEN_ADVANCED) {
        return c->screen == DPC245_APP_SCREEN_EDIT_AUTO_OFF ||
            c->screen == DPC245_APP_SCREEN_EDIT_BRIGHTNESS ||
            c->screen == DPC245_APP_SCREEN_EDIT_AL_SENS;
    }
    switch (c->advanced.screen) {
    case EVIST_ADV_WHEEL:
    case EVIST_ADV_ASSIST:
    case EVIST_ADV_ACCEL:
    case EVIST_ADV_GRAPH:
        return true;
    case EVIST_ADV_FIRMWARE:
    case EVIST_ADV_PANELS:
    case EVIST_ADV_BATTERY:
    case EVIST_ADV_MODES:
        return false;
    case EVIST_ADV_LIGHT:
        return c->advanced.cursor != 0u;
    case EVIST_ADV_QUICK_WALK:
        return c->advanced.cursor != 0u;
    case EVIST_ADV_MODE:
        return c->advanced.cursor != 0u;
    case EVIST_ADV_TEMP_VIEW:
        return true;
    default:
        return false;
    }
}

void dpc245_app_init(dpc245_app_t *c,
                             const dpc245_settings_t *settings) {
    if (!c) return;
    dpc245_runtime_init(&c->runtime, settings);
    c->screen = DPC245_APP_SCREEN_DASHBOARD;
    evist_preferences_defaults(&c->preferences);
    c->dashboard_field = DPC245_DASH_FIELD_TRIP;
    c->selection = 0u;
    c->detail_page = 0u;
    c->assist_level_count = 5u; /* Default UI profile; controller negotiation remains separate. */
    c->advanced.accel_mode = EVIST_ACCEL_UNKNOWN;
    c->advanced.accel_valid = false;
    evist_advanced_open(&c->advanced,&c->preferences,c->assist_level_count);
    c->assist_level = 0u;
    c->walk = false;
    c->up_repeat_seen = false;
    c->down_repeat_seen = false;
    c->walk_armed = false;
    c->walk_active = false;
    c->walk_quick = false;
    c->walk_hold_latched = false;
    c->suppress_down_short_once = false;
    c->walk_deadline_ms = 0u;
    c->light_toggle_requested = false;
    c->settings_dirty = false;
    c->preferences_dirty = false;
    c->trip_reset_done = false;
    c->key_events = 0u;
    c->last_key_event_ms = 0u;
    c->idle_controller_busy = false;
    c->light_als_filtered = 0u;
    c->light_backlight_position = 0u;
    c->ride_indicator = DPC245_RIDE_IND_NONE;
    c->ride_cfg=evist_ride_defaults();
    c->ride_session=DPC245_RIDE_SESSION_NONE;c->ride_confirm_pending=false;c->ride_combo_step=0u;
    c->ride_combo_started_ms=0u;c->ride_confirm_deadline_ms=0u;
    c->ride_apply_requested=false;c->ride_cfg_dirty=false;c->ride_pending_session=DPC245_RIDE_SESSION_NONE;
    c->ride_combo_apply=false;c->ride_toast_active=false;c->ride_toast_deadline_ms=0u;
    c->temp_warning_sources=0u;c->temp_warning_text_until_ms=0u;
    c->temp_screen_seen=false;c->temp_screen_sources=0u;
    c->temp_screen_uptime_s=0u;c->temp_screen_render_ms=0u;
    c->temp_warning_activations=0u;
    c->temp_warning_last=(dpc245_temp_warning_record_t){0};
    c->temp_motor_max_c=c->temp_ctrl_max_c=0;
    c->temp_motor_max_valid=c->temp_ctrl_max_valid=false;
    c->temp_live[0]=(dpc245_temp_live_source_t){0};
    c->temp_live[1]=(dpc245_temp_live_source_t){0};
    for(uint8_t i=0u;i<2u;++i){c->temp_transition_started_ms[i]=0u;c->temp_transition_pending[i]=false;}
    c->temp_sample_received=false;c->temp_sample_rx_ms=0u;
    c->temp_motor_c=c->temp_ctrl_c=0;c->temp_motor_valid=c->temp_ctrl_valid=false;
    c->temp_raw6=c->temp_raw7=0u;
    c->ride_last_applied_x100=0u;c->ride_apply_result=EVIST_RIDE_APPLY_IDLE;
    c->ride_last_error=EVIST_TXN_NO_READBACK;
    c->loop_max_us = 0u;
    c->loop_1s_max_us = 0u;
    c->list_selection = 0u;
    c->pending_theme = dpc245_theme(&c->runtime.settings);
    c->stored_theme = c->pending_theme;
    c->theme_save_target = c->pending_theme;
    c->theme_save_pending = false;
    c->theme_save_failed = false;
    c->theme_status = 0;
    c->advanced_parent = DPC245_APP_SCREEN_ROOT_MENU;
    c->advanced_parent_selection = 0u;
}

void dpc245_app_apply_preferences(dpc245_app_t *c,const evist_preferences_t *prefs) {
    if(!c||!prefs||!evist_preferences_validate(prefs))return;
    c->preferences=*prefs;
    c->assist_level=prefs->assist_level;
    reconcile_selected_assist(c);
    if((c->preferences.panel_mask&(uint16_t)(1u<<c->preferences.lower_panel))==0u)
        c->preferences.lower_panel=next_enabled_panel(c,c->preferences.lower_panel);
    evist_advanced_open(&c->advanced,&c->preferences,c->assist_level_count);
    evist_advanced_set_ride(&c->advanced,&c->ride_cfg);
}

size_t dpc245_app_menu_count(dpc245_app_screen_t screen) {
    switch (screen) {
    case DPC245_APP_SCREEN_ROOT_MENU: return sizeof(root_menu)/sizeof(root_menu[0]);
    case DPC245_APP_SCREEN_DISPLAY_MENU: return sizeof(display_menu)/sizeof(display_menu[0]);
    case DPC245_APP_SCREEN_BIKE_MENU: return sizeof(bike_menu)/sizeof(bike_menu[0]);
    case DPC245_APP_SCREEN_INFO_MENU: return sizeof(info_menu)/sizeof(info_menu[0]);
    case DPC245_APP_SCREEN_THEMES_MENU: return sizeof(theme_menu)/sizeof(theme_menu[0]);
    default: return 0u;
    }
}


size_t dpc245_app_detail_page_count(dpc245_app_screen_t screen) {
    switch (screen) {
    case DPC245_APP_SCREEN_INFO_BATTERY: return 5u;
    case DPC245_APP_SCREEN_INFO_CONTROLLER: return 2u;
    case DPC245_APP_SCREEN_INFO_SENSOR: return 2u;
    default: return 1u;
    }
}

static bool is_paged_info(dpc245_app_screen_t screen) {
    return screen == DPC245_APP_SCREEN_INFO_BATTERY ||
           screen == DPC245_APP_SCREEN_INFO_CONTROLLER ||
           screen == DPC245_APP_SCREEN_INFO_SENSOR;
}

const char *dpc245_app_menu_label(dpc245_app_screen_t screen, size_t index) {
    switch (screen) {
    case DPC245_APP_SCREEN_ROOT_MENU:
        return index < sizeof(root_menu)/sizeof(root_menu[0]) ? root_menu[index] : "";
    case DPC245_APP_SCREEN_DISPLAY_MENU:
        return index < sizeof(display_menu)/sizeof(display_menu[0]) ? display_menu[index] : "";
    case DPC245_APP_SCREEN_BIKE_MENU:
        return index < sizeof(bike_menu)/sizeof(bike_menu[0]) ? bike_menu[index] : "";
    case DPC245_APP_SCREEN_INFO_MENU:
        return index < sizeof(info_menu)/sizeof(info_menu[0]) ? info_menu[index] : "";
    case DPC245_APP_SCREEN_THEMES_MENU:
        return index < sizeof(theme_menu)/sizeof(theme_menu[0]) ? theme_menu[index] : "";
    default: return "";
    }
}

static void edit_u8_wrap(uint8_t *v, uint8_t minv, uint8_t maxv, dpc245_app_key_t key, bool repeat) {
    if (!v) return;
    if (key == DPC245_APP_KEY_UP) { if (*v < maxv) ++*v; else if (!repeat) *v = minv; }
    if (key == DPC245_APP_KEY_DOWN) { if (*v > minv) --*v; else if (!repeat) *v = maxv; }
}

void dpc245_app_key(dpc245_app_t *c, dpc245_app_key_t key) {
    if (!c || key == DPC245_APP_KEY_NONE) return;
    c->key_events++;
    c->trip_reset_done = false;

    if (c->screen == DPC245_APP_SCREEN_ADVANCED) {
        evist_adv_key_t ak = key == DPC245_APP_KEY_UP ? EVIST_ADV_KEY_UP :
                             key == DPC245_APP_KEY_DOWN ? EVIST_ADV_KEY_DOWN :
                             key == DPC245_APP_KEY_POWER ? EVIST_ADV_KEY_POWER : EVIST_ADV_KEY_NONE;
        evist_advanced_key(&c->advanced, ak);
        evist_ride_cfg_t ride;
        if(evist_advanced_take_ride(&c->advanced,&ride)){
            bool mode_changed=ride.mode!=c->ride_cfg.mode;
            evist_ride_mode_t effective=c->ride_session==DPC245_RIDE_SESSION_OFFROAD?
                EVIST_RIDE_OFFROAD:c->ride_session==DPC245_RIDE_SESSION_LEGAL?
                EVIST_RIDE_LEGAL:c->ride_cfg.mode;
            uint16_t old_target=effective==EVIST_RIDE_OFFROAD?
                c->ride_cfg.offroad_x100:c->ride_cfg.legal_x100;
            if(ride.start==EVIST_RIDE_START_LAST)ride.last_mode=effective;
            c->ride_cfg=ride;c->ride_cfg_dirty=true;
            if(mode_changed){
                c->ride_session=DPC245_RIDE_SESSION_NONE;
                c->ride_pending_session=DPC245_RIDE_SESSION_NONE;
                c->ride_apply_requested=true;
            }else{
                uint16_t new_target=effective==EVIST_RIDE_OFFROAD?ride.offroad_x100:ride.legal_x100;
                if(new_target!=old_target){
                    c->ride_pending_session=c->ride_session;
                    c->ride_apply_requested=true;
                }
            }
        }
        evist_preferences_t p;
        if (evist_advanced_take_preferences(&c->advanced,&p)) {
            c->preferences=p;
            reconcile_selected_assist(c);
            if((c->preferences.panel_mask&(uint16_t)(1u<<c->preferences.lower_panel))==0u)
                c->preferences.lower_panel=next_enabled_panel(c,c->preferences.lower_panel);
            c->preferences_dirty=true;
        }
        if (evist_advanced_take_exit(&c->advanced) || c->advanced.screen == EVIST_ADV_MENU)
            return_from_advanced(c);
        return;
    }

    if (c->screen == DPC245_APP_SCREEN_DASHBOARD) {
        if (key == DPC245_APP_KEY_UP || key == DPC245_APP_KEY_DOWN) {
            uint8_t next=cycle_enabled_assist(c,key==DPC245_APP_KEY_UP);
            /* The established UP, DOWN, UP ride-mode chord passes through OFF. */
            if(key==DPC245_APP_KEY_DOWN&&c->ride_combo_step==2u)next=0u;
            if(next!=c->assist_level){
                c->assist_level=next;
                c->preferences.assist_level=next;
                c->preferences_dirty=true;
                c->walk_armed=c->walk_active=c->walk=false;
                c->walk_hold_latched=false;
            }
        } else if (key == DPC245_APP_KEY_POWER) {
            c->preferences.lower_panel=next_enabled_panel(c,c->preferences.lower_panel);
            c->preferences_dirty=true;
        }
        return;
    }

    size_t count = dpc245_app_menu_count(c->screen);
    if (count != 0u) {
        if (key == DPC245_APP_KEY_UP) { c->selection = wrap_up(c->selection, count); return; }
        if (key == DPC245_APP_KEY_DOWN) { c->selection = wrap_down(c->selection, count); return; }
        if (key != DPC245_APP_KEY_POWER) return;
        c->list_selection = c->selection;

        if (c->screen == DPC245_APP_SCREEN_ROOT_MENU) {
            switch (c->selection) {
            case 0: enter(c, DPC245_APP_SCREEN_DISPLAY_MENU); return;
            case 1: enter(c, DPC245_APP_SCREEN_BIKE_MENU); return;
            case 2: enter(c, DPC245_APP_SCREEN_INFO_MENU); return;
            default: enter(c, DPC245_APP_SCREEN_DASHBOARD); return;
            }
        }
        if (c->screen == DPC245_APP_SCREEN_DISPLAY_MENU) {
            switch (c->selection) {
            case 0: enter(c, DPC245_APP_SCREEN_EDIT_UNIT); return;
            case 1: open_advanced(c, EVIST_ADV_LIGHT); return;
            case 2: open_advanced(c, EVIST_ADV_BATTERY); return;
            case 3: open_advanced(c, EVIST_ADV_PANELS); return;
            case 4: open_advanced(c, EVIST_ADV_TEMP_VIEW); return;
            case 5: open_advanced(c, EVIST_ADV_GRAPH); return;
            case 6:
                enter(c, DPC245_APP_SCREEN_THEMES_MENU);
                c->pending_theme=c->stored_theme;
                c->selection=c->pending_theme;
                c->theme_status=0;
                return;
            case 7: enter(c, DPC245_APP_SCREEN_EDIT_SERVICE_TIPS); return;
            case 8: enter(c, DPC245_APP_SCREEN_EDIT_AUTO_OFF); return;
            default: c->screen=DPC245_APP_SCREEN_ROOT_MENU; c->selection=0u; return;
            }
        }
        if (c->screen == DPC245_APP_SCREEN_BIKE_MENU) {
            switch (c->selection) {
            case 0: open_advanced(c, EVIST_ADV_ASSIST); return;
            case 1: open_advanced(c, EVIST_ADV_ACCEL); return;
            case 2: open_advanced(c, EVIST_ADV_FIRMWARE); return;
            case 3: open_advanced(c, EVIST_ADV_WHEEL); return;
            case 4: open_advanced(c, EVIST_ADV_MODE); return;
            case 5: open_advanced(c, EVIST_ADV_QUICK_WALK); return;
            case 6: open_advanced(c, EVIST_ADV_MODES); return;
            default: c->screen=DPC245_APP_SCREEN_ROOT_MENU; c->selection=1u; return;
            }
        }
        if (c->screen == DPC245_APP_SCREEN_INFO_MENU) {
            switch (c->selection) {
            case 0: enter(c, DPC245_APP_SCREEN_INFO_HMI); return;
            case 1: enter(c, DPC245_APP_SCREEN_INFO_BATTERY); return;
            case 2: enter(c, DPC245_APP_SCREEN_INFO_CONTROLLER); return;
            case 3: enter(c, DPC245_APP_SCREEN_INFO_SENSOR); return;
            case 4: enter(c, DPC245_APP_SCREEN_INFO_ERRORS); return;
            case 5: open_advanced(c, EVIST_ADV_DIAG); return;
            case 6: enter(c, DPC245_APP_SCREEN_TRIP_RESET); return;
            default: c->screen=DPC245_APP_SCREEN_ROOT_MENU; c->selection=2u; return;
            }
        }
        if (c->screen == DPC245_APP_SCREEN_THEMES_MENU) {
            if (c->selection < 4u) {
                c->pending_theme=c->selection;
                c->theme_status=0;
            } else if(c->selection==4u){
                if(c->theme_save_pending && c->pending_theme==c->theme_save_target){
                    c->theme_status="SAVE IN PROGRESS";
                }else if(c->pending_theme!=c->stored_theme || c->theme_save_failed){
                    dpc245_set_theme(&c->runtime.settings,c->pending_theme);
                    dirty(c);
                    c->theme_save_target=c->pending_theme;
                    c->theme_save_pending=true;
                    c->theme_save_failed=false;
                    c->theme_status="SAVE IN PROGRESS";
                }else c->theme_status="NO CHANGE";
            } else {
                leave_themes(c);
                c->screen=DPC245_APP_SCREEN_DISPLAY_MENU;
                c->selection=6u;
            }
            return;
        }
    }

    /* Adjustment/detail screens. POWER commits/returns; edits are already held
     * in the canonical record so the target may choose whether to persist. */
    switch (c->screen) {
    case DPC245_APP_SCREEN_EDIT_UNIT: {
        uint8_t v = dpc245_unit(&c->runtime.settings);
        if (key == DPC245_APP_KEY_UP || key == DPC245_APP_KEY_DOWN) {
            v ^= 1u; dpc245_set_unit(&c->runtime.settings, v); dirty(c);
        } else return_to(c, DPC245_APP_SCREEN_DISPLAY_MENU);
        break;
    }
    case DPC245_APP_SCREEN_EDIT_SERVICE_TIPS: {
        uint8_t v = dpc245_service_tips(&c->runtime.settings);
        if (key == DPC245_APP_KEY_UP || key == DPC245_APP_KEY_DOWN) {
            v ^= 1u; dpc245_set_service_tips(&c->runtime.settings, v); dirty(c);
        } else return_to(c, DPC245_APP_SCREEN_DISPLAY_MENU);
        break;
    }
    case DPC245_APP_SCREEN_EDIT_AUTO_OFF: {
        uint8_t v = dpc245_auto_off_min(&c->runtime.settings);
        if (key == DPC245_APP_KEY_UP || key == DPC245_APP_KEY_DOWN) {
            edit_u8_wrap(&v, 0u, 10u, key, c->advanced.repeat_active); dpc245_set_auto_off_min(&c->runtime.settings, v); dirty(c);
        } else return_to(c, DPC245_APP_SCREEN_DISPLAY_MENU);
        break;
    }
    case DPC245_APP_SCREEN_EDIT_BRIGHTNESS: {
        uint8_t v = dpc245_brightness_level(&c->runtime.settings);
        if (v < 1u || v > 4u) v = 4u;
        if (key == DPC245_APP_KEY_UP || key == DPC245_APP_KEY_DOWN) {
            edit_u8_wrap(&v, 1u, 4u, key, c->advanced.repeat_active); dpc245_set_brightness_level(&c->runtime.settings, v); dirty(c);
        } else return_to(c, DPC245_APP_SCREEN_DISPLAY_MENU);
        break;
    }
    case DPC245_APP_SCREEN_EDIT_AL_SENS: {
        uint8_t v = dpc245_al_sensitivity(&c->runtime.settings);
        if (v > 5u) v = 3u;
        if (key == DPC245_APP_KEY_UP || key == DPC245_APP_KEY_DOWN) {
            edit_u8_wrap(&v, 0u, 5u, key, c->advanced.repeat_active); dpc245_set_al_sensitivity(&c->runtime.settings, v); dirty(c);
        } else return_to(c, DPC245_APP_SCREEN_DISPLAY_MENU);
        break;
    }
    case DPC245_APP_SCREEN_TRIP_RESET:
        if (key == DPC245_APP_KEY_POWER) {
            dpc245_runtime_trip_reset(&c->runtime);
            dpc245_settings_finalize(&c->runtime.settings);
            c->settings_dirty = true;
            c->trip_reset_done = true;
            return_to(c, DPC245_APP_SCREEN_INFO_MENU);
        } else if (key == DPC245_APP_KEY_DOWN) return_to(c, DPC245_APP_SCREEN_INFO_MENU);
        break;
    case DPC245_APP_SCREEN_INFO_BATTERY:
    case DPC245_APP_SCREEN_INFO_CONTROLLER:
    case DPC245_APP_SCREEN_INFO_SENSOR:
        if (key == DPC245_APP_KEY_POWER) {
            return_to(c, DPC245_APP_SCREEN_INFO_MENU);
        } else if (is_paged_info(c->screen)) {
            size_t pages = dpc245_app_detail_page_count(c->screen);
            if (key == DPC245_APP_KEY_UP) c->detail_page = wrap_up(c->detail_page, pages);
            else if (key == DPC245_APP_KEY_DOWN) c->detail_page = wrap_down(c->detail_page, pages);
        }
        break;
    case DPC245_APP_SCREEN_INFO_HMI:
    case DPC245_APP_SCREEN_INFO_WHEEL:
    case DPC245_APP_SCREEN_INFO_SPEED_LIMIT:
    case DPC245_APP_SCREEN_INFO_ERRORS:
        if (key == DPC245_APP_KEY_POWER) return_to(c, DPC245_APP_SCREEN_INFO_MENU);
        break;
    default:
        if (key == DPC245_APP_KEY_POWER) enter(c, DPC245_APP_SCREEN_DASHBOARD);
        break;
    }
}


const char *dpc245_app_dashboard_field_label(dpc245_dashboard_field_t field) {
    static const char *const labels[] = {"TRIP","ODO","MAX","AVG","RANGE","CADENCE","CAL","TIME","WATT"};
    return (unsigned)field < (unsigned)DPC245_DASH_FIELD_COUNT ? labels[(unsigned)field] : "?";
}

static void ride_assist_zero(dpc245_app_t *c) {
    if(c->assist_level==0u)return;
    c->assist_level=0u;c->preferences.assist_level=0u;c->preferences_dirty=true;
}

static bool ride_walk_busy(const dpc245_app_t *c) {
    return c->walk || c->walk_active || c->walk_armed;
}

static bool ride_effective_offroad(const dpc245_app_t *c) {
    return c->ride_session==DPC245_RIDE_SESSION_OFFROAD ||
        (c->ride_session==DPC245_RIDE_SESSION_NONE && c->ride_cfg.mode==EVIST_RIDE_OFFROAD);
}

static void ride_combo_cancel(dpc245_app_t *c) {
    c->ride_combo_step=0u;
    if(c->ride_confirm_pending){
        c->ride_confirm_pending=false;
        if(!ride_walk_busy(c))ride_assist_zero(c);
    }
}

static bool ride_short(dpc245_app_t *c,dpc245_key_event_t event,uint32_t now_ms) {
    if(c->screen!=DPC245_APP_SCREEN_DASHBOARD)return false;
    if(c->ride_confirm_pending && (int32_t)(now_ms-c->ride_confirm_deadline_ms)>=0)
        ride_combo_cancel(c);
    if(c->ride_confirm_pending){
        if(event==DPC245_KEY_EVENT_UP_SHORT || event==DPC245_KEY_EVENT_DOWN_SHORT){
            ride_combo_cancel(c);return false; /* cancellation key keeps its normal action */
        }
        if(event==DPC245_KEY_EVENT_DOWN_PRESS){
            ride_combo_cancel(c);return false;
        }
        if(event==DPC245_KEY_EVENT_POWER_SHORT){
            c->ride_confirm_pending=false;c->ride_combo_step=0u;
            ride_assist_zero(c);
            if(!ride_walk_busy(c)){
                c->ride_pending_session=ride_effective_offroad(c)?
                    DPC245_RIDE_SESSION_LEGAL:DPC245_RIDE_SESSION_OFFROAD;
                c->ride_apply_requested=true;c->ride_combo_apply=true;
            }
            return true;
        }
        ride_combo_cancel(c);
        return false;
    }
    if(ride_walk_busy(c)){
        c->ride_combo_step=0u;return false;
    }
    if(c->ride_combo_step && (int32_t)(now_ms-c->ride_combo_started_ms)>1500)
        c->ride_combo_step=0u;
    if(event==DPC245_KEY_EVENT_UP_PRESS || event==DPC245_KEY_EVENT_DOWN_PRESS ||
       event==DPC245_KEY_EVENT_DOWN_RELEASE)
        return false; /* short actions are emitted separately on release */
    if(event==DPC245_KEY_EVENT_UP_SHORT){
        if(c->ride_combo_step==0u && c->assist_level==0u){
            c->ride_combo_step=1u;c->ride_combo_started_ms=now_ms;
            return false; /* first UP retains the normal assist transition */
        }
        if(c->ride_combo_step==2u && c->assist_level==0u){
            c->ride_combo_step=0u;c->ride_confirm_pending=true;
            c->ride_confirm_deadline_ms=now_ms+3000u;ride_assist_zero(c);
            return true;
        }
        c->ride_combo_step=0u;
    } else if(event==DPC245_KEY_EVENT_DOWN_SHORT){
        if(c->ride_combo_step==1u && c->assist_level==1u){
            c->ride_combo_step=2u;return false; /* DOWN returns assist to zero */
        }
        c->ride_combo_step=0u;
    } else c->ride_combo_step=0u;
    return false;
}

void dpc245_app_event(dpc245_app_t *c,
                              dpc245_key_event_t event,
                              uint32_t now_ms) {
    if (!c || event == DPC245_KEY_EVENT_NONE) return;
    c->last_key_event_ms = now_ms;
    if (event == DPC245_KEY_EVENT_DOWN_PRESS) {
        c->down_held = true;
        c->down_repeat_seen = false;
        c->suppress_down_short_once = false;
    }
    else if (event == DPC245_KEY_EVENT_DOWN_RELEASE) c->down_held = false;
    if (event == DPC245_KEY_EVENT_UP_PRESS) c->up_repeat_seen = false;
    c->key_events++;
    if (event == DPC245_KEY_EVENT_UP_REPEAT || event == DPC245_KEY_EVENT_DOWN_REPEAT ||
        event == DPC245_KEY_EVENT_UP_REPEAT_FAST || event == DPC245_KEY_EVENT_DOWN_REPEAT_FAST) {
        bool up = event == DPC245_KEY_EVENT_UP_REPEAT || event == DPC245_KEY_EVENT_UP_REPEAT_FAST;
        if (up) c->up_repeat_seen = true; else c->down_repeat_seen = true;
        if(c->screen==DPC245_APP_SCREEN_DASHBOARD)ride_short(c,event,now_ms);
        bool fast = event == DPC245_KEY_EVENT_UP_REPEAT_FAST || event == DPC245_KEY_EVENT_DOWN_REPEAT_FAST;
        bool editing = value_edit_active(c);
        if (!editing || !value_edit_repeatable(c)) return;
        uint8_t steps = 1u;
        if (fast && c->screen == DPC245_APP_SCREEN_ADVANCED &&
            ((c->advanced.screen == EVIST_ADV_WHEEL && c->advanced.cursor < 2u) ||
             c->advanced.screen == EVIST_ADV_GRAPH || c->advanced.screen == EVIST_ADV_ASSIST ||
             (c->advanced.screen == EVIST_ADV_MODE && c->advanced.cursor >= 2u &&
              c->advanced.cursor <= 3u))) steps = 5u;
        c->advanced.repeat_active = true;
        for (uint8_t i = 0u; i < steps; ++i)
            dpc245_app_key(c, up ? DPC245_APP_KEY_UP : DPC245_APP_KEY_DOWN);
        c->advanced.repeat_active = false;
        return;
    }
    if(event==DPC245_KEY_EVENT_DOWN_SHORT && c->suppress_down_short_once){
        c->suppress_down_short_once=false;
        return;
    }
    if(c->ride_confirm_pending && ride_walk_busy(c))ride_combo_cancel(c);
    if(ride_short(c,event,now_ms))return;

    switch (event) {
    case DPC245_KEY_EVENT_SETTINGS_CHORD:
        if (c->screen == DPC245_APP_SCREEN_DASHBOARD) {
            c->walk = c->walk_active = c->walk_armed = false;
            c->walk_quick = false;
            c->walk_hold_latched = false;
            c->walk_deadline_ms = 0u;
            enter(c, DPC245_APP_SCREEN_ROOT_MENU);
        }
        return;
    case DPC245_KEY_EVENT_UP_HOLD:
        if (c->screen == DPC245_APP_SCREEN_DASHBOARD)
            c->light_toggle_requested = true;
        return;
    case DPC245_KEY_EVENT_POWER_DOWN_HOLD:
        /* Reserved combination event.  It is intentionally not treated as a
         * software power-off request. */
        return;
    case DPC245_KEY_EVENT_DOWN_PRESS:
        /* Standard level-0 Walk is armed by the released DOWN action.  A press
         * at a non-zero level is used only by the optional Quick Walk shortcut. */
        if (c->ride_combo_step==1u || c->ride_confirm_pending) return;
        if (c->screen == DPC245_APP_SCREEN_DASHBOARD &&
            c->preferences.quick_walk_enabled && c->assist_level >= 1u) {
            c->walk_armed = true;
            c->walk_active = false;
            c->walk = false;
            c->walk_quick = true;
            c->walk_deadline_ms = now_ms + (uint32_t)c->preferences.quick_walk_hold_steps * 500u;
        }
        return;
    case DPC245_KEY_EVENT_DOWN_HOLD:
        /* A held DOWN action refreshes the five-second standard Walk window.
         * Quick Walk is a physical dead-man and therefore needs no software
         * expiry while the key remains held. */
        if (c->screen == DPC245_APP_SCREEN_DASHBOARD &&
            c->walk_armed && c->walk_active && !c->walk_quick) {
            c->walk = true;
            c->walk_deadline_ms = now_ms + DPC245_WALK_WINDOW_MS;
            c->walk_hold_latched = true;
        }
        return;
    case DPC245_KEY_EVENT_DOWN_RELEASE:
        if(c->walk_quick && c->walk_active)c->suppress_down_short_once=true;
        if (c->walk_quick || (c->walk_active &&
            (c->walk_hold_latched ||
             (int32_t)(now_ms - c->walk_deadline_ms) >= 0))) {
            c->walk = false;
            c->walk_active = false;
            c->walk_armed = false;
            c->walk_quick = false;
            c->walk_hold_latched = false;
            c->walk_deadline_ms = 0u;
        }
        return;
    case DPC245_KEY_EVENT_UP_SHORT:
        if (c->up_repeat_seen && value_edit_active(c)) return;
        c->up_repeat_seen = false;
        dpc245_app_key(c, DPC245_APP_KEY_UP);
        return;
    case DPC245_KEY_EVENT_DOWN_SHORT:
        if (c->down_repeat_seen && value_edit_active(c)) return;
        c->down_repeat_seen = false;
        if (c->screen == DPC245_APP_SCREEN_DASHBOARD && c->assist_level == 0u && c->ride_combo_step!=2u) {
            c->walk = true;
            c->walk_armed = true;
            c->walk_active = true;
            c->walk_quick = false;
            c->walk_hold_latched = false;
            c->walk_deadline_ms = now_ms + DPC245_WALK_WINDOW_MS;
        } else {
            dpc245_app_key(c, DPC245_APP_KEY_DOWN);
        }
        return;
    case DPC245_KEY_EVENT_POWER_SHORT:
        dpc245_app_key(c, DPC245_APP_KEY_POWER);
        return;
    default:
        return;
    }
}

void dpc245_app_down_state(dpc245_app_t *c,bool down_pressed,uint32_t now_ms){
    if(!c || down_pressed)return;
    if(c->down_held || c->walk_quick || c->walk_hold_latched)
        dpc245_app_event(c,DPC245_KEY_EVENT_DOWN_RELEASE,now_ms);
}

void dpc245_app_temperature_sample(dpc245_app_t *c, const bafang_telemetry_t *t) {
    if(!c||!t)return;
    c->temp_sample_received=t->have_controller_temp||t->have_motor_temp;
    c->temp_sample_rx_ms=t->controller_fast1_rx_ms;
    c->temp_motor_c=t->motor_temp_c;
    c->temp_ctrl_c=t->controller_temp_c;
    c->temp_motor_valid=t->have_motor_temp&&bafang_temperature_raw_valid(t->controller_aux_temp_raw);
    c->temp_ctrl_valid=t->have_controller_temp&&bafang_temperature_raw_valid(t->controller_temp_raw);
    c->temp_raw6=t->controller_temp_raw;
    c->temp_raw7=t->controller_aux_temp_raw;
}

void dpc245_app_dashboard_render_complete(dpc245_app_t *c,
                                          const dpc245_app_t *rendered,uint32_t completed_at_ms){
    if(!c||!rendered||rendered->screen!=DPC245_APP_SCREEN_DASHBOARD)return;
    c->temp_screen_seen=true;
    c->temp_screen_sources=rendered->temp_warning_sources;
    c->temp_screen_uptime_s=completed_at_ms/1000u;
    c->temp_screen_render_ms=completed_at_ms;
}

static void temperature_warning_tick(dpc245_app_t *c,uint32_t now_ms) {
    bool fresh=c->temp_sample_received&&(uint32_t)(now_ms-c->temp_sample_rx_ms)<=3000u;
    uint8_t old=c->temp_warning_sources;
    if(fresh&&c->temp_motor_valid&&(!c->temp_motor_max_valid||c->temp_motor_c>c->temp_motor_max_c)){
        c->temp_motor_max_c=c->temp_motor_c;c->temp_motor_max_valid=true;
    }
    if(fresh&&c->temp_ctrl_valid&&(!c->temp_ctrl_max_valid||c->temp_ctrl_c>c->temp_ctrl_max_c)){
        c->temp_ctrl_max_c=c->temp_ctrl_c;c->temp_ctrl_max_valid=true;
    }
    for(uint8_t i=0u;i<2u;++i){
        uint8_t bit=(uint8_t)(1u<<i);
        uint8_t limit=i==0u?c->preferences.motor_temp_limit_c:c->preferences.ctrl_temp_limit_c;
        bool valid=fresh&&(i==0u?c->temp_motor_valid:c->temp_ctrl_valid);
        int16_t value=i==0u?c->temp_motor_c:c->temp_ctrl_c;
        bool active=(c->temp_warning_sources&bit)!=0u;
        bool transition=active?(!valid||limit==0u||value<=(int16_t)limit-5):
                              (valid&&limit!=0u&&value>=limit);
        dpc245_temp_live_source_t *live=&c->temp_live[i];
        live->sampled_c=value;live->limit_c=limit;
        live->sample_received=c->temp_sample_received;
        live->sensor_valid=i==0u?c->temp_motor_valid:c->temp_ctrl_valid;
        live->fresh=fresh;live->decision_valid=valid;live->transition=transition;
        live->sample_age_ms=c->temp_sample_received?(uint32_t)(now_ms-c->temp_sample_rx_ms):UINT32_MAX;
        if(!transition)c->temp_transition_pending[i]=false;
        else if(!c->temp_transition_pending[i]){
            c->temp_transition_pending[i]=true;
            c->temp_transition_started_ms[i]=now_ms;
        }else if((uint32_t)(now_ms-c->temp_transition_started_ms[i])>=2000u){
            c->temp_warning_sources^=bit;
            c->temp_transition_pending[i]=false;
        }
        live->pending=c->temp_transition_pending[i];
        live->pending_ms=live->pending?(uint32_t)(now_ms-c->temp_transition_started_ms[i]):0u;
    }
    if((c->temp_warning_sources&~old)!=0u)
        c->temp_warning_text_until_ms=now_ms+3000u;
    if(old==0u&&c->temp_warning_sources!=0u){
        dpc245_temp_warning_record_t *record=&c->temp_warning_last;
        if(c->temp_warning_activations!=UINT32_MAX)c->temp_warning_activations++;
        record->uptime_s=now_ms/1000u;
        record->sources=c->temp_warning_sources;
        record->motor_c=c->temp_motor_c;record->ctrl_c=c->temp_ctrl_c;
        record->motor_valid=fresh&&c->temp_motor_valid;
        record->ctrl_valid=fresh&&c->temp_ctrl_valid;
        record->sample_age_ms=fresh?(uint32_t)(now_ms-c->temp_sample_rx_ms):UINT32_MAX;
        record->motor_limit_c=c->preferences.motor_temp_limit_c;
        record->ctrl_limit_c=c->preferences.ctrl_temp_limit_c;
        record->raw6=c->temp_raw6;record->raw7=c->temp_raw7;
    }
}

void dpc245_app_tick(dpc245_app_t *c, uint32_t now_ms) {
    if (!c) return;
    temperature_warning_tick(c,now_ms);
    if (c->screen != DPC245_APP_SCREEN_DASHBOARD) {
        bool controller_busy = c->advanced.busy || c->advanced.request != EVIST_ADV_REQ_NONE;
        if (controller_busy) {
            c->idle_controller_busy = true;
            c->last_key_event_ms = now_ms;
        } else if (c->idle_controller_busy) {
            c->idle_controller_busy = false;
            c->last_key_event_ms = now_ms;
        } else if ((int32_t)(now_ms - c->last_key_event_ms) >= 30000) {
            if (c->screen == DPC245_APP_SCREEN_ADVANCED)
                evist_advanced_abandon(&c->advanced);
            if (c->screen == DPC245_APP_SCREEN_THEMES_MENU) leave_themes(c);
            else {
                c->pending_theme=c->stored_theme;
                c->theme_status=0;
            }
            enter(c, DPC245_APP_SCREEN_DASHBOARD);
        }
    }
    if(c->ride_toast_active){
        if(c->ride_toast_deadline_ms==0u)c->ride_toast_deadline_ms=now_ms+2000u;
        else if((int32_t)(now_ms-c->ride_toast_deadline_ms)>=0)c->ride_toast_active=false;
    }
    if(c->ride_confirm_pending && (int32_t)(now_ms-c->ride_confirm_deadline_ms)>=0)
        ride_combo_cancel(c);
    if(c->ride_combo_step && (int32_t)(now_ms-c->ride_combo_started_ms)>1500)
        c->ride_combo_step=0u;
    if (!c->walk_armed || c->walk_deadline_ms == 0u) return;
    if (c->walk_quick && !c->walk_active) {
        if ((int32_t)(now_ms - c->walk_deadline_ms) >= 0) {
            c->walk_active = true;
            c->walk = true;
        }
        return;
    }
    if (!c->walk_quick && c->walk_active && !c->down_held &&
        (int32_t)(now_ms - c->walk_deadline_ms) >= 0) {
        c->walk = false;
        c->walk_active = false;
        c->walk_armed = false;
        c->walk_hold_latched = false;
        c->walk_deadline_ms = 0u;
    }
}

void dpc245_app_power_graph_tick(dpc245_app_t *c, int32_t measured_w, uint32_t now_ms) {
    if (!c) return;
    if (measured_w < 0) measured_w = 0;
    uint32_t dt = c->power_graph_tick_ms ? now_ms - c->power_graph_tick_ms : 0u;
    c->power_graph_tick_ms = now_ms;
    if (dt > 100u) dt = 100u;
    if (dt == 0u) return;
    uint32_t target = (uint32_t)measured_w;
    uint32_t current = c->power_graph_w;
    uint32_t delta = current > target ? current - target : target - current;
    uint32_t step = (delta * dt + 250u) / (250u + dt);
    if (step == 0u && delta) step = 1u;
    c->power_graph_w = current > target ? current - step : current + step;
}

bool dpc245_app_take_light_toggle(dpc245_app_t *c) {
    if (!c) return false;
    bool v = c->light_toggle_requested;
    c->light_toggle_requested = false;
    return v;
}

void dpc245_app_set_ride(dpc245_app_t *c,const evist_ride_cfg_t *cfg) {
    if(!c || !evist_ride_cfg_valid(cfg))return;
    c->ride_cfg=*cfg;
    c->ride_session=evist_ride_boot_mode(cfg)==EVIST_RIDE_OFFROAD?
        DPC245_RIDE_SESSION_OFFROAD:DPC245_RIDE_SESSION_LEGAL;
    c->ride_pending_session=c->ride_session;
    c->ride_indicator=c->ride_session==DPC245_RIDE_SESSION_OFFROAD?
        DPC245_RIDE_IND_OFFROAD:DPC245_RIDE_IND_LEGAL;
    evist_advanced_set_ride(&c->advanced,cfg);
}

bool dpc245_app_take_ride_cfg_dirty(dpc245_app_t *c,evist_ride_cfg_t *out) {
    if(!c || !c->ride_cfg_dirty)return false;
    if(out)*out=c->ride_cfg;c->ride_cfg_dirty=false;return true;
}

bool dpc245_app_take_ride_apply(dpc245_app_t *c,uint16_t *target) {
    if(!c || !c->ride_apply_requested)return false;
    if(target)*target=c->ride_pending_session==DPC245_RIDE_SESSION_OFFROAD?c->ride_cfg.offroad_x100:
        c->ride_pending_session==DPC245_RIDE_SESSION_LEGAL?c->ride_cfg.legal_x100:
        evist_ride_target_x100(&c->ride_cfg);
    c->ride_apply_requested=false;return true;
}

void dpc245_app_ride_applied(dpc245_app_t *c,const evist_ride_apply_t *result) {
    if(!c || !result)return;
    c->ride_apply_result=result->phase;
    c->ride_last_error=result->last_result;
    bool session_switch = c->ride_combo_apply;
    if(c->ride_combo_apply){
        c->ride_toast_active=result->phase==EVIST_RIDE_APPLY_OK;
        c->ride_toast_deadline_ms=0u;
        c->ride_combo_apply=false;
    }
    if(result->phase==EVIST_RIDE_APPLY_OK){
        c->ride_last_applied_x100=result->last_applied_x100;
        c->ride_session=c->ride_pending_session;
        if (c->ride_cfg.start == EVIST_RIDE_START_LAST &&
            (session_switch || c->ride_session == DPC245_RIDE_SESSION_NONE) &&
            c->ride_cfg.last_mode != (ride_effective_offroad(c) ? EVIST_RIDE_OFFROAD : EVIST_RIDE_LEGAL)) {
            c->ride_cfg.last_mode = ride_effective_offroad(c) ? EVIST_RIDE_OFFROAD : EVIST_RIDE_LEGAL;
            c->ride_cfg_dirty = true;
        }
        c->ride_indicator=ride_effective_offroad(c)
            ?DPC245_RIDE_IND_OFFROAD:DPC245_RIDE_IND_LEGAL;
    }
}

void dpc245_app_ride_wheel_verified(dpc245_app_t *c,const uint8_t data[64]) {
    if(!c || !data)return;
    c->ride_last_applied_x100=(uint16_t)(data[0]|((uint16_t)data[1]<<8));
    c->ride_apply_result=EVIST_RIDE_APPLY_OK;
    c->ride_last_error=EVIST_TXN_OK;
    if(c->ride_cfg.mode==EVIST_RIDE_LEGAL && c->ride_session==DPC245_RIDE_SESSION_NONE){
        c->ride_session=DPC245_RIDE_SESSION_NONE;
        c->ride_pending_session=DPC245_RIDE_SESSION_NONE;
        c->ride_indicator=DPC245_RIDE_IND_LEGAL;
    }
}

bool dpc245_app_take_preferences_dirty(dpc245_app_t *c,evist_preferences_t *out) {
    if(!c||!c->preferences_dirty)return false;
    if(out)*out=c->preferences;
    c->preferences_dirty=false;
    return true;
}

bool dpc245_app_take_settings_dirty(dpc245_app_t *c) {
    if (!c) return false;
    bool v = c->settings_dirty;
    c->settings_dirty = false;
    return v;
}

void dpc245_app_theme_store_result(dpc245_app_t *c, bool saved, uint8_t theme) {
    if(!c)return;
    if(saved)c->stored_theme=theme;
    if(c->theme_save_pending && theme==c->theme_save_target){
        c->theme_save_pending=false;
        c->theme_save_failed=!saved;
    }
}

const char *dpc245_app_theme_row_value(const dpc245_app_t *c, uint8_t theme) {
    if(!c || theme>=4u)return 0;
    if(theme==c->pending_theme && theme!=c->stored_theme)return "SELECTED";
    return theme==c->stored_theme?"ACTIVE":0;
}
