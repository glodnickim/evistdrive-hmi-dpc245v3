/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "app_config.h"
#include "dpc245_board.h"
#include "dpc245_clock.h"
#include "st7789_8080.h"
#include "dpc245_backlight.h"
#include "dpc245_buttons.h"
#include "dpc245_adc.h"
#include "dpc245_can_init.h"
#include "dpc245_recovery.h"
#include "dpc245_recovery_trigger.h"
#include "dpc245_preflight.h"
#include "dpc245_probe.h"
#include "dpc245_hwinfo.h"
#include "dpc245_ble_uart.h"
#include "dpc245_ble_proto.h"
#include "dpc245_ble_app.h"
#include "dpc245_spi_mem.h"
#include "dpc245_persist.h"
#include "native_store.h"
#include "evist_preferences.h"
#include "dpc245_app.h"
#include "dpc245_keys.h"
#include "dpc245_settings.h"
#include "dpc245_tx.h"
#include "dpc245_hmi_service.h"
#include "dpc245_hmi_control.h"
#include "dpc245_rtc_board.h"
#include "dpc245_update.h"
#include "dpc245_v3_profile.h"
#include "light_controller.h"
#include "bxcan_raw.h"
#include "can_router.h"
#include "controller_write.h"
#include "ui_dashboard.h"
#include "ui_strip.h"
#include "ride_mode.h"
#include "loop_stats.h"
#include "minifont.h"
#include <stdbool.h>
#include <stdint.h>

#define DPC245_APP_CAN_TX EVIST_ENABLE_CAN_TX
#define DPC245_APP_SERVICE_TX EVIST_ENABLE_SERVICE_TX
#define DPC245_APP_PERSIST_WRITE EVIST_ENABLE_PERSISTENCE
#define DPC245_APP_BLE_TX EVIST_ENABLE_BLE_TX
#define DPC245_APP_CONFIG_WRITES EVIST_ENABLE_CONFIG_WRITES

#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define SYST_CSR REG32(0xE000E010u)
#define SYST_RVR REG32(0xE000E014u)
#define SYST_CVR REG32(0xE000E018u)
#define SCB_ICSR REG32(0xE000ED04u)
#define ICSR_PENDSTSET (1u<<26)   /* SysTick exception pending */
#define HSI_HZ 8000000u
extern volatile uint32_t evist_ms_ticks;
static void tick_init(uint32_t core_hz){SYST_RVR=(core_hz/1000u)-1u;SYST_CVR=0u;SYST_CSR=7u;}
static uint32_t millis(void){return evist_ms_ticks;}
static bool settings_erase_start(void *ctx,uint32_t address){
    (void)ctx;return dpc245_spi_mem_erase_start(address);
}
static int settings_ready(void *ctx){(void)ctx;return dpc245_spi_mem_ready();}
/* Microseconds from the 1 ms SysTick: whole ms from the interrupt counter plus the
 * part of the current ms from the down-counting SysTick value. 32-bit only:
 * (RVR - CVR) <= 120 000 at 120 MHz, x1000 fits. Wraps after ~71 min; callers use
 * unsigned differences. */
static uint32_t micros(void){
    uint32_t ms1,cvr,ms2;bool pend;
    do{ms1=evist_ms_ticks;cvr=SYST_CVR;pend=(SCB_ICSR&ICSR_PENDSTSET)!=0u;ms2=evist_ms_ticks;}while(ms1!=ms2);
    /* Timing: reload with the interrupt still pending is handled in
     * evist_micros_compose (host-tested). */
    return evist_micros_compose(ms1,cvr,SYST_RVR,pend);
}

enum { ST_CAN_RX, ST_BLE_RX, ST_ADC, ST_APP, ST_SPI, ST_CAN_TX, ST_BLE_TX, ST_LCD };
static uint32_t stage_end(dpc245_app_t *app, unsigned stage, uint32_t start) {
    uint32_t end=micros(), elapsed=end-start;
    if(elapsed<0x80000000u && elapsed>app->stage_max_us[stage]) app->stage_max_us[stage]=elapsed;
    return end;
}

/* Screen output: each frame is computed in strips and only the 16x10 tiles that
 * changed reach the panel (ui_strip.h). The panel is never cleared in normal use,
 * so a changed digit rewrites only its own tiles and nothing flickers. */
typedef struct {
    const dpc245_app_t *app; const evist_can_router_t *router;
    const dpc245_adc_snapshot_t *adc; const dpc245_hwinfo_t *hw;
    uint8_t slot; bool loaded; uint32_t now_ms; dpc245_ui_status_t status;
} screen_ctx_t;
static void screen_draw(void *p){
    const screen_ctx_t *s=(const screen_ctx_t *)p;
    ui_dashboard_draw(s->app,s->router,s->adc,s->hw,s->slot,s->loaded,
                      DPC245_APP_PERSIST_WRITE!=0,(DPC245_APP_CAN_TX!=0)||(DPC245_APP_SERVICE_TX!=0),s->now_ms,&s->status);
}
static void screen_push(uint16_t x,uint16_t y,uint16_t w,uint16_t h,const uint16_t *px,uint16_t stride){
    dpc245_lcd_set_window(x,y,(uint16_t)(x+w-1u),(uint16_t)(y+h-1u));
    for(uint16_t r=0u;r<h;r++)for(uint16_t c=0u;c<w;c++)dpc245_lcd_data16(px[(uint32_t)r*stride+c]);
}
/* Bounded even if the tick never advances: LCD bring-up must never stall
 * the start-up indefinitely. */
#define DELAY_STALL_SPINS 2000000u
static void delay_ms(uint32_t n){
    uint32_t s=millis(),seen=s,stall=0u;
    while((uint32_t)(millis()-s)<n){
        uint32_t t=millis();
        if(t!=seen){seen=t;stall=0u;}
        else if(++stall>=DELAY_STALL_SPINS)return;
    }
}

#if DPC245_APP_CAN_TX || DPC245_APP_SERVICE_TX
static bool send_normal(evist_can_router_t *router,const bafang_can_frame_t *f){
    return evist_can_router_tx_allowed(router,f,false) && dpc245_can_transmit(f);
}
#if DPC245_APP_SERVICE_TX
static void send_service_ack(evist_can_router_t *router){
    bafang_can_frame_t ack;
    while(dpc245_can_tx_idle() && evist_can_router_peek_service_ack(router,&ack)){
        if(!send_normal(router,&ack)) break;
        evist_can_router_pop_service_ack(router);
    }
}
#endif
#endif

/* Runs the controller READ/WRITE requested by the ADVANCED SETTINGS pages
 * (WHEEL 0x3203, ASSIST 0x6011) and reports the verified outcome back. */
static void controller_config_service(evist_ctrl_txn_t *txn,dpc245_app_t *app,
                                      const evist_can_router_t *router,
                                      evist_ride_apply_t *ride,bool *ride_owner,
                                      dpc245_ble_app_t *ble,bool controller_seen,uint32_t now){
    evist_advanced_t *adv=&app->advanced;
    evist_adv_request_t req;
    if((ride->phase==EVIST_RIDE_APPLY_READ || ride->phase==EVIST_RIDE_APPLY_WRITE) &&
       txn->step==EVIST_TXN_STEP_IDLE)
        *ride_owner=evist_ride_apply_start_txn(ride,txn,now);
    if(ride->phase==EVIST_RIDE_APPLY_IDLE && txn->step==EVIST_TXN_STEP_IDLE &&
       evist_advanced_take_request(adv,&req)){
        bool started=false;
        const char *refusal="REQUEST REJECTED";
        switch(req){
        case EVIST_ADV_REQ_READ_WHEEL:
            started=evist_ctrl_txn_read_wheel(txn,now);
            break;
        case EVIST_ADV_REQ_READ_ASSIST:
            started=evist_ctrl_txn_read_assist(txn,now);
            break;
        case EVIST_ADV_REQ_DISCOVER_ACCEL:
            started=evist_ctrl_txn_discover_accel(txn,now);
            break;
        case EVIST_ADV_REQ_READ_ACCEL_P0:
            started=evist_ctrl_txn_read_accel_p0(txn,now);
            break;
        case EVIST_ADV_REQ_READ_ACCEL_P2:
            started=evist_ctrl_txn_read_accel_p2(txn,now);
            break;
        case EVIST_ADV_REQ_WRITE_ACCEL_P0:
#if DPC245_APP_CONFIG_WRITES
            started=evist_ctrl_txn_write_app_p0(txn,adv->accel_block,adv->accel_committed[63],now);
#else
            refusal="WRITES DISABLED IN THIS BUILD";
#endif
            break;
        case EVIST_ADV_REQ_WRITE_ACCEL_P2:
#if DPC245_APP_CONFIG_WRITES
            started=evist_ctrl_txn_write_app_p2(txn,adv->accel_block,now);
#else
            refusal="WRITES DISABLED IN THIS BUILD";
#endif
            break;
        case EVIST_ADV_REQ_WRITE_WHEEL:
        case EVIST_ADV_REQ_WRITE_ASSIST:
#if DPC245_APP_CONFIG_WRITES || EVIST_ENABLE_RIDE_SPEED_WRITE
            if(req==EVIST_ADV_REQ_WRITE_WHEEL){
#if EVIST_ENABLE_RIDE_SPEED_WRITE
                evist_wheel_config_t target;
                evist_advanced_wheel_target(adv,&target);
#if !DPC245_APP_CONFIG_WRITES
                if(adv->ride_committed.mode!=EVIST_RIDE_LEGAL || adv->wheel_dirty){
                    refusal="WRITES DISABLED IN THIS BUILD";break;
                }
#endif
                started=evist_ctrl_txn_write_wheel(txn,&target,now);
#else
                refusal="RIDE SPEED WRITE DISABLED";
#endif
            }else{
#if DPC245_APP_CONFIG_WRITES
                started=evist_ctrl_txn_write_assist(txn,adv->assist_block,adv->committed.firmware_profile,now);
#else
                refusal="WRITES DISABLED IN THIS BUILD";
#endif
            }
#else
            refusal="WRITES DISABLED IN THIS BUILD";
#endif
            break;
        default:
            break;
        }
        if(!started)evist_advanced_request_failed(adv,refusal);
    }
    if(ride->phase==EVIST_RIDE_APPLY_IDLE && txn->step==EVIST_TXN_STEP_IDLE){
        ble->controller_seen=controller_seen;
        (void)dpc245_ble_app_start_controller(ble,txn,adv->committed.firmware_profile,now);
    }
    evist_ctrl_txn_tick(txn,now);
    bafang_can_frame_t tx;
    /* A frame is taken from the transaction only when it can leave at once,
     * so a busy mailbox delays it instead of losing it. */
    if(dpc245_can_tx_idle()&&router->service_ack_count==0u&&
       evist_ctrl_txn_next_tx(txn,now,&tx))(void)dpc245_can_transmit(&tx);
    evist_txn_kind_t kind;
    evist_txn_result_t result;
    uint8_t data[64];
    if(evist_ctrl_txn_take_result(txn,&kind,&result,data)){
        if(kind==EVIST_TXN_WRITE_WHEEL || kind==EVIST_TXN_WRITE_ASSIST ||
           kind==EVIST_TXN_WRITE_APP_P1 || kind==EVIST_TXN_WRITE_APP_P0 ||
           kind==EVIST_TXN_WRITE_APP_P2){
            app->last_write_seen=true;
            app->last_write_command=kind==EVIST_TXN_WRITE_WHEEL?0x3203u:
                kind==EVIST_TXN_WRITE_APP_P0?0x6010u:
                kind==EVIST_TXN_WRITE_APP_P2?0x6012u:0x6011u;
            app->last_write_acks=txn->ack_count;
            app->last_write_final_ack=txn->final_ack_seen;
            app->last_write_error=txn->error_seen;
            for(uint8_t i=0u;i<4u;i++)app->last_write_error_payload[i]=
                i<txn->error_len?txn->error_payload[i]:0u;
            app->last_write_result=result;
        }
        if(*ride_owner){
            evist_ride_apply_on_result(ride,result,data);
            *ride_owner=false;
        }else if(ble->controller_txn_active){
            dpc245_ble_app_on_controller_result(ble,kind,result,data);
        }else{
            evist_advanced_on_result(adv,kind,result,data);
            dpc245_ble_app_capture_adv_result(ble,kind,result,data);
            if(kind==EVIST_TXN_WRITE_WHEEL && result==EVIST_TXN_OK)
                dpc245_app_ride_wheel_verified(app,data);
        }
    }
    app->start_angle_status=ble->download_status[7];
    app->ble_rejected_writes=ble->rejected_write_frames;
    app->ble_rejected_class=ble->rejected_class;
    app->ble_rejected_command=ble->rejected_command;
    app->ble_rejected_operation=ble->rejected_operation;
    app->ble_rejected_selector=ble->rejected_selector;
    app->ble_rejected_reason=(uint8_t)ble->rejected_reason;
    app->start_angle_raw=(uint16_t)(ble->start_angle[0] | ((uint16_t)ble->start_angle[1] << 8));
    if(ride->phase==EVIST_RIDE_APPLY_OK || ride->phase==EVIST_RIDE_APPLY_FAILED){
        dpc245_app_ride_applied(app,ride);
        ride->phase=EVIST_RIDE_APPLY_IDLE;
    }
}

/* Bring the core to the 120 MHz HXTAL tree and report the tree that is
 * really running. If Reset_Handler already found that tree (left by the
 * updater, HSI not confirmed), it is kept: PLL settings cannot be rewritten
 * while the PLL clocks the core. */
static dpc245_clock_tree_t start_clock(void){
    if(dpc245_early_clock_tree==(uint8_t)DPC245_CLOCK_TREE_HXTAL_PLL)
        return DPC245_CLOCK_TREE_HXTAL_PLL;
    /* A failed setup may stop half way (e.g. APB1 already /2, PLL not
     * locked); return to the clean HSI tree the fallback timings expect. */
    if(!dpc245_clock_init()) return dpc245_clock_fallback_hsi();
    return dpc245_clock_tree_now();
}

#if EVIST_BUILD_PROFILE != EVIST_PROFILE_PROBE
/* Oscillator failure: stay on the 8 MHz internal clock and do nothing but
 * serve the recovery requests. The product is never started on a wrong clock. */
static void __attribute__((noreturn)) clock_failure_safe_mode(void){
    static dpc245_preflight_stats_t stats;
    tick_init(HSI_HZ);
    dpc245_buttons_init();
    dpc245_preflight_begin(DPC245_PREFLIGHT_CLOCK_HSI,&stats);
    dpc245_preflight_run(DPC245_PREFLIGHT_FOREVER,millis,&stats);
    for(;;){}
}
#endif

int main(void){
#if EVIST_BUILD_PROFILE == EVIST_PROFILE_PROBE
    /* The marker was already invalidated first thing in Reset_Handler. */
    static dpc245_probe_boot_t probe_boot;
    dpc245_probe_boot_init(&probe_boot);
#endif
    /* Tick, CAN and UART timings exist only for the two supported trees;
     * anything else is left through a reset (see Reset_Handler). */
    const dpc245_clock_tree_t clock_tree=start_clock();
    if(clock_tree==DPC245_CLOCK_TREE_OTHER) dpc245_system_reset();
    const bool clock_ok=clock_tree==DPC245_CLOCK_TREE_HXTAL_PLL;
#if EVIST_BUILD_PROFILE != EVIST_PROFILE_PROBE
    if(!clock_ok) clock_failure_safe_mode();
    tick_init(DPC245_SYSCLK_HZ);
#else
    probe_boot.clock_pll=clock_ok;
    probe_boot.clock_kept=dpc245_early_clock_tree==(uint8_t)DPC245_CLOCK_TREE_HXTAL_PLL;
    tick_init(clock_ok?DPC245_SYSCLK_HZ:HSI_HZ);
#endif
    dpc245_buttons_init();

    /* Recovery requests are served before LCD, SPI memory, RTC and UI. */
    static dpc245_preflight_stats_t preflight;
    dpc245_preflight_begin(clock_ok?DPC245_PREFLIGHT_CLOCK_PLL:DPC245_PREFLIGHT_CLOCK_HSI,&preflight);
    dpc245_preflight_run(EVIST_PREFLIGHT_WINDOW_MS,millis,&preflight);

#if EVIST_BUILD_PROFILE == EVIST_PROFILE_PROBE
    dpc245_probe_run(&probe_boot,millis,delay_ms,&preflight);
#elif EVIST_BUILD_PROFILE == EVIST_PROFILE_RECOVERY_FAULT
    /* Deliberate hang with a valid marker: only the early chord and the
     * preflight window of the next boot can recover the display. The screen
     * only tells the user that the window is over. */
    dpc245_lcd_init(delay_ms);
    dpc245_backlight_init();
    dpc245_backlight_set_level(2u);
    dpc245_lcd_fill(0x0000u);
    ui_text(16u,50u,"RECOVERY TEST",0xFFFFu,0x0000u,3u);
    ui_text(16u,100u,"WINDOW CLOSED - SIMULATED HANG",0xFD20u,0x0000u,1u);
    ui_text(16u,120u,"POWER CYCLE TO REOPEN THE WINDOW",0x7BEFu,0x0000u,1u);
    ui_text(16u,150u,EVIST_BUILD_GIT,0x7BEFu,0x0000u,1u);
    __asm volatile ("cpsid i" ::: "memory");
    for(;;){}
#endif

    bool adc_ok=dpc245_adc_init();
    dpc245_lcd_init(delay_ms);
    dpc245_backlight_init();
    ui_boot_splash_draw();
    delay_ms(500u);

    /* Load the unified HMI settings image. */
    bool spi_ok=dpc245_spi_mem_init();
    dpc245_persist_io_t persist_io=dpc245_spi_mem_persist_io();
    evist_native_io_t native_io={&persist_io,settings_erase_start,settings_ready,0};
    static evist_native_store_t native_store;
    static evist_native_values_t loaded_values;
    bool store_ok=evist_native_store_init(&native_store,spi_ok?&native_io:0,&loaded_values);
    (void)store_ok;
    static dpc245_settings_t settings;
    settings=loaded_values.hmi;
    uint8_t persist_slot=0xFFu;
    bool persist_loaded=store_ok&&native_store.valid;
    if(persist_loaded)persist_slot=(uint8_t)native_store.page;

    static dpc245_hmi_identity_records_t hmi_identity;
    dpc245_hmi_identity_records_init(&hmi_identity);
    if(spi_ok) (void)dpc245_hmi_identity_records_load(&persist_io,&hmi_identity);
    /* Expose the resident bootloader identification bytes through service info. */
    {
        const volatile uint8_t *bl = (const volatile uint8_t *)0x08005FF0u;
        for(uint8_t i=0u;i<8u;++i) hmi_identity.bootloader_info[i]=bl[i];
    }

    static evist_preferences_t preferences;
#if DPC245_APP_PERSIST_WRITE
    static evist_native_values_t pending_values;
    bool native_pending=store_ok&&(!native_store.valid||native_store.options_reset);
#endif
    preferences=loaded_values.preferences;

    static dpc245_app_t app;
    dpc245_app_init(&app,&settings);
    dpc245_app_apply_preferences(&app,&preferences);
    static evist_ride_cfg_t ride_saved;
    ride_saved=loaded_values.ride;
    dpc245_app_set_ride(&app,&ride_saved);
    bool ride_boot_pending=true;
    bool ride_controller_seen=false;

    uint8_t brightness=dpc245_brightness_level(&app.runtime.settings);
    if(brightness<1u||brightness>4u)brightness=4u;
    dpc245_backlight_set_level(brightness);

    static evist_can_router_t router;evist_can_router_init(&router);
    static dpc245_update_t update; dpc245_update_init(&update);
    dpc245_rtc_io_t rtc_io=dpc245_rtc_board_io();
    static evist_ctrl_txn_t ctrl_txn;evist_ctrl_txn_init(&ctrl_txn);
    static evist_ride_apply_t ride_apply;evist_ride_apply_init(&ride_apply);
    bool ride_txn_owner=false;
#if DPC245_APP_CAN_TX
    router.tx_mode=EVIST_TX_NORMAL_HMI;
#endif
    /* CAN and the BLE UART (USART1 PA9/PA10, 230400 baud) were brought up by
     * the preflight recovery layer. */
    static dpc245_ble_parser_t ble_parser;
    static dpc245_ble_frame_t ble_frame;
    static dpc245_ble_app_t ble_app;
    static dpc245_ble_app_result_t ble_result;
    dpc245_ble_parser_init(&ble_parser);
    dpc245_ble_app_init(&ble_app);
#if DPC245_APP_BLE_TX
    (void)dpc245_ble_uart_send_device_name_command(100000u);
#endif

    static dpc245_hwinfo_t hw;dpc245_hwinfo_read(&hw);
    static dpc245_adc_snapshot_t adc;
    static dpc245_adc_window_filter_t adc_window_filter;
    dpc245_adc_window_filter_init(&adc_window_filter);
    uint16_t btn_sig_counter=0u;
    static evist_light_controller_t light;
    evist_light_init(&light,dpc245_al_sensitivity(&app.runtime.settings));
    evist_light_configure(&light, preferences.light_mode==EVIST_PREF_LIGHT_AUTO,
                          preferences.light_gain,preferences.light_dwell,
                          preferences.light_threshold,preferences.light_level);

    static dpc245_key_sampler_t key_sampler;
    dpc245_buttons_key_sampler_start(&key_sampler,millis());
    uint32_t last_adc=0u,last_draw=0u,last_ride_500=0u,last_ble_500=0u;
#if DPC245_APP_PERSIST_WRITE
    bool was_moving=false;
#endif
    uint32_t last_ble_app_rx=0u;
    bool have_ble_app_rx=false;
#if DPC245_APP_CAN_TX
    static dpc245_v3_tx_schedule_t tx_schedule;
    dpc245_v3_tx_schedule_init(&tx_schedule,millis());
#endif
#if DPC245_APP_SERVICE_TX
    uint32_t last_info_query=0u; size_t info_query_index=0u;
    dpc245_app_screen_t previous_info_screen=app.screen;
    static dpc245_hmi_service_tx_t hmi_service_tx;
    dpc245_hmi_service_tx_init(&hmi_service_tx);
#endif

    if(adc_ok){
        /* Give diagnostics an immediate value; automatic light starts after the first 32 x 40 ms filter window. */
        dpc245_adc_sample32(&adc);
    }

    static dpc245_app_t render_app;
    static evist_can_router_t render_router;
    static dpc245_adc_snapshot_t render_adc;
    static dpc245_hwinfo_t render_hw;
    static screen_ctx_t render_ctx;

    {   /* First frame after the splash: panel content unknown, send every tile. */
        render_app=app;render_router=router;render_adc=adc;render_hw=hw;
        screen_ctx_t sc={&render_app,&render_router,&render_adc,&render_hw,persist_slot,persist_loaded,millis(),
            {light.light_on,false,false}};
        ui_strip_invalidate();
        (void)ui_strip_render(screen_draw,&sc,screen_push);
        if(!ui_strip_render_active())
            dpc245_app_dashboard_render_complete(&app,sc.app,millis());
    }

    /* Main-loop iteration time for the DIAG timing page. */
    static evist_loop_stats_t loop_stats;
    evist_loop_stats_init(&loop_stats,millis());
    uint32_t loop_prev_us=micros();
    bool redraw_pending=false;
    bool navigation_frame_pending=false;

    for(;;){
        uint32_t stage_start;
        {   /* Measured first, so passes that end early with continue are counted too. */
            uint32_t loop_now_us=micros();
            uint32_t pass_us=loop_now_us-loop_prev_us;
            /* A step backwards (> 2^31 as unsigned) would be a false maximum: skip it. */
            if(pass_us<0x80000000u)evist_loop_stats_add(&loop_stats,pass_us,millis());
            loop_prev_us=loop_now_us;
            app.loop_max_us=loop_stats.max_us;
            app.loop_1s_max_us=loop_stats.last_window_max_us;
            stage_start=loop_now_us;
        }
        bafang_can_frame_t f;
        while(dpc245_can_receive_poll(&f)){
            const uint32_t rx_now=millis();
            if(bafang_can_id_decode(f.id).src==BAFANG_NODE_CONTROLLER)ride_controller_seen=true;
            evist_ctrl_txn_on_frame(&ctrl_txn,&f,rx_now);
            /* Updater request (CAN contract): hand over at once, no reply. */
            if (dpc245_is_can_update_request(&f))
                dpc245_enter_resident_bootloader();
#if DPC245_APP_SERVICE_TX
            (void)dpc245_hmi_service_tx_on_ack(&hmi_service_tx,&f);
            {
                bafang_can_frame_t direct_reply;
                if(dpc245_hmi_control_build_read_reply(&f,&app.runtime,&direct_reply))
                    (void)send_normal(&router,&direct_reply);
            }
            if(!hmi_service_tx.active){
                dpc245_hmi_service_reply_t sr;
                if(dpc245_hmi_service_build_read_reply(&f,&hmi_identity,&app.runtime.settings,&sr))
                    (void)dpc245_hmi_service_tx_start(&hmi_service_tx,&sr);
            }
            {
                dpc245_hmi_control_result_t cr;
                if(dpc245_hmi_control_process(&f,&app.runtime,&rtc_io,&cr)){
                    if(cr.settings_dirty) app.settings_dirty=true;
                    if(cr.have_reply) (void)send_normal(&router,&cr.reply);
                }
            }
#endif
            evist_can_router_rx(&router,&f,rx_now);
#if DPC245_APP_SERVICE_TX
            /* Source-3 settings reads stop after each fragment until this ACK.
             * Do not defer it behind the rest of a possibly 50 ms loop pass. */
            send_service_ack(&router);
#endif
            if(router.service_event)evist_ctrl_txn_on_service(&ctrl_txn,&router.last_service,rx_now);
            (void)dpc245_runtime_on_can(&app.runtime,&f);
        }
        stage_start=stage_end(&app,ST_CAN_RX,stage_start);
        dpc245_ble_uart_poll();
        {
            uint8_t ble_byte;
            while(dpc245_ble_uart_read_byte(&ble_byte)) {
                if(dpc245_ble_parser_feed(&ble_parser,ble_byte,&ble_frame)) {
                    if(dpc245_ble_app_handle(&ble_app,&ble_frame,&app,&router,&hmi_identity,
                                             light.light_on,&update,&ble_result)) {
                        /* A recent valid app request is used as the online indicator;
                         * the radio UART provides no confirmed RF link state. */
                        last_ble_app_rx=millis();
                        have_ble_app_rx=true;
#if DPC245_APP_BLE_TX
                        if(ble_result.have_tx)
                            (void)dpc245_ble_uart_write(ble_result.tx,(uint16_t)ble_result.tx_len,100000u);
#endif
                        if(ble_result.settings_dirty) app.settings_dirty=true;
                    }
                }
            }
        }
        stage_start=stage_end(&app,ST_BLE_RX,stage_start);

        uint32_t now=millis();

        if(update.state != DPC245_UPDATE_IDLE){
            bafang_can_frame_t update_tx;
            if(dpc245_update_next_can(&update,&update_tx)){
                if(dpc245_can_transmit(&update_tx)) dpc245_update_can_sent(&update,now);
            }
            if(dpc245_update_tick(&update,now,dpc245_can_tx_idle()))
                dpc245_enter_resident_bootloader();
            (void)stage_end(&app,ST_CAN_TX,stage_start);
            /* Do not emit normal heartbeat/service traffic once update handoff
             * has started; only drain RX/BLE and complete the handoff. */
            continue;
        }

        if(adc_ok&&(uint32_t)(now-last_adc)>=40u){
            if(dpc245_adc_window_filter_tick(&adc_window_filter,&adc)){
                adc.button_signal_high=dpc245_button_signal_filter(adc.button_signal_raw,&btn_sig_counter);
                light.sensitivity=dpc245_al_sensitivity(&app.runtime.settings);
                (void)evist_light_update_als(&light,adc.als_raw);
                dpc245_backlight_set_position(evist_light_backlight_position(&light));
                app.light_als_filtered=light.shaped_als;
                app.light_backlight_position=evist_light_backlight_position(&light);
            }
            last_adc=now;
        }
        stage_start=stage_end(&app,ST_ADC,stage_start);

        bool key_event_seen=(dpc245_buttons_take_released()&DPC245_KEY_DOWN)!=0u;
        if(key_event_seen)dpc245_app_down_state(&app,false,now);
        bool key_navigation_seen=false;
        dpc245_key_sample_event_t key_sample;
        if(!navigation_frame_pending)while(dpc245_buttons_key_event(&key_sample)){
            uint32_t key_events=key_sample.events;
            uint32_t key_time=dpc245_key_time_for_pass(key_sample.time_ms,now);
            key_event_seen=true;
            if(key_events&(DPC245_KEY_EVENT_SETTINGS_CHORD|DPC245_KEY_EVENT_UP_SHORT|
                           DPC245_KEY_EVENT_DOWN_SHORT|DPC245_KEY_EVENT_POWER_SHORT))
                key_navigation_seen=true;
            static const dpc245_key_event_t order[]={
                DPC245_KEY_EVENT_SETTINGS_CHORD,
                DPC245_KEY_EVENT_UP_PRESS,
                DPC245_KEY_EVENT_UP_HOLD,
                DPC245_KEY_EVENT_POWER_DOWN_HOLD,
                DPC245_KEY_EVENT_DOWN_PRESS,
                DPC245_KEY_EVENT_UP_REPEAT,
                DPC245_KEY_EVENT_DOWN_REPEAT,
                DPC245_KEY_EVENT_UP_REPEAT_FAST,
                DPC245_KEY_EVENT_DOWN_REPEAT_FAST,
                DPC245_KEY_EVENT_DOWN_HOLD,
                DPC245_KEY_EVENT_DOWN_RELEASE,
                DPC245_KEY_EVENT_UP_SHORT,
                DPC245_KEY_EVENT_DOWN_SHORT,
                DPC245_KEY_EVENT_POWER_SHORT
            };
            for(size_t i=0u;i<sizeof(order)/sizeof(order[0]);i++)
                if((key_events&(uint32_t)order[i])!=0u)dpc245_app_event(&app,order[i],key_time);
            /* Draw this selection before the next queued action can use it. */
            if(key_navigation_seen){navigation_frame_pending=true;break;}
        }
        dpc245_app_down_state(&app,(dpc245_buttons_state()&DPC245_KEY_DOWN)!=0u,now);
        dpc245_app_temperature_sample(&app,&router.telemetry);
        dpc245_app_tick(&app,now);
        dpc245_app_power_graph_tick(&app,bafang_power_w(&router.telemetry),now);
        {
            evist_ride_cfg_t ride_new;
            if(dpc245_app_take_ride_cfg_dirty(&app,&ride_new)){
#if DPC245_APP_PERSIST_WRITE
                ride_saved=ride_new;
                if(store_ok)native_pending=true;
                if(app.screen==DPC245_APP_SCREEN_ADVANCED)
                    evist_advanced_set_status(&app.advanced,store_ok?
                        evist_native_result_text(EVIST_NATIVE_BUSY):
                        evist_native_result_text(EVIST_NATIVE_NO_MEMORY));
#else
                (void)ride_new;
                if(app.screen==DPC245_APP_SCREEN_ADVANCED)
                    evist_advanced_set_status(&app.advanced,"SAVING DISABLED IN THIS BUILD");
#endif
                ride_boot_pending=false;
            }
            uint16_t ride_target;
            if(ride_apply.phase==EVIST_RIDE_APPLY_IDLE &&
               dpc245_app_take_ride_apply(&app,&ride_target)){
                evist_ride_apply_begin(&ride_apply,ride_target);
                app.ride_apply_result=EVIST_RIDE_APPLY_READ;
                ride_boot_pending=false;
            }else if(ride_boot_pending && ride_controller_seen &&
                     ride_apply.phase==EVIST_RIDE_APPLY_IDLE){
                evist_ride_apply_begin(&ride_apply,app.ride_session==DPC245_RIDE_SESSION_OFFROAD?
                    app.ride_cfg.offroad_x100:app.ride_cfg.legal_x100);
                app.ride_apply_result=EVIST_RIDE_APPLY_READ;
                ride_boot_pending=false;
            }
        }
#if DPC245_APP_SERVICE_TX
        send_service_ack(&router);
#endif
        controller_config_service(&ctrl_txn,&app,&router,&ride_apply,&ride_txn_owner,
                                  &ble_app,ride_controller_seen,now);
        dpc245_ble_app_apply_advanced_cache(&ble_app,&app.advanced);
        {
            evist_ride_cfg_t wheel_ride;
            if(evist_advanced_take_ride(&app.advanced,&wheel_ride)){
                app.ride_cfg=wheel_ride;app.ride_cfg_dirty=true;
            }
        }
        {
            /* LIGHT edits (GAIN, DWELL, ...) act on the runtime before SAVE. */
            evist_preferences_t light_runtime;
            if(evist_advanced_take_light_runtime(&app.advanced,&light_runtime))
                evist_light_configure(&light,light_runtime.light_mode==EVIST_PREF_LIGHT_AUTO,
                                      light_runtime.light_gain,light_runtime.light_dwell,
                                      light_runtime.light_threshold,light_runtime.light_level);
        }

        if(dpc245_app_take_light_toggle(&app)){
            (void)evist_light_user_toggle(&light);
            /* Long-UP controls the bike light state; display brightness remains
             * governed by the selected display-lighting profile. */
        }
        stage_start=stage_end(&app,ST_APP,stage_start);

        {
            evist_preferences_t changed;
            if(dpc245_app_take_preferences_dirty(&app,&changed)){
#if DPC245_APP_PERSIST_WRITE
                /* Runtime values apply immediately, including on memory failure. */
                preferences=changed;
                evist_light_configure(&light,changed.light_mode==EVIST_PREF_LIGHT_AUTO,
                                      changed.light_gain,changed.light_dwell,
                                      changed.light_threshold,changed.light_level);
                dpc245_backlight_set_position(evist_light_backlight_position(&light));
                if(store_ok)native_pending=true;
                else if(app.screen==DPC245_APP_SCREEN_ADVANCED)
                    evist_advanced_set_status(&app.advanced,evist_native_result_text(EVIST_NATIVE_NO_MEMORY));
#else
                (void)changed;
                if(app.screen==DPC245_APP_SCREEN_ADVANCED)
                    evist_advanced_set_status(&app.advanced,"SAVING DISABLED IN THIS BUILD");
#endif
            }
        }

        if(dpc245_app_take_settings_dirty(&app)){
            light.sensitivity=dpc245_al_sensitivity(&app.runtime.settings);
#if DPC245_APP_PERSIST_WRITE
            if(store_ok)native_pending=true;
            else if(app.theme_save_pending){
                dpc245_app_theme_store_result(&app,false,app.theme_save_target);
                if(app.screen==DPC245_APP_SCREEN_THEMES_MENU)
                    app.theme_status=evist_native_result_text(EVIST_NATIVE_NO_MEMORY);
            }
#else
            if(app.theme_save_pending){
                dpc245_app_theme_store_result(&app,true,app.theme_save_target);
                if(app.screen==DPC245_APP_SCREEN_THEMES_MENU)
                    app.theme_status="SAVING DISABLED IN THIS BUILD";
            }
#endif
        }
        /* Ride-time counters advance in 500 ms steps while speed is non-zero. */
        bool moving=router.telemetry.have_speed && router.telemetry.speed_centi_kph!=0u;
        if(!moving)last_ride_500=now;
        else if((uint32_t)(now-last_ride_500)>=500u){
            dpc245_runtime_tick_500ms(&app.runtime,true);
            last_ride_500=now;
        }
#if DPC245_APP_PERSIST_WRITE
        /* Distance and ride time are accumulated locally. Persist once at a
         * transition to rest rather than on every CAN distance update. */
        if(was_moving&&!moving&&store_ok)native_pending=true;
#endif
#if DPC245_APP_PERSIST_WRITE
        was_moving=moving;
        if(store_ok&&native_pending&&!native_store.busy){
            pending_values.preferences=preferences;
            pending_values.hmi=app.runtime.settings;
            pending_values.ride=ride_saved;
            bool accepted=evist_native_store_begin(&native_store,&pending_values);
            native_pending=false;
            uint8_t target_theme=dpc245_theme(&pending_values.hmi);
            bool theme_request=app.theme_save_pending && target_theme==app.theme_save_target;
            if(!accepted && theme_request)dpc245_app_theme_store_result(&app,false,target_theme);
            const char *save_text=evist_native_result_text(accepted?EVIST_NATIVE_BUSY:EVIST_NATIVE_INVALID);
            if(app.screen==DPC245_APP_SCREEN_ADVANCED)evist_advanced_set_status(&app.advanced,save_text);
            else if(app.screen==DPC245_APP_SCREEN_THEMES_MENU && theme_request)app.theme_status=save_text;
        }
        if(store_ok&&native_store.busy){
            dpc245_spi_mem_set_limits(DPC245_SPI_STEP_FLAG_POLLS,DPC245_SPI_STEP_STATUS_POLLS);
            evist_native_result_t result=evist_native_store_step(&native_store,&native_io,now);
            dpc245_spi_mem_end_step();
            if(native_store.valid){persist_slot=(uint8_t)native_store.page;persist_loaded=true;}
            if(result!=EVIST_NATIVE_BUSY){
                uint8_t target_theme=dpc245_theme(&native_store.target.hmi);
                bool theme_request=app.theme_save_pending && target_theme==app.theme_save_target;
                dpc245_app_theme_store_result(&app,result==EVIST_NATIVE_OK,target_theme);
                const char *save_text=evist_native_result_text(result==EVIST_NATIVE_OK&&native_pending?
                    EVIST_NATIVE_BUSY:result);
                if(app.screen==DPC245_APP_SCREEN_ADVANCED)evist_advanced_set_status(&app.advanced,save_text);
                else if(app.screen==DPC245_APP_SCREEN_THEMES_MENU && theme_request)
                    app.theme_status=evist_native_result_text(result);
            }
        }
#endif
        stage_start=stage_end(&app,ST_SPI,stage_start);
        /* A release during a long store step must clear Walk before CAN TX. */
        if((dpc245_buttons_take_released()&DPC245_KEY_DOWN)!=0u){
            dpc245_app_down_state(&app,false,now);
            key_event_seen=true;
        }

#if DPC245_APP_SERVICE_TX
        /* Respond to read-only BESST/service queries addressed to the HMI using
         * the long-transfer service handshake. */
        {
            bafang_can_frame_t sr_tx;
            if(dpc245_hmi_service_tx_next(&hmi_service_tx,&sr_tx))
                (void)send_normal(&router,&sr_tx);
        }

        /* Multiframe identity/service replies require ACK after START/CONT/END. */
        send_service_ack(&router);

        if(app.screen!=previous_info_screen){
            previous_info_screen=app.screen; info_query_index=0u; last_info_query=0u;
        }
        if((uint32_t)(now-last_info_query)>=250u){
            size_t qcount=dpc245_service_query_count(app.screen);
            if(qcount!=0u){
                bafang_can_frame_t iq;
                if(info_query_index>=qcount)info_query_index=0u;
                if(dpc245_build_service_query(app.screen,info_query_index,&iq))
                    (void)send_normal(&router,&iq);
                info_query_index=(info_query_index+1u)%qcount;
            }
            last_info_query=now;
        }
#endif

#if DPC245_APP_CAN_TX
        const uint32_t tx_due=dpc245_v3_tx_schedule_poll(&tx_schedule,now);
        for(size_t i=0u;i<dpc245_v3_periodic_can_count;i++){
            const dpc245_v3_periodic_can_t *sp=&dpc245_v3_periodic_can[i];
            if((tx_due&(1u<<i))!=0u){
                bafang_can_frame_t tx;
                uint8_t event_bits = sp->can_id==0x03106300u ?
                                     dpc245_ble_app_take_hmi_event_bits(&ble_app) : 0u;
                if(dpc245_build_periodic_frame(sp->can_id,&app,light.light_on,event_bits,1u,&tx))
                    (void)send_normal(&router,&tx);
            }
        }
#endif

#if DPC245_APP_BLE_TX
        stage_start=stage_end(&app,ST_CAN_TX,stage_start);
        if((uint32_t)(now-last_ble_500)>=500u){
            static uint8_t ble_periodic[DPC245_BLE_MAX_WIRE];
            size_t ble_n=dpc245_ble_app_build_periodic(ble_periodic,sizeof(ble_periodic),
                                                       &app,&router,light.light_on);
            if(ble_n!=0u)
                (void)dpc245_ble_uart_write(ble_periodic,(uint16_t)ble_n,100000u);
            last_ble_500=now;
        }
#else
        stage_start=stage_end(&app,ST_CAN_TX,stage_start);
#endif
        stage_start=stage_end(&app,ST_BLE_TX,stage_start);

        if(key_event_seen){
            redraw_pending=true;
            if(key_navigation_seen && ui_strip_render_active()){
                ui_strip_render_cancel();
            }
        }
        if(!ui_strip_render_active() && (redraw_pending||(uint32_t)(now-last_draw)>=250u)){
            render_app=app;
            render_router=router;
            render_adc=adc;
            render_hw=hw;
            render_ctx=(screen_ctx_t){&render_app,&render_router,&render_adc,&render_hw,persist_slot,persist_loaded,now,
                {light.light_on,have_ble_app_rx && (uint32_t)(now-last_ble_app_rx)<=5000u,
                 app.runtime.have_controller_error && app.runtime.controller_error>=4u && app.runtime.controller_error!=6u}};
            ui_strip_render_begin(screen_draw,&render_ctx,screen_push);
            redraw_pending=false;
            last_draw=now;
        }
        if(ui_strip_render_active()){
            (void)ui_strip_render_step(3u);
            if(!ui_strip_render_active())
                dpc245_app_dashboard_render_complete(&app,render_ctx.app,millis());
            if(navigation_frame_pending && !ui_strip_render_active())
                navigation_frame_pending=false;
        }
        (void)stage_end(&app,ST_LCD,stage_start);
    }
}
