/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Renders the real dashboard and menu code to RGB565 frames for layout checks.
 * usage: render_screens OUT_DIR
 * Each screen is written as OUT_DIR/<name>.rgb565 (320x170, little-endian pixels).
 * Telemetry is a fixed sample, so frames are reproducible. */

#include "dpc245_app.h"
#include "dpc245_ble_app.h"
#include "ui_dashboard.h"
#include "can_router.h"
#include "advanced_settings.h"
#include "dpc245_settings.h"
#include "dpc245_hwinfo.h"
#include "dpc245_adc.h"
#include "st7789_8080.h"
#include "ui_arc_geometry.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <time.h>

extern uint16_t sim_fb[DPC245_LCD_HEIGHT][DPC245_LCD_WIDTH];

static const char *out_dir;
static dpc245_ui_status_t status;
static const dpc245_app_t *layout_app;

/* Layout check: every drawn string's ink box (from ui_font1_text). Two strings
 * overlapping, or glyphs left off the screen, are reported per frame. */
extern void (*ui_font1_trace)(int x0, int y0, int x1, int y1, unsigned skipped);
typedef struct { int x0, y0, x1, y1; unsigned skipped; } box_t;
static box_t boxes[256];
static unsigned box_count;
static unsigned layout_issues;
static unsigned arc_frame_colours_seen;
static int check_dashboard_zones;
static int check_lower_separator = 1;
static int check_badge_geometry;
static int check_speed_cells;
static unsigned speed_cell_frames;
static int speed_cell_baseline[7];
static int speed_cell_seen;
#define LOWER_SEPARATOR_X0 139
#define LOWER_SEPARATOR_X1 140
#define LOWER_PANEL_Y0 132
#define LOWER_PANEL_Y1 169
#define BATTERY_BAR_X0 0
#define BATTERY_BAR_X1 7
#define BATTERY_BAR_Y0 2
#define BATTERY_BAR_Y1 168
#define POWER_BAR_X0 287
#define POWER_BAR_X1 312
#define STATUS_ICON_Y0 8
#define STATUS_ICON_Y1 27
static void trace_box(int x0, int y0, int x1, int y1, unsigned skipped) {
    if (box_count < sizeof(boxes) / sizeof(boxes[0])) boxes[box_count++] = (box_t){x0, y0, x1, y1, skipped};
}
static void trace_speed_cells(int units_x0,int dot_x,int tenths_x0,int tens_x0,
                              int tens_x1,int units_x1,int tenths_x1,int tens_drawn){
    if(!check_speed_cells)return;
    static const int tens_expected[6]={0,0,0,1,1,1};
    int current[7]={units_x0,dot_x,tenths_x0,tens_x0,tens_x1,units_x1,tenths_x1};
    assert(units_x0==157 && dot_x==231 && tenths_x0==243);
    assert(tens_x0==81 && tens_x1==156 && units_x1==232 && tenths_x1==281);
    assert(tens_drawn==tens_expected[speed_cell_frames%6u]);
    if(speed_cell_frames==0u)memcpy(speed_cell_baseline,current,sizeof(current));
    else assert(memcmp(speed_cell_baseline,current,sizeof(current))==0);
    speed_cell_frames++;speed_cell_seen++;
}
static void arc_frame_pixel(const char *name,unsigned x,unsigned y,uint16_t expected){
    if(sim_fb[y][x]!=expected){
        printf("LAYOUT %s: arc frame pixel %u,%u is %04X, expected %04X\n",
               name,x,y,sim_fb[y][x],expected);
        layout_issues++;
    }
}
static void check_arc_frame(const char *name,uint16_t colour){
    const unsigned middle=(ARC_FRAME_TOP+ARC_FRAME_BOTTOM)/2u;
    for(unsigned i=0u;i<ARC_FRAME_W;i++){
        arc_frame_pixel(name,100u,ARC_FRAME_TOP+i,colour);
        arc_frame_pixel(name,100u,ARC_FRAME_BOTTOM-i,colour);
        arc_frame_pixel(name,ARC_FRAME_LEFT+i,middle,colour);
    }
    arc_frame_pixel(name,100u,ARC_FRAME_TOP+ARC_FRAME_W,0u);
    arc_frame_pixel(name,100u,ARC_FRAME_BOTTOM-ARC_FRAME_W,0u);
    arc_frame_pixel(name,ARC_FRAME_LEFT+ARC_FRAME_W,middle,0u);
    for(unsigned y=ARC_FRAME_TOP+20u;y<=ARC_FRAME_TOP+40u;y+=20u){
        unsigned x=ARC_FRAME_RIGHT+(y-ARC_FRAME_TOP)*(ARC_FRAME_TIP-ARC_FRAME_RIGHT)/(ARC_FRAME_MID-ARC_FRAME_TOP);
        for(unsigned i=0u;i<ARC_FRAME_W;i++){
            arc_frame_pixel(name,x-i,y,colour);
            arc_frame_pixel(name,x-i,ARC_FRAME_TOP+ARC_FRAME_BOTTOM-y,colour);
        }
        arc_frame_pixel(name,x-ARC_FRAME_W,y,0u);
        arc_frame_pixel(name,x-ARC_FRAME_W,ARC_FRAME_TOP+ARC_FRAME_BOTTOM-y,0u);
    }
    unsigned speed_glyphs=0u;
    const box_t *unit=0,*tenth=0;
    for(unsigned i=0u;i<box_count;i++){
        const box_t *b=&boxes[i];
        if(b->x0<(int)ARC_FRAME_LEFT||b->x1>(int)ARC_FRAME_TIP||
           b->y0<(int)ARC_FRAME_TOP||b->y1>(int)ARC_FRAME_BOTTOM)continue;
        speed_glyphs++;
        if(b->y1<(int)ARC_CELL_TOP)unit=b;
        if(b->x0>=(int)(ARC_DIGIT_X+2u*ARC_DIGIT_W+ARC_DOT_W)&&
           b->y0>=(int)ARC_CELL_TOP)tenth=b;
        int gap=b->x0-(int)(ARC_FRAME_LEFT+ARC_FRAME_W-1u)-1;
        int top=b->y0-(int)(ARC_FRAME_TOP+ARC_FRAME_W-1u)-1;
        int bottom=(int)(ARC_FRAME_BOTTOM-ARC_FRAME_W+1u)-b->y1-1;
        if(top<gap)gap=top;if(bottom<gap)gap=bottom;
        for(int y=b->y0;y<=b->y1;y++){
            unsigned edge_y=(unsigned)y<=ARC_FRAME_MID?(unsigned)y:ARC_FRAME_TOP+ARC_FRAME_BOTTOM-(unsigned)y;
            unsigned x=ARC_FRAME_RIGHT+(edge_y-ARC_FRAME_TOP)*(ARC_FRAME_TIP-ARC_FRAME_RIGHT)/(ARC_FRAME_MID-ARC_FRAME_TOP);
            int right=(int)(x-ARC_FRAME_W+1u)-b->x1-1;
            if(right<gap)gap=right;
        }
        if(gap<3){
            printf("LAYOUT %s: arc speed ink %d,%d-%d,%d has only %d px frame clearance\n",
                   name,b->x0,b->y0,b->x1,b->y1,gap);
            layout_issues++;
        }
    }
    if(speed_glyphs<4u){printf("LAYOUT %s: arc speed glyphs missing\n",name);layout_issues++;}
    if(!unit||!tenth||unit->y1>=tenth->y0||
       unit->x1<(int)ARC_UNIT_RIGHT-3||unit->x1>=(int)ARC_UNIT_RIGHT){
        printf("LAYOUT %s: arc unit must sit above and align with the tenth column\n",name);
        layout_issues++;
    }
}
static void check_layout(const char *name) {
    printf("%-20s %3u strings checked\n", name, box_count);
    uint8_t theme=layout_app?dpc245_theme(&layout_app->runtime.settings):0u;
    if(check_dashboard_zones&&theme==DPC245_THEME_ARC){
        int offroad=layout_app->ride_indicator==DPC245_RIDE_IND_OFFROAD;
        arc_frame_colours_seen|=offroad?2u:1u;
        check_arc_frame(name,offroad?ARC_FRAME_OFFROAD_COL:ARC_FRAME_LEGAL_COL);
    }
    int warning_banner=layout_app && layout_app->temp_warning_sources &&
        !layout_app->ride_confirm_pending && !layout_app->ride_toast_active &&
        layout_app->ride_apply_result!=EVIST_RIDE_APPLY_FAILED;
    if (layout_app && layout_app->screen == DPC245_APP_SCREEN_DASHBOARD &&
        layout_app->power_graph_w > 0u && !(warning_banner&&theme==DPC245_THEME_WEDGE)) {
        int visible = 0;
        unsigned y0=theme==DPC245_THEME_WEDGE?148u:0u;
        unsigned y1=theme==DPC245_THEME_WEDGE?166u:170u;
        unsigned x0=theme==DPC245_THEME_ARC?5u:(theme==DPC245_THEME_WEDGE?11u:289u);
        unsigned x1=theme==DPC245_THEME_ARC?65u:(theme==DPC245_THEME_WEDGE?309u:313u);
        for (unsigned y = y0; y < y1 && !visible; ++y)
            for (unsigned x = x0; x < x1; ++x)
                if (sim_fb[y][x] != 0u) { visible = 1; break; }
        if (!visible) {
            printf("LAYOUT %s: power bar missing for nonzero power\n", name);
            layout_issues++;
        }
    }
    if (check_badge_geometry) {
        const box_t *badge = 0, *unit = 0, *tenths = 0;
        box_t badge_pair={0};
        box_t speed_box={0}; int have_speed=false;
        for (unsigned i = 0; i < box_count; ++i) {
            const box_t *b = &boxes[i];
            if (b->x0 < 100 && b->y0 >= 51 && b->y1 <= 98) {
                if (strstr(name,"Splus")) {
                    if(!badge)badge_pair=*b;
                    else {
                        if(b->x0<badge_pair.x0)badge_pair.x0=b->x0;
                        if(b->y0<badge_pair.y0)badge_pair.y0=b->y0;
                        if(b->x1>badge_pair.x1)badge_pair.x1=b->x1;
                        if(b->y1>badge_pair.y1)badge_pair.y1=b->y1;
                    }
                    badge=&badge_pair;
                } else badge = b;
            }
            if (b->x0 >= 75 && b->x1 <= 282 && b->y0 >= 28 && b->y1 < 132) {
                if(!have_speed){speed_box=*b;have_speed=true;}
                else {
                    if(b->x0<speed_box.x0)speed_box.x0=b->x0;
                    if(b->y0<speed_box.y0)speed_box.y0=b->y0;
                    if(b->x1>speed_box.x1)speed_box.x1=b->x1;
                    if(b->y1>speed_box.y1)speed_box.y1=b->y1;
                }
                if(b->y0>=35 && b->y1<=65 && b->x0>=200)unit=b;
                if(b->y0>65 && b->y1<132 && b->x0>=200) {
                    if(!tenths || b->y0<tenths->y0)tenths=b;
                }
            }
        }
        if (!badge) { printf("LAYOUT %s: badge ink not found\n", name); layout_issues++; }
        else {
            int curved = 99;
            for (int y = badge->y0; y <= badge->y1; ++y) {
                int row = y - 51;
                int inset = 0;
                if (row < 14 || row >= 34) {
                    int d = row < 14 ? row : 47 - row;
                    int dy = 13 - d;
                    int rem = 14 * 14 - dy * dy;
                    int root = 0;
                    while ((root + 1) * (root + 1) <= rem) root++;
                    inset = 14 - root;
                }
                int span_left = 19 + inset;
                int span_right = 66 - inset;
                int clear = badge->x0 - span_left;
                if (span_right - badge->x1 < clear) clear = span_right - badge->x1;
                if (clear < curved) curved = clear;
            }
            printf("BADGE %s: ink %d,%d-%d,%d; curved inner-edge clearance %d px\n", name,
                   badge->x0,badge->y0,badge->x1,badge->y1,curved);
            if (curved < 2) {
                printf("LAYOUT %s: badge ink clearance below 2 px on curved edge\n", name); layout_issues++;
            }
        }
        if (!have_speed || speed_box.x0 < 81 || speed_box.x1 > 282 || speed_box.y0 < 28 || speed_box.y1 > 130) {
            printf("LAYOUT %s: speed ink outside x78..282/y28..130 bounds\n", name); layout_issues++;
        } else {
            int tenths_unit_gap=unit&&tenths?tenths->y0-unit->y1-1:-999;
            printf("SPEED %s: box %d,%d-%d,%d; ring gap %d px; power gap %d px; lower gap %d px; tenths/unit gap %d px\n", name,
                   speed_box.x0,speed_box.y0,speed_box.x1,speed_box.y1,speed_box.x0-77,286-speed_box.x1-1,131-speed_box.y1,tenths_unit_gap);
            if(tenths_unit_gap<2){printf("LAYOUT %s: tenths/unit gap below 2 px\n",name);layout_issues++;}
        }
    }
    for (unsigned i = 0; i < box_count; ++i) {
        const box_t *a = &boxes[i];
        if(check_dashboard_zones && theme>=DPC245_THEME_ARC){
            static const box_t arc[]={{0,0,27,169,0},{28,0,238,25,0},{28,26,238,133,0},
                                      {28,134,238,169,0},{239,0,319,84,0},{239,85,319,169,0}};
            static const box_t wedge[]={{0,0,215,34,0},{0,35,215,143,0},
                                         {210,0,319,143,0},{0,144,319,169,0}};
            const box_t *regions=theme==DPC245_THEME_ARC?arc:wedge;
            unsigned count=theme==DPC245_THEME_ARC?6u:4u;
            int contained=0;
            for(unsigned k=0;k<count;k++)if(a->x0>=regions[k].x0&&a->y0>=regions[k].y0&&
                a->x1<=regions[k].x1&&a->y1<=regions[k].y1){contained=1;break;}
            if(!contained){printf("LAYOUT %s: text outside theme region in box %d,%d-%d,%d\n",
                                  name,a->x0,a->y0,a->x1,a->y1);layout_issues++;}
            if((theme==DPC245_THEME_WEDGE&&a->y1>=144&&!warning_banner)||
               (theme==DPC245_THEME_ARC&&a->x0<28&&a->y0<=137&&a->y1>=60)){
                printf("LAYOUT %s: text touches power area in box %d,%d-%d,%d\n",
                       name,a->x0,a->y0,a->x1,a->y1);layout_issues++;
            }
            if(theme==DPC245_THEME_WEDGE&&a->x0>=250&&
                (a->x0<252||a->x1>315)){
                printf("LAYOUT %s: text lacks tile margin in box %d,%d-%d,%d\n",
                       name,a->x0,a->y0,a->x1,a->y1);layout_issues++;
            }
            if(theme==DPC245_THEME_ARC && a->x0>=238 && a->y0<168){
                int edge=a->y0<85?(a->y0<23?257-(a->y0-3)*22/20:
                    235+(a->y1-23)*5/57):240+(a->y1-88)/4;
                if(a->x0-edge<3 || a->x1>315){
                    printf("LAYOUT %s: arc tile ink lacks 3 px margin in box %d,%d-%d,%d\n",
                           name,a->x0,a->y0,a->x1,a->y1);layout_issues++;
                }
            }
            /* The speed ink and frame are checked together above. */
            if(theme==DPC245_THEME_WEDGE && a->x0>=250 && a->x1<=318){
                if(a->x0<252 || a->x1>315 || (a->y0<=79 && a->y1>=78) || a->y0<12 || a->y1>139){
                    printf("LAYOUT %s: wedge tile ink lacks 3 px margin in box %d,%d-%d,%d\n",
                           name,a->x0,a->y0,a->x1,a->y1);layout_issues++;
                }
            }
        }
        if (a->skipped) { printf("LAYOUT %s: %u glyph(s) missing or off screen in box %d,%d-%d,%d\n", name, a->skipped, a->x0, a->y0, a->x1, a->y1); layout_issues++; }
        if (check_dashboard_zones && theme<DPC245_THEME_ARC && check_lower_separator &&
            !warning_banner &&
            a->x0 <= LOWER_SEPARATOR_X1 && a->x1 >= LOWER_SEPARATOR_X0 &&
            a->y0 <= LOWER_PANEL_Y1 && a->y1 >= LOWER_PANEL_Y0) {
            printf("LAYOUT %s: text touches lower separator in box %d,%d-%d,%d\n",
                   name, a->x0, a->y0, a->x1, a->y1);
            layout_issues++;
        }
        if (check_dashboard_zones && theme<DPC245_THEME_ARC && a->x0 <= POWER_BAR_X1 && a->x1 >= POWER_BAR_X0) {
            printf("LAYOUT %s: text overlaps power bar in box %d,%d-%d,%d\n",
                   name, a->x0, a->y0, a->x1, a->y1);
            layout_issues++;
        }
        if (check_dashboard_zones && theme<DPC245_THEME_ARC && layout_app && (layout_app->preferences.battery_mask&EVIST_BATTERY_BAR) &&
            a->x0 <= BATTERY_BAR_X1 && a->x1 >= BATTERY_BAR_X0 &&
            a->y0 <= BATTERY_BAR_Y1 && a->y1 >= BATTERY_BAR_Y0) {
            printf("LAYOUT %s: text overlaps battery bar in box %d,%d-%d,%d\n",
                   name, a->x0, a->y0, a->x1, a->y1);
            layout_issues++;
        }
        if (check_dashboard_zones && theme<DPC245_THEME_ARC && a->y0 <= STATUS_ICON_Y1 && a->y1 >= STATUS_ICON_Y0 &&
            ((status.light_on && a->x0 <= 27 && a->x1 >= 8) ||
             (status.ble_online && a->x0 <= 55 && a->x1 >= 36) ||
             (status.controller_error && a->x0 <= 83 && a->x1 >= 64))) {
            printf("LAYOUT %s: text overlaps status icon in box %d,%d-%d,%d\n",
                   name, a->x0, a->y0, a->x1, a->y1);
            layout_issues++;
        }
        for (unsigned j = i + 1; j < box_count; ++j) {
            const box_t *b = &boxes[j];
            /* Same row and closer than 4 px: reported as tight (not a failure). */
            if (a->y0 <= b->y1 && b->y0 <= a->y1) {
                int gap = b->x0 > a->x1 ? b->x0 - a->x1 - 1 : (a->x0 > b->x1 ? a->x0 - b->x1 - 1 : -1);
                if (gap >= 0 && gap < 4)
                    printf("TIGHT  %s: %d px between %d,%d-%d,%d and %d,%d-%d,%d\n", name, gap,
                           a->x0, a->y0, a->x1, a->y1, b->x0, b->y0, b->x1, b->y1);
            }
            if (a->x0 <= b->x1 && b->x0 <= a->x1 && a->y0 <= b->y1 && b->y0 <= a->y1) {
                printf("LAYOUT %s: overlap %d,%d-%d,%d with %d,%d-%d,%d\n", name, a->x0, a->y0, a->x1, a->y1, b->x0, b->y0, b->x1, b->y1);
                layout_issues++;
            }
        }
    }
    box_count = 0;
}

static void dump(const char *name) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.rgb565", out_dir, name);
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    for (unsigned y = 0u; y < DPC245_LCD_HEIGHT; ++y)
        for (unsigned x = 0u; x < DPC245_LCD_WIDTH; ++x) {
            uint16_t v = sim_fb[y][x];
            fputc(v & 0xFFu, f);
            fputc(v >> 8, f);
        }
    fclose(f);
    printf("%s\n", name);
}

static void sample_telemetry(evist_can_router_t *r) {
    bafang_telemetry_t *t = &r->telemetry;
    t->have_speed = true;          t->speed_centi_kph = 2180;
    t->have_cadence = true;        t->cadence_rpm = 78;
    t->have_torque_sensor = true;  t->torque_sensor_raw = 1520; t->torque_signal_mv = 1310;
    t->torque_sensor_cadence_rpm = 78;
    t->have_voltage = true;        t->voltage_centi_volt = 3660;
    t->have_current = true;        t->current_centi_amp = 640;
    t->have_battery_pct = true;    t->battery_pct = 76;
    t->have_battery_state = true;  t->battery_temp_c = 24; t->battery_voltage_centi_volt = 3655;
    t->battery_current_centi_amp = 640;
    t->have_battery_capacity = true; t->battery_remaining_capacity_raw = 1180;
    t->battery_full_capacity_raw = 1500;
    t->have_range = true;          t->range_centi_km = 4320;
    t->have_controller_temp = true; t->controller_temp_c = 41;
    t->have_motor_temp = true; t->motor_temp_c = 38;
}

int main(int argc, char **argv) {
    out_dir = argc > 1 ? argv[1] : "sim/out";
    ui_font1_trace = trace_box;
    ui_dashboard_speed_cells_trace=trace_speed_cells;

    dpc245_settings_t settings;
    dpc245_settings_defaults(&settings);
    static dpc245_app_t app;
    dpc245_app_init(&app, &settings);
    layout_app=&app;
    /* Static dashboard frames represent settled telemetry unless a sequence
     * below explicitly exercises the smoothing transition. */
    app.power_graph_w = 234u;
    static evist_can_router_t router;
    evist_can_router_init(&router);
    sample_telemetry(&router);

    dpc245_adc_snapshot_t adc;
    memset(&adc, 0, sizeof(adc));
    adc.valid = true;
    adc.als_raw = 2140;
    adc.button_signal_raw = 3100;
    dpc245_hwinfo_t hw;
    memset(&hw, 0, sizeof(hw));

    /* LIGHT preview mirrors what main.c publishes from the light controller. */
    app.light_als_filtered = adc.als_raw;
    app.light_backlight_position = 6u;
    app.assist_level = 1u;   /* mode E */
    app.ride_indicator = DPC245_RIDE_IND_LEGAL;
    app.ride_last_applied_x100 = 7000u; /* longest DIAG ride-page speed text */
    app.ride_apply_result = EVIST_RIDE_APPLY_OK;
    app.loop_max_us = 3850u;      /* sample values for the DIAG timing-page preview */
    app.last_write_seen = true;
    app.last_write_command = 0x6011u;
    app.last_write_acks = 10u;
    app.last_write_final_ack = true;
    app.last_write_error = true;
    app.last_write_error_payload[0] = 1u;
    app.last_write_error_payload[2] = 2u;
    app.last_write_error_payload[3] = 6u;
    app.last_write_result = EVIST_TXN_REJECTED;
    app.ble_rejected_writes = 123u;
    app.ble_rejected_class = 0x11u;
    app.ble_rejected_command = 0xA9u;
    app.ble_rejected_operation = 2u;
    app.ble_rejected_selector = 0xFFu;
    app.ble_rejected_reason = DPC245_BLE_REJECT_READ_REQUIRED;
    app.loop_1s_max_us = 1200u;

    struct frame { const char *name; dpc245_app_screen_t screen; uint8_t detail; int adv; evist_adv_screen_t adv_screen; uint8_t diag_page; uint8_t selection; };
    static const struct frame frames[] = {
        { "01_dashboard",       DPC245_APP_SCREEN_DASHBOARD,     0, 0, EVIST_ADV_MENU,       0, 0 },
        { "02_settings_root",   DPC245_APP_SCREEN_ROOT_MENU,     0, 0, EVIST_ADV_MENU,       0, 0 },
        { "03_display_menu",    DPC245_APP_SCREEN_DISPLAY_MENU,  0, 0, EVIST_ADV_MENU,       0, 0 },
        { "03_display_last",    DPC245_APP_SCREEN_DISPLAY_MENU,  0, 0, EVIST_ADV_MENU,       0, 9 },
        { "03_bike_menu",      DPC245_APP_SCREEN_BIKE_MENU,     0, 0, EVIST_ADV_MENU,       0, 0 },
        { "03_bike_last",      DPC245_APP_SCREEN_BIKE_MENU,     0, 0, EVIST_ADV_MENU,       0, 6 },
        { "04_info_menu",       DPC245_APP_SCREEN_INFO_MENU,     0, 0, EVIST_ADV_MENU,       0, 0 },
        { "04_info_last",       DPC245_APP_SCREEN_INFO_MENU,     0, 0, EVIST_ADV_MENU,       0, 7 },
        { "05_info_battery",    DPC245_APP_SCREEN_INFO_BATTERY,  0, 0, EVIST_ADV_MENU,       0, 0 },
        { "06_info_sensor",     DPC245_APP_SCREEN_INFO_SENSOR,   0, 0, EVIST_ADV_MENU,       0, 0 },
        { "08_adv_light",       DPC245_APP_SCREEN_ADVANCED,      0, 1, EVIST_ADV_LIGHT,      0, 0 },
        { "09_adv_quick_walk",  DPC245_APP_SCREEN_ADVANCED,      0, 1, EVIST_ADV_QUICK_WALK, 0, 0 },
        { "10_adv_diag_p1",     DPC245_APP_SCREEN_ADVANCED,      0, 1, EVIST_ADV_DIAG,       0, 0 },
        { "11_adv_diag_p2",     DPC245_APP_SCREEN_ADVANCED,      0, 1, EVIST_ADV_DIAG,       1, 0 },
        { "12_adv_diag_p3",     DPC245_APP_SCREEN_ADVANCED,      0, 1, EVIST_ADV_DIAG,       2, 0 },
        { "13_adv_diag_p4",     DPC245_APP_SCREEN_ADVANCED,      0, 1, EVIST_ADV_DIAG,       3, 0 },
        { "14_adv_diag_p5",     DPC245_APP_SCREEN_ADVANCED,      0, 1, EVIST_ADV_DIAG,       4, 0 },
        { "14b_adv_diag_temp_warn", DPC245_APP_SCREEN_ADVANCED, 0, 1, EVIST_ADV_DIAG,       5, 0 },
        { "14c_adv_diag_temp_now", DPC245_APP_SCREEN_ADVANCED, 0, 1, EVIST_ADV_DIAG,        6, 0 },
        { "15_info_hmi",        DPC245_APP_SCREEN_INFO_HMI,        0, 0, EVIST_ADV_MENU,   0, 0 },
        { "16_info_controller", DPC245_APP_SCREEN_INFO_CONTROLLER, 0, 0, EVIST_ADV_MENU,   0, 0 },
        { "17_info_errors",     DPC245_APP_SCREEN_INFO_ERRORS,     0, 0, EVIST_ADV_MENU,   0, 0 },
        { "19_edit_auto_off",   DPC245_APP_SCREEN_EDIT_AUTO_OFF,   0, 0, EVIST_ADV_MENU,   0, 0 },
        { "20_edit_unit",       DPC245_APP_SCREEN_EDIT_UNIT,       0, 0, EVIST_ADV_MENU,   0, 0 },
        { "21_adv_wheel",       DPC245_APP_SCREEN_ADVANCED,        0, 1, EVIST_ADV_WHEEL,  0, 0 },
        { "22_adv_assist",      DPC245_APP_SCREEN_ADVANCED,        0, 1, EVIST_ADV_ASSIST, 0, 0 },
        { "23_adv_panels",      DPC245_APP_SCREEN_ADVANCED,        0, 1, EVIST_ADV_PANELS, 0, 0 },
        { "23a_temp_view",      DPC245_APP_SCREEN_ADVANCED,        0, 1, EVIST_ADV_TEMP_VIEW, 0, 0 },
        { "24_info_sensor_p2",  DPC245_APP_SCREEN_INFO_SENSOR,     1, 0, EVIST_ADV_MENU,   0, 0 },
        { "43_adv_mode",        DPC245_APP_SCREEN_ADVANCED,        0, 1, EVIST_ADV_MODE,   0, 0 },
        { "55_adv_battery",     DPC245_APP_SCREEN_ADVANCED,        0, 1, EVIST_ADV_BATTERY, 0, 0 },
    };

    for (size_t i = 0; i < sizeof(frames) / sizeof(frames[0]); ++i) {
        const struct frame *f = &frames[i];
        app.screen = f->screen;
        app.selection = f->selection;
        app.detail_page = f->detail;
        if (f->adv) {
            evist_advanced_open(&app.advanced, &app.preferences, app.assist_level_count ? app.assist_level_count : 5u);
            app.advanced.screen = f->adv_screen;
            app.advanced.diag_page = f->diag_page;
        }
        check_dashboard_zones = f->screen == DPC245_APP_SCREEN_DASHBOARD;
        box_count = 0;
        ui_dashboard_draw(&app, &router, &adc, &hw, 0u, true, true, true, 0u, &status);
        check_layout(f->name);
        dump(f->name);
    }

    app.screen=DPC245_APP_SCREEN_ADVANCED;
    app.advanced.screen=EVIST_ADV_DIAG;app.advanced.diag_page=5u;
    app.temp_warning_activations=4294967295u;app.temp_warning_sources=3u;
    app.temp_warning_last.uptime_s=4294967u;app.temp_warning_last.sources=3u;
    app.temp_warning_last.motor_c=215;app.temp_warning_last.ctrl_c=215;
    app.temp_warning_last.motor_valid=true;app.temp_warning_last.ctrl_valid=true;
    app.temp_warning_last.sample_age_ms=3000u;
    app.temp_warning_last.raw6=255u;app.temp_warning_last.raw7=255u;
    app.temp_warning_last.motor_limit_c=150u;app.temp_warning_last.ctrl_limit_c=150u;
    app.temp_motor_max_valid=true;app.temp_ctrl_max_valid=true;
    app.temp_motor_max_c=215;app.temp_ctrl_max_c=215;
    app.preferences.motor_temp_limit_c=150u;app.preferences.ctrl_temp_limit_c=150u;
    app.advanced.working.motor_temp_limit_c=150u;app.advanced.working.ctrl_temp_limit_c=150u;
    box_count=0u;check_dashboard_zones=0;
    ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
    check_layout("diag_temp_warn_max");dump("diag_temp_warn_max");
    app.advanced.diag_page=6u;
    app.temp_screen_seen=true;app.temp_screen_sources=3u;
    app.temp_screen_uptime_s=0u;app.temp_screen_render_ms=0u;
    app.temp_live[0]=(dpc245_temp_live_source_t){215,150u,4294967295u,1999u,
        true,true,false,false,true,true};
    app.temp_live[1]=(dpc245_temp_live_source_t){-40,150u,3000u,1999u,
        true,false,true,false,true,true};
    router.telemetry.have_motor_temp=true;router.telemetry.motor_temp_c=18;
    router.telemetry.have_controller_temp=true;router.telemetry.controller_temp_c=18;
    box_count=0u;
    ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,4294967000u,&status);
    check_layout("diag_temp_now_max");dump("diag_temp_now_max");
    {
        static const char *const names[4]={"diag_sources_none","diag_sources_motor",
                                          "diag_sources_ctrl","diag_sources_both"};
        for(uint8_t bits=0u;bits<4u;++bits){
            app.temp_warning_sources=bits;app.temp_screen_sources=bits;
            box_count=0u;
            ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,4294967000u,&status);
            check_layout(names[bits]);
        }
    }
    app.temp_warning_activations=0u;app.temp_warning_sources=0u;
    app.preferences.motor_temp_limit_c=0u;app.preferences.ctrl_temp_limit_c=0u;

    app.screen=DPC245_APP_SCREEN_ADVANCED;
    evist_advanced_open(&app.advanced,&app.preferences,5u);
    app.advanced.screen=EVIST_ADV_MODE;
    evist_ride_cfg_t page_ride=evist_ride_defaults();
    page_ride.legal_x100=3200u;page_ride.offroad_x100=7000u;
    page_ride.start=EVIST_RIDE_START_LAST;page_ride.last_mode=EVIST_RIDE_OFFROAD;
    evist_advanced_set_ride(&app.advanced,&page_ride);
    app.advanced.cursor=0u;box_count=0u;
    ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
    check_layout("ride_mode_last_32_kmh");dump("43b_ride_mode_last_32_kmh");
    dpc245_set_unit(&app.runtime.settings,DPC245_UNIT_IMPERIAL);
    app.advanced.cursor=5u;box_count=0u;
    ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
    check_layout("ride_mode_last_32_mph_back");dump("43c_ride_mode_last_32_mph_back");
    dpc245_set_unit(&app.runtime.settings,DPC245_UNIT_METRIC);

    app.screen=DPC245_APP_SCREEN_ADVANCED;
    app.advanced.screen=EVIST_ADV_DIAG;
    app.advanced.diag_page=1u;
    router.telemetry.have_controller_temp=true;
    router.telemetry.controller_temp_c=30;
    router.telemetry.have_motor_temp=true;
    router.telemetry.motor_temp_c=35;
    box_count=0u;
    ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
    check_layout("diag_temperatures");dump("11_adv_diag_p2");
    router.telemetry.have_controller_temp=false;
    router.telemetry.have_motor_temp=false;
    box_count=0u;
    ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
    check_layout("diag_temperatures_missing");dump("diag_temperatures_missing");
    router.telemetry.have_controller_temp=true;
    router.telemetry.have_motor_temp=true;

    app.screen = DPC245_APP_SCREEN_ADVANCED;
    app.advanced.screen = EVIST_ADV_ACCEL;
    app.advanced.accel_mode = EVIST_ACCEL_PER_LEVEL;
    app.advanced.accel_valid = true;
    app.advanced.status = "";
    for (uint8_t i = 0u; i < 9u; ++i) app.advanced.accel_values[i] = (uint8_t)(i % 8u + 1u);
    box_count = 0; check_dashboard_zones = false;
    ui_dashboard_draw(&app, &router, &adc, &hw, 0u, true, true, true, 0u, &status);
    check_layout("accel_per_level"); dump("accel_per_level");
    app.advanced.edit = true;
    app.advanced.accel_dirty = true;
    box_count = 0;
    ui_dashboard_draw(&app, &router, &adc, &hw, 0u, true, true, true, 0u, &status);
    check_layout("accel_per_level_edit"); dump("accel_per_level_edit");
    app.advanced.edit = false;
    app.advanced.accel_dirty = false;
    app.advanced.accel_mode = EVIST_ACCEL_GLOBAL;
    app.advanced.accel_global = 7u;
    box_count = 0;
    ui_dashboard_draw(&app, &router, &adc, &hw, 0u, true, true, true, 0u, &status);
    check_layout("accel_global"); dump("accel_global");
    app.advanced.edit = true;
    app.advanced.accel_dirty = true;
    app.advanced.cursor = 0u;
    box_count = 0;
    ui_dashboard_draw(&app, &router, &adc, &hw, 0u, true, true, true, 0u, &status);
    check_layout("accel_global_edit"); dump("accel_global_edit");
    app.advanced.edit = false;
    app.advanced.accel_dirty = false;
    app.advanced.accel_valid = false;
    app.advanced.status = "P2 NO RESPONSE";
    box_count = 0;
    ui_dashboard_draw(&app, &router, &adc, &hw, 0u, true, true, true, 0u, &status);
    check_layout("accel_error"); dump("accel_error");

    /* Exercise every badge label at the 88.8 speed layout boundary. */
    static const struct { const char *name; uint8_t level; bool offroad; bool walking; } badge_frames[] = {
        { "badge_0", 0u, false, false }, { "badge_E", 1u, false, false },
        { "badge_T", 2u, false, false }, { "badge_S", 3u, false, false },
        { "badge_Splus", 4u, false, false }, { "badge_B", 5u, false, false },
        { "badge_09", 9u, false, false }, { "badge_W", 0u, false, true },
        { "badge_Splus_offroad", 4u, true, false },
        { "badge_0_offroad", 0u, true, false }, { "badge_E_offroad", 1u, true, false },
        { "badge_T_offroad", 2u, true, false }, { "badge_S_offroad", 3u, true, false },
        { "badge_B_offroad", 5u, true, false }, { "badge_09_offroad", 9u, true, false },
        { "badge_W_offroad", 0u, true, true }
    };
    app.screen = DPC245_APP_SCREEN_DASHBOARD;
    router.telemetry.speed_centi_kph = 8880u;
    check_dashboard_zones = 1;
    for (size_t i = 0; i < sizeof(badge_frames) / sizeof(badge_frames[0]); ++i) {
        app.assist_level = badge_frames[i].level;
        app.walk = badge_frames[i].walking;
        app.ride_indicator = badge_frames[i].offroad ? DPC245_RIDE_IND_OFFROAD : DPC245_RIDE_IND_LEGAL;
        check_badge_geometry = 1;
        box_count = 0;
        ui_dashboard_draw(&app, &router, &adc, &hw, 0u, true, true, true, 0u, &status);
        check_layout(badge_frames[i].name);
        check_badge_geometry = 0;
        dump(badge_frames[i].name);
    }
    app.walk = false;
    app.ride_indicator = DPC245_RIDE_IND_LEGAL;

    /* Cover every battery zone and both low-zone pulse phases in the renderer. */
    static const uint8_t bar_levels[]={3u,7u,12u,30u,100u};
    app.preferences.battery_mask=EVIST_BATTERY_ALL;
    for(size_t i=0u;i<sizeof(bar_levels)/sizeof(bar_levels[0]);i++){
        router.telemetry.battery_pct=bar_levels[i];
        for(uint8_t phase=0u;phase<(bar_levels[i]<=5u?2u:1u);phase++){
            const char *phase_name=phase?"off":"on";char name[32];
            snprintf(name,sizeof(name),"bar_%u_%s",bar_levels[i],phase_name);
            box_count=0u;
            ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,phase?600u:0u,&status);
            check_layout(name);dump(name);
        }
    }
    app.preferences.battery_mask=(uint8_t)(app.preferences.battery_mask&~EVIST_BATTERY_BAR);

    app.screen = DPC245_APP_SCREEN_DASHBOARD;
    app.ride_confirm_pending = true;
    check_dashboard_zones = 1;
    check_lower_separator = 0; /* the full-width notice replaces the lower pair */
    box_count = 0;
    ui_dashboard_draw(&app, &router, &adc, &hw, 0u, true, true, true, 0u, &status);
    check_layout("ride_confirm_banner");
    dump("44_ride_confirm_banner");
    evist_ride_cfg_t sim_ride=evist_ride_defaults();sim_ride.mode=EVIST_RIDE_OFFROAD;
    sim_ride.offroad_x100=3500u;sim_ride.start=EVIST_RIDE_START_OFFROAD;
    dpc245_app_set_ride(&app,&sim_ride);
    box_count = 0;
    ui_dashboard_draw(&app, &router, &adc, &hw, 0u, true, true, true, 0u, &status);
    check_layout("ride_back_to_legal_banner");
    dump("44b_ride_back_to_legal_banner");
    app.ride_session = DPC245_RIDE_SESSION_LEGAL;
    app.ride_indicator = DPC245_RIDE_IND_LEGAL;
    app.ride_cfg.legal_x100 = 3150u;
    app.ride_confirm_pending = false;
    app.ride_toast_active = true;
    box_count = 0;
    ui_dashboard_draw(&app, &router, &adc, &hw, 0u, true, true, true, 0u, &status);
    check_layout("ride_legal_toast");
    dump("44c_ride_legal_toast");
    app.ride_session = DPC245_RIDE_SESSION_OFFROAD;
    app.ride_indicator = DPC245_RIDE_IND_OFFROAD;
    box_count = 0;
    ui_dashboard_draw(&app, &router, &adc, &hw, 0u, true, true, true, 0u, &status);
    check_layout("ride_offroad_toast");
    dump("44d_ride_offroad_toast");
    app.ride_session = DPC245_RIDE_SESSION_LEGAL;
    app.ride_indicator = DPC245_RIDE_IND_LEGAL;
    app.ride_toast_active = false;
    app.ride_apply_result = EVIST_RIDE_APPLY_FAILED;
    box_count = 0;
    ui_dashboard_draw(&app, &router, &adc, &hw, 0u, true, true, true, 0u, &status);
    check_layout("ride_apply_failure");
    dump("45_ride_apply_failure");
    app.ride_apply_result = EVIST_RIDE_APPLY_IDLE;
    check_lower_separator = 1;

    app.screen = DPC245_APP_SCREEN_ADVANCED;
    evist_advanced_open(&app.advanced,&app.preferences,5u);
    app.advanced.screen = EVIST_ADV_MODE;
    check_dashboard_zones = 0;
    app.advanced.status = "SAVING DISABLED IN THIS BUILD";
    box_count = 0;
    ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
    check_layout("ride_save_disabled");
    dump("46_ride_save_disabled");
    app.advanced.status = "NOT SAVED - LOG FULL";
    box_count = 0;
    ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
    check_layout("ride_log_full");
    dump("47_ride_log_full");
    app.advanced.status = "";
    app.advanced.screen = EVIST_ADV_DIAG;
    app.advanced.diag_page = 2u;
    app.ride_apply_result = EVIST_RIDE_APPLY_FAILED;
    app.ride_last_error = EVIST_TXN_MISMATCH;
    box_count = 0;
    ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
    check_layout("ride_diag_failed");
    dump("48_ride_diag_failed");
    app.ride_apply_result = EVIST_RIDE_APPLY_IDLE;

    app.preferences.quick_walk_enabled = true;
    app.preferences.quick_walk_hold_steps = 10u;
    app.assist_level = 9u;
    app.ride_cfg.mode = EVIST_RIDE_OFFROAD;
    app.ride_session = DPC245_RIDE_SESSION_OFFROAD;
    app.ride_apply_result = EVIST_RIDE_APPLY_OK;
    box_count = 0;
    ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
    check_layout("ride_diag_full");
    dump("49_ride_diag_full");
    app.ride_apply_result = EVIST_RIDE_APPLY_IDLE;

    /* Layout: exercise every lower pair and each five-level mode at real font size. */
    app.screen = DPC245_APP_SCREEN_DASHBOARD;
    app.ride_indicator = DPC245_RIDE_IND_LEGAL;
    for (uint8_t panel = 0u; panel < EVIST_PANEL_COUNT; ++panel) {
        app.preferences.lower_panel = panel;
        check_dashboard_zones = 1;
        box_count = 0;
        ui_dashboard_draw(&app, &router, &adc, &hw, 0u, true, true, true, 0u, &status);
        char name[32];
        snprintf(name, sizeof(name), "lower_pair_%u", panel);
        check_layout(name);
        char filename[32];
        snprintf(filename, sizeof(filename), "%u_%s", 33u + panel, name);
        dump(filename);
    }
    app.preferences.lower_panel = 0u;
    router.telemetry.have_controller_temp=true;router.telemetry.controller_temp_c=41;
    router.telemetry.have_motor_temp=true;router.telemetry.controller_aux_temp_raw=95u;
    router.telemetry.motor_temp_c=55;
    app.preferences.lower_panel=9u;
    for(unsigned view=0u;view<4u;++view){
        app.preferences.temp_view=(evist_temp_view_t)view;
        box_count=0;
        ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,view==EVIST_TEMP_ALTERNATE?3000u:0u,&status);
        char name[32];snprintf(name,sizeof(name),"temp_view_%u",view);
        check_layout(name);dump(name);
    }
    router.telemetry.controller_aux_temp_raw=0u;
    app.preferences.temp_view=EVIST_TEMP_BOTH;box_count=0;
    ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
    check_layout("temp_missing_motor");dump("temp_missing_motor");
    router.telemetry.controller_aux_temp_raw=95u;
    app.preferences.lower_panel=0u;app.preferences.temp_view=EVIST_TEMP_BOTH;
    uint8_t saved_battery_mask = app.preferences.battery_mask;
    app.preferences.battery_mask = EVIST_BATTERY_ALL;
    status = (dpc245_ui_status_t){true,true,true};
    check_dashboard_zones = 1;
    box_count = 0;
    ui_dashboard_draw(&app, &router, &adc, &hw, 0u, true, true, true, 0u, &status);
    check_layout("status_all_on");
    dump("42_status_all_on");
    status = (dpc245_ui_status_t){0};
    app.preferences.battery_mask = saved_battery_mask;
    {
        static const uint16_t speeds[]={0u,550u,990u,1000u,2550u,8880u};
        static const char *const speed_names[]={"0_0","5_5","9_9","10_0","25_5","88_8"};
        app.assist_level=4u;app.ride_indicator=DPC245_RIDE_IND_OFFROAD;
        for(unsigned bar=0u;bar<2u;bar++){
          app.preferences.battery_mask=(uint8_t)((saved_battery_mask&~EVIST_BATTERY_BAR)|(bar?EVIST_BATTERY_BAR:0u));
          for(unsigned i=0u;i<6u;i++){
            router.telemetry.speed_centi_kph=speeds[i];box_count=0;
            check_badge_geometry=1;
            check_speed_cells=1;speed_cell_seen=0;
            ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
            assert(speed_cell_seen==1);check_speed_cells=0;
            char name[40];snprintf(name,sizeof(name),"speed_%s_bar%u",speed_names[i],bar);
            check_layout(name);check_badge_geometry=0;dump(name);
          }
        }
        assert(speed_cell_frames==12u);
        app.preferences.battery_mask=saved_battery_mask|EVIST_BATTERY_BAR;
        for(uint8_t panel=0u;panel<EVIST_PANEL_COUNT;panel++){
            app.preferences.lower_panel=panel;box_count=0;
            ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
            char name[32];snprintf(name,sizeof(name),"bar_lower_pair_%u",panel);
            check_layout(name);dump(name);
        }
        app.preferences.lower_panel=0u;app.ride_confirm_pending=true;check_lower_separator=0;
        box_count=0;ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
        check_layout("bar_ride_confirm_banner");dump("bar_ride_confirm_banner");
        app.ride_confirm_pending=false;app.ride_toast_active=true;
        box_count=0;ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
        check_layout("bar_ride_toast");dump("bar_ride_toast");
        app.ride_toast_active=false;check_lower_separator=1;
        app.preferences.lower_panel=0u;app.preferences.battery_mask=saved_battery_mask;
        app.assist_level=1u;app.ride_indicator=DPC245_RIDE_IND_LEGAL;
        router.telemetry.speed_centi_kph=2180u;
    }
    {
        static const uint8_t levels[] = { 0u, 1u, 2u, 3u, 4u, 5u };
        static const char *const names[] = { "mode_0", "mode_E", "mode_T", "mode_S", "mode_Splus", "mode_B" };
        for (unsigned i = 0u; i < sizeof(levels) / sizeof(levels[0]); ++i) {
            app.assist_level = levels[i];
            app.ride_indicator = DPC245_RIDE_IND_LEGAL;
            check_dashboard_zones = 1;
            box_count = 0;
            ui_dashboard_draw(&app, &router, &adc, &hw, 0u, true, true, true, 0u, &status);
            check_layout(names[i]);
            char filename[32];
            snprintf(filename, sizeof(filename), "%u_%s", 26u + i, names[i]);
            dump(filename);
        }
        app.assist_level = 4u;
        app.ride_indicator = DPC245_RIDE_IND_OFFROAD;
        check_dashboard_zones = 1;
        box_count = 0;
        ui_dashboard_draw(&app, &router, &adc, &hw, 0u, true, true, true, 0u, &status);
        check_layout("mode_Splus_offroad");
        dump("32_mode_Splus_offroad");
        app.assist_level = 1u;
    }
    /* Check every badge label against the enlarged curved inner edge with
     * the power bar enabled, for both ride modes. */
    {
        static const uint8_t levels[] = { 0u, 1u, 2u, 3u, 4u, 5u, 9u };
        static const char *const labels[] = { "0", "E", "T", "S", "Splus", "B", "09" };
        uint8_t saved_mask=app.preferences.battery_mask;
        app.preferences.battery_mask=(uint8_t)(saved_mask|EVIST_BATTERY_BAR);
        for(unsigned road=0u;road<2u;++road){
            app.ride_indicator=road?DPC245_RIDE_IND_OFFROAD:DPC245_RIDE_IND_LEGAL;
            for(unsigned i=0u;i<sizeof(levels)/sizeof(levels[0]);++i){
                app.assist_level=levels[i];app.walk=false;app.walk_active=false;
                box_count=0;check_badge_geometry=1;
                ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
                char name[40];snprintf(name,sizeof(name),"b9_%s_%s_bar",labels[i],road?"offroad":"legal");
                check_layout(name);dump(name);
            }
            app.assist_level=0u;app.walk=true;app.walk_active=true;
            box_count=0;check_badge_geometry=1;
            ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
            char name[40];snprintf(name,sizeof(name),"b9_W_%s_bar",road?"offroad":"legal");
            check_layout(name);dump(name);
        }
        check_badge_geometry=0;app.walk=false;app.walk_active=false;
        app.assist_level=1u;app.ride_indicator=DPC245_RIDE_IND_LEGAL;
        app.preferences.battery_mask=saved_mask;
    }
    app.walk_armed=true;app.walk_active=true;app.walk=true;app.assist_level=0u;
    app.down_held=false;router.telemetry.have_speed=true;router.telemetry.speed_centi_kph=0u;
    router.telemetry.have_controller_status_bit0=true;router.telemetry.controller_status_bit0=true;
    router.telemetry.controller_fast2_rx_ms=0u;
    box_count=0;ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
    check_layout("walk_amber_bit0_speed_0");dump("54_walk_amber_bit0_speed_0");
    router.telemetry.controller_status_bit0=false;
    box_count=0;ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,501u,&status);
    check_layout("walk_white_stale_no_down");dump("54b_walk_white_stale_no_down");
    router.telemetry.controller_status_bit0=true;
    app.down_held=true;app.ride_indicator=DPC245_RIDE_IND_LEGAL;router.telemetry.speed_centi_kph=550u;
    box_count=0;ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
    check_layout("walk_amber_speed_5_5");dump("55_walk_amber_speed_5_5");
    router.telemetry.speed_centi_kph=2550u;
    box_count=0;ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
    check_layout("walk_amber_speed_25_5");dump("57_walk_amber_speed_25_5");
    app.walk=false;app.walk_active=false;app.walk_armed=true;
    box_count=0;ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
    check_layout("level1_down_held_walk_off");dump("56_level1_down_held_walk_off");
    app.walk_armed=false;app.down_held=false;app.walk_active=false;router.telemetry.speed_centi_kph=2180u;

    /* Power bar smoothing snapshots: 0 -> 80% -> 20%, with intermediate
     * states rendered after 100 ms and after settling. */
    {
        app.power_graph_w=0u;app.power_graph_tick_ms=1000u;
        box_count=0;ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
        check_layout("power_seq_0");dump("power_seq_0");
        dpc245_app_power_graph_tick(&app,800,1100u);
        assert(app.power_graph_w > 0u && app.power_graph_w < 800u);
        box_count=0;ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
        check_layout("power_seq_80_t100ms");dump("power_seq_80_t100ms");
        for(uint32_t now=1200u;now<=3200u;now+=100u)dpc245_app_power_graph_tick(&app,800,(uint32_t)now);
        assert(app.power_graph_w > 790u && app.power_graph_w <= 800u);
        box_count=0;ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
        check_layout("power_seq_80_settled");dump("power_seq_80_settled");
        dpc245_app_power_graph_tick(&app,200,3300u);
        assert(app.power_graph_w < 800u && app.power_graph_w > 200u);
        box_count=0;ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
        check_layout("power_seq_20_t100ms");dump("power_seq_20_t100ms");
        for(uint32_t now=3400u;now<=5400u;now+=100u)dpc245_app_power_graph_tick(&app,200,(uint32_t)now);
        assert(app.power_graph_w >= 200u && app.power_graph_w < 210u);
        box_count=0;ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
        check_layout("power_seq_20_settled");dump("power_seq_20_settled");
        app.power_graph_w=234u;app.power_graph_tick_ms=0u;
    }

    /* OFFROAD at 90 % of the power scale: tag, frame and all four colour zones. */
    app.screen = DPC245_APP_SCREEN_DASHBOARD;
    app.ride_indicator = DPC245_RIDE_IND_OFFROAD;
    router.telemetry.current_centi_amp = 2459;   /* 36.60 V x 24.59 A = 900 W */
    app.power_graph_w = 900u;
    box_count = 0;
    ui_dashboard_draw(&app, &router, &adc, &hw, 0u, true, true, true, 0u, &status);
    check_layout("25_dashboard_offroad");
    dump("25_dashboard_offroad");

    /* DIAG timing page with the largest values (10 digits). */
    app.screen = DPC245_APP_SCREEN_ADVANCED;
    evist_advanced_open(&app.advanced, &app.preferences, app.assist_level_count ? app.assist_level_count : 5u);
    app.advanced.screen = EVIST_ADV_DIAG;
    app.advanced.diag_page = 3u;
    app.loop_max_us = 4294967295u;
    app.loop_1s_max_us = 4294967295u;
    for (uint8_t i = 0u; i < 8u; ++i) app.stage_max_us[i] = 999999u;
    app.stage_max_us[7] = 1000000u; /* The overflow marker must also fit. */
    check_dashboard_zones = 0;
    box_count = 0;
    ui_dashboard_draw(&app, &router, &adc, &hw, 0u, true, true, true, 0u, &status);
    check_layout("diag_p4_max_values");
    app.loop_max_us = 3850u;
    app.loop_1s_max_us = 1200u;
    app.screen = DPC245_APP_SCREEN_DASHBOARD;

    /* Layout: battery group, all 8 masks x 4 data states, layout-checked
     * (no images written). */
    {
        static const char *const state_name[4] = { "normal", "nodata", "soc150", "volt655" };
        app.ride_indicator = DPC245_RIDE_IND_NONE;
        for (unsigned mask = 0u; mask < 8u; ++mask)
            for (unsigned st = 0u; st < 4u; ++st) {
                bafang_telemetry_t *t = &router.telemetry;
                sample_telemetry(&router);
                if (st == 1u) { t->have_battery_pct = false; t->have_voltage = false; }
                if (st == 2u) t->battery_pct = 150u;
                if (st == 3u) t->voltage_centi_volt = 65535u;
                app.preferences.battery_mask = (uint8_t)mask;
                char name[48];
                snprintf(name, sizeof(name), "battery_mask%u_%s", mask, state_name[st]);
                box_count = 0;
                ui_dashboard_draw(&app, &router, &adc, &hw, 0u, true, true, true, 0u, &status);
                check_layout(name);
            }
        sample_telemetry(&router);
        app.preferences.battery_mask = EVIST_BATTERY_ALL;
    }

    /* Four layouts, the speed endpoints and middle values, both ride states,
     * three power levels, and the selected mode set. */
    {
        static const uint16_t speeds[]={0u,550u,2550u,8880u};
        static const uint8_t levels[]={0u,1u,4u,0u};
        static const char *const modes[]={"0","E","Splus","W"};
        static const char *const themes[]={"standard","yellow","arc","wedge"};
        uint64_t worst_us[4]={0u,0u,0u,0u};
        app.screen=DPC245_APP_SCREEN_DASHBOARD;
        status.light_on=true;status.ble_online=true;
        sample_telemetry(&router);
        for(uint8_t theme=0u;theme<4u;theme++){
            dpc245_set_theme(&app.runtime.settings,theme);
            for(uint8_t speed=0u;speed<4u;speed++)for(uint8_t road=0u;road<2u;road++)
                for(uint8_t power=0u;power<5u;power++)for(uint8_t mode=0u;mode<4u;mode++){
                    router.telemetry.speed_centi_kph=speeds[speed];
                    app.ride_indicator=road?DPC245_RIDE_IND_OFFROAD:DPC245_RIDE_IND_LEGAL;
                    app.power_graph_w=(uint32_t)power*250u;
                    app.assist_level=levels[mode];app.walk=mode==3u;
                    char name[96];snprintf(name,sizeof(name),"theme_%s_s%u_%s_p%u_%s",
                        themes[theme],(unsigned)speeds[speed],road?"offroad":"legal",
                        (unsigned)power,modes[mode]);
                    box_count=0;check_dashboard_zones=1;
                    clock_t begin=clock();
                    for(unsigned rep=0u;rep<50u;rep++){
                        box_count=0;
                        ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
                    }
                    clock_t end=clock();
                    uint64_t elapsed=(uint64_t)(end-begin)*1000000u/(uint64_t)CLOCKS_PER_SEC/50u;
                    if(elapsed>worst_us[theme])worst_us[theme]=elapsed;
                    check_layout(name);dump(name);
                }
            app.walk=false;app.assist_level=1u;app.ride_indicator=DPC245_RIDE_IND_LEGAL;
            app.power_graph_w=500u;router.telemetry.speed_centi_kph=2550u;
            char name[48];
            app.ride_toast_active=true;box_count=0;check_dashboard_zones=1;
            ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
            snprintf(name,sizeof(name),"theme_%s_toast",themes[theme]);check_layout(name);dump(name);
            app.ride_toast_active=false;app.ride_apply_result=EVIST_RIDE_APPLY_FAILED;
            status.controller_error=true;
            box_count=0;ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
            snprintf(name,sizeof(name),"theme_%s_error",themes[theme]);check_layout(name);dump(name);
            app.ride_apply_result=EVIST_RIDE_APPLY_OK;status.controller_error=false;
            app.ride_confirm_pending=true;box_count=0;check_lower_separator=0;
            ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
            snprintf(name,sizeof(name),"theme_%s_confirm",themes[theme]);check_layout(name);dump(name);
            app.ride_confirm_pending=false;check_lower_separator=1;
        }
        for(uint8_t theme=0u;theme<4u;theme++)
            printf("theme %s worst host draw: %llu us\n",themes[theme],
                   (unsigned long long)worst_us[theme]);
        dpc245_set_theme(&app.runtime.settings,DPC245_THEME_STANDARD);
        app.walk=false;status=(dpc245_ui_status_t){0};
    }

    /* THEMES selection and result text, then every pair in both unit systems. */
    app.screen=DPC245_APP_SCREEN_THEMES_MENU;
    dpc245_set_theme(&app.runtime.settings,DPC245_THEME_STANDARD);
    for(uint8_t selected=0u;selected<6u;selected++)for(uint8_t pending=0u;pending<2u;pending++){
        app.selection=selected;app.pending_theme=pending?DPC245_THEME_WEDGE:DPC245_THEME_STANDARD;
        app.stored_theme=DPC245_THEME_STANDARD;
        app.theme_save_failed=pending!=0u;
        dpc245_set_theme(&app.runtime.settings,app.pending_theme);
        app.theme_status=0;box_count=0;check_dashboard_zones=0;
        ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
        char name[48];snprintf(name,sizeof(name),"themes_row%u_pending%u",selected,pending);
        check_layout(name);dump(name);
    }
    app.theme_save_failed=false;
    static const char *const statuses[]={"SAVE IN PROGRESS","SAVED","NO CHANGE",
        "NO MEMORY - NOT SAVED","SAVING DISABLED IN THIS BUILD"};
    for(uint8_t i=0u;i<5u;i++){
        app.selection=4u;app.theme_status=statuses[i];box_count=0;
        ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
        char name[48];snprintf(name,sizeof(name),"themes_status%u",i);
        check_layout(name);dump(name);
    }
    app.theme_status=0;app.screen=DPC245_APP_SCREEN_DASHBOARD;
    for(uint8_t theme=DPC245_THEME_ARC;theme<=DPC245_THEME_WEDGE;theme++)
        for(uint8_t panel=0u;panel<EVIST_PANEL_COUNT;panel++)
            for(uint8_t unit=0u;unit<2u;unit++)
                for(uint8_t unknown=0u;unknown<2u;unknown++){
                    dpc245_set_theme(&app.runtime.settings,theme);
                    dpc245_set_unit(&app.runtime.settings,unit);
                    app.preferences.lower_panel=panel;
                    sample_telemetry(&router);
                    if(!unknown){
                        dpc245_set_u32_le(&app.runtime.settings,DPC245_OFS_TRIP_DISTANCE_RAW,99990u);
                        dpc245_set_u32_le(&app.runtime.settings,DPC245_OFS_TOTAL_DISTANCE_RAW,99990u);
                        dpc245_set_u16_le(&app.runtime.settings,DPC245_OFS_MAX_SPEED_RAW,9999u);
                        dpc245_set_u16_le(&app.runtime.settings,DPC245_OFS_AVG_SPEED_RAW,9999u);
                        router.telemetry.range_centi_km=65534u;
                        router.telemetry.voltage_centi_volt=9999u;
                        router.telemetry.cadence_rpm=255u;
                        router.telemetry.torque_signal_mv=9999u;
                    }
                    if(unknown){router.telemetry.have_range=false;router.telemetry.have_voltage=false;
                        router.telemetry.have_controller_temp=false;router.telemetry.have_motor_temp=false;}
                    box_count=0;check_dashboard_zones=1;
                    ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,0u,&status);
                    char name[64];snprintf(name,sizeof(name),"theme_%s_pair%u_%s_%s",
                        theme==DPC245_THEME_ARC?"arc":"wedge",panel,unit?"mph":"kmh",unknown?"unknown":"known");
                    check_layout(name);dump(name);
                }
    dpc245_set_theme(&app.runtime.settings,DPC245_THEME_STANDARD);
    dpc245_set_unit(&app.runtime.settings,0u);
    dpc245_set_u32_le(&app.runtime.settings,DPC245_OFS_TRIP_DISTANCE_RAW,0u);
    dpc245_set_u32_le(&app.runtime.settings,DPC245_OFS_TOTAL_DISTANCE_RAW,0u);
    app.preferences.lower_panel=0u;check_dashboard_zones=0;

    /* Temperature warning: each source combination and RIDE priority in every theme/unit. */
    {
        static const char *const themes[4]={"standard","yellow","arc","wedge"};
        static const char *const states[5]={"none","motor","ctrl","both","ride"};
        app.screen=DPC245_APP_SCREEN_DASHBOARD;app.assist_level=1u;
        app.power_graph_w=200u;app.ride_indicator=DPC245_RIDE_IND_LEGAL;
        sample_telemetry(&router);router.telemetry.speed_centi_kph=2550u;
        app.temp_motor_c=126;app.temp_ctrl_c=111;app.temp_warning_text_until_ms=4000u;
        for(uint8_t theme=0u;theme<4u;++theme)for(uint8_t unit=0u;unit<2u;++unit)
            for(uint8_t state=0u;state<5u;++state){
                dpc245_set_theme(&app.runtime.settings,theme);
                dpc245_set_unit(&app.runtime.settings,unit);
                app.temp_warning_sources=state==0u?0u:state==1u?1u:state==2u?2u:3u;
                app.ride_toast_active=state==4u;
                char name[64];snprintf(name,sizeof(name),"temp_warn_%s_%s_%s",themes[theme],
                                       states[state],unit?"mph":"kmh");
                box_count=0u;check_dashboard_zones=1;
                ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,1000u,&status);
                check_layout(name);dump(name);
            }
        app.ride_toast_active=false;app.temp_warning_sources=0u;
        dpc245_set_theme(&app.runtime.settings,DPC245_THEME_STANDARD);
        dpc245_set_unit(&app.runtime.settings,DPC245_UNIT_METRIC);
        app.screen=DPC245_APP_SCREEN_ADVANCED;
        evist_advanced_open(&app.advanced,&app.preferences,5u);
        app.advanced.screen=EVIST_ADV_TEMP_VIEW;
        app.advanced.working.motor_temp_limit_c=150u;
        app.advanced.working.ctrl_temp_limit_c=40u;
        for(uint8_t row=0u;row<5u;++row)for(uint8_t edit=0u;edit<2u;++edit){
            app.advanced.cursor=row;app.advanced.edit=edit!=0u;
            char name[40];snprintf(name,sizeof(name),"temp_limit_row%u_edit%u",row,edit);
            box_count=0u;check_dashboard_zones=0;
            ui_dashboard_draw(&app,&router,&adc,&hw,0u,true,true,true,1000u,&status);
            check_layout(name);dump(name);
        }
        app.advanced.edit=false;
        app.screen=DPC245_APP_SCREEN_DASHBOARD;
        check_dashboard_zones=0;
    }

    ui_boot_splash_draw();
    dump("14_boot_splash");
    if(arc_frame_colours_seen!=3u){printf("LAYOUT: both ARC frame colours were not checked\n");layout_issues++;}
    printf("layout issues: %u\n", layout_issues);
    return layout_issues ? 1 : 0;
}
