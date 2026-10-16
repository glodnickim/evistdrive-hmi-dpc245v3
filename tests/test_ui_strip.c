/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Minimal screen update (ui_strip): the tile-composed panel must equal a direct
 * draw pixel for pixel, an unchanged frame must send nothing, and a one-digit
 * change must send only the tiles under that digit. Uses the real screen code
 * with the host panel (sim/host_lcd.c). */

#include "dpc245_app.h"
#include "dpc245_key_sampler.h"
#include "ui_dashboard.h"
#include "ui_strip.h"
#include "can_router.h"
#include "advanced_settings.h"
#include "dpc245_settings.h"
#include "dpc245_hwinfo.h"
#include "dpc245_adc.h"
#include "st7789_8080.h"
#include "ui_font1.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

extern uint16_t sim_fb[DPC245_LCD_HEIGHT][DPC245_LCD_WIDTH];
static uint16_t direct[DPC245_LCD_HEIGHT][DPC245_LCD_WIDTH];

static dpc245_app_t app;
static evist_can_router_t router;
static dpc245_adc_snapshot_t adc;
static dpc245_hwinfo_t hw;
typedef struct {
    dpc245_app_t app;
    evist_can_router_t router;
    dpc245_adc_snapshot_t adc;
    dpc245_hwinfo_t hw;
} frame_state_t;
static frame_state_t frame_state;
static dpc245_ui_status_t status;
static uint32_t test_now_ms;
static unsigned captured_strips;

static void draw(void *ctx) {
    if(ui_strip_capturing())captured_strips++;
    const frame_state_t *s = (const frame_state_t *)ctx;
    ui_dashboard_draw(s ? &s->app : &app, s ? &s->router : &router,
                      s ? &s->adc : &adc, s ? &s->hw : &hw, 0u, true, true, true, test_now_ms, &status);
}
static void snapshot_frame(void) {
    frame_state.app = app;
    frame_state.router = router;
    frame_state.adc = adc;
    frame_state.hw = hw;
}

static uint16_t touched_x0, touched_x1, touched_y0, touched_y1;
static bool count_pushes;
static bool check_walk_tiles;
static bool walk_tiles_valid;
static uint16_t pushes_per_strip[UI_STRIPS];
static void push(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *px, uint16_t stride) {
    if (check_walk_tiles) {
        bool in_square = x + w <= 80u && y >= 50u && y + h <= 110u;
        if (!in_square) walk_tiles_valid = false;
    }
    if (count_pushes) ++pushes_per_strip[y / UI_STRIP_H];
    if (x < touched_x0) touched_x0 = x;
    if (y < touched_y0) touched_y0 = y;
    if (x + w > touched_x1) touched_x1 = (uint16_t)(x + w);
    if (y + h > touched_y1) touched_y1 = (uint16_t)(y + h);
    dpc245_lcd_set_window(x, y, (uint16_t)(x + w - 1u), (uint16_t)(y + h - 1u));
    for (uint16_t r = 0u; r < h; ++r)
        for (uint16_t c = 0u; c < w; ++c) dpc245_lcd_data16(px[(uint32_t)r * stride + c]);
}

static void reset_touched(void) { touched_x0 = touched_y0 = 0xFFFFu; touched_x1 = touched_y1 = 0u; }

static unsigned mode_ring_pixels(uint16_t colour) {
    unsigned count = 0u;
    for (unsigned y = 49u; y < 109u; ++y)
        for (unsigned x = 6u; x < 76u; ++x)
            if (sim_fb[y][x] == colour) ++count;
    return count;
}
static unsigned badge_pixels(uint16_t colour) {
    unsigned count=0u;
    for(unsigned y=50u;y<110u;y++)for(unsigned x=7u;x<77u;x++)if(direct[y][x]==colour)count++;
    return count;
}

/* Direct draw of the current state as the reference image. The panel (sim_fb)
 * is saved and restored, so it keeps exactly what the tiles put there. */
static uint16_t panel_save[DPC245_LCD_HEIGHT][DPC245_LCD_WIDTH];
static void reference(void) {
    memcpy(panel_save, sim_fb, sizeof(panel_save));
    draw(0);
    memcpy(direct, sim_fb, sizeof(direct));
    memcpy(sim_fb, panel_save, sizeof(panel_save));
}

static void assert_standard_warning_text(const char *expected) {
    memset(sim_fb,0,sizeof(sim_fb));
    (void)ui_font1_text(&ui_font1_dashboard,8u,156,expected,0xF800u,0u);
    for(unsigned y=145u;y<170u;++y)
        for(unsigned x=8u;x<288u;++x)
            assert((direct[y][x]==0xF800u)==(sim_fb[y][x]==0xF800u));
    memcpy(sim_fb,panel_save,sizeof(panel_save));
}

static uint16_t render(void) { reset_touched(); return ui_strip_render(draw, 0, push); }

typedef struct {
    bool gated,redraw,physical_down;
    unsigned passes,can_service,ble_service,store_service,processed,displayed;
    unsigned max_strips;
    uint32_t order[16];
} navigation_model_t;

static void navigation_pass(dpc245_key_sampler_t *keys,navigation_model_t *model){
    bool nav=false;
    dpc245_key_sample_event_t event;
    model->passes++;
    model->can_service++;
    model->ble_service++;
    model->store_service++;
    while(!model->gated && dpc245_key_sampler_pop(keys,&event)){
        model->redraw=true;
        if(event.events&DPC245_KEY_EVENT_DOWN_RELEASE)
            dpc245_app_event(&app,DPC245_KEY_EVENT_DOWN_RELEASE,event.time_ms);
        uint32_t action=event.events&(DPC245_KEY_EVENT_DOWN_SHORT|
            DPC245_KEY_EVENT_UP_SHORT|DPC245_KEY_EVENT_POWER_SHORT);
        if(action){
            assert(model->displayed==model->processed);
            assert(model->processed<sizeof model->order/sizeof model->order[0]);
            model->order[model->processed++]=action;
            dpc245_app_event(&app,(dpc245_key_event_t)action,event.time_ms);
            model->gated=nav=true;
            break;
        }
    }
    dpc245_app_down_state(&app,model->physical_down,model->passes*13u);
    if(nav){
        model->redraw=true;
        if(ui_strip_render_active())ui_strip_render_cancel();
    }
    if(!ui_strip_render_active()&&model->redraw){
        snapshot_frame();
        ui_strip_render_begin(draw,&frame_state,push);
        model->redraw=false;
    }
    captured_strips=0u;
    if(ui_strip_render_active()){
        (void)ui_strip_render_step(3u);
        assert(captured_strips<=3u);
        if(captured_strips>model->max_strips)model->max_strips=captured_strips;
        if(model->gated&&!ui_strip_render_active()){
            reference();
            assert(memcmp(sim_fb,direct,sizeof(direct))==0);
            model->displayed++;
            model->gated=false;
        }
    }
}

/* A blit clipped at the right and bottom edges keeps its source stride. */
static const uint16_t edge_px[4] = { 0x1111u, 0x2222u, 0x3333u, 0x4444u };   /* 2 x 2 */
static void draw_edge(void *ctx) {
    (void)ctx;
    dpc245_lcd_fill(0x0000u);
    dpc245_lcd_blit_rgb565(319u, 100u, 2u, 2u, edge_px);   /* only column 0 visible */
    dpc245_lcd_blit_rgb565(10u, 169u, 2u, 2u, edge_px);    /* only row 0 visible */
}

int main(void) {
    char limit_text[6];
    ui_dashboard_temp_limit_text(limit_text,40u);assert(strcmp(limit_text,"40 C")==0);
    ui_dashboard_temp_limit_text(limit_text,99u);assert(strcmp(limit_text,"99 C")==0);
    ui_dashboard_temp_limit_text(limit_text,100u);assert(strcmp(limit_text,"100 C")==0);
    ui_dashboard_temp_limit_text(limit_text,150u);assert(strcmp(limit_text,"150 C")==0);
    ui_dashboard_temp_limit_text(limit_text,0u);assert(strcmp(limit_text,"OFF")==0);
    puts("TEMP VIEW limit text: 40, 99, 100, 150 C and OFF passed");

    dpc245_settings_t settings;
    dpc245_settings_defaults(&settings);
    dpc245_app_init(&app, &settings);
    evist_can_router_init(&router);
    bafang_telemetry_t *t = &router.telemetry;
    t->have_speed = true; t->speed_centi_kph = 2180;
    t->have_voltage = true; t->voltage_centi_volt = 3660;
    t->have_battery_pct = true; t->battery_pct = 76;
    t->have_controller_status_bit0=true;t->controller_status_bit0=true;t->controller_fast2_rx_ms=100u;
    adc.valid = true; adc.als_raw = 2140;
    app.assist_level = 1u;
    app.screen = DPC245_APP_SCREEN_DASHBOARD;

    /* The power model advances in tick time, remains between samples, and
     * decays without crossing the measured target. */
    app.power_graph_w=0u;app.power_graph_tick_ms=1000u;
    dpc245_app_power_graph_tick(&app,800,1000u);
    assert(app.power_graph_w==0u);
    dpc245_app_power_graph_tick(&app,800,1100u);
    uint32_t power_80_a=app.power_graph_w;
    dpc245_app_power_graph_tick(&app,800,1200u);
    uint32_t power_80_b=app.power_graph_w;
    assert(power_80_a>0u && power_80_b>power_80_a && power_80_b<800u);
    dpc245_app_power_graph_tick(&app,200,1300u);
    uint32_t power_20_a=app.power_graph_w;
    dpc245_app_power_graph_tick(&app,200,1400u);
    assert(power_20_a<power_80_b && app.power_graph_w<power_20_a && app.power_graph_w>200u);
    printf("power smoothing 0->80%%->20%%: %u, %u, %u, %u W\n",
           (unsigned)power_80_a,(unsigned)power_80_b,(unsigned)power_20_a,(unsigned)app.power_graph_w);
    app.power_graph_w=0u;app.power_graph_tick_ms=0u;

    /* Controller Walk status is valid through 500 ms, then expires. */
    assert(bafang_walk_bit_fresh(t,600u));
    assert(!bafang_walk_bit_fresh(t,601u));
    test_now_ms=600u;app.walk=true;app.assist_level=0u;
    reference();
    assert(badge_pixels(0xFFFFu)>0u);
    assert(badge_pixels(0xFDE9u)>0u);
    test_now_ms=601u;reference();
    assert(badge_pixels(0xFDE9u)==0u);
    app.walk=false;reference();assert(badge_pixels(0xFFFFu)>0u);
    app.assist_level=1u;test_now_ms=0u;

    /* 1. First frame after invalidate: every tile is sent, result equals direct draw. */
    app.screen = DPC245_APP_SCREEN_DASHBOARD;
    reference();
    memset(sim_fb, 0x5A, sizeof(sim_fb));             /* unknown panel content */
    ui_strip_invalidate();
    uint16_t sent = render();
    assert(sent == UI_STRIPS * UI_TILES_X);
    assert(memcmp(sim_fb, direct, sizeof(direct)) == 0);

    /* 2. Nothing changed: nothing is sent. */
    assert(render() == 0u);

    /* 3. One digit changes (21.8 -> 21.9): only tiles under the speed are sent. */
    t->speed_centi_kph = 2190;
    reference();
    sent = render();
    assert(sent > 0u && sent <= 20u);                  /* enlarged tenths digit, not the screen */
    assert(touched_x0 >= 240u && touched_x1 <= 288u);  /* right aligned decimal group */
    assert(touched_y0 >= 80u && touched_y1 <= 130u);
    assert(memcmp(sim_fb, direct, sizeof(direct)) == 0);
    printf("one digit change: %u tiles (%u x %u px area)\n", sent,
           (unsigned)(touched_x1 - touched_x0), (unsigned)(touched_y1 - touched_y0));

    /* 3a. A mode change repaints only the mode badge tiles. */
    app.assist_level = 2u;
    reference();
    reset_touched();
    sent = render();
    assert(sent > 0u && sent <= 40u);
    assert(touched_x1 <= 80u);
    assert(touched_y0 >= 40u && touched_y1 <= 110u);
    assert(memcmp(sim_fb, direct, sizeof(direct)) == 0);
    printf("mode change: %u tiles (%u x %u px area)\n", sent,
           (unsigned)(touched_x1 - touched_x0), (unsigned)(touched_y1 - touched_y0));
    app.assist_level = 1u;
    app.screen = DPC245_APP_SCREEN_DASHBOARD;
    reference();
    (void)render();
    assert(memcmp(sim_fb, direct, sizeof(direct)) == 0);

    /* The orange ride ring is present only in OFFROAD mode. */
    app.ride_indicator = DPC245_RIDE_IND_LEGAL;
    reference(); (void)render();
    assert(mode_ring_pixels(0xFD20u) == 0u);
    app.ride_indicator = DPC245_RIDE_IND_OFFROAD;
    reference(); (void)render();
    assert(mode_ring_pixels(0xFD20u) > 0u);
    app.ride_indicator = DPC245_RIDE_IND_LEGAL;
    reference(); (void)render();

    /* W changes from white to amber with unchanged speed
     * digits. Only mode-square tiles may be sent; output matches full draw. */
    app.ride_indicator = DPC245_RIDE_IND_LEGAL;
    app.assist_level = 0u;
    app.walk = true; app.walk_active = true; app.walk_armed = true; app.down_held = true;
    t->speed_centi_kph = 2550u;
    reference(); (void)render();
    app.down_held = false;
    reference(); reset_touched(); walk_tiles_valid = true; check_walk_tiles = true;
    sent = render();
    check_walk_tiles = false;
    assert(sent > 0u && walk_tiles_valid);
    assert(memcmp(sim_fb, direct, sizeof(direct)) == 0);
    printf("Walk amber to white: %u local tiles\n", sent);
    app.walk = false; app.walk_active = false; app.walk_armed = false; app.down_held = false;
    reference(); (void)render();
    assert(memcmp(sim_fb, direct, sizeof(direct)) == 0);

    /* SOC changes with BAR enabled repaint only the narrow bar. */
    app.preferences.battery_mask = (uint8_t)((app.preferences.battery_mask|EVIST_BATTERY_BAR)&
        (uint8_t)~(EVIST_BATTERY_SOC|EVIST_BATTERY_ICON));
    t->battery_pct=35u;reference();(void)render();
    t->battery_pct=36u;reference();reset_touched();sent=render();
    assert(sent>0u && touched_x0==0u && touched_x1<=16u && touched_y0<=168u && touched_y1>=2u);
    assert(memcmp(sim_fb,direct,sizeof(direct))==0);
    printf("battery bar change: %u tiles (%u x %u px area)\n",sent,
           (unsigned)(touched_x1-touched_x0),(unsigned)(touched_y1-touched_y0));
    app.preferences.battery_mask &= (uint8_t)~EVIST_BATTERY_BAR;
    reference();(void)render();assert(memcmp(sim_fb,direct,sizeof(direct))==0);

    /* Shared battery zones and synchronized low phase across the widget and BAR. */
    app.preferences.battery_mask = EVIST_BATTERY_ICON|EVIST_BATTERY_SOC|EVIST_BATTERY_VOLT|EVIST_BATTERY_BAR;
    test_now_ms=0u;t->battery_pct=3u;reference();(void)render();
    assert(direct[166][3]==0xF800u && direct[3][3]==0u && direct[11][240]==0xF800u);
    uint16_t voltage_pixels[25][71];
    for(unsigned y=0u;y<25u;y++)for(unsigned x=0u;x<71u;x++)voltage_pixels[y][x]=direct[y+8u][x+160u];
    unsigned red_percent_pixels=0u;
    for(unsigned y=8u;y<32u;y++)for(unsigned x=100u;x<190u;x++)if(direct[y][x]==0xF800u)red_percent_pixels++;
    assert(red_percent_pixels>0u);
    test_now_ms=599u;reference();(void)render();assert(direct[166][3]==0xF800u);
    test_now_ms=600u;reference();reset_touched();sent=render();
    assert(direct[166][3]==0u && direct[11][240]==0u && sent>0u && touched_x0==0u && touched_x1<=280u && touched_y0<=168u && touched_y1<=170u);
    red_percent_pixels=0u;
    for(unsigned y=8u;y<32u;y++)for(unsigned x=100u;x<190u;x++)if(direct[y][x]==0xF800u)red_percent_pixels++;
    assert(red_percent_pixels==0u);
    for(unsigned y=0u;y<25u;y++)for(unsigned x=0u;x<71u;x++)assert(voltage_pixels[y][x]==direct[y+8u][x+160u]);
    assert(memcmp(sim_fb,direct,sizeof(direct))==0);
    printf("battery low phase transition: %u local tiles\n",sent);
    test_now_ms=900u;reference();(void)render();assert(direct[166][3]==0xF800u);
    test_now_ms=0u;t->battery_pct=7u;reference();(void)render();assert(direct[166][3]==0xF800u && direct[11][240]==0xF800u);
    t->battery_pct=12u;reference();(void)render();assert(direct[166][3]==0xFD20u && direct[11][240]==0xFD20u);
    t->battery_pct=30u;reference();(void)render();assert(direct[166][3]==0xFFFFu && direct[11][240]==0xFFFFu);
    t->battery_pct=100u;reference();(void)render();assert(direct[166][3]==0xFFFFu && direct[11][240]==0xFFFFu);
    assert(direct[158][6]==0xF800u && direct[151][6]==0xFD20u && direct[141][6]==0xFFFFu && direct[85][6]==0xFFFFu);
    app.preferences.battery_mask &= (uint8_t)~EVIST_BATTERY_BAR;
    test_now_ms=0u;t->battery_pct=76u;reference();(void)render();

    /* The ring changes badge tiles; the confirmation changes lower tiles. */
    app.ride_indicator = DPC245_RIDE_IND_OFFROAD;
    reference();reset_touched();sent=render();
    assert(sent>0u && sent<=40u && touched_x1<=80u && touched_y0>=40u && touched_y1<=110u);
    assert(memcmp(sim_fb,direct,sizeof(direct))==0);
    printf("ride ring change: %u tiles\n",sent);
    app.ride_confirm_pending=true;
    reference();reset_touched();sent=render();
    assert(sent>0u && touched_y0>=130u && touched_y1<=170u && touched_x1<=280u);
    assert(memcmp(sim_fb,direct,sizeof(direct))==0);
    printf("ride banner change: %u tiles\n",sent);
    app.ride_confirm_pending=false;app.ride_indicator=DPC245_RIDE_IND_LEGAL;
    reference();(void)render();assert(memcmp(sim_fb,direct,sizeof(direct))==0);
    app.ride_toast_active=true;
    reference();reset_touched();sent=render();
    assert(sent>0u && touched_y0>=130u && touched_y1<=170u && touched_x1<=280u);
    assert(memcmp(sim_fb,direct,sizeof(direct))==0);
    printf("ride toast change: %u tiles\n",sent);
    app.ride_toast_active=false;
    reference();(void)render();assert(memcmp(sim_fb,direct,sizeof(direct))==0);

    /* TEMP ALTERNATE changes only its temperature cell at each 3 s boundary. */
    app.preferences.lower_panel=9u;
    app.preferences.temp_view=EVIST_TEMP_ALTERNATE;
    t->have_controller_temp=true;t->controller_temp_raw=81u;t->controller_temp_c=41;
    t->have_motor_temp=true;t->controller_aux_temp_raw=95u;t->motor_temp_c=55;
    {
        char left[6],right[6];const char *ll,*rl,*lu,*ru;
        app.preferences.temp_view=EVIST_TEMP_BOTH;
        ui_dashboard_lower_pair_text(&app,t,9u,0u,left,right,&ll,&rl,&lu,&ru);
        assert(strcmp(ll,"CTRL")==0 && strcmp(left,"41")==0 && strcmp(lu,"C")==0);
        assert(strcmp(rl,"MOTOR")==0 && strcmp(right,"55")==0 && strcmp(ru,"C")==0);
        app.preferences.temp_view=EVIST_TEMP_1;
        ui_dashboard_lower_pair_text(&app,t,9u,0u,left,right,&ll,&rl,&lu,&ru);
        assert(strcmp(ll,"CTRL")==0 && strcmp(left,"41")==0 && strcmp(rl,"WATT")==0 && strcmp(ru,"W")==0);
        app.preferences.temp_view=EVIST_TEMP_2;
        ui_dashboard_lower_pair_text(&app,t,9u,0u,left,right,&ll,&rl,&lu,&ru);
        assert(strcmp(ll,"MOTOR")==0 && strcmp(left,"55")==0 && strcmp(rl,"WATT")==0);
        app.preferences.temp_view=EVIST_TEMP_ALTERNATE;
        ui_dashboard_lower_pair_text(&app,t,9u,2999u,left,right,&ll,&rl,&lu,&ru);
        assert(strcmp(ll,"CTRL")==0 && strcmp(left,"41")==0);
        ui_dashboard_lower_pair_text(&app,t,9u,3000u,left,right,&ll,&rl,&lu,&ru);
        assert(strcmp(ll,"MOTOR")==0 && strcmp(left,"55")==0);
        ui_dashboard_lower_pair_text(&app,t,5u,3000u,left,right,&ll,&rl,&lu,&ru);
        assert(strcmp(rl,"MOTOR")==0 && strcmp(right,"55")==0);
        ui_dashboard_lower_pair_text(&app,t,8u,0u,left,right,&ll,&rl,&lu,&ru);
        assert(strcmp(rl,"CTRL")==0 && strcmp(right,"41")==0);
        t->controller_aux_temp_raw=0u;
        ui_dashboard_lower_pair_text(&app,t,9u,3000u,left,right,&ll,&rl,&lu,&ru);
        assert(strcmp(ll,"MOTOR")==0 && strcmp(left,"--")==0);
        t->controller_aux_temp_raw=0xFFu;
        ui_dashboard_lower_pair_text(&app,t,9u,3000u,left,right,&ll,&rl,&lu,&ru);
        assert(strcmp(ll,"MOTOR")==0 && strcmp(left,"--")==0);
        t->controller_aux_temp_raw=95u;
        t->controller_temp_raw=0xFFu;
        ui_dashboard_lower_pair_text(&app,t,9u,0u,left,right,&ll,&rl,&lu,&ru);
        assert(strcmp(ll,"CTRL")==0 && strcmp(left,"--")==0);
        t->controller_temp_raw=81u;
    }
    test_now_ms=2999u;reference();(void)render();
    test_now_ms=3000u;reference();reset_touched();sent=render();
    assert(sent>0u && touched_x1<=144u && touched_y0>=130u && touched_y1<=170u);
    assert(memcmp(sim_fb,direct,sizeof(direct))==0);
    assert(render()==0u);
    test_now_ms=6000u;reference();reset_touched();sent=render();
    assert(sent>0u && touched_x1<=144u && touched_y0>=130u && touched_y1<=170u);
    assert(memcmp(sim_fb,direct,sizeof(direct))==0);
    app.preferences.lower_panel=5u;test_now_ms=2999u;reference();(void)render();
    test_now_ms=3000u;reference();reset_touched();sent=render();
    assert(sent>0u && touched_x0>=128u && touched_y0>=130u && touched_y1<=170u);
    assert(memcmp(sim_fb,direct,sizeof(direct))==0);
    app.preferences.lower_panel=0u;app.preferences.temp_view=EVIST_TEMP_BOTH;test_now_ms=0u;
    reference();(void)render();

    /* A light transition sends only tiles in its fixed top-left icon slot. */
    status.light_on = true;
    reference();
    reset_touched();
    sent = render();
    assert(sent > 0u && sent <= 6u);
    assert(touched_x0 < 32u && touched_x1 <= 32u);
    assert(touched_y0 < 30u && touched_y1 <= 30u);
    assert(memcmp(sim_fb, direct, sizeof(direct)) == 0);
    printf("light icon change: %u tiles\n", sent);
    status.light_on = false;
    reference();
    (void)render();
    assert(memcmp(sim_fb, direct, sizeof(direct)) == 0);

    /* 4. Screen change to a menu and back: result always equals direct draw. */
    app.screen = DPC245_APP_SCREEN_ROOT_MENU;
    reference();
    (void)render();
    assert(memcmp(sim_fb, direct, sizeof(direct)) == 0);
    app.screen = DPC245_APP_SCREEN_DASHBOARD;
    reference();
    (void)render();
    assert(memcmp(sim_fb, direct, sizeof(direct)) == 0);

    /* 5. Computing one dashboard frame costs about one screen of pixel
     *    writes (each strip draws only its own rows), not one screen per strip. */
    app.screen = DPC245_APP_SCREEN_DASHBOARD;
    ui_strip_invalidate();
    (void)render();
    uint32_t ops = ui_strip_last_frame_ops();
    printf("dashboard frame: %u pixel writes (screen = %u)\n", (unsigned)ops,
           (unsigned)(DPC245_LCD_WIDTH * DPC245_LCD_HEIGHT));
    assert(ops < 2u * DPC245_LCD_WIDTH * DPC245_LCD_HEIGHT);

    /* Every layout compares by tiles; a speed, mode or power change stays in
     * the corresponding fixed region. */
    t->have_range=true;t->range_centi_km=4320u;
    uint32_t max_theme_ops=0u;
    uint8_t max_theme=DPC245_THEME_STANDARD;
    for(uint8_t theme=DPC245_THEME_STANDARD;theme<=DPC245_THEME_WEDGE;theme++){
        dpc245_set_theme(&app.runtime.settings,theme);
        app.assist_level=1u;t->speed_centi_kph=2180u;app.power_graph_w=250u;
        t->battery_pct=76u;t->range_centi_km=4320u;
        reference();(void)render();assert(memcmp(sim_fb,direct,sizeof(direct))==0);
        uint32_t frame_ops=ui_strip_last_frame_ops();
        if(frame_ops>max_theme_ops){max_theme_ops=frame_ops;max_theme=theme;}
        assert(frame_ops<2u*DPC245_LCD_WIDTH*DPC245_LCD_HEIGHT);
        assert(render()==0u);
        t->speed_centi_kph=2190u;reference();sent=render();
        assert(sent>0u && memcmp(sim_fb,direct,sizeof(direct))==0);
        if(theme==DPC245_THEME_ARC)assert(touched_x0>=16u&&touched_x1<=240u&&touched_y1<=140u);
        if(theme==DPC245_THEME_WEDGE)assert(touched_x1<=224u&&touched_y0>=30u&&touched_y1<=140u);
        app.assist_level=2u;reference();sent=render();
        assert(sent>0u && memcmp(sim_fb,direct,sizeof(direct))==0);
        if(theme==DPC245_THEME_ARC)assert(touched_x1<=240u);
        if(theme==DPC245_THEME_WEDGE)assert(touched_x0>=208u);
        app.power_graph_w=750u;reference();sent=render();
        assert(sent>0u && memcmp(sim_fb,direct,sizeof(direct))==0);
        if(theme==DPC245_THEME_ARC)assert(touched_x1<=64u);
        if(theme==DPC245_THEME_WEDGE)assert(touched_y0>=140u);
        t->battery_pct=77u;reference();sent=render();
        assert(sent>0u&&memcmp(sim_fb,direct,sizeof(direct))==0);
        if(theme==DPC245_THEME_ARC)assert(touched_x0>=16u&&touched_x1<=240u&&touched_y0>=130u);
        if(theme==DPC245_THEME_WEDGE)assert(touched_x1<=112u&&touched_y1<=40u);
        if(theme>=DPC245_THEME_ARC){
            uint8_t before=app.preferences.lower_panel;
            dpc245_app_key(&app,DPC245_APP_KEY_POWER);
            assert(app.preferences.lower_panel!=before);
            reference();sent=render();
            assert(sent>0u&&memcmp(sim_fb,direct,sizeof(direct))==0);
            if(theme==DPC245_THEME_ARC)assert(touched_x0>=224u);
            if(theme==DPC245_THEME_WEDGE)assert(touched_x0>=240u&&touched_y0<140u);
            app.preferences.lower_panel=0u;
        }
        printf("theme %u: speed/mode/power local, last %u tiles, full render %u writes\n",
               theme,sent,(unsigned)frame_ops);
    }
    dpc245_set_theme(&app.runtime.settings,DPC245_THEME_STANDARD);
    t->have_range=false;t->range_centi_km=0u;t->battery_pct=76u;
    app.assist_level=1u;t->speed_centi_kph=2190u;app.power_graph_w=0u;
    reference();(void)render();assert(memcmp(sim_fb,direct,sizeof(direct))==0);

    for(uint8_t theme=DPC245_THEME_STANDARD;theme<=DPC245_THEME_WEDGE;theme++){
        dpc245_set_theme(&app.runtime.settings,theme);
        app.temp_warning_sources=0u;app.temp_warning_text_until_ms=0u;
        test_now_ms=1000u;reference();(void)render();
        app.temp_motor_c=126;app.temp_ctrl_c=111;
        app.temp_warning_sources=1u;app.temp_warning_text_until_ms=4000u;
        reference();reset_touched();sent=render();
        assert(sent>0u&&memcmp(sim_fb,direct,sizeof(direct))==0);
        assert(touched_x1<=(theme==DPC245_THEME_WEDGE?320u:theme==DPC245_THEME_ARC?240u:288u));
        assert(touched_y0>=30u&&touched_y1<=170u);
        test_now_ms=3999u;assert(render()==0u);
        test_now_ms=4000u;reference();reset_touched();sent=render();
        assert(sent>0u&&memcmp(sim_fb,direct,sizeof(direct))==0);
        assert(touched_y0>=130u&&touched_y1<=170u); /* text expires; speed remains red */
        app.temp_warning_sources=3u;app.temp_warning_text_until_ms=8000u;
        test_now_ms=5000u;reference();reset_touched();sent=render();
        assert(sent>0u&&memcmp(sim_fb,direct,sizeof(direct))==0);
        assert(touched_y0>=130u&&touched_y1<=170u);
        app.temp_warning_sources=0u;test_now_ms=8000u;
        reference();reset_touched();sent=render();
        assert(sent>0u&&memcmp(sim_fb,direct,sizeof(direct))==0);
        assert(touched_x1<=(theme==DPC245_THEME_WEDGE?320u:theme==DPC245_THEME_ARC?240u:288u));
        assert(touched_y1<=170u);
        printf("theme %u warning on/text expiry/join/off: local tiles\n",theme);
    }
    dpc245_set_theme(&app.runtime.settings,DPC245_THEME_STANDARD);
    test_now_ms=1000u;app.temp_warning_text_until_ms=4000u;
    app.temp_warning_sources=1u;app.temp_motor_valid=false;app.temp_motor_c=215;
    reference();assert_standard_warning_text("MOTOR HOT --C");
    app.temp_warning_sources=2u;app.temp_ctrl_valid=false;app.temp_ctrl_c=215;
    reference();assert_standard_warning_text("CTRL HOT --C");
    app.temp_warning_sources=3u;app.temp_motor_valid=true;app.temp_motor_c=65;
    reference();assert_standard_warning_text("MOTOR+CTRL HOT 65C");
    app.temp_ctrl_valid=true;app.temp_ctrl_c=70;
    reference();assert_standard_warning_text("MOTOR+CTRL HOT 70C");
    app.temp_motor_valid=false;
    reference();assert_standard_warning_text("MOTOR+CTRL HOT 70C");
    app.temp_ctrl_valid=false;
    reference();assert_standard_warning_text("MOTOR+CTRL HOT --C");
    app.temp_warning_sources=0u;
    test_now_ms=0u;

    /* 6. Clipped blit: the direct path equals the strip path. */
    memcpy(panel_save, sim_fb, sizeof(panel_save));
    draw_edge(0);
    memcpy(direct, sim_fb, sizeof(direct));
    assert(direct[100][319] == 0x1111u && direct[101][319] == 0x3333u);   /* stride 2 */
    assert(direct[169][10] == 0x1111u && direct[169][11] == 0x2222u);
    memcpy(sim_fb, panel_save, sizeof(panel_save));
    ui_strip_invalidate();
    reset_touched();
    (void)ui_strip_render(draw_edge, 0, push);
    assert(memcmp(sim_fb, direct, sizeof(direct)) == 0);

    /* 7. A frame spread over bounded passes still reaches the direct image;
     * an unchanged staged frame does not touch the panel. */
    app.screen = DPC245_APP_SCREEN_DASHBOARD;
    reference();
    ui_strip_invalidate();
    reset_touched();
    ui_strip_render_begin(draw, 0, push);
    assert(ui_strip_render_active());
    sent = 0u;
    unsigned steps = 0u;
    while (ui_strip_render_active()) {
        sent = (uint16_t)(sent + ui_strip_render_step(3u));
        ++steps;
        assert(steps <= 6u);
    }
    assert(steps == 6u && sent == UI_STRIPS * UI_TILES_X);
    assert(memcmp(sim_fb, direct, sizeof(direct)) == 0);
    ui_strip_render_begin(draw, 0, push);
    sent = 0u;
    while (ui_strip_render_active()) sent = (uint16_t)(sent + ui_strip_render_step(3u));
    assert(sent == 0u);

    /* 8. Telemetry changing halfway through a digit cannot tear this frame. */
    t->speed_centi_kph = 2180u;
    snapshot_frame();
    reference();
    ui_strip_render_begin(draw, &frame_state, push);
    (void)ui_strip_render_step(8u);
    t->speed_centi_kph = 3290u;
    while (ui_strip_render_active()) (void)ui_strip_render_step(3u);
    assert(memcmp(sim_fb, direct, sizeof(direct)) == 0);
    snapshot_frame();
    reference();
    ui_strip_render_begin(draw, &frame_state, push);
    while (ui_strip_render_active()) (void)ui_strip_render_step(3u);
    assert(memcmp(sim_fb, direct, sizeof(direct)) == 0);

    /* 9. Invalidation after partial progress resends every strip, including
     * strips already drawn before the call. */
    ui_strip_render_begin(draw, &frame_state, push);
    (void)ui_strip_render_step(3u);
    memset(pushes_per_strip, 0, sizeof(pushes_per_strip));
    count_pushes = true;
    ui_strip_invalidate();
    while (ui_strip_render_active()) (void)ui_strip_render_step(3u);
    count_pushes = false;
    for (unsigned s = 0u; s < UI_STRIPS; ++s)
        assert(pushes_per_strip[s] == UI_TILES_X);
    assert(memcmp(sim_fb, direct, sizeof(direct)) == 0);
    assert(render() == 0u);

    /* Menu work is independent of the selected dashboard layout. */
    app.screen=DPC245_APP_SCREEN_THEMES_MENU;
    app.selection=4u;
    uint32_t menu_ops=0u;
    for(uint8_t theme=DPC245_THEME_STANDARD;theme<=DPC245_THEME_WEDGE;theme++){
        dpc245_set_theme(&app.runtime.settings,theme);
        ui_strip_invalidate();
        (void)render();
        uint32_t measured=ui_strip_last_frame_ops();
        if(theme==DPC245_THEME_STANDARD)menu_ops=measured;
        else if(theme>=DPC245_THEME_ARC)assert(measured==menu_ops);
        printf("theme menu %u: %u pixel writes, 6 passes\n",theme,(unsigned)measured);
    }
    printf("full frame cost: menu=%u ops, heaviest dashboard theme=%u with %u ops\n",
           (unsigned)menu_ops,max_theme,(unsigned)max_theme_ops);

    /* A key event drops the old snapshot and starts at the current row. */
    dpc245_set_theme(&app.runtime.settings,DPC245_THEME_STANDARD);
    app.selection=4u;
    snapshot_frame();
    ui_strip_render_begin(draw,&frame_state,push);
    (void)ui_strip_render_step(3u);
    app.selection=5u;
    ui_strip_render_cancel();
    assert(!ui_strip_render_active());
    snapshot_frame();
    reference();
    ui_strip_render_begin(draw,&frame_state,push);
    while(ui_strip_render_active())(void)ui_strip_render_step(3u);
    assert(memcmp(sim_fb,direct,sizeof(direct))==0);

    /* A DOWN selection is fully visible before POWER acts on that row. */
    app.screen=DPC245_APP_SCREEN_DISPLAY_MENU;
    app.selection=5u;
    dpc245_key_sampler_t queued;
    dpc245_key_sampler_init(&queued,0u,0u);
    queued.queue[0]=(dpc245_key_sample_event_t){DPC245_KEY_EVENT_DOWN_SHORT,100u};
    queued.queue[1]=(dpc245_key_sample_event_t){DPC245_KEY_EVENT_POWER_SHORT,280u};
    queued.write=2u;
    navigation_model_t model={0};
    snapshot_frame();
    ui_strip_render_begin(draw,&frame_state,push);
    (void)ui_strip_render_step(3u);
    navigation_pass(&queued,&model);
    assert(app.selection==6u);
    assert(app.screen==DPC245_APP_SCREEN_DISPLAY_MENU);
    assert(model.gated && model.displayed==0u);
    dpc245_key_sample_event_t next_event;
    assert(queued.read==1u && queued.queue[queued.read].events==DPC245_KEY_EVENT_POWER_SHORT);
    for(unsigned i=0u;i<5u;i++)navigation_pass(&queued,&model);
    assert(!model.gated && model.displayed==1u && model.processed==1u);
    navigation_pass(&queued,&model);
    assert(app.screen==DPC245_APP_SCREEN_THEMES_MENU);
    for(unsigned i=0u;i<5u;i++)navigation_pass(&queued,&model);
    assert(!model.gated && model.displayed==2u && model.processed==2u);
    assert(model.order[0]==DPC245_KEY_EVENT_DOWN_SHORT &&
           model.order[1]==DPC245_KEY_EVENT_POWER_SHORT);
    assert(!dpc245_key_sampler_pop(&queued,&next_event));
    assert(model.passes==12u && model.can_service==12u && model.ble_service==12u &&
           model.store_service==12u && model.max_strips==3u);
    puts("navigation then POWER uses displayed row; CAN, BLE and store service run in all 12 passes");

    /* Fast taps remain queued until each preceding selection reaches the panel. */
    app.screen=DPC245_APP_SCREEN_THEMES_MENU;
    app.selection=0u;
    dpc245_key_sampler_init(&queued,0u,0u);
    const uint32_t actions[]={DPC245_KEY_EVENT_DOWN_SHORT,DPC245_KEY_EVENT_DOWN_SHORT,
        DPC245_KEY_EVENT_UP_SHORT,DPC245_KEY_EVENT_DOWN_SHORT,
        DPC245_KEY_EVENT_UP_SHORT,DPC245_KEY_EVENT_DOWN_SHORT,
        DPC245_KEY_EVENT_DOWN_SHORT,DPC245_KEY_EVENT_UP_SHORT};
    for(unsigned i=0u;i<sizeof actions/sizeof actions[0];i++)
        queued.queue[i]=(dpc245_key_sample_event_t){actions[i],(uint32_t)(180u*i+100u)};
    queued.write=(uint8_t)(sizeof actions/sizeof actions[0]);
    model=(navigation_model_t){0};
    for(unsigned i=0u;i<48u;i++)navigation_pass(&queued,&model);
    assert(model.processed==8u && model.displayed==8u && !model.gated);
    for(unsigned i=0u;i<8u;i++)assert(model.order[i]==actions[i]);
    assert(model.max_strips==3u && model.can_service==48u && model.ble_service==48u &&
           model.store_service==48u);
    printf("fast taps: 8 ordered actions, 8 displayed frames, %u service passes, max %u strips/pass\n",
           model.passes,model.max_strips);

    app.screen=DPC245_APP_SCREEN_DASHBOARD;
    app.assist_level=2u;
    dpc245_key_sampler_init(&queued,0u,0u);
    queued.queue[0]=(dpc245_key_sample_event_t){DPC245_KEY_EVENT_UP_SHORT,100u};
    queued.queue[1]=(dpc245_key_sample_event_t){DPC245_KEY_EVENT_DOWN_SHORT,280u};
    queued.write=2u;
    model=(navigation_model_t){0};
    navigation_pass(&queued,&model);
    assert(app.assist_level==3u && model.gated);
    for(unsigned i=0u;i<5u;i++)navigation_pass(&queued,&model);
    assert(model.displayed==1u && app.assist_level==3u);
    navigation_pass(&queued,&model);
    assert(app.assist_level==2u);
    for(unsigned i=0u;i<5u;i++)navigation_pass(&queued,&model);
    assert(model.displayed==2u && model.max_strips==3u);
    puts("dashboard assist actions wait for their preceding displayed frame");

    /* Physical release clears Walk without moving queued gestures past the gate. */
    app.screen=DPC245_APP_SCREEN_DASHBOARD;
    app.walk=app.walk_active=app.walk_quick=app.walk_armed=app.down_held=true;
    dpc245_key_sampler_init(&queued,0u,0u);
    queued.queue[0]=(dpc245_key_sample_event_t){DPC245_KEY_EVENT_POWER_SHORT,100u};
    queued.queue[1]=(dpc245_key_sample_event_t){DPC245_KEY_EVENT_UP_SHORT,280u};
    queued.queue[2]=(dpc245_key_sample_event_t){DPC245_KEY_EVENT_DOWN_RELEASE,290u};
    queued.write=3u;
    model=(navigation_model_t){0};
    model.physical_down=true;
    navigation_pass(&queued,&model);
    assert(model.gated && app.walk);
    model.physical_down=false;
    navigation_pass(&queued,&model);
    assert(!app.walk && model.processed==1u && queued.read==1u);
    puts("physical DOWN release clears Walk while later gestures stay queued");

    puts("ui strip tests passed");
    return 0;
}
