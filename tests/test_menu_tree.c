/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_app.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    dpc245_app_screen_t screen;
    evist_adv_screen_t advanced;
} leaf_t;

static void down(dpc245_app_t *app, unsigned count) {
    for (unsigned i = 0u; i < count; ++i) dpc245_app_key(app, DPC245_APP_KEY_DOWN);
}

static void enter_branch(dpc245_app_t *app, unsigned root_row) {
    dpc245_app_event(app, DPC245_KEY_EVENT_SETTINGS_CHORD, 1u);
    assert(app->screen == DPC245_APP_SCREEN_ROOT_MENU);
    dpc245_app_key(app, DPC245_APP_KEY_UP);
    assert(app->selection == 3u);
    dpc245_app_key(app, DPC245_APP_KEY_DOWN);
    assert(app->selection == 0u);
    down(app, root_row);
    dpc245_app_key(app, DPC245_APP_KEY_POWER);
}

static void check_leaf(unsigned root_row, unsigned row, dpc245_app_screen_t branch,
                       leaf_t leaf) {
    dpc245_settings_t settings;
    dpc245_settings_defaults(&settings);
    dpc245_app_t app;
    memset(&app, 0, sizeof(app));
    dpc245_app_init(&app, &settings);
    enter_branch(&app, root_row);
    assert(app.screen == branch);
    dpc245_app_key(&app, DPC245_APP_KEY_UP);
    assert(app.selection == dpc245_app_menu_count(branch) - 1u);
    dpc245_app_key(&app, DPC245_APP_KEY_DOWN);
    assert(app.selection == 0u);
    down(&app, row);
    dpc245_app_key(&app, DPC245_APP_KEY_POWER);
    assert(app.screen == leaf.screen);
    if (leaf.screen == DPC245_APP_SCREEN_ADVANCED) {
        assert(app.advanced.screen == leaf.advanced);
        if (leaf.advanced != EVIST_ADV_DIAG)
            app.advanced.cursor = (uint8_t)(evist_advanced_row_count(&app.advanced) - 1u);
        dpc245_app_key(&app, DPC245_APP_KEY_POWER);
    } else if (leaf.screen == DPC245_APP_SCREEN_THEMES_MENU) {
        dpc245_app_key(&app, DPC245_APP_KEY_POWER);
        assert(app.screen == DPC245_APP_SCREEN_THEMES_MENU && app.selection == 0u);
        assert(!app.settings_dirty);
        down(&app, 5u);
        dpc245_app_key(&app, DPC245_APP_KEY_POWER);
    } else if (leaf.screen == DPC245_APP_SCREEN_TRIP_RESET) {
        dpc245_app_key(&app, DPC245_APP_KEY_DOWN);
    } else {
        dpc245_app_key(&app, DPC245_APP_KEY_POWER);
    }
    assert(app.screen == branch && app.selection == row);
    down(&app, (unsigned)(dpc245_app_menu_count(branch) - 1u - row));
    dpc245_app_key(&app, DPC245_APP_KEY_POWER);
    assert(app.screen == DPC245_APP_SCREEN_ROOT_MENU && app.selection == root_row);
    down(&app, 3u - root_row);
    dpc245_app_key(&app, DPC245_APP_KEY_POWER);
    assert(app.screen == DPC245_APP_SCREEN_DASHBOARD);
}

static void open_display_edit(dpc245_app_t *app, dpc245_settings_t *settings,
                              unsigned row, dpc245_app_screen_t screen) {
    memset(app, 0, sizeof(*app));
    dpc245_settings_defaults(settings);
    dpc245_app_init(app, settings);
    enter_branch(app, 0u);
    down(app, row);
    dpc245_app_key(app, DPC245_APP_KEY_POWER);
    assert(app->screen == screen);
}

static void open_advanced_edit(dpc245_app_t *app, dpc245_settings_t *settings,
                               unsigned branch, unsigned row, evist_adv_screen_t screen) {
    memset(app, 0, sizeof(*app));
    dpc245_settings_defaults(settings);
    dpc245_app_init(app, settings);
    enter_branch(app, branch);
    down(app, row);
    dpc245_app_key(app, DPC245_APP_KEY_POWER);
    assert(app->screen == DPC245_APP_SCREEN_ADVANCED);
    assert(app->advanced.screen == screen && app->advanced.cursor == 0u);
    dpc245_app_key(app, DPC245_APP_KEY_POWER);
    assert(app->advanced.edit);
}

static void repeat_burst(dpc245_app_t *app) {
    static const dpc245_key_event_t events[] = {
        DPC245_KEY_EVENT_UP_REPEAT, DPC245_KEY_EVENT_UP_REPEAT_FAST,
        DPC245_KEY_EVENT_DOWN_REPEAT, DPC245_KEY_EVENT_DOWN_REPEAT_FAST
    };
    dpc245_app_screen_t screen = app->screen;
    dpc245_settings_t settings = app->runtime.settings;
    evist_preferences_t working = app->advanced.working;
    evist_ride_cfg_t ride = app->advanced.ride_working;
    uint8_t cursor = app->advanced.cursor;
    for (unsigned i = 0u; i < 12u; ++i) {
        dpc245_app_event(app, events[i % 4u], 1000u + i);
        assert(app->screen == screen && app->advanced.cursor == cursor);
        assert(memcmp(&app->runtime.settings, &settings, sizeof(settings)) == 0);
        assert(memcmp(&app->advanced.working, &working, sizeof(working)) == 0);
        assert(memcmp(&app->advanced.ride_working, &ride, sizeof(ride)) == 0);
    }
}

static uint8_t display_switch_value(const dpc245_app_t *app,
                                    dpc245_app_screen_t screen) {
    return screen == DPC245_APP_SCREEN_EDIT_UNIT
        ? dpc245_unit(&app->runtime.settings)
        : dpc245_service_tips(&app->runtime.settings);
}

static void test_display_switch_repeat(unsigned row, dpc245_app_screen_t screen) {
    dpc245_settings_t settings;
    dpc245_app_t app;
    open_display_edit(&app, &settings, row, screen);
    uint8_t before = display_switch_value(&app, screen);
    repeat_burst(&app);
    assert(display_switch_value(&app, screen) == before);
    assert(!app.settings_dirty);
    dpc245_app_event(&app, DPC245_KEY_EVENT_UP_SHORT, 1100u);
    assert(display_switch_value(&app, screen) == before);
    assert(!app.settings_dirty);

    open_display_edit(&app, &settings, row, screen);
    before = display_switch_value(&app, screen);
    dpc245_app_event(&app, DPC245_KEY_EVENT_UP_SHORT, 1200u);
    assert(display_switch_value(&app, screen) == (uint8_t)(before ^ 1u));
    assert(app.settings_dirty);
}

static void test_advanced_switch_repeat(unsigned branch, unsigned row,
                                        evist_adv_screen_t screen) {
    dpc245_settings_t settings;
    dpc245_app_t app;
    open_advanced_edit(&app, &settings, branch, row, screen);
    evist_pref_light_mode_t light = app.advanced.working.light_mode;
    bool quick = app.advanced.working.quick_walk_enabled;
    evist_ride_mode_t mode = app.advanced.ride_working.mode;
    repeat_burst(&app);
    assert(app.advanced.edit && app.advanced.cursor == 0u);
    assert(app.advanced.working.light_mode == light);
    assert(app.advanced.working.quick_walk_enabled == quick);
    assert(app.advanced.ride_working.mode == mode);
    dpc245_app_event(&app, DPC245_KEY_EVENT_UP_SHORT, 1100u);
    assert(app.advanced.working.light_mode == light);
    assert(app.advanced.working.quick_walk_enabled == quick);
    assert(app.advanced.ride_working.mode == mode);
}

static void test_repeatable_advanced_rows(void) {
    dpc245_settings_t settings;
    dpc245_app_t app;

    open_advanced_edit(&app, &settings, 0u, 1u, EVIST_ADV_LIGHT);
    dpc245_app_key(&app, DPC245_APP_KEY_POWER);
    dpc245_app_key(&app, DPC245_APP_KEY_DOWN);
    dpc245_app_key(&app, DPC245_APP_KEY_POWER);
    assert(app.advanced.edit && app.advanced.cursor == 1u);
    app.advanced.working.light_gain = 8u;
    dpc245_app_event(&app, DPC245_KEY_EVENT_UP_REPEAT, 1000u);
    assert(app.advanced.working.light_gain == 9u);
    dpc245_app_event(&app, DPC245_KEY_EVENT_UP_REPEAT_FAST, 1010u);
    assert(app.advanced.working.light_gain == 9u);
    app.advanced.working.light_gain = 2u;
    dpc245_app_event(&app, DPC245_KEY_EVENT_DOWN_REPEAT, 1020u);
    dpc245_app_event(&app, DPC245_KEY_EVENT_DOWN_REPEAT_FAST, 1030u);
    assert(app.advanced.working.light_gain == 1u);

    open_advanced_edit(&app, &settings, 1u, 5u, EVIST_ADV_QUICK_WALK);
    dpc245_app_key(&app, DPC245_APP_KEY_POWER);
    dpc245_app_key(&app, DPC245_APP_KEY_DOWN);
    dpc245_app_key(&app, DPC245_APP_KEY_POWER);
    assert(app.advanced.edit && app.advanced.cursor == 1u);
    app.advanced.working.quick_walk_hold_steps = 9u;
    dpc245_app_event(&app, DPC245_KEY_EVENT_UP_REPEAT, 1000u);
    dpc245_app_event(&app, DPC245_KEY_EVENT_UP_REPEAT_FAST, 1010u);
    assert(app.advanced.working.quick_walk_hold_steps == 10u);
    app.advanced.working.quick_walk_hold_steps = 2u;
    dpc245_app_event(&app, DPC245_KEY_EVENT_DOWN_REPEAT, 1020u);
    dpc245_app_event(&app, DPC245_KEY_EVENT_DOWN_REPEAT_FAST, 1030u);
    assert(app.advanced.working.quick_walk_hold_steps == 1u);

    open_advanced_edit(&app, &settings, 1u, 4u, EVIST_ADV_MODE);
    dpc245_app_key(&app, DPC245_APP_KEY_POWER);
    dpc245_app_key(&app, DPC245_APP_KEY_DOWN);
    dpc245_app_key(&app, DPC245_APP_KEY_POWER);
    assert(app.advanced.edit && app.advanced.cursor == 1u);
    app.advanced.ride_working.start = EVIST_RIDE_START_LEGAL;
    dpc245_app_event(&app, DPC245_KEY_EVENT_UP_REPEAT, 1000u);
    dpc245_app_event(&app, DPC245_KEY_EVENT_UP_REPEAT_FAST, 1010u);
    assert(app.advanced.ride_working.start == EVIST_RIDE_START_OFFROAD);
    dpc245_app_event(&app, DPC245_KEY_EVENT_DOWN_REPEAT, 1020u);
    dpc245_app_event(&app, DPC245_KEY_EVENT_DOWN_REPEAT_FAST, 1030u);
    assert(app.advanced.ride_working.start == EVIST_RIDE_START_LAST);
}

static void test_nonrepeatable_advanced_pages(void) {
    dpc245_settings_t settings;
    dpc245_app_t app;

    open_advanced_edit(&app, &settings, 1u, 2u, EVIST_ADV_FIRMWARE);
    evist_firmware_profile_t profile = app.advanced.working.firmware_profile;
    repeat_burst(&app);
    assert(app.advanced.edit && app.advanced.working.firmware_profile == profile);

    open_advanced_edit(&app, &settings, 0u, 3u, EVIST_ADV_PANELS);
    uint16_t panels = app.advanced.working.panel_mask;
    repeat_burst(&app);
    assert(app.advanced.edit && app.advanced.working.panel_mask == panels);

    open_advanced_edit(&app, &settings, 0u, 2u, EVIST_ADV_BATTERY);
    uint8_t battery = app.advanced.working.battery_mask;
    repeat_burst(&app);
    assert(app.advanced.edit && app.advanced.working.battery_mask == battery);
}

int main(void) {
    test_display_switch_repeat(0u, DPC245_APP_SCREEN_EDIT_UNIT);
    test_display_switch_repeat(7u, DPC245_APP_SCREEN_EDIT_SERVICE_TIPS);
    test_advanced_switch_repeat(0u, 1u, EVIST_ADV_LIGHT);
    test_advanced_switch_repeat(1u, 5u, EVIST_ADV_QUICK_WALK);
    test_advanced_switch_repeat(1u, 4u, EVIST_ADV_MODE);
    test_repeatable_advanced_rows();
    test_nonrepeatable_advanced_pages();
    puts("key repeat: two-state edits, bounded rows and nonrepeatable pages passed");

    static const leaf_t display[] = {
        {DPC245_APP_SCREEN_EDIT_UNIT, EVIST_ADV_MENU},
        {DPC245_APP_SCREEN_ADVANCED, EVIST_ADV_LIGHT},
        {DPC245_APP_SCREEN_ADVANCED, EVIST_ADV_BATTERY},
        {DPC245_APP_SCREEN_ADVANCED, EVIST_ADV_PANELS},
        {DPC245_APP_SCREEN_ADVANCED, EVIST_ADV_TEMP_VIEW},
        {DPC245_APP_SCREEN_ADVANCED, EVIST_ADV_GRAPH},
        {DPC245_APP_SCREEN_THEMES_MENU, EVIST_ADV_MENU},
        {DPC245_APP_SCREEN_EDIT_SERVICE_TIPS, EVIST_ADV_MENU},
        {DPC245_APP_SCREEN_EDIT_AUTO_OFF, EVIST_ADV_MENU}
    };
    static const leaf_t bike[] = {
        {DPC245_APP_SCREEN_ADVANCED, EVIST_ADV_ASSIST},
        {DPC245_APP_SCREEN_ADVANCED, EVIST_ADV_ACCEL},
        {DPC245_APP_SCREEN_ADVANCED, EVIST_ADV_FIRMWARE},
        {DPC245_APP_SCREEN_ADVANCED, EVIST_ADV_WHEEL},
        {DPC245_APP_SCREEN_ADVANCED, EVIST_ADV_MODE},
        {DPC245_APP_SCREEN_ADVANCED, EVIST_ADV_QUICK_WALK},
        {DPC245_APP_SCREEN_ADVANCED, EVIST_ADV_MODES}
    };
    static const leaf_t info[] = {
        {DPC245_APP_SCREEN_INFO_HMI, EVIST_ADV_MENU},
        {DPC245_APP_SCREEN_INFO_BATTERY, EVIST_ADV_MENU},
        {DPC245_APP_SCREEN_INFO_CONTROLLER, EVIST_ADV_MENU},
        {DPC245_APP_SCREEN_INFO_SENSOR, EVIST_ADV_MENU},
        {DPC245_APP_SCREEN_INFO_ERRORS, EVIST_ADV_MENU},
        {DPC245_APP_SCREEN_ADVANCED, EVIST_ADV_DIAG},
        {DPC245_APP_SCREEN_TRIP_RESET, EVIST_ADV_MENU}
    };
    for (unsigned i = 0u; i < sizeof(display)/sizeof(display[0]); ++i)
        check_leaf(0u, i, DPC245_APP_SCREEN_DISPLAY_MENU, display[i]);
    for (unsigned i = 0u; i < sizeof(bike)/sizeof(bike[0]); ++i)
        check_leaf(1u, i, DPC245_APP_SCREEN_BIKE_MENU, bike[i]);
    for (unsigned i = 0u; i < sizeof(info)/sizeof(info[0]); ++i)
        check_leaf(2u, i, DPC245_APP_SCREEN_INFO_MENU, info[i]);
    puts("menu tree: 23 leaves, parent rows, branch BACK and root EXIT passed");

    {
        dpc245_settings_t settings;
        dpc245_settings_defaults(&settings);
        dpc245_app_t app;
        memset(&app, 0, sizeof(app));
        dpc245_app_init(&app, &settings);
        enter_branch(&app, 0u);
        down(&app, 4u); /* DISPLAY -> TEMP VIEW */
        dpc245_app_key(&app, DPC245_APP_KEY_POWER);
        assert(app.screen == DPC245_APP_SCREEN_ADVANCED && app.advanced.screen == EVIST_ADV_TEMP_VIEW);
        dpc245_app_key(&app, DPC245_APP_KEY_POWER);
        dpc245_app_key(&app, DPC245_APP_KEY_UP);
        dpc245_app_key(&app, DPC245_APP_KEY_POWER);
        dpc245_app_key(&app, DPC245_APP_KEY_DOWN); /* MOTOR LIMIT */
        dpc245_app_key(&app, DPC245_APP_KEY_POWER);
        dpc245_app_key(&app, DPC245_APP_KEY_UP);
        assert(app.advanced.working.motor_temp_limit_c==40u);
        dpc245_app_key(&app, DPC245_APP_KEY_DOWN);
        assert(app.advanced.working.motor_temp_limit_c==0u);
        dpc245_app_key(&app, DPC245_APP_KEY_UP);
        app.advanced.working.motor_temp_limit_c=150u;
        dpc245_app_event(&app,DPC245_KEY_EVENT_UP_REPEAT,1000u);
        assert(app.advanced.working.motor_temp_limit_c==150u);
        dpc245_app_key(&app, DPC245_APP_KEY_POWER);
        dpc245_app_key(&app,DPC245_APP_KEY_DOWN); /* CTRL LIMIT */
        dpc245_app_key(&app,DPC245_APP_KEY_POWER);
        dpc245_app_key(&app,DPC245_APP_KEY_UP);
        assert(app.advanced.working.ctrl_temp_limit_c==40u);
        dpc245_app_event(&app,DPC245_KEY_EVENT_DOWN_REPEAT,1010u);
        assert(app.advanced.working.ctrl_temp_limit_c==0u);
        dpc245_app_event(&app,DPC245_KEY_EVENT_DOWN_REPEAT,1020u);
        assert(app.advanced.working.ctrl_temp_limit_c==0u);
        dpc245_app_key(&app,DPC245_APP_KEY_POWER);
        dpc245_app_key(&app,DPC245_APP_KEY_DOWN); /* SAVE */
        dpc245_app_key(&app, DPC245_APP_KEY_POWER);
        assert(app.preferences.temp_view == EVIST_TEMP_ALTERNATE && app.preferences_dirty &&
               app.preferences.motor_temp_limit_c==150u);
        dpc245_app_key(&app, DPC245_APP_KEY_DOWN); /* BACK */
        dpc245_app_key(&app, DPC245_APP_KEY_POWER);
        assert(app.screen == DPC245_APP_SCREEN_DISPLAY_MENU && app.selection == 4u);
    }
    puts("TEMP VIEW: DISPLAY reach, edit, SAVE, and BACK passed");

    {
        dpc245_settings_t st;dpc245_settings_defaults(&st);
        dpc245_app_t app;memset(&app,0,sizeof(app));dpc245_app_init(&app,&st);
        enter_branch(&app,0u);down(&app,6u);dpc245_app_key(&app,DPC245_APP_KEY_POWER);
        for(unsigned row=0u;row<5u;row++){
            app.selection=(uint8_t)row;
            dpc245_app_key(&app,DPC245_APP_KEY_POWER);
            assert(app.screen==DPC245_APP_SCREEN_THEMES_MENU);
        }
        app.selection=5u;dpc245_app_key(&app,DPC245_APP_KEY_POWER);
        assert(app.screen==DPC245_APP_SCREEN_DISPLAY_MENU && app.selection==6u);
    }
    puts("THEMES: theme rows and SAVE stay in menu; BACK returns to DISPLAY");

    dpc245_settings_t settings;
    dpc245_settings_defaults(&settings);
    for (unsigned branch = 0u; branch < 3u; ++branch) {
        dpc245_app_t app;
        memset(&app, 0, sizeof(app));
        dpc245_app_init(&app, &settings);
        enter_branch(&app, branch);
        if (branch == 0u) {
            down(&app, 1u); /* BACKLIGHT */
            dpc245_app_key(&app, DPC245_APP_KEY_POWER);
            app.advanced.working.light_gain++;
            app.advanced.light_runtime_dirty = true;
        } else if (branch == 1u) {
            down(&app, 4u); /* RIDE MODE */
            dpc245_app_key(&app, DPC245_APP_KEY_POWER);
            app.advanced.ride_working.mode = EVIST_RIDE_OFFROAD;
        } else {
            down(&app, 5u); /* DIAGNOSTICS has no editable state */
            dpc245_app_key(&app, DPC245_APP_KEY_POWER);
        }
        dpc245_app_tick(&app, 30001u);
        assert(app.screen == DPC245_APP_SCREEN_DASHBOARD);
        assert(!app.advanced.preferences_changed && !app.advanced.ride_changed);
        assert(app.advanced.working.light_gain == app.advanced.committed.light_gain);
        assert(app.advanced.ride_working.mode == app.advanced.ride_committed.mode);
    }
    puts("menu idle: all three branches return; unsaved LIGHT and RIDE edits abandoned");
    return 0;
}
