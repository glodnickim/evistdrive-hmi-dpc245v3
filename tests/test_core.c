/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "boot_splash.h"
#include "ride_mode.h"
#define RIDE_CFG(m, s) ((evist_ride_cfg_t){ (m), (s), EVIST_RIDE_LEGAL_X100, \
    (m)==EVIST_RIDE_OFFROAD?EVIST_RIDE_START_OFFROAD:EVIST_RIDE_START_LEGAL, (m) })
#include "loop_stats.h"
#include "bafang_can.h"
#include "bafang_service.h"
#include "can_router.h"
#include "bafang_config.h"
#include "light_controller.h"
#include "dpc245_backlight_model.h"
#include "dpc245_ble_proto.h"
#include "dpc245_ble_app.h"
#include "dpc245_v3_profile.h"
#include "dpc245_settings.h"
#include "dpc245_persist.h"
#include "dpc245_runtime.h"
#include "dpc245_app.h"
#include "dpc245_keys.h"
#include "dpc245_tx.h"
#include "dpc245_hmi_service.h"
#include "dpc245_update.h"
#include "dpc245_rtc.h"
#include "dpc245_hmi_control.h"
#include "evist_preferences.h"
#include "advanced_settings.h"
#include "controller_write.h"
#include "assist_profile.h"
#include "dpc245_recovery_trigger.h"
#include "dpc245_clock_tree.h"
#include "dpc245_buttons.h"
#include "app_config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>


typedef struct { uint8_t mem[0x11000]; } fake_mem_t;
static bool fm_read(void *ctx,uint32_t a,uint8_t *d,size_t n){fake_mem_t *m=(fake_mem_t*)ctx;if(a+n>sizeof(m->mem))return false;memcpy(d,&m->mem[a],n);return true;}
static bool fm_write(void *ctx,uint32_t a,const uint8_t *d,size_t n){fake_mem_t *m=(fake_mem_t*)ctx;if(a+n>sizeof(m->mem))return false;for(size_t i=0;i<n;i++)m->mem[a+i]&=d[i];return true;}
static bool fm_erase(void *ctx,uint32_t a){fake_mem_t *m=(fake_mem_t*)ctx;uint32_t p=a&~0xffu;if(p+0x100u>sizeof(m->mem))return false;memset(&m->mem[p],0xff,0x100u);return true;}

static uint8_t pref_mem[0x200000];
static bool pm_read(void *ctx,uint32_t a,uint8_t *d,size_t n){(void)ctx;if(a+n>sizeof(pref_mem))return false;memcpy(d,&pref_mem[a],n);return true;}
static bool pm_write(void *ctx,uint32_t a,const uint8_t *d,size_t n){(void)ctx;if(a+n>sizeof(pref_mem))return false;for(size_t i=0;i<n;i++)pref_mem[a+i]&=d[i];return true;}
static bool pm_erase(void *ctx,uint32_t a){(void)ctx;uint32_t p=a&~0xffu;if(p+0x100u>sizeof(pref_mem))return false;memset(&pref_mem[p],0xff,0x100u);return true;}
#if !EVIST_ENABLE_PERSISTENCE
static unsigned ride_write_attempts;
static bool ride_write_probe(void *ctx,uint32_t a,const uint8_t *d,size_t n){ride_write_attempts++;return pm_write(ctx,a,d,n);}
#endif

/* Marker-clear fault simulation: fail the first `clear_fail_left` calls. */
static unsigned clear_calls, clear_fail_left;
static bool fake_clear(void){clear_calls++;if(clear_fail_left){clear_fail_left--;return false;}return true;}

typedef struct { uint8_t r[256]; unsigned writes; } rtc_fake_t;
static bool rtc_fake_read(void *ctx,uint8_t reg,uint8_t *v){rtc_fake_t *x=(rtc_fake_t*)ctx;*v=x->r[reg];return true;}
static bool rtc_fake_write(void *ctx,uint8_t reg,uint8_t v){rtc_fake_t *x=(rtc_fake_t*)ctx;x->r[reg]=v;x->writes++;return true;}

static bafang_can_frame_t ride_reply(uint8_t op, uint8_t cmd, const uint8_t *payload, uint8_t len) {
    bafang_can_frame_t f={0};
    f.id=bafang_can_id_encode((bafang_can_id_t){BAFANG_NODE_CONTROLLER,BAFANG_NODE_HMI,op,cmd,3u});
    f.dlc=len;
    if(payload)memcpy(f.data,payload,len);
    return f;
}

/* 0x3200 realtime frame with a known torque sample and cadence. */
static bafang_can_frame_t pedal_frame(uint16_t torque, uint8_t cadence) {
    bafang_can_frame_t f={0};
    f.id=0x02F83200u;
    f.dlc=8u;
    f.data[3]=cadence;
    f.data[4]=(uint8_t)torque;
    f.data[5]=(uint8_t)(torque>>8);
    return f;
}

static void pedal_at(dpc245_runtime_t *rt, uint32_t now_ms, uint16_t torque, uint8_t cadence) {
    bafang_can_frame_t f=pedal_frame(torque,cadence);
    rt->now_ms=now_ms;
    (void)dpc245_runtime_on_can(rt,&f);
}

/* Exercise the same nonblocking read/write transaction driven by main.c. */
#if EVIST_ENABLE_RIDE_SPEED_WRITE
static void ride_controller_case(unsigned fault) {
    evist_ride_apply_t a;evist_ctrl_txn_t txn;
    evist_ride_apply_init(&a);evist_ctrl_txn_init(&txn);
    evist_ride_apply_begin(&a,3000u);
    const uint8_t original[6]={0xC4u,0x09u,0x1Au,0x00u,0xA4u,0x08u};
    const uint8_t wanted[6]={0xB8u,0x0Bu,0x1Au,0x00u,0xA4u,0x08u};
    unsigned reads=0u,writes=0u;
    for(unsigned attempt=0u;attempt<3u && a.phase!=EVIST_RIDE_APPLY_OK && a.phase!=EVIST_RIDE_APPLY_FAILED;attempt++){
        uint32_t now=attempt*2000u;
        assert(evist_ride_apply_start_txn(&a,&txn,now));
        bafang_can_frame_t tx;
        assert(evist_ctrl_txn_next_tx(&txn,now,&tx));reads++;
        assert(tx.id==bafang_can_id_encode((bafang_can_id_t){BAFANG_NODE_HMI,BAFANG_NODE_CONTROLLER,BAFANG_CAN_OP_READ,0x32u,3u}) && tx.dlc==0u);
        if(fault==0u)printf("ride boot READ EFF %08lX DLC0\n",(unsigned long)tx.id);
        if(fault==2u){
            evist_ctrl_txn_tick(&txn,now+221u);
            assert(evist_ctrl_txn_next_tx(&txn,now+221u,&tx));
            assert(tx.id==bafang_can_id_encode((bafang_can_id_t){BAFANG_NODE_HMI,BAFANG_NODE_CONTROLLER,BAFANG_CAN_OP_READ,0xF2u,3u}));
            evist_ctrl_txn_tick(&txn,now+502u);
        }else{
            bafang_can_frame_t reply=ride_reply(BAFANG_CAN_OP_NORMAL_ACK,0x32u,original,6u);
            if(fault==0u)printf("ride boot READ RESP EFF %08lX DLC6 C4 09 1A 00 A4 08\n",(unsigned long)reply.id);
            evist_ctrl_txn_on_frame(&txn,&reply,now+1u);
        }
        evist_txn_kind_t kind;evist_txn_result_t result;uint8_t data[64]={0};
        assert(evist_ctrl_txn_take_result(&txn,&kind,&result,data));
        assert(kind==EVIST_TXN_READ_WHEEL);
        evist_ride_apply_on_result(&a,result,data);
        if(a.phase!=EVIST_RIDE_APPLY_WRITE)continue;
        assert(evist_ride_apply_start_txn(&a,&txn,now+510u));
        assert(evist_ctrl_txn_next_tx(&txn,now+510u,&tx));writes++;
        assert(tx.id==bafang_can_id_encode((bafang_can_id_t){BAFANG_NODE_HMI,BAFANG_NODE_CONTROLLER,BAFANG_CAN_OP_WRITE,0x32u,3u}) && tx.dlc==6u && memcmp(tx.data,wanted,6u)==0);
        if(fault==0u)printf("ride boot WRITE EFF %08lX DLC6 B8 0B 1A 00 A4 08\n",(unsigned long)tx.id);
        if(fault==1u){
            const uint8_t reason2[4]={1u,0u,2u,6u};
            bafang_can_frame_t nack=ride_reply(BAFANG_CAN_OP_ERROR_ACK,0x32u,reason2,4u);
            evist_ctrl_txn_on_frame(&txn,&nack,now+511u);
            evist_ctrl_txn_tick(&txn,now+1110u);
            evist_ctrl_txn_tick(&txn,now+1510u);
            assert(evist_ctrl_txn_next_tx(&txn,now+1510u,&tx));
            evist_ctrl_txn_tick(&txn,now+2110u);
        }else if(fault==4u)evist_ctrl_txn_tick(&txn,now+1111u);
        else{
            bafang_can_frame_t ack=ride_reply(BAFANG_CAN_OP_NORMAL_ACK,0x32u,0,0u);
            if(fault==0u)printf("ride boot ACK EFF %08lX DLC0\n",(unsigned long)ack.id);
            evist_ctrl_txn_on_frame(&txn,&ack,now+511u);
            evist_ctrl_txn_tick(&txn,now+512u);
            evist_ctrl_txn_tick(&txn,now+912u);
            assert(evist_ctrl_txn_next_tx(&txn,now+912u,&tx));
            assert(tx.id==bafang_can_id_encode((bafang_can_id_t){BAFANG_NODE_HMI,BAFANG_NODE_CONTROLLER,BAFANG_CAN_OP_READ,0x32u,3u}) && tx.dlc==0u);
            if(fault==0u)printf("ride boot REREAD EFF %08lX DLC0\n",(unsigned long)tx.id);
            uint8_t returned[6];memcpy(returned,wanted,6u);
            if(fault==3u)returned[4]++;
            bafang_can_frame_t verify=ride_reply(BAFANG_CAN_OP_NORMAL_ACK,0x32u,returned,6u);
            if(fault==0u)printf("ride boot REREAD RESP EFF %08lX DLC6 B8 0B 1A 00 A4 08\n",(unsigned long)verify.id);
            evist_ctrl_txn_on_frame(&txn,&verify,now+913u);
        }
        memset(data,0,sizeof(data));
        assert(evist_ctrl_txn_take_result(&txn,&kind,&result,data));
        assert(kind==EVIST_TXN_WRITE_WHEEL);
        if(fault==1u)assert(result==EVIST_TXN_NO_READBACK);
        if(fault==3u)assert(result==EVIST_TXN_MISMATCH);
        if(fault==4u)assert(result==EVIST_TXN_NO_ACK);
        evist_ride_apply_on_result(&a,result,data);
    }
    assert(reads<=3u && writes<=3u);
    if(fault==0u)assert(a.phase==EVIST_RIDE_APPLY_OK && writes==1u && a.last_applied_x100==3000u);
    else assert(a.phase==EVIST_RIDE_APPLY_FAILED && reads==3u && (fault==2u?writes==0u:writes==3u));
    dpc245_settings_t settings;dpc245_settings_defaults(&settings);dpc245_app_t app;
    dpc245_app_init(&app,&settings);dpc245_app_ride_applied(&app,&a);
    assert(app.ride_apply_result==a.phase && app.ride_last_error==a.last_result);
    printf("ride CAN fault=%u: reads=%u writes=%u result=%s\n",fault,reads,writes,
           a.phase==EVIST_RIDE_APPLY_OK?"OK":"FAILED");
}
#endif

static void ride_behavior_tests(void) {
    dpc245_persist_io_t io={pm_read,pm_write,pm_erase,0};
    evist_ride_cfg_t cfg=RIDE_CFG(EVIST_RIDE_LEGAL,EVIST_RIDE_OFFROAD_DEFAULT),loaded;
    uint32_t next;
    memset(&pref_mem[EVIST_PREF_RIDE_BASE],0xff,EVIST_PREF_RIDE_END-EVIST_PREF_RIDE_BASE);
    assert(!evist_ride_store_load(&io,&loaded,&next));
    assert(loaded.mode==EVIST_RIDE_LEGAL && next==EVIST_PREF_RIDE_BASE);
    cfg.mode=EVIST_RIDE_OFFROAD;cfg.offroad_x100=3500u;
    uint8_t older_record[EVIST_RIDE_REC_SIZE],prior_record[EVIST_RIDE_REC_SIZE];
    assert(evist_ride_record_encode(&loaded,older_record));
    assert(evist_ride_record_encode(&cfg,prior_record));
    memcpy(&pref_mem[EVIST_PREF_RIDE_BASE],older_record,sizeof(older_record));
    memcpy(&pref_mem[EVIST_PREF_RIDE_BASE+EVIST_RIDE_REC_SIZE],prior_record,sizeof(prior_record));
    assert(evist_ride_boot_load(&io,&loaded,&next) && loaded.mode==EVIST_RIDE_OFFROAD &&
           loaded.offroad_x100==3500u && evist_ride_target_x100(&loaded)==3500u &&
           next==EVIST_PREF_RIDE_BASE+2u*EVIST_RIDE_REC_SIZE);
#if !EVIST_ENABLE_PERSISTENCE
    evist_ride_cfg_t changed=RIDE_CFG(EVIST_RIDE_LEGAL,3500u);
    dpc245_persist_io_t probe_io=io;probe_io.write=ride_write_probe;ride_write_attempts=0u;
    assert(evist_ride_store_append(&probe_io,&loaded,&changed,&next)==EVIST_RIDE_STORE_NO_MEMORY);
    assert(ride_write_attempts==0u && next==EVIST_PREF_RIDE_BASE+2u*EVIST_RIDE_REC_SIZE);
    assert(memcmp(&pref_mem[EVIST_PREF_RIDE_BASE],older_record,sizeof(older_record))==0);
    assert(memcmp(&pref_mem[EVIST_PREF_RIDE_BASE+EVIST_RIDE_REC_SIZE],prior_record,sizeof(prior_record))==0);
    assert(evist_ride_boot_load(&io,&loaded,&next) && loaded.mode==EVIST_RIDE_OFFROAD);
#endif
    assert(!evist_ride_boot_load(0,&loaded,&next) && loaded.mode==EVIST_RIDE_LEGAL &&
           next==EVIST_PREF_RIDE_END);
    memset(&pref_mem[EVIST_PREF_RIDE_BASE],0xff,EVIST_PREF_RIDE_END-EVIST_PREF_RIDE_BASE);
    assert(!evist_ride_boot_load(&io,&loaded,&next) && loaded.mode==EVIST_RIDE_LEGAL &&
           next==EVIST_PREF_RIDE_BASE);
    loaded=evist_ride_defaults();next=EVIST_PREF_RIDE_BASE;
#if EVIST_ENABLE_PERSISTENCE
    assert(evist_ride_store_append(&io,&loaded,&cfg,&next)==EVIST_RIDE_STORE_OK);
    assert(strcmp(evist_ride_store_status(EVIST_RIDE_STORE_OK),"SAVED")==0);
    assert(strcmp(evist_ride_store_status(EVIST_RIDE_STORE_FULL),"NOT SAVED - LOG FULL")==0);
    assert(next==EVIST_PREF_RIDE_BASE+6u);
    assert(evist_ride_store_append(&io,&cfg,&cfg,&next)==EVIST_RIDE_STORE_OK && next==EVIST_PREF_RIDE_BASE+6u);
    assert(evist_ride_store_load(&io,&loaded,&next) && loaded.mode==EVIST_RIDE_OFFROAD && loaded.offroad_x100==3500u);
    pref_mem[next]=0x00u; /* torn/nonblank record; append skips it */
    assert(evist_ride_store_load(&io,&loaded,&next) && next==EVIST_PREF_RIDE_BASE+12u);
    evist_ride_cfg_t legal=RIDE_CFG(EVIST_RIDE_LEGAL,3000u);
    assert(evist_ride_store_append(&io,&loaded,&legal,&next)==EVIST_RIDE_STORE_OK);
    assert(evist_ride_store_load(&io,&loaded,&next) && loaded.mode==EVIST_RIDE_LEGAL && loaded.offroad_x100==3000u);
    next=EVIST_PREF_RIDE_END;
    assert(evist_ride_store_append(&io,&loaded,&cfg,&next)==EVIST_RIDE_STORE_FULL);
#else
    assert(evist_ride_boot_load(&io,&loaded,&next)==false && loaded.mode==EVIST_RIDE_LEGAL);
    assert(evist_ride_store_append(&io,&loaded,&cfg,&next)==EVIST_RIDE_STORE_NO_MEMORY);
    assert(next==EVIST_PREF_RIDE_BASE);
    for(size_t i=0u;i<EVIST_RIDE_REC_SIZE;i++)assert(pref_mem[EVIST_PREF_RIDE_BASE+i]==0xffu);
    assert(strcmp(evist_ride_store_status(EVIST_RIDE_STORE_NO_MEMORY),"SAVING DISABLED IN THIS BUILD")==0);
#endif
    assert(evist_ride_store_append(0,&loaded,&cfg,&next)==EVIST_RIDE_STORE_NO_MEMORY);
#if EVIST_ENABLE_RIDE_SPEED_WRITE
    for(unsigned i=0;i<5u;i++)ride_controller_case(i);
#endif
    {
        evist_ride_apply_t a;evist_ctrl_txn_t txn;evist_ride_apply_init(&a);evist_ctrl_txn_init(&txn);
        evist_ride_apply_begin(&a,2500u);assert(evist_ride_apply_start_txn(&a,&txn,0u));
        bafang_can_frame_t tx;assert(evist_ctrl_txn_next_tx(&txn,0u,&tx));
        const uint8_t equal[6]={0xC4u,0x09u,0x1Au,0u,0xA4u,0x08u};
        bafang_can_frame_t reply=ride_reply(BAFANG_CAN_OP_NORMAL_ACK,0x32u,equal,6u);
        evist_ctrl_txn_on_frame(&txn,&reply,1u);
        evist_txn_kind_t kind;evist_txn_result_t result;uint8_t data[64]={0};
        assert(evist_ctrl_txn_take_result(&txn,&kind,&result,data));
        evist_ride_apply_on_result(&a,result,data);
        assert(a.phase==EVIST_RIDE_APPLY_OK && a.attempts==1u);
        assert(!evist_ctrl_txn_next_tx(&txn,2u,&tx));
    }
#if EVIST_ENABLE_RIDE_SPEED_WRITE
    {
        /* No saved record selects LEGAL and writes 25.00 when controller has 30.00. */
        evist_ride_apply_t a;evist_ctrl_txn_t txn;evist_ride_apply_init(&a);evist_ctrl_txn_init(&txn);
        evist_ride_cfg_t no_record=evist_ride_defaults();
        evist_ride_apply_begin(&a,evist_ride_target_x100(&no_record));
        assert(evist_ride_apply_start_txn(&a,&txn,0u));
        bafang_can_frame_t tx;assert(evist_ctrl_txn_next_tx(&txn,0u,&tx));
        const uint8_t old[6]={0xB8u,0x0Bu,0xD0u,0x01u,0xC0u,0x08u};
        bafang_can_frame_t rx=ride_reply(BAFANG_CAN_OP_NORMAL_ACK,0x32u,old,6u);
        evist_ctrl_txn_on_frame(&txn,&rx,1u);
        evist_txn_kind_t kind;evist_txn_result_t result;uint8_t data[64]={0};
        assert(evist_ctrl_txn_take_result(&txn,&kind,&result,data));
        evist_ride_apply_on_result(&a,result,data);
        assert(a.phase==EVIST_RIDE_APPLY_WRITE && evist_ride_apply_start_txn(&a,&txn,2u));
        assert(evist_ctrl_txn_next_tx(&txn,2u,&tx));
        const uint8_t legal_wire[6]={0xC4u,0x09u,0xD0u,0x01u,0xC0u,0x08u};
        assert(tx.dlc==6u && memcmp(tx.data,legal_wire,6u)==0);
    }
#endif
    dpc245_settings_t settings;dpc245_settings_defaults(&settings);dpc245_app_t app;
    dpc245_app_init(&app,&settings);evist_ride_cfg_t initial=RIDE_CFG(EVIST_RIDE_LEGAL,3500u);dpc245_app_set_ride(&app,&initial);
    app.screen=DPC245_APP_SCREEN_DASHBOARD;
    app.preferences.quick_walk_enabled=true;
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_PRESS,50u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,100u);
    assert(app.ride_combo_step==1u && app.assist_level==1u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_DOWN_PRESS,150u);
    assert(!app.walk_armed);
    dpc245_app_event(&app,DPC245_KEY_EVENT_DOWN_SHORT,200u);
    assert(app.ride_combo_step==2u && app.assist_level==0u && !app.walk);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_PRESS,250u);
    assert(app.ride_combo_step==2u && app.assist_level==0u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,300u);
    assert(app.ride_confirm_pending && app.assist_level==0u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_POWER_SHORT,400u);
    assert(!app.ride_confirm_pending && app.assist_level==0u);
    uint16_t target=0u;assert(dpc245_app_take_ride_apply(&app,&target) && target==3500u);
    assert(!dpc245_app_take_ride_apply(&app,&target));
    evist_ride_apply_t applied;evist_ride_apply_init(&applied);applied.phase=EVIST_RIDE_APPLY_OK;
    applied.last_applied_x100=3500u;dpc245_app_ride_applied(&app,&applied);
    assert(app.ride_session==DPC245_RIDE_SESSION_OFFROAD && app.ride_indicator==DPC245_RIDE_IND_OFFROAD);
    assert(app.ride_toast_active);
    dpc245_app_tick(&app,500u);assert(app.ride_toast_active);
    dpc245_app_tick(&app,2500u);assert(!app.ride_toast_active);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,1000u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_DOWN_SHORT,1100u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,1200u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_POWER_SHORT,1300u);
    assert(dpc245_app_take_ride_apply(&app,&target) && target==2500u);
    applied.last_applied_x100=2500u;dpc245_app_ride_applied(&app,&applied);
    assert(app.ride_session==DPC245_RIDE_SESSION_LEGAL && app.ride_indicator==DPC245_RIDE_IND_LEGAL);
    /* A saved OFFROAD record can be overridden in both directions for this session. */
    dpc245_app_init(&app,&settings);
    initial=RIDE_CFG(EVIST_RIDE_OFFROAD,3500u);initial.legal_x100=3150u;
    dpc245_app_set_ride(&app,&initial);
    assert(app.ride_session==DPC245_RIDE_SESSION_OFFROAD && app.ride_indicator==DPC245_RIDE_IND_OFFROAD);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,100u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_DOWN_SHORT,200u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,300u);
    assert(app.ride_confirm_pending && app.ride_indicator==DPC245_RIDE_IND_OFFROAD);
    dpc245_app_event(&app,DPC245_KEY_EVENT_POWER_SHORT,400u);
    assert(dpc245_app_take_ride_apply(&app,&target) && target==3150u);
    applied.phase=EVIST_RIDE_APPLY_FAILED;
    dpc245_app_ride_applied(&app,&applied);
    assert(app.ride_session==DPC245_RIDE_SESSION_OFFROAD && app.ride_indicator==DPC245_RIDE_IND_OFFROAD);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,500u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_DOWN_SHORT,600u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,700u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_POWER_SHORT,800u);
    assert(dpc245_app_take_ride_apply(&app,&target) && target==3150u);
    applied.phase=EVIST_RIDE_APPLY_OK;applied.last_applied_x100=3150u;
    dpc245_app_ride_applied(&app,&applied);
    assert(app.ride_session==DPC245_RIDE_SESSION_LEGAL && app.ride_indicator==DPC245_RIDE_IND_LEGAL);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,900u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_DOWN_SHORT,1000u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,1100u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_POWER_SHORT,1200u);
    assert(dpc245_app_take_ride_apply(&app,&target) && target==3500u);
    applied.phase=EVIST_RIDE_APPLY_FAILED;dpc245_app_ride_applied(&app,&applied);
    assert(app.ride_session==DPC245_RIDE_SESSION_LEGAL && app.ride_indicator==DPC245_RIDE_IND_LEGAL);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,1300u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_DOWN_SHORT,1400u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,1500u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_POWER_SHORT,1600u);
    assert(dpc245_app_take_ride_apply(&app,&target) && target==3500u);
    applied.phase=EVIST_RIDE_APPLY_OK;
    applied.last_applied_x100=3500u;dpc245_app_ride_applied(&app,&applied);
    assert(app.ride_session==DPC245_RIDE_SESSION_OFFROAD && app.ride_indicator==DPC245_RIDE_IND_OFFROAD);
    dpc245_app_init(&app,&settings);
    initial=RIDE_CFG(EVIST_RIDE_OFFROAD,3500u);dpc245_app_set_ride(&app,&initial);
    assert(app.ride_session==DPC245_RIDE_SESSION_OFFROAD && app.ride_indicator==DPC245_RIDE_IND_OFFROAD);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,2000u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_DOWN_SHORT,2100u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,2200u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_DOWN_SHORT,2300u);
    assert(!app.ride_confirm_pending && app.assist_level==0u && app.walk_armed);
    dpc245_app_init(&app,&settings);app.screen=DPC245_APP_SCREEN_DASHBOARD;
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,3000u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_DOWN_SHORT,3100u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,3200u);
    dpc245_app_tick(&app,6200u);
    assert(!app.ride_confirm_pending && app.assist_level==0u);
    app.assist_level=2u;dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,7000u);
    assert(app.ride_combo_step==0u);
    app.assist_level=0u;app.walk_armed=true;
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,8000u);
    assert(app.ride_combo_step==0u);
    app.walk_armed=false;
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,9000u);
    dpc245_app_tick(&app,10501u);
    assert(app.ride_combo_step==0u);
    /* Wrong ordering and an active walk must never create a confirmation. */
    dpc245_app_event(&app,DPC245_KEY_EVENT_DOWN_SHORT,11000u);
    assert(app.ride_combo_step==0u && !app.ride_confirm_pending);
    dpc245_app_init(&app,&settings);app.screen=DPC245_APP_SCREEN_DASHBOARD;
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,100u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,200u);
    assert(app.ride_combo_step==0u && !app.ride_confirm_pending);
    dpc245_app_init(&app,&settings);app.screen=DPC245_APP_SCREEN_DASHBOARD;
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,100u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_DOWN_SHORT,200u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,1701u);
    assert(!app.ride_confirm_pending);
    dpc245_app_init(&app,&settings);app.screen=DPC245_APP_SCREEN_DASHBOARD;
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,100u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_DOWN_SHORT,200u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,1600u); /* inclusive 1500 ms window */
    assert(app.ride_confirm_pending);
    dpc245_app_init(&app,&settings);app.screen=DPC245_APP_SCREEN_DASHBOARD;
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,100u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_HOLD,150u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_DOWN_SHORT,200u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,300u);
    assert(!app.ride_confirm_pending);
    dpc245_app_init(&app,&settings);app.screen=DPC245_APP_SCREEN_DASHBOARD;
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_PRESS,50u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,100u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_DOWN_SHORT,200u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_PRESS,250u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_REPEAT,650u);
    assert(app.ride_combo_step==0u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,700u);
    assert(!app.ride_confirm_pending);
    dpc245_app_init(&app,&settings);app.screen=DPC245_APP_SCREEN_DASHBOARD;
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,100u);
    app.walk_armed=true;
    dpc245_app_event(&app,DPC245_KEY_EVENT_DOWN_SHORT,200u);
    assert(!app.ride_confirm_pending && app.ride_combo_step==0u);
    dpc245_app_init(&app,&settings);app.screen=DPC245_APP_SCREEN_DASHBOARD;
    app.walk_active=true;
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,100u);
    assert(!app.ride_confirm_pending && app.ride_combo_step==0u);
    dpc245_app_init(&app,&settings);app.screen=DPC245_APP_SCREEN_DASHBOARD;
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,100u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_DOWN_SHORT,200u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,300u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_PRESS,350u); /* UP press cancels */
    assert(!app.ride_confirm_pending && app.assist_level==0u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,400u);
    assert(!app.ride_confirm_pending && app.assist_level==1u);
    dpc245_app_init(&app,&settings);app.screen=DPC245_APP_SCREEN_DASHBOARD;
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,100u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_DOWN_SHORT,200u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,300u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_DOWN_SHORT,400u); /* DOWN cancels and arms Walk at zero */
    assert(!app.ride_confirm_pending && app.assist_level==0u && app.walk_armed);
    dpc245_app_init(&app,&settings);app.screen=DPC245_APP_SCREEN_DASHBOARD;
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,500u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_DOWN_SHORT,600u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,700u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_POWER_SHORT,3700u); /* exact expiry */
    assert(!app.ride_apply_requested && !app.ride_confirm_pending);
    /* Every cancelling key reaches the ordinary handler within 400 ms. */
    dpc245_app_init(&app,&settings);app.screen=DPC245_APP_SCREEN_DASHBOARD;
    const dpc245_key_event_t trace[]={DPC245_KEY_EVENT_UP_SHORT,DPC245_KEY_EVENT_DOWN_SHORT,
        DPC245_KEY_EVENT_UP_SHORT,DPC245_KEY_EVENT_UP_SHORT,DPC245_KEY_EVENT_DOWN_SHORT,
        DPC245_KEY_EVENT_UP_SHORT,DPC245_KEY_EVENT_DOWN_SHORT,DPC245_KEY_EVENT_DOWN_SHORT,
        DPC245_KEY_EVENT_UP_SHORT,DPC245_KEY_EVENT_POWER_SHORT};
    const uint8_t levels[]={1u,0u,0u,1u,0u,1u,0u,0u,1u,1u};
    for(unsigned i=0u;i<10u;i++){
        dpc245_app_event(&app,trace[i],(i+1u)*400u);
        assert(app.assist_level==levels[i]);
        assert(app.ride_confirm_pending==(i==2u));
        if(i==7u)assert(app.walk_armed);
    }
    assert(app.preferences.lower_panel==1u && !app.ride_apply_requested);
    dpc245_app_init(&app,&settings);app.screen=DPC245_APP_SCREEN_DASHBOARD;
    app.ride_confirm_pending=true;app.ride_confirm_deadline_ms=3000u;
    app.assist_level=1u;app.walk_active=true;
    dpc245_app_tick(&app,3000u);
    assert(!app.ride_confirm_pending && app.assist_level==1u && app.walk_active);
    /* MODE page edit stays on its row and WHEEL MAX is locked in LEGAL. */
    evist_advanced_t adv;evist_preferences_t pref;evist_preferences_defaults(&pref);
    evist_advanced_open(&adv,&pref,5u);evist_ride_cfg_t test_ride=RIDE_CFG(EVIST_RIDE_LEGAL,3000u);evist_advanced_set_ride(&adv,&test_ride);
    adv.screen=EVIST_ADV_MODE;adv.cursor=0u;
    evist_advanced_key(&adv,EVIST_ADV_KEY_POWER);
    evist_advanced_key(&adv,EVIST_ADV_KEY_UP);
    evist_advanced_key(&adv,EVIST_ADV_KEY_POWER);
    assert(!adv.edit && adv.cursor==0u && adv.ride_working.mode==EVIST_RIDE_OFFROAD);
    adv.cursor=3u;evist_advanced_key(&adv,EVIST_ADV_KEY_POWER);
    evist_advanced_key(&adv,EVIST_ADV_KEY_UP);
    evist_advanced_key(&adv,EVIST_ADV_KEY_POWER);
    assert(adv.cursor==3u && adv.ride_working.offroad_x100==3050u);
    adv.cursor=4u;evist_advanced_key(&adv,EVIST_ADV_KEY_POWER);
    evist_ride_cfg_t saved;assert(evist_advanced_take_ride(&adv,&saved) && saved.mode==EVIST_RIDE_OFFROAD && saved.offroad_x100==3050u);
    evist_advanced_set_ride(&adv,&test_ride);
    adv.screen=EVIST_ADV_WHEEL;adv.wheel_valid=true;adv.wheel_speed_centi_kph=4000u;
    adv.wheel_circumference_mm=2200u;adv.wheel_read.wheel_code=26u;adv.cursor=EVIST_WHEEL_ROW_SPEED;
    evist_advanced_key(&adv,EVIST_ADV_KEY_POWER);
    assert(!adv.edit && strcmp(adv.status,"LEGAL MODE - MAX LOCKED")==0);
    adv.cursor=EVIST_WHEEL_ROW_SAVE;evist_advanced_key(&adv,EVIST_ADV_KEY_POWER);
    evist_wheel_config_t wheel;evist_advanced_wheel_target(&adv,&wheel);
    assert(wheel.speed_limit_centi_kph==2500u && wheel.wheel_code==26u && wheel.circumference_mm==2200u);
    evist_advanced_set_ride(&adv,&test_ride);adv.screen=EVIST_ADV_MODE;
    adv.cursor=1u;evist_advanced_key(&adv,EVIST_ADV_KEY_POWER);
    evist_advanced_key(&adv,EVIST_ADV_KEY_DOWN);evist_advanced_key(&adv,EVIST_ADV_KEY_POWER);
    assert(adv.ride_working.start==EVIST_RIDE_START_LAST && !adv.edit);
    adv.cursor=2u;evist_advanced_key(&adv,EVIST_ADV_KEY_POWER);
    evist_advanced_key(&adv,EVIST_ADV_KEY_UP);evist_advanced_key(&adv,EVIST_ADV_KEY_POWER);
    assert(adv.ride_working.legal_x100==2550u && adv.ride_working.offroad_x100==3000u);
    adv.cursor=4u;evist_advanced_key(&adv,EVIST_ADV_KEY_POWER);
    assert(evist_advanced_take_ride(&adv,&saved) && saved.start==EVIST_RIDE_START_LAST &&
           saved.legal_x100==2550u);
    evist_advanced_set_ride(&adv,&test_ride);
    adv.ride_effective_mode=EVIST_RIDE_OFFROAD;adv.screen=EVIST_ADV_MODE;
    adv.cursor=1u;evist_advanced_key(&adv,EVIST_ADV_KEY_POWER);
    evist_advanced_key(&adv,EVIST_ADV_KEY_DOWN);evist_advanced_key(&adv,EVIST_ADV_KEY_POWER);
    adv.cursor=4u;evist_advanced_key(&adv,EVIST_ADV_KEY_POWER);
    assert(evist_advanced_take_ride(&adv,&saved) && saved.start==EVIST_RIDE_START_LAST &&
           saved.last_mode==EVIST_RIDE_OFFROAD);
    dpc245_app_init(&app,&settings);
    app.ride_session=DPC245_RIDE_SESSION_OFFROAD;app.ride_indicator=DPC245_RIDE_IND_OFFROAD;
    uint8_t wheel_verified[64]={0xC4u,0x09u,0x1Au,0u,0x98u,0x08u};
    dpc245_app_ride_wheel_verified(&app,wheel_verified);
    assert(app.ride_session==DPC245_RIDE_SESSION_OFFROAD && app.ride_indicator==DPC245_RIDE_IND_OFFROAD &&
           app.ride_last_applied_x100==2500u && app.ride_apply_result==EVIST_RIDE_APPLY_OK);
    printf("ride persistence/combo tests passed\n");
}

static void ride_v2_tests(void) {
    dpc245_persist_io_t io={pm_read,pm_write,pm_erase,0};
    uint32_t next=EVIST_PREF_RIDE_BASE;
    evist_ride_cfg_t old=RIDE_CFG(EVIST_RIDE_OFFROAD,3000u),loaded;
    uint8_t rec[EVIST_RIDE_REC_SIZE];
    memset(&pref_mem[EVIST_PREF_RIDE_BASE],0xff,EVIST_PREF_RIDE_END-EVIST_PREF_RIDE_BASE);
    assert(evist_ride_record_encode(&old,rec));
    memcpy(&pref_mem[next],rec,sizeof(rec));next+=EVIST_RIDE_REC_SIZE;
    assert(evist_ride_store_load(&io,&loaded,&next));
    assert(loaded.legal_x100==2500u && loaded.start==EVIST_RIDE_START_LEGAL &&
           evist_ride_boot_mode(&loaded)==EVIST_RIDE_LEGAL);
    evist_ride_cfg_t new_cfg=loaded;
    new_cfg.legal_x100=3000u;new_cfg.offroad_x100=3500u;
    new_cfg.start=EVIST_RIDE_START_LAST;new_cfg.last_mode=EVIST_RIDE_OFFROAD;
    assert(evist_ride_cfg_valid(&new_cfg) && !evist_ride_cfg_valid(&(evist_ride_cfg_t){
        EVIST_RIDE_LEGAL,2500u,3000u,EVIST_RIDE_START_LAST,EVIST_RIDE_LEGAL}));
    assert(evist_ride_record_v2_encode(&new_cfg,rec));
    evist_ride_cfg_t decoded;
    assert(evist_ride_record_v2_decode(rec,&decoded) && decoded.legal_x100==3000u &&
           decoded.start==EVIST_RIDE_START_LAST && decoded.last_mode==EVIST_RIDE_OFFROAD);
    rec[5]^=1u;assert(!evist_ride_record_v2_decode(rec,&decoded));
    assert(evist_ride_legal_speed_valid(1500u) && evist_ride_legal_speed_valid(3200u) &&
           !evist_ride_legal_speed_valid(1450u) && !evist_ride_legal_speed_valid(3250u));
#if EVIST_ENABLE_PERSISTENCE
    assert(evist_ride_store_append(&io,&loaded,&new_cfg,&next)==EVIST_RIDE_STORE_OK);
    assert(next==EVIST_PREF_RIDE_BASE+12u);
    assert(evist_ride_store_load(&io,&loaded,&next) && loaded.legal_x100==3000u &&
           loaded.start==EVIST_RIDE_START_LAST && evist_ride_boot_mode(&loaded)==EVIST_RIDE_OFFROAD);
    evist_ride_cfg_t speed_only=new_cfg;speed_only.legal_x100=3150u;
    assert(evist_ride_store_append(&io,&new_cfg,&speed_only,&next)==EVIST_RIDE_STORE_OK);
    assert(next==EVIST_PREF_RIDE_BASE+18u);
    pref_mem[next]=0x00u;
    assert(evist_ride_store_load(&io,&loaded,&next) && next==EVIST_PREF_RIDE_BASE+24u &&
           loaded.legal_x100==3150u);
    evist_ride_cfg_t choice=speed_only;choice.start=EVIST_RIDE_START_OFFROAD;
    assert(evist_ride_store_append(&io,&speed_only,&choice,&next)==EVIST_RIDE_STORE_OK);
    assert(evist_ride_store_load(&io,&loaded,&next) && loaded.start==EVIST_RIDE_START_OFFROAD);
    for(unsigned i=0u;i<3u;i++) {
        loaded.start=(evist_ride_start_t)i;
        evist_ride_mode_t want=i==EVIST_RIDE_START_LEGAL?EVIST_RIDE_LEGAL:EVIST_RIDE_OFFROAD;
        assert(evist_ride_boot_mode(&loaded)==want);
    }
    dpc245_settings_t settings;dpc245_settings_defaults(&settings);
    dpc245_app_t app;dpc245_app_init(&app,&settings);
    loaded.start=EVIST_RIDE_START_LAST;loaded.last_mode=EVIST_RIDE_OFFROAD;
    dpc245_app_set_ride(&app,&loaded);
    assert(app.ride_session==DPC245_RIDE_SESSION_OFFROAD);
    app.ride_combo_apply=true;app.ride_pending_session=DPC245_RIDE_SESSION_LEGAL;
    evist_ride_apply_t applied;evist_ride_apply_init(&applied);
    applied.phase=EVIST_RIDE_APPLY_OK;applied.last_applied_x100=3150u;
    dpc245_app_ride_applied(&app,&applied);
    assert(app.ride_cfg.last_mode==EVIST_RIDE_LEGAL && app.ride_cfg_dirty);
    evist_ride_cfg_t switched;
    assert(dpc245_app_take_ride_cfg_dirty(&app,&switched));
    assert(evist_ride_store_append(&io,&choice,&switched,&next)==EVIST_RIDE_STORE_OK);
    assert(evist_ride_store_load(&io,&loaded,&next) && loaded.last_mode==EVIST_RIDE_LEGAL &&
           evist_ride_boot_mode(&loaded)==EVIST_RIDE_LEGAL);
    dpc245_app_init(&app,&settings);dpc245_app_set_ride(&app,&loaded);
    assert(app.ride_session==DPC245_RIDE_SESSION_LEGAL && app.ride_cfg.legal_x100==3150u);
    app.ride_pending_session=DPC245_RIDE_SESSION_LEGAL;app.ride_apply_requested=true;
    uint16_t target=0u;assert(dpc245_app_take_ride_apply(&app,&target) && target==3150u);
    evist_advanced_t adv;evist_preferences_t prefs;evist_preferences_defaults(&prefs);
    evist_advanced_open(&adv,&prefs,5u);evist_advanced_set_ride(&adv,&loaded);
    adv.ride_effective_mode=EVIST_RIDE_LEGAL;
    adv.screen=EVIST_ADV_WHEEL;adv.wheel_valid=true;adv.wheel_speed_centi_kph=4000u;
    adv.wheel_circumference_mm=2200u;adv.wheel_read.wheel_code=26u;
    evist_wheel_config_t wheel;evist_advanced_wheel_target(&adv,&wheel);
    assert(wheel.speed_limit_centi_kph==3150u);
    adv.cursor=EVIST_WHEEL_ROW_SPEED;evist_advanced_key(&adv,EVIST_ADV_KEY_POWER);
    assert(!adv.edit && strcmp(adv.status,"LEGAL MODE - MAX LOCKED")==0);
    /* A later version-1 setting is a new update, not the paired slot. */
    evist_ride_cfg_t older_update=RIDE_CFG(EVIST_RIDE_OFFROAD,4000u);
    assert(evist_ride_record_encode(&older_update,rec));
    memcpy(&pref_mem[next],rec,sizeof(rec));
    assert(evist_ride_store_load(&io,&loaded,&next) && loaded.mode==EVIST_RIDE_OFFROAD &&
           loaded.offroad_x100==4000u && loaded.legal_x100==3150u &&
           loaded.start==EVIST_RIDE_START_LAST && loaded.last_mode==EVIST_RIDE_OFFROAD);
    evist_ride_cfg_t scanned;
    assert(evist_ride_log_scan(&pref_mem[EVIST_PREF_RIDE_BASE],
           (next-EVIST_PREF_RIDE_BASE)/EVIST_RIDE_REC_SIZE,&scanned));
    assert(scanned.mode==loaded.mode && scanned.offroad_x100==loaded.offroad_x100 &&
           scanned.legal_x100==loaded.legal_x100 && scanned.start==loaded.start &&
           scanned.last_mode==loaded.last_mode);
    memset(&pref_mem[EVIST_PREF_RIDE_BASE],0xff,EVIST_PREF_RIDE_END-EVIST_PREF_RIDE_BASE);
    evist_ride_cfg_t base=evist_ride_defaults();
    base.legal_x100=2000u;base.offroad_x100=3000u;base.start=EVIST_RIDE_START_OFFROAD;
    assert(evist_ride_record_v2_encode(&base,rec));
    memcpy(&pref_mem[EVIST_PREF_RIDE_BASE],rec,sizeof(rec));
    older_update.offroad_x100=2200u;
    older_update.legal_x100=2000u;
    assert(evist_ride_record_encode(&older_update,rec));
    memcpy(&pref_mem[EVIST_PREF_RIDE_BASE+EVIST_RIDE_REC_SIZE],rec,sizeof(rec));
    assert(evist_ride_store_load(&io,&loaded,&next) && loaded.legal_x100==2000u &&
           loaded.start==EVIST_RIDE_START_OFFROAD && loaded.mode==EVIST_RIDE_OFFROAD &&
           loaded.offroad_x100==2200u);
    assert(evist_ride_log_scan(&pref_mem[EVIST_PREF_RIDE_BASE],2u,&scanned) &&
           scanned.legal_x100==loaded.legal_x100 && scanned.start==loaded.start &&
           scanned.mode==loaded.mode && scanned.offroad_x100==loaded.offroad_x100);
    base.mode=EVIST_RIDE_OFFROAD;base.offroad_x100=2200u;
    base.start=EVIST_RIDE_START_LAST;
    base.last_mode=EVIST_RIDE_LEGAL;
    assert(evist_ride_record_v2_encode(&base,rec));
    rec[2]|=0x10u;rec[5]=rec[0]^rec[1]^rec[2]^rec[3]^rec[4];
    memcpy(&pref_mem[EVIST_PREF_RIDE_BASE],rec,sizeof(rec));
    assert(evist_ride_store_load(&io,&loaded,&next) && loaded.offroad_x100==2200u &&
           loaded.last_mode==EVIST_RIDE_LEGAL && evist_ride_boot_mode(&loaded)==EVIST_RIDE_LEGAL);
    assert(evist_ride_log_scan(&pref_mem[EVIST_PREF_RIDE_BASE],2u,&scanned) &&
           scanned.offroad_x100==2200u && scanned.last_mode==EVIST_RIDE_LEGAL);
    dpc245_settings_t switch_settings;dpc245_settings_defaults(&switch_settings);
    for(unsigned start=EVIST_RIDE_START_LAST;start<=EVIST_RIDE_START_OFFROAD;start++) {
        memset(&pref_mem[EVIST_PREF_RIDE_BASE],0xff,EVIST_PREF_RIDE_END-EVIST_PREF_RIDE_BASE);
        next=EVIST_PREF_RIDE_BASE;
        evist_ride_cfg_t before=evist_ride_defaults();
        before.start=(evist_ride_start_t)start;
        dpc245_app_init(&app,&switch_settings);dpc245_app_set_ride(&app,&before);
        for(unsigned turn=0u;turn<200u;turn++) {
            evist_ride_mode_t effective=(turn&1u)?EVIST_RIDE_LEGAL:EVIST_RIDE_OFFROAD;
            app.ride_pending_session=effective==EVIST_RIDE_OFFROAD?
                DPC245_RIDE_SESSION_OFFROAD:DPC245_RIDE_SESSION_LEGAL;
            app.ride_combo_apply=true;
            dpc245_app_ride_applied(&app,&applied);
            evist_ride_cfg_t changed;
            bool dirty=dpc245_app_take_ride_cfg_dirty(&app,&changed);
            assert(dirty==(start==EVIST_RIDE_START_LAST));
            if(dirty) {
                evist_ride_store_result_t result=evist_ride_store_append(&io,&before,&changed,&next);
                assert(result==(turn<EVIST_RIDE_LOG_COUNT?EVIST_RIDE_STORE_OK:EVIST_RIDE_STORE_FULL));
                before=changed;
            }
        }
        assert(next==EVIST_PREF_RIDE_BASE+
               (start==EVIST_RIDE_START_LAST?EVIST_RIDE_LOG_COUNT:0u)*EVIST_RIDE_REC_SIZE);
    }
#endif
    printf("ride v2 migration/start/speed tests passed\n");
}

/* Streams the splash decoder output into a standard CRC-32 of RGB565 LE bytes. */
struct splash_crc_state { uint32_t *crc; uint32_t *pixels; };
static void splash_crc_emit(void *ctx, uint16_t pixel, uint8_t count) {
    struct splash_crc_state *st = (struct splash_crc_state *)ctx;
    for (uint8_t n = 0u; n < count; ++n) {
        for (uint8_t b = 0u; b < 2u; ++b) {
            uint8_t byte = (uint8_t)((b == 0u) ? (pixel & 0xFFu) : (pixel >> 8));
            *st->crc ^= byte;
            for (uint8_t k = 0u; k < 8u; ++k)
                *st->crc = (*st->crc & 1u) ? (*st->crc >> 1) ^ 0xEDB88320u : (*st->crc >> 1);
        }
        (*st->pixels)++;
    }
}

static void test_temperature_warning(void) {
    dpc245_settings_t settings;dpc245_settings_defaults(&settings);
    dpc245_app_t app;dpc245_app_init(&app,&settings);
    bafang_telemetry_t sample={0};
    sample.have_motor_temp=true;sample.have_controller_temp=true;
    sample.controller_aux_temp_raw=160u;sample.controller_temp_raw=140u;
    sample.motor_temp_c=120;sample.controller_temp_c=100;
    sample.controller_fast1_rx_ms=1000u;
    dpc245_app_temperature_sample(&app,&sample);
    dpc245_app_tick(&app,1000u);dpc245_app_tick(&app,3000u);
    assert(app.temp_warning_sources==0u); /* both limits OFF */
    assert(app.temp_warning_activations==0u);
    assert(app.temp_motor_max_valid&&app.temp_motor_max_c==120);
    assert(app.temp_ctrl_max_valid&&app.temp_ctrl_max_c==100);
    app.preferences.motor_temp_limit_c=120u;app.preferences.ctrl_temp_limit_c=100u;
    sample.motor_temp_c=119;dpc245_app_temperature_sample(&app,&sample);
    dpc245_app_tick(&app,3100u);assert(app.temp_warning_sources==0u);
    sample.motor_temp_c=120;sample.controller_temp_c=99;sample.controller_temp_raw=139u;
    sample.controller_fast1_rx_ms=3200u;
    dpc245_app_temperature_sample(&app,&sample);dpc245_app_tick(&app,3200u);
    sample.motor_temp_c=121;sample.controller_fast1_rx_ms=5000u;dpc245_app_temperature_sample(&app,&sample);
    dpc245_app_tick(&app,5199u);assert(app.temp_warning_sources==0u);
    dpc245_app_tick(&app,5200u);assert(app.temp_warning_sources==1u);
    assert(app.temp_warning_text_until_ms==8200u);
    assert(app.temp_warning_activations==1u);
    assert(app.temp_warning_last.uptime_s==5u&&app.temp_warning_last.sources==1u);
    assert(app.temp_warning_last.motor_c==121&&app.temp_warning_last.ctrl_c==99);
    assert(app.temp_warning_last.motor_valid&&app.temp_warning_last.ctrl_valid);
    assert(app.temp_warning_last.sample_age_ms==200u);
    assert(app.temp_warning_last.motor_limit_c==120u&&app.temp_warning_last.ctrl_limit_c==100u);
    assert(app.temp_warning_last.raw6==139u&&app.temp_warning_last.raw7==160u);
    sample.motor_temp_c=116;sample.controller_fast1_rx_ms=6200u;
    dpc245_app_temperature_sample(&app,&sample);dpc245_app_tick(&app,6200u);
    assert(app.temp_warning_sources==1u);
    sample.controller_temp_c=100;sample.controller_fast1_rx_ms=7000u;
    dpc245_app_temperature_sample(&app,&sample);dpc245_app_tick(&app,7000u);
    sample.controller_fast1_rx_ms=9000u;dpc245_app_temperature_sample(&app,&sample);
    dpc245_app_tick(&app,9000u);assert(app.temp_warning_sources==3u);
    assert(app.temp_warning_text_until_ms==12000u);
    assert(app.temp_warning_activations==1u); /* adding a source is not another activation */
    sample.motor_temp_c=115;sample.controller_temp_c=96;sample.controller_fast1_rx_ms=9100u;
    dpc245_app_temperature_sample(&app,&sample);dpc245_app_tick(&app,9100u);
    sample.controller_fast1_rx_ms=11100u;dpc245_app_temperature_sample(&app,&sample);
    dpc245_app_tick(&app,11100u);assert(app.temp_warning_sources==2u); /* 96 > 100-5 */
    sample.controller_temp_c=95;sample.controller_fast1_rx_ms=11200u;
    dpc245_app_temperature_sample(&app,&sample);dpc245_app_tick(&app,11200u);
    sample.controller_fast1_rx_ms=13199u;dpc245_app_temperature_sample(&app,&sample);
    dpc245_app_tick(&app,13199u);assert(app.temp_warning_sources==2u);
    sample.controller_fast1_rx_ms=13200u;dpc245_app_temperature_sample(&app,&sample);
    dpc245_app_tick(&app,13200u);assert(app.temp_warning_sources==0u);
    sample.controller_temp_c=101;sample.controller_temp_raw=141u;sample.controller_fast1_rx_ms=13300u;
    dpc245_app_temperature_sample(&app,&sample);dpc245_app_tick(&app,13300u);
    sample.controller_fast1_rx_ms=15300u;dpc245_app_temperature_sample(&app,&sample);
    dpc245_app_tick(&app,15300u);assert(app.temp_warning_sources==2u);
    assert(app.temp_warning_text_until_ms==18300u);
    assert(app.temp_warning_activations==2u);
    assert(app.temp_warning_last.uptime_s==15u&&app.temp_warning_last.sources==2u);
    assert(app.temp_warning_last.ctrl_c==101&&app.temp_warning_last.sample_age_ms==0u);
    assert(app.temp_warning_last.raw6==141u&&app.temp_warning_last.raw7==160u);
    assert(app.temp_motor_max_c==121&&app.temp_ctrl_max_c==101);
    sample.controller_aux_temp_raw=0u;sample.controller_temp_c=-40;
    sample.controller_fast1_rx_ms=15400u;dpc245_app_temperature_sample(&app,&sample);
    dpc245_app_tick(&app,15400u);dpc245_app_tick(&app,17400u);
    assert(app.temp_warning_sources==0u);
    assert(app.temp_motor_max_c==121&&app.temp_ctrl_max_c==101);
    sample.controller_temp_c=101;sample.controller_fast1_rx_ms=17500u;
    dpc245_app_temperature_sample(&app,&sample);dpc245_app_tick(&app,17500u);
    sample.controller_fast1_rx_ms=19500u;dpc245_app_temperature_sample(&app,&sample);
    dpc245_app_tick(&app,19500u);assert(app.temp_warning_sources==2u);
    assert(app.temp_warning_activations==3u);
    dpc245_app_tick(&app,22501u);assert(app.temp_warning_sources==2u);
    assert(app.temp_motor_max_c==121&&app.temp_ctrl_max_c==101); /* stale sample */
    dpc245_app_tick(&app,24501u);assert(app.temp_warning_sources==0u);
    dpc245_app_init(&app,&settings);
    assert(app.temp_warning_activations==0u&&!app.temp_motor_max_valid&&!app.temp_ctrl_max_valid);
    app.preferences.motor_temp_limit_c=40u;app.preferences.ctrl_temp_limit_c=40u;
    sample.controller_aux_temp_raw=0u;sample.controller_temp_c=-40;
    sample.controller_fast1_rx_ms=25000u;dpc245_app_temperature_sample(&app,&sample);
    dpc245_app_tick(&app,25000u);
    sample.controller_fast1_rx_ms=27000u;dpc245_app_temperature_sample(&app,&sample);
    dpc245_app_tick(&app,27000u);assert(app.temp_warning_sources==0u);
    puts("temperature warning timing and sources passed");
}

static void test_temperature_unavailable_bytes(void) {
    dpc245_settings_t settings;dpc245_settings_defaults(&settings);
    dpc245_app_t app;dpc245_app_init(&app,&settings);
    app.preferences.motor_temp_limit_c=40u;
    app.preferences.ctrl_temp_limit_c=40u;
    bafang_telemetry_t sample={0};
    bafang_can_frame_t frame={.id=0x02F83201u,.dlc=8u,
                              .data={0u,0u,0u,0u,0u,0u,58u,0xFFu}};
    for(unsigned i=0u;i<2u;++i){
        frame.data[7]=i==0u?0xFFu:0u;
        assert(bafang_decode_telemetry(&frame,&sample));
        assert(!sample.have_motor_temp&&sample.controller_aux_temp_raw==frame.data[7]);
        sample.controller_fast1_rx_ms=1000u+i*3000u;
        dpc245_app_temperature_sample(&app,&sample);
        dpc245_app_tick(&app,sample.controller_fast1_rx_ms);
        dpc245_app_tick(&app,sample.controller_fast1_rx_ms+2000u);
        assert(!app.temp_motor_valid&&!app.temp_motor_max_valid);
        assert(app.temp_warning_sources==0u&&app.temp_warning_activations==0u);
    }
    frame.data[6]=0xFFu;frame.data[7]=80u;
    assert(bafang_decode_telemetry(&frame,&sample));
    assert(!sample.have_controller_temp&&sample.have_motor_temp&&sample.motor_temp_c==40);
    sample.controller_fast1_rx_ms=7000u;
    dpc245_app_temperature_sample(&app,&sample);dpc245_app_tick(&app,7000u);
    assert(!app.temp_ctrl_valid&&app.temp_ctrl_max_valid&&app.temp_ctrl_max_c==18);
    frame.data[6]=0u;
    assert(bafang_decode_telemetry(&frame,&sample)&&!sample.have_controller_temp);
    sample.controller_fast1_rx_ms=7500u;
    dpc245_app_temperature_sample(&app,&sample);dpc245_app_tick(&app,7500u);
    assert(!app.temp_ctrl_valid&&app.temp_ctrl_max_c==18);
    frame.data[6]=80u;
    assert(bafang_decode_telemetry(&frame,&sample));
    assert(sample.have_controller_temp&&sample.controller_temp_c==40);
    sample.controller_fast1_rx_ms=8000u;
    dpc245_app_temperature_sample(&app,&sample);dpc245_app_tick(&app,8000u);
    sample.controller_fast1_rx_ms=10000u;
    dpc245_app_temperature_sample(&app,&sample);dpc245_app_tick(&app,10000u);
    assert(app.temp_warning_sources==3u&&app.temp_motor_max_c==40&&app.temp_ctrl_max_c==40);
    frame.data[7]=0xFFu;
    assert(bafang_decode_telemetry(&frame,&sample)&&!sample.have_motor_temp);
    sample.controller_fast1_rx_ms=10100u;
    dpc245_app_temperature_sample(&app,&sample);dpc245_app_tick(&app,10100u);
    assert(app.temp_warning_sources==3u&&app.temp_live[0].pending);
    sample.controller_fast1_rx_ms=12100u;
    dpc245_app_temperature_sample(&app,&sample);dpc245_app_tick(&app,12100u);
    assert(app.temp_warning_sources==2u&&!app.temp_live[0].decision_valid);
    assert(app.temp_motor_max_c==40&&app.temp_warning_activations==1u);
    puts("temperature unavailable bytes and active warning clear passed");
}

static void test_temperature_live_diagnostics(void) {
    dpc245_settings_t settings;dpc245_settings_defaults(&settings);
    dpc245_app_t app;dpc245_app_init(&app,&settings);
    app.preferences.motor_temp_limit_c=40u;app.preferences.ctrl_temp_limit_c=40u;
    bafang_telemetry_t sample={0};
    sample.have_motor_temp=true;sample.have_controller_temp=true;
    sample.controller_aux_temp_raw=90u;sample.controller_temp_raw=58u;
    sample.motor_temp_c=50;sample.controller_temp_c=18;
    sample.controller_fast1_rx_ms=1000u;
    dpc245_app_temperature_sample(&app,&sample);
    dpc245_app_tick(&app,1200u);
    const dpc245_temp_live_source_t *motor=&app.temp_live[0], *ctrl=&app.temp_live[1];
    assert(motor->sampled_c==50&&motor->limit_c==40u);
    assert(motor->sample_received&&motor->sensor_valid&&motor->fresh&&motor->decision_valid);
    assert(motor->sample_age_ms==200u&&motor->transition&&motor->pending&&motor->pending_ms==0u);
    assert(ctrl->sampled_c==18&&ctrl->limit_c==40u);
    assert(ctrl->sensor_valid&&ctrl->fresh&&ctrl->decision_valid);
    assert(ctrl->sample_age_ms==200u&&!ctrl->transition&&!ctrl->pending);
    dpc245_app_tick(&app,1300u);
    assert(motor->pending_ms==100u&&motor->sample_age_ms==300u);
    dpc245_app_tick(&app,3200u);
    assert(app.temp_warning_sources==1u&&motor->transition&&!motor->pending);
    dpc245_app_tick(&app,4201u);
    assert(motor->sample_age_ms==3201u&&!motor->fresh&&!motor->decision_valid);
    assert(motor->transition&&motor->pending&&motor->pending_ms==0u);
    assert(!ctrl->fresh&&!ctrl->decision_valid&&!ctrl->transition);
    sample.motor_temp_c=18;sample.controller_fast1_rx_ms=4300u;
    dpc245_app_temperature_sample(&app,&sample);dpc245_app_tick(&app,4300u);
    assert(motor->sampled_c==18&&motor->sample_age_ms==0u&&motor->fresh);
    assert(motor->transition&&motor->pending&&motor->pending_ms==99u);
    puts("temperature live decision inputs and transitions passed");
}

static void test_temperature_screen_record(void) {
    dpc245_settings_t settings;dpc245_settings_defaults(&settings);
    dpc245_app_t app;dpc245_app_init(&app,&settings);
    dpc245_app_t rendered=app;
    assert(!app.temp_screen_seen);
    rendered.temp_warning_sources=3u;
    dpc245_app_dashboard_render_complete(&app,&rendered,12345u);
    assert(app.temp_screen_seen&&app.temp_screen_sources==3u);
    assert(app.temp_screen_uptime_s==12u&&app.temp_screen_render_ms==12345u);
    app.temp_warning_sources=0u; /* live decision has cleared since the displayed frame */
    rendered.screen=DPC245_APP_SCREEN_ADVANCED;
    rendered.temp_warning_sources=2u;
    dpc245_app_dashboard_render_complete(&app,&rendered,13000u);
    assert(app.temp_screen_sources==3u&&app.temp_screen_render_ms==12345u);
    rendered.screen=DPC245_APP_SCREEN_DASHBOARD;
    rendered.temp_warning_sources=1u;
    dpc245_app_dashboard_render_complete(&app,&rendered,14000u);
    assert(app.temp_screen_sources==1u&&app.temp_screen_uptime_s==14u);
    assert(app.temp_warning_sources==0u);
    for(uint8_t theme=0u;theme<4u;++theme){
        dpc245_set_theme(&rendered.runtime.settings,theme);
        rendered.temp_warning_sources=theme;
        dpc245_app_dashboard_render_complete(&app,&rendered,15000u+theme);
        assert(app.temp_screen_sources==theme&&app.temp_screen_seen);
    }
    puts("completed dashboard snapshot record passed");
}

int main(void) {
    test_temperature_warning();
    test_temperature_unavailable_bytes();
    test_temperature_live_diagnostics();
    test_temperature_screen_record();
    uint8_t assist_block[EVIST_ASSIST_BLOCK_LEN] = {0};
    assist_block[0] = 0x28u;
    assist_block[EVIST_ASSIST_BLOCK_LEN - 1u] = evist_assist_checksum(assist_block);
    assert(evist_assist_block_valid(assist_block));
    assist_block[EVIST_ASSIST_BLOCK_LEN - 1u] ^= 1u;
    assert(!evist_assist_block_valid(assist_block));
    {
        dpc245_settings_t settings; dpc245_app_t app;
        dpc245_settings_defaults(&settings);
        dpc245_app_init(&app,&settings);
        dpc245_app_event(&app,DPC245_KEY_EVENT_SETTINGS_CHORD,100u);
        assert(app.screen==DPC245_APP_SCREEN_ROOT_MENU);
        dpc245_app_tick(&app,30099u);
        assert(app.screen==DPC245_APP_SCREEN_ROOT_MENU); /* 29,999 ms */
        dpc245_app_tick(&app,30100u);
        assert(app.screen==DPC245_APP_SCREEN_DASHBOARD); /* 30,000 ms */

        dpc245_app_init(&app,&settings);
        dpc245_app_event(&app,DPC245_KEY_EVENT_SETTINGS_CHORD,100u);
        dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,101u);
        dpc245_app_tick(&app,100u);
        assert(app.screen==DPC245_APP_SCREEN_ROOT_MENU);
        dpc245_app_tick(&app,30100u);
        assert(app.screen==DPC245_APP_SCREEN_ROOT_MENU);
        dpc245_app_tick(&app,30101u);
        assert(app.screen==DPC245_APP_SCREEN_DASHBOARD);

        dpc245_app_init(&app,&settings);
        dpc245_app_event(&app,DPC245_KEY_EVENT_SETTINGS_CHORD,100u);
        dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,20100u);
        dpc245_app_tick(&app,50099u);
        assert(app.screen==DPC245_APP_SCREEN_ROOT_MENU); /* key restarted timer */
        dpc245_app_tick(&app,50100u);
        assert(app.screen==DPC245_APP_SCREEN_DASHBOARD);

        dpc245_app_init(&app,&settings);
        app.screen=DPC245_APP_SCREEN_ADVANCED;
        app.advanced.screen=EVIST_ADV_WHEEL;
        app.advanced.wheel_dirty=true; /* unsaved edit is discarded */
        app.advanced.request=EVIST_ADV_REQ_NONE;
        app.last_key_event_ms=1000u;
        dpc245_app_tick(&app,30999u);
        assert(app.screen==DPC245_APP_SCREEN_ADVANCED);
        dpc245_app_tick(&app,31000u);
        assert(app.screen==DPC245_APP_SCREEN_DASHBOARD);
        assert(!app.advanced.wheel_dirty && app.advanced.request==EVIST_ADV_REQ_NONE);
        assert(!app.preferences_dirty && !app.ride_apply_requested);

        dpc245_app_init(&app,&settings);
        app.screen=DPC245_APP_SCREEN_ADVANCED;
        app.advanced.busy=true;
        app.last_key_event_ms=0u;
        dpc245_app_tick(&app,30000u);
        assert(app.screen==DPC245_APP_SCREEN_ADVANCED);
        app.advanced.busy=false;
        dpc245_app_tick(&app,59999u);
        assert(app.screen==DPC245_APP_SCREEN_ADVANCED);
        dpc245_app_tick(&app,89998u);
        assert(app.screen==DPC245_APP_SCREEN_ADVANCED);
        dpc245_app_tick(&app,89999u);
        assert(app.screen==DPC245_APP_SCREEN_DASHBOARD);

        dpc245_app_tick(&app,100000u);
        assert(app.screen==DPC245_APP_SCREEN_DASHBOARD);
        puts("idle return: threshold, key reset, busy hold, unsaved discard, dashboard stay PASS");
    }
    {
        dpc245_settings_t settings;dpc245_settings_defaults(&settings);
        dpc245_app_t app;dpc245_app_init(&app,&settings);
        dpc245_app_event(&app,DPC245_KEY_EVENT_UP_SHORT,100u);
        assert(app.ride_combo_step==1u);
        dpc245_app_tick(&app,99u);
        assert(app.ride_combo_step==1u);
        dpc245_app_event(&app,DPC245_KEY_EVENT_DOWN_SHORT,99u);
        assert(app.ride_combo_step==2u);
        dpc245_app_tick(&app,1600u);
        assert(app.ride_combo_step==2u);
        dpc245_app_tick(&app,1601u);
        assert(app.ride_combo_step==0u);
    }
    ride_behavior_tests();
    ride_v2_tests();
    bafang_can_id_t f = {3,2,0,0x63,0};
    assert(bafang_can_id_encode(f) == 0x03106300u);
    bafang_can_id_t d = bafang_can_id_decode(0x02F83201u);
    assert(d.src==2 && d.dst==31 && d.type==0 && d.cmd==0x32 && d.sub==1);

    /* Marker invalidation is retried; the attempt number is reported, 0 = failed. */
    clear_calls=0u;clear_fail_left=0u;
    assert(dpc245_marker_clear_retry(fake_clear,DPC245_MARKER_CLEAR_ATTEMPTS)==1u&&clear_calls==1u);
    clear_calls=0u;clear_fail_left=2u;
    assert(dpc245_marker_clear_retry(fake_clear,DPC245_MARKER_CLEAR_ATTEMPTS)==3u&&clear_calls==3u);
    clear_calls=0u;clear_fail_left=99u;
    assert(dpc245_marker_clear_retry(fake_clear,DPC245_MARKER_CLEAR_ATTEMPTS)==0u&&clear_calls==DPC245_MARKER_CLEAR_ATTEMPTS);
    assert(dpc245_marker_clear_retry(0,DPC245_MARKER_CLEAR_ATTEMPTS)==0u);
    clear_calls=0u;clear_fail_left=99u;
    assert(dpc245_marker_clear_retry(fake_clear,0u)==0u&&clear_calls==0u);
    clear_calls=0u;clear_fail_left=0u;
    assert(dpc245_marker_clear_retry(fake_clear,1u)==1u&&clear_calls==1u);
    clear_calls=0u;clear_fail_left=1000u;  /* Limit 255 must terminate */
    assert(dpc245_marker_clear_retry(fake_clear,255u)==0u&&clear_calls==255u);
    clear_calls=0u;clear_fail_left=254u;
    assert(dpc245_marker_clear_retry(fake_clear,255u)==255u&&clear_calls==255u);

    /* Clock tree. Register values follow GD32F30x RCU; the
     * updater leaves CFG0 0x0037040A (HXTAL/2 x15, APB1 /2, PLL selected). */
    {
        const uint32_t HSISTB=1u<<1,HXTALEN=1u<<16,HXTALSTB=1u<<17,PLLEN=1u<<24,PLLSTB=1u<<25;
        const uint32_t pll_ctl=0x83u|HXTALEN|HXTALSTB|PLLEN|PLLSTB;
        assert(dpc245_clock_classify(pll_ctl,0x0037040Au,0u)==DPC245_CLOCK_TREE_HXTAL_PLL);
        assert(dpc245_clock_classify(0x00000083u,0u,0u)==DPC245_CLOCK_TREE_HSI8);       /* reset state */
        assert(dpc245_clock_classify(pll_ctl,0x0037040Au|(1u<<27),0u)==DPC245_CLOCK_TREE_OTHER); /* x30 */
        assert(dpc245_clock_classify(pll_ctl,0x0037040Au,1u<<30)==DPC245_CLOCK_TREE_OTHER);      /* IRC48M pre-source */
        assert(dpc245_clock_classify(pll_ctl&~PLLSTB,0x0037040Au,0u)==DPC245_CLOCK_TREE_OTHER);  /* PLL not locked */
        assert(dpc245_clock_classify(pll_ctl,0x00370408u,0u)==DPC245_CLOCK_TREE_OTHER);          /* switch pending */
        assert(dpc245_clock_classify(0x83u,0x00000400u,0u)==DPC245_CLOCK_TREE_OTHER);            /* HSI, APB1 /2 */
        assert(dpc245_clock_classify(0x83u,0x00000080u,0u)==DPC245_CLOCK_TREE_OTHER);            /* AHB /2 */
        assert(dpc245_clock_classify(0x81u,0u,0u)==DPC245_CLOCK_TREE_OTHER);                     /* HSI not stable */
        assert(dpc245_clock_tree_core_hz(DPC245_CLOCK_TREE_HSI8)==8000000u);
        assert(dpc245_clock_tree_core_hz(DPC245_CLOCK_TREE_HXTAL_PLL)==120000000u);
        assert(dpc245_clock_tree_core_hz(DPC245_CLOCK_TREE_OTHER)==0u);  /* -> reset, never "8 MHz" */

        /* Plain variables never change by themselves: every poll can only end
         * through the limit, which proves termination of both waits. */
        volatile uint32_t ctl,cfg0,cfg1;dpc245_rcu_regs_t r={&ctl,&cfg0,&cfg1};
        /* HSI-ready timeout on the updater tree: untouched, PLL kept. */
        ctl=pll_ctl&~HSISTB;cfg0=0x0037040Au;cfg1=0u;
        assert(dpc245_clock_force_hsi(&r,1000u)==DPC245_CLOCK_TREE_HXTAL_PLL&&cfg0==0x0037040Au&&(ctl&PLLEN));
        /* HSI-ready timeout on a foreign tree: OTHER (caller resets). */
        ctl=pll_ctl&~HSISTB;cfg0=0x0037040Au|(1u<<27);
        assert(dpc245_clock_force_hsi(&r,1000u)==DPC245_CLOCK_TREE_OTHER);
        /* Source-switch timeout: HSI requested, PLL still reported -> OTHER, not HSI. */
        ctl=pll_ctl;cfg0=0x0037040Au;
        assert(dpc245_clock_force_hsi(&r,1000u)==DPC245_CLOCK_TREE_OTHER&&(cfg0&3u)==0u&&((cfg0>>2)&3u)==2u);
        /* Half-way failed setup on HSI (APB1 /2, PLL on): normalised to HSI8. */
        ctl=0x83u|HXTALEN|HXTALSTB|PLLEN;cfg0=0x00370400u;cfg1=1u<<30;
        assert(dpc245_clock_force_hsi(&r,1000u)==DPC245_CLOCK_TREE_HSI8&&cfg0==0u&&cfg1==0u&&(ctl&(PLLEN|HXTALEN))==0u);
    }

    /* Updater request: exact CAN entry contract. */
    bafang_can_frame_t upreq={.id=0x05194000u,.dlc=4u,.data={0x08u,0x42u,0x03u,0x00u}};
    assert(dpc245_is_can_update_request(&upreq));
    upreq.data[3]=0x7Fu;            /* data[3] is not checked */
    assert(dpc245_is_can_update_request(&upreq));
    upreq.id=0x02194000u;           /* source is not checked */
    assert(dpc245_is_can_update_request(&upreq));
    upreq.id=0x05204000u;           /* destination 4 (battery) */
    assert(!dpc245_is_can_update_request(&upreq));
    upreq.id=0x05184000u;           /* type 0 instead of 1 */
    assert(!dpc245_is_can_update_request(&upreq));
    upreq.id=0x05194100u;           /* command 0x41 */
    assert(!dpc245_is_can_update_request(&upreq));
    upreq.id=0x05194001u;           /* sub 0x01 */
    assert(!dpc245_is_can_update_request(&upreq));
    upreq.id=0x05194000u; upreq.dlc=3u;
    assert(!dpc245_is_can_update_request(&upreq));
    upreq.dlc=4u; upreq.data[2]=0x02u;
    assert(!dpc245_is_can_update_request(&upreq));
    assert(!dpc245_is_can_update_request(NULL));
    /* Regression: the updater announcement 0x05FF3005 (#00 while
     * waiting, #01 at the end) must never send a running APP to the updater. */
    bafang_can_frame_t announce={.id=0x05FF3005u,.dlc=1u,.data={0x00u}};
    assert(!dpc245_is_can_update_request(&announce));
    announce.data[0]=0x01u;
    assert(!dpc245_is_can_update_request(&announce));

    /* BLE request, end to end through the wire parser. */
    {
        uint8_t wire[DPC245_BLE_MAX_WIRE];
        size_t n=dpc245_ble_frame_build(wire,sizeof wire,0x11u,0xA5u,0x30u,0x00u,NULL,0u);
        assert(n==9u);
        dpc245_ble_parser_t bp; dpc245_ble_frame_t bf; dpc245_ble_parser_init(&bp);
        bool got=false;
        for(size_t i=0;i<n;i++) if(dpc245_ble_parser_feed(&bp,wire[i],&bf)) got=true;
        assert(got && dpc245_is_ble_update_request(&bf));
        bf.selector=0x01u; assert(!dpc245_is_ble_update_request(&bf));
        bf.selector=0x00u; bf.operation=0x31u; assert(!dpc245_is_ble_update_request(&bf));
        bf.operation=0x30u; bf.command_id=0xA4u; assert(!dpc245_is_ble_update_request(&bf));
        bf.command_id=0xA5u; bf.class_id=0x10u; assert(!dpc245_is_ble_update_request(&bf));
        printf("ble-update-request-wire:");
        for(size_t i=0;i<n;i++) printf(" %02X",wire[i]);
        printf("\n");
    }

    /* POWER+UP+DOWN chord: 1500 ms continuous hold. */
    {
        const uint8_t all=DPC245_KEY_POWER|DPC245_KEY_UP|DPC245_KEY_DOWN;
        dpc245_recovery_chord_t ch; dpc245_recovery_chord_init(&ch);
        assert(!dpc245_recovery_chord_update(&ch,all,1000u));
        assert(!dpc245_recovery_chord_update(&ch,all,2499u));
        assert(dpc245_recovery_chord_update(&ch,all,2500u));
        assert(!dpc245_recovery_chord_update(&ch,DPC245_KEY_POWER|DPC245_KEY_UP,2600u));
        assert(!dpc245_recovery_chord_update(&ch,all,2700u));
        assert(!dpc245_recovery_chord_update(&ch,all,4199u));
        assert(dpc245_recovery_chord_update(&ch,all,4200u));
        dpc245_recovery_chord_init(&ch);  /* wrap-around of the ms counter */
        assert(!dpc245_recovery_chord_update(&ch,all,0xFFFFFF00u));
        assert(dpc245_recovery_chord_update(&ch,all,0xFFFFFF00u+1500u));
    }

    /* Update transition coordinates with the controller before APP invalidation. */
    dpc245_update_t upd; dpc245_update_init(&upd);
    assert(dpc245_update_request(&upd));
    bafang_can_frame_t uc={0};
    assert(dpc245_update_next_can(&upd,&uc));
    assert(uc.id==0x13FF3005u && uc.dlc==1u && uc.data[0]==0u);
    dpc245_update_can_sent(&upd,100u);
    assert(!dpc245_update_tick(&upd,100u,true));
    assert(!dpc245_update_tick(&upd,101u,false));
    assert(dpc245_update_tick(&upd,101u,true));
    assert(dpc245_update_tick(&upd,102u,true));

    bafang_telemetry_t t = {0};
    bafang_can_frame_t tel = {.id=0x02F83201u,.dlc=8,.data={0xC4,0x09,0xD2,0x04,0xC4,0x13,70,75}};
    assert(bafang_decode_telemetry(&tel,&t));
    assert(t.speed_centi_kph==2500);
    assert(t.current_centi_amp==1234);
    assert(t.voltage_centi_volt==5060);
    assert(t.controller_temp_c==30);
    assert(t.have_controller_aux_temp_raw && t.controller_aux_temp_raw==75u);
    assert(t.have_motor_temp && t.motor_temp_c==35);
    tel.data[7]=0u;
    assert(bafang_decode_telemetry(&tel,&t) && !t.have_motor_temp && t.motor_temp_c==-40);
    tel.data[7]=70u;
    assert(bafang_decode_telemetry(&tel,&t) && t.motor_temp_c==t.controller_temp_c);
    assert(bafang_power_w(&t)==624);


    bafang_can_frame_t tq={.id=0x01F83100u,.dlc=4,.data={0xEE,0x02,77,1}};
    assert(bafang_decode_telemetry(&tq,&t));
    assert(t.have_torque_sensor && t.torque_sensor_raw==0x02EEu);
    assert(t.torque_sensor_cadence_rpm==77u && t.torque_sensor_progressive==1u);
    assert(bafang_frame_classify(0x02F83202u)==BAFANG_FRAME_CONTROLLER_FAST_2_RAW);
    {
        bafang_telemetry_t st={0};
        bafang_can_frame_t f2={.id=0x02F83202u,.dlc=1u,.data={0x05u}};
        assert(bafang_decode_telemetry(&f2,&st));
        assert(st.have_controller_status_bit0&&st.controller_status_bit0&&st.controller_fast2_raw==0x05u);
        st.controller_fast2_rx_ms=100u;
        assert(bafang_walk_bit_fresh(&st,600u));
        assert(!bafang_walk_bit_fresh(&st,601u));
        f2.data[0]=0u;assert(bafang_decode_telemetry(&f2,&st));
        assert(!bafang_walk_bit_fresh(&st,100u));
        bafang_can_frame_t f5={.id=0x02F83205u,.dlc=2u,.data={0x5Au,0x00u}};
        assert(bafang_decode_telemetry(&f5,&st));
        assert(st.have_controller_fast5_raw&&st.controller_fast5_raw==0x005Au);
    }
    {
        evist_can_router_t timed;evist_can_router_init(&timed);
        bafang_can_frame_t f2={.id=0x02F83202u,.dlc=1u,.data={0x01u}};
        evist_can_router_rx(&timed,&f2,1234u);
        assert(timed.telemetry.controller_fast2_rx_ms==1234u);
        assert(bafang_walk_bit_fresh(&timed.telemetry,1734u));
        assert(!bafang_walk_bit_fresh(&timed.telemetry,1735u));
    }
    assert(bafang_frame_classify(0x04F83401u)==BAFANG_FRAME_BATTERY_FAST_1_RAW);

    bafang_can_frame_t hb;
    /* V3 heartbeat: physical keys are deliberately absent from the API. */
    assert(bafang_build_hmi_heartbeat_v3_default(&hb,9,5,false,true));
    assert(hb.id==0x03106300u && hb.dlc==4 && hb.data[1]==0x02u);
    assert(hb.data[2]==0x01u); /* light state only */
    assert(hb.data[3]==0x01u); /* default raw status byte 3 */

    /* One-shot event bits 1..3 are passed through raw. */
    assert(bafang_build_hmi_heartbeat_v3(&hb,5,3,false,true,0x02u,1u));
    assert(hb.data[0]==5u && hb.data[1]==0x15u && hb.data[2]==0x03u && hb.data[3]==1u);
    assert(bafang_build_hmi_heartbeat_v3(&hb,5,3,false,false,0x04u,0u));
    assert(hb.data[2]==0x04u && hb.data[3]==0u);
    assert(bafang_build_hmi_heartbeat_v3(&hb,5,3,false,false,0x08u,1u));
    assert(hb.data[2]==0x08u);
    assert(!bafang_build_hmi_heartbeat_v3(&hb,5,3,false,false,0x20u,1u));
    assert(!bafang_build_hmi_heartbeat_v3(&hb,5,3,false,false,0u,2u));

    /* Exact assist-code tables defined/validated for 3/5/9 modes. */
    assert(bafang_assist_code(3,0,false)==0x00u);
    assert(bafang_assist_code(3,1,false)==0x0Cu);
    assert(bafang_assist_code(3,2,false)==0x02u);
    assert(bafang_assist_code(3,3,false)==0x03u);
    assert(bafang_assist_code(5,1,false)==0x0Bu);
    assert(bafang_assist_code(5,2,false)==0x0Du);
    assert(bafang_assist_code(5,3,false)==0x15u);
    assert(bafang_assist_code(5,4,false)==0x17u);
    assert(bafang_assist_code(5,5,false)==0x03u);
    assert(bafang_assist_code(9,9,false)==0x03u);
    assert(bafang_assist_code(5,2,true)==0x06u);
    assert(bafang_assist_code(4,1,false)==0xFFu);
    assert(!bafang_build_hmi_heartbeat_v3_default(&hb,4,1,false,false));

    /* PB11 brightness controller: five user levels and all 16x16 forward-only transitions. */
    assert(dpc245_backlight_level_to_position(0u)==16u);
    assert(dpc245_backlight_level_to_position(1u)==13u);
    assert(dpc245_backlight_level_to_position(2u)==9u);
    assert(dpc245_backlight_level_to_position(3u)==5u);
    assert(dpc245_backlight_level_to_position(4u)==1u);
    assert(dpc245_backlight_level_to_position(5u)==1u);
    for(uint8_t cur=1u;cur<=16u;++cur){
        for(uint8_t target=1u;target<=16u;++target){
            const uint8_t steps=dpc245_backlight_forward_steps(cur,target);
            assert(steps<=15u);
            assert((uint8_t)(((uint16_t)(cur-1u)+steps)%16u+1u)==target);
        }
    }

    evist_light_controller_t lc;
    evist_light_init(&lc, 0u);
    evist_light_configure(&lc,true,5u,1u,3200u,2u);
    /* AL sensitivity OFF: ambient input must not change light state. */
    assert(!evist_light_update_als(&lc,3199u));
    assert(!lc.light_on);
    assert(!evist_light_update_als(&lc,4095u));
    assert(!lc.light_on);
    lc.sensitivity=1u;
    assert(evist_light_update_als(&lc,3200u));
    assert(lc.light_on);
    /* Manual long-UP override survives unchanged ambient readings. */
    assert(!evist_light_user_toggle(&lc));
    assert(!evist_light_update_als(&lc,3200u) && !lc.light_on);
    evist_light_controller_t lc2;
    evist_light_init(&lc2,1u);
    evist_light_configure(&lc2,true,5u,1u,3200u,2u);
    for(unsigned i=0u;i<8u;i++) (void)evist_light_update_als(&lc2,1000u);
    assert(!lc2.light_on);
    for(unsigned i=0u;i<32u && !lc2.light_on;i++) (void)evist_light_update_als(&lc2,4095u);
    assert(lc2.light_on);
    evist_light_set_mode(&lc,EVIST_LIGHT_MANUAL_OFF);
    assert(!lc.light_on);
    assert(!evist_light_update_als(&lc,4095u) && !lc.light_on);

    evist_can_router_t r; evist_can_router_init(&r);
    bafang_can_frame_t unknown={.id=0x12345678u,.dlc=2,.data={0xAA,0x55}};
    evist_can_router_rx(&r,&unknown,100u);
    assert(r.log_count==1);
    assert(evist_can_router_log_newest(&r,0)->frame.id==0x12345678u);

    bafang_can_frame_t q;
    assert(bafang_build_read_query(&q,BAFANG_NODE_CONTROLLER,0x60,0x01));
    assert(q.id==0x03116001u);
    assert(evist_can_router_tx_allowed(&r,&q,false));
    assert(!evist_can_router_tx_allowed(&r,&hb,false));
    r.tx_mode=EVIST_TX_NORMAL_HMI;
    assert(evist_can_router_tx_allowed(&r,&hb,false));
    assert(!evist_can_router_tx_allowed(&r,&hb,true));


    assert(bafang_wheel_tenths_to_raw(290)==0x01D0u);
    assert(bafang_wheel_tenths_to_raw(275)==0x01B5u);
    assert(bafang_wheel_raw_to_tenths(0x01D0u)==290u);
    assert(bafang_wheel_raw_to_tenths(0x01B5u)==275u);


    bafang_can_frame_t cfg_rx={.id=0x02F83203u,.dlc=6,.data={0xC4,0x09,0xD0,0x01,0xE8,0x08}};
    assert(bafang_decode_telemetry(&cfg_rx,&t));
    assert(t.speed_limit_centi_kph==2500u);
    assert(t.wheel_size_raw==0x01D0u && t.wheel_size_tenth_in==290u);
    assert(t.circumference_mm==2280u);

    bafang_can_frame_t cfg;
    assert(bafang_build_besst_controller_config_write(&cfg,2500,290,2280));
    assert(cfg.id==0x05103203u && cfg.dlc==6);
    assert(cfg.data[0]==0xC4 && cfg.data[1]==0x09);
    assert(cfg.data[2]==0xD0 && cfg.data[3]==0x01); /* 29.0 in -> 0x01D0 */
    assert(cfg.data[4]==0xE8 && cfg.data[5]==0x08);
    assert(!evist_can_router_tx_allowed(&r,&cfg,true));
    r.tx_mode=EVIST_TX_EXPERT;
    assert(evist_can_router_tx_allowed(&r,&cfg,true));

    evist_bafang_config_t ec={.speed_limit_centi_kph=2500,.wheel_size_tenth_in=290,.circumference_mm=2280,.dirty=true};
    assert(evist_bafang_config_validate(&ec));
    r.tx_mode=EVIST_TX_READONLY;
    assert(!evist_bafang_config_build_write(&ec,&r,true,&cfg));
    r.tx_mode=EVIST_TX_EXPERT;
    assert(!evist_bafang_config_build_write(&ec,&r,false,&cfg));
    assert(evist_bafang_config_build_write(&ec,&r,true,&cfg));


    /* DPC245 BLE wire protocol. */
    {
        uint8_t payload[3]={0x12u,0x34u,0x56u};
        uint8_t wire[32]={0};
        size_t n=dpc245_ble_frame_build(wire,sizeof(wire),0x11u,0x10u,0x01u,0x59u,payload,3u);
        assert(n==12u);
        assert(wire[0]==0x55u && wire[1]==0xAAu && wire[2]==3u);
        assert(wire[3]==0x11u && wire[4]==0x10u && wire[5]==1u && wire[6]==0x59u);
        uint16_t chk=dpc245_ble_checksum(&wire[2],8u);
        assert(wire[10]==(uint8_t)chk && wire[11]==(uint8_t)(chk>>8));

        dpc245_ble_parser_t bp; dpc245_ble_parser_init(&bp);
        dpc245_ble_frame_t bf={0}; bool got=false;
        for(size_t i=0;i<n;i++) if(dpc245_ble_parser_feed(&bp,wire[i],&bf)) got=true;
        assert(got && bp.valid_frames==1u && bp.bad_checksums==0u);
        assert(bf.length==3u && bf.class_id==0x11u && bf.command_id==0x10u);
        assert(bf.operation==1u && bf.selector==0x59u && memcmp(bf.payload,payload,3u)==0);

        /* Corrupted checksum must be rejected, then parser must resync. */
        wire[10]^=1u; got=false;
        for(size_t i=0;i<n;i++) if(dpc245_ble_parser_feed(&bp,wire[i],&bf)) got=true;
        assert(!got && bp.bad_checksums==1u);
        wire[10]^=1u;
        (void)dpc245_ble_parser_feed(&bp,0x55u,&bf);
        (void)dpc245_ble_parser_feed(&bp,0x55u,&bf); /* repeated sync0 */
        for(size_t i=1;i<n;i++) if(dpc245_ble_parser_feed(&bp,wire[i],&bf)) got=true;
        assert(got && bp.valid_frames==2u);
    }

    /* BLE application dispatcher: typed setting, bulk data, periodic status and update handoff. */
    {
        dpc245_settings_t bs; dpc245_settings_defaults(&bs);
        dpc245_app_t ba; dpc245_app_init(&ba,&bs);
        evist_can_router_t br; evist_can_router_init(&br);
        dpc245_hmi_identity_records_t bid; dpc245_hmi_identity_records_init(&bid);
        dpc245_update_t bu; dpc245_update_init(&bu);
        dpc245_ble_app_t bap; dpc245_ble_app_init(&bap);
        dpc245_ble_app_result_t rr;
        dpc245_ble_frame_t q={0};
        q.class_id=0x11u;q.command_id=0x10u;q.operation=1u;q.selector=0x59u;
        ba.runtime.settings.raw[0x2Eu]=0x5Au;dpc245_settings_finalize(&ba.runtime.settings);
        assert(dpc245_ble_app_handle(&bap,&q,&ba,&br,&bid,false,&bu,&rr));
        assert(rr.have_tx && rr.tx_len==10u);
        dpc245_ble_parser_t rp;dpc245_ble_parser_init(&rp);dpc245_ble_frame_t rf={0};bool rgot=false;
        for(size_t i=0u;i<rr.tx_len;i++)if(dpc245_ble_parser_feed(&rp,rr.tx[i],&rf))rgot=true;
        assert(rgot&&rf.class_id==0x10u&&rf.command_id==0x11u&&rf.operation==4u&&rf.selector==0x59u);
        assert(rf.length==1u&&rf.payload[0]==0x5Au);

        q.operation=2u;q.length=1u;q.payload[0]=0x22u;
        assert(dpc245_ble_app_handle(&bap,&q,&ba,&br,&bid,false,&bu,&rr));
        assert(rr.settings_dirty&&ba.runtime.settings.raw[0x2Eu]==0x22u&&dpc245_settings_validate(&ba.runtime.settings));

        /* A3 chunk returns current typed controller data in its stable offsets. */
        strcpy(br.controller_identity.hardware,"CTRL-HW");
        br.telemetry.have_speed=true;br.telemetry.speed_centi_kph=2345u;
        br.telemetry.have_voltage=true;br.telemetry.voltage_centi_volt=4321u;
        q.command_id=0xA3u;q.operation=1u;q.selector=172u;q.length=1u;q.payload[0]=6u;
        assert(dpc245_ble_app_handle(&bap,&q,&ba,&br,&bid,false,&bu,&rr));
        dpc245_ble_parser_init(&rp);rgot=false;
        for(size_t i=0u;i<rr.tx_len;i++)if(dpc245_ble_parser_feed(&rp,rr.tx[i],&rf))rgot=true;
        assert(rgot&&rf.length==6u&&rf.payload[0]==0x29u&&rf.payload[1]==0x09u);
        assert(rf.payload[4]==0xE1u&&rf.payload[5]==0x10u);

        /* A5/A6 produces the documented one-shot HMI event bit. */
        ba.runtime.settings.raw[0x2Eu]=0u;dpc245_settings_finalize(&ba.runtime.settings);
        q.command_id=0xA5u;q.operation=2u;q.selector=0xA6u;q.length=1u;q.payload[0]=1u;
        assert(dpc245_ble_app_handle(&bap,&q,&ba,&br,&bid,false,&bu,&rr));
        assert(dpc245_ble_app_take_hmi_event_bits(&bap)==0x08u);
        assert(dpc245_ble_app_take_hmi_event_bits(&bap)==0u);

        /* Updater selector acknowledges first and enters the common handoff state. */
        dpc245_update_init(&bu);q.operation=0x30u;q.selector=0u;q.length=0u;
        assert(dpc245_ble_app_handle(&bap,&q,&ba,&br,&bid,false,&bu,&rr));
        assert(rr.have_tx&&rr.update_requested&&bu.state==DPC245_UPDATE_SEND_COORDINATION);

        br.telemetry.have_battery_pct=true;br.telemetry.battery_pct=77u;
        br.telemetry.have_speed=true;br.telemetry.speed_centi_kph=1234u;
        uint8_t periodic[DPC245_BLE_MAX_WIRE];
        size_t pn=dpc245_ble_app_build_periodic(periodic,sizeof(periodic),&ba,&br,true);
        assert(pn==48u);
        dpc245_ble_parser_init(&rp);rgot=false;
        for(size_t i=0u;i<pn;i++)if(dpc245_ble_parser_feed(&rp,periodic[i],&rf))rgot=true;
        assert(rgot&&rf.class_id==0x10u&&rf.command_id==0x11u&&rf.operation==0x06u&&rf.selector==1u);
        assert(rf.length==39u&&rf.payload[4]==1u&&rf.payload[7]==77u);

        /* The status bytes follow the display serializer. */
        ba.assist_level=2u;
        ba.walk=false;
        ba.runtime.settings.raw[0x2Eu]=0u;
        dpc245_set_unit(&ba.runtime.settings,1u);
        dpc245_set_u32_le(&ba.runtime.settings,DPC245_OFS_TRIP_DISTANCE_RAW,123456u);
        dpc245_set_u32_le(&ba.runtime.settings,DPC245_OFS_TOTAL_DISTANCE_RAW,987654u);
        dpc245_set_u32_le(&ba.runtime.settings,DPC245_OFS_TRIP_TIME_HALFSEC,3720u);
        dpc245_set_u16_le(&ba.runtime.settings,DPC245_OFS_AVG_SPEED_RAW,321u);
        dpc245_set_u16_le(&ba.runtime.settings,DPC245_OFS_MAX_SPEED_RAW,654u);
        ba.runtime.rider_energy_half_joule=258u*8368u;
        br.telemetry.speed_centi_kph=2345u;
        br.telemetry.have_range=true;br.telemetry.range_centi_km=0x1234u;
        const uint8_t expected_live[39]={
            [3]=1u,[4]=1u,[5]=2u,[6]=5u,[7]=77u,
            [9]=0x24u,[10]=0x09u,[11]=0x39u,[12]=0x30u,
            [15]=0x02u,[16]=0x01u,[17]=0x34u,[18]=0x12u,
            [19]=0x95u,[20]=0x26u,[21]=31u,[24]=0x02u,[25]=0x01u,
            [28]=31u,[32]=1u,[35]=0x41u,[36]=0x01u,
            [37]=0x8Eu,[38]=0x02u
        };
        pn=dpc245_ble_app_build_periodic(periodic,sizeof(periodic),&ba,&br,true);
        assert(pn==48u);
        dpc245_ble_parser_init(&rp);rgot=false;
        for(size_t i=0u;i<pn;i++)if(dpc245_ble_parser_feed(&rp,periodic[i],&rf))rgot=true;
        assert(rgot&&rf.length==sizeof(expected_live));
        for(size_t i=0u;i<sizeof(expected_live);i++)assert(rf.payload[i]==expected_live[i]);

        /* rider_power_avg: [26..27], bit15 = valid, bits 0..14 = watts. */
        pn=dpc245_ble_app_build_periodic(periodic,sizeof(periodic),&ba,&br,true);
        dpc245_ble_parser_init(&rp);rgot=false;
        for(size_t i=0u;i<pn;i++)if(dpc245_ble_parser_feed(&rp,periodic[i],&rf))rgot=true;
        assert(rgot&&rf.payload[26]==0u&&rf.payload[27]==0u); /* no sample yet */
        pedal_at(&ba.runtime,2000u,5182u,0u);
        pn=dpc245_ble_app_build_periodic(periodic,sizeof(periodic),&ba,&br,true);
        dpc245_ble_parser_init(&rp);rgot=false;
        for(size_t i=0u;i<pn;i++)if(dpc245_ble_parser_feed(&rp,periodic[i],&rf))rgot=true;
        assert(rgot&&rf.payload[26]==0x00u&&rf.payload[27]==0x80u); /* valid 0 W */
        pedal_at(&ba.runtime,4000u,2966u,30u);
        pn=dpc245_ble_app_build_periodic(periodic,sizeof(periodic),&ba,&br,true);
        dpc245_ble_parser_init(&rp);rgot=false;
        for(size_t i=0u;i<pn;i++)if(dpc245_ble_parser_feed(&rp,periodic[i],&rf))rgot=true;
        assert(rgot&&rf.payload[26]==0xFAu&&rf.payload[27]==0x80u); /* valid 250 W */
        for(size_t i=0u;i<sizeof(expected_live);i++)
            if(i!=26u&&i!=27u)assert(rf.payload[i]==expected_live[i]);

        ba.assist_level=3u;
        pn=dpc245_ble_app_build_periodic(periodic,sizeof(periodic),&ba,&br,true);
        dpc245_ble_parser_init(&rp);rgot=false;
        for(size_t i=0u;i<pn;i++)if(dpc245_ble_parser_feed(&rp,periodic[i],&rf))rgot=true;
        assert(rgot&&rf.payload[5]==3u);
        ba.walk=true;
        pn=dpc245_ble_app_build_periodic(periodic,sizeof(periodic),&ba,&br,true);
        dpc245_ble_parser_init(&rp);rgot=false;
        for(size_t i=0u;i<pn;i++)if(dpc245_ble_parser_feed(&rp,periodic[i],&rf))rgot=true;
        assert(rgot&&rf.payload[5]==0xFFu);
        ba.runtime.settings.raw[0x2Eu]=1u;
        pn=dpc245_ble_app_build_periodic(periodic,sizeof(periodic),&ba,&br,true);
        dpc245_ble_parser_init(&rp);rgot=false;
        for(size_t i=0u;i<pn;i++)if(dpc245_ble_parser_feed(&rp,periodic[i],&rf))rgot=true;
        assert(rgot&&rf.payload[5]==0u);
    }

    /* DPC245 V3 periodic schedule table. */
    {
        const dpc245_v3_periodic_can_t *sp=dpc245_v3_periodic_find(0x03106300u);
        assert(sp && sp->period_ms==100u && sp->dlc==4u && sp->frame_slot==1u);
        sp=dpc245_v3_periodic_find(0x03106301u);
        assert(sp && sp->period_ms==200u && sp->dlc==8u && sp->frame_slot==5u);
        sp=dpc245_v3_periodic_find(0x03986207u);
        assert(sp && sp->period_ms==1000u && sp->dlc==1u && sp->frame_slot==7u);
        assert(!sp->transmitted);
        assert(dpc245_v3_periodic_can_count==6u);
        assert(dpc245_v3_periodic_find(0x12345678u)==NULL);

        /* Emission intervals follow from the 33 ms job:
         * 0x6300 132 ms, 0x6301/02/04 231 ms, 0x6303 1023 ms, 0x6207 never.
         * The main loop polls every 1 ms; start phase is arbitrary. */
        dpc245_v3_tx_schedule_t ts; dpc245_v3_tx_schedule_init(&ts,5000u);
        uint32_t last_emit[8]={0}, first_gap[8]={0}, emits[8]={0};
        for(uint32_t now=5000u; now<5000u+20000u; ++now){
            uint32_t due=dpc245_v3_tx_schedule_poll(&ts,now);
            for(unsigned i=0;i<dpc245_v3_periodic_can_count;i++) if(due&(1u<<i)){
                if(emits[i]>=1u){
                    uint32_t gap=now-last_emit[i];
                    if(emits[i]==1u) first_gap[i]=gap;
                    assert(gap==first_gap[i]);   /* steady, jitter-free */
                }
                last_emit[i]=now; emits[i]++;
            }
        }
        static const uint32_t expect_gap[6]={132u,231u,231u,231u,1023u,0u};
        for(unsigned i=0;i<6u;i++){
            if(!dpc245_v3_periodic_can[i].transmitted){ assert(emits[i]==0u); continue; }
            assert(first_gap[i]==expect_gap[i]);
        }

        /* Status frame serializers. Inputs are named by their offset in the
           settings record. */
        dpc245_v3_hmi_stats_raw_t hs={0};
        hs.raw06=0x66u; hs.raw07=0u; hs.raw0a=0xAAu;
        hs.raw10=123450u;  /* (123450+50)/100 = 1235 = 0x0004D3 */
        hs.raw14=0x01234560u; /* /10 = 0x001D2089 */
        hs.raw18=0x00123450u; /* /10 = 0x0001D208 */
        hs.raw2c=0xBEEFu; hs.raw2f=0xCAFEu;
        bafang_can_frame_t df;
        assert(dpc245_v3_build_status_frame_4(&df,&hs));
        assert(df.id==0x03106304u && df.dlc==4u);
        assert(df.data[0]==0x05u && df.data[1]==0xAAu && df.data[2]==0x04u && df.data[3]==0x66u);
        assert(dpc245_v3_build_status_frame_1(&df,&hs));
        assert(df.id==0x03106301u && df.dlc==8u);
        assert(df.data[0]==0xD3u && df.data[1]==0x04u && df.data[2]==0x00u);
        { uint32_t b=hs.raw14/10u; assert(df.data[3]==(uint8_t)b && df.data[4]==(uint8_t)(b>>8) && df.data[5]==(uint8_t)(b>>16)); }
        assert(df.data[6]==0xFEu && df.data[7]==0xCAu);
        assert(dpc245_v3_build_status_frame_2(&df,&hs));
        assert(df.id==0x03106302u && df.dlc==5u);
        assert(df.data[0]==0xEFu && df.data[1]==0xBEu);
        { uint32_t x=hs.raw18/10u; assert(df.data[2]==(uint8_t)x && df.data[3]==(uint8_t)(x>>8) && df.data[4]==(uint8_t)(x>>16)); }
        assert(dpc245_v3_build_status_frame_3(&df,&hs));
        assert(df.id==0x03106303u && df.dlc==1u && df.data[0]==0xFFu);
        hs.raw07=0x42u; assert(dpc245_v3_build_status_frame_3(&df,&hs) && df.data[0]==0x42u);
        assert(!dpc245_v3_build_status_frame_1(NULL,&hs));
    }


    /* 02F83200 distance accounting and Trip reset. */
    {
        bafang_can_frame_t fast0={.id=0x02F83200u,.dlc=8,.data={88u,0x34u,0x12u,77u,0xAAu,0x01u,0x10u,0x27u}};
        bafang_telemetry_t ft={0};
        assert(bafang_decode_telemetry(&fast0,&ft));
        assert(ft.have_distance_counter && ft.distance_counter_raw==0x1234u);
        assert(ft.cadence_rpm==77u && ft.torque_signal_mv==0x01AAu && ft.range_centi_km==0x2710u);

        dpc245_settings_t base; dpc245_settings_defaults(&base);
        dpc245_set_u32_le(&base,DPC245_OFS_TOTAL_DISTANCE_RAW,1000u);
        dpc245_set_u32_le(&base,DPC245_OFS_SERVICE_DISTANCE_RAW,2000u);
        dpc245_runtime_t sr; dpc245_runtime_init(&sr,&base);
        assert(dpc245_runtime_on_can(&sr,&fast0)); /* baseline only */
        assert(dpc245_get_u32_le(&sr.settings,DPC245_OFS_TOTAL_DISTANCE_RAW)==1000u);
        fast0.data[1]=0x3Eu; fast0.data[2]=0x12u; /* +10 */
        assert(dpc245_runtime_on_can(&sr,&fast0));
        assert(dpc245_get_u32_le(&sr.settings,DPC245_OFS_TOTAL_DISTANCE_RAW)==1010u);
        assert(dpc245_get_u32_le(&sr.settings,DPC245_OFS_TRIP_DISTANCE_RAW)==10u);
        assert(dpc245_get_u32_le(&sr.settings,DPC245_OFS_SERVICE_DISTANCE_RAW)==2010u);
        assert(dpc245_get_u32_le(&sr.settings,DPC245_OFS_DISTANCE_AUX_RAW)==10u);
        /* Backwards/reset sample establishes a new baseline without adding. */
        fast0.data[1]=5u; fast0.data[2]=0u;
        assert(dpc245_runtime_on_can(&sr,&fast0));
        assert(dpc245_get_u32_le(&sr.settings,DPC245_OFS_TOTAL_DISTANCE_RAW)==1010u);

        dpc245_set_u32_le(&sr.settings,DPC245_OFS_TRIP_TIME_HALFSEC,11u);
        dpc245_set_u32_le(&sr.settings,DPC245_OFS_TIME_AUX_HALFSEC,12u);
        dpc245_set_u16_le(&sr.settings,DPC245_OFS_MAX_SPEED_RAW,3333u);
        dpc245_set_u16_le(&sr.settings,DPC245_OFS_AVG_SPEED_RAW,2222u);
        dpc245_runtime_tick_500ms(&sr,true);
        assert(dpc245_get_u32_le(&sr.settings,DPC245_OFS_TRIP_TIME_HALFSEC)==12u);
        assert(dpc245_get_u32_le(&sr.settings,DPC245_OFS_TIME_AUX_HALFSEC)==13u);
        /* MAX/AVG: max follows peak speed; avg is
           720*distance_aux/time_halfsec and is clamped to max. */
        dpc245_set_u16_le(&sr.settings,DPC245_OFS_MAX_SPEED_RAW,0u);
        dpc245_set_u16_le(&sr.settings,DPC245_OFS_AVG_SPEED_RAW,0u);
        dpc245_set_u32_le(&sr.settings,DPC245_OFS_DISTANCE_AUX_RAW,100u);
        dpc245_set_u32_le(&sr.settings,DPC245_OFS_TIME_AUX_HALFSEC,20u);
        bafang_can_frame_t fast1={.id=0x02F83201u,.dlc=8,.data={0xB8u,0x0Bu,0,0,0,0,0,0}}; /* 3000 centi-km/h = 30.0 km/h */
        assert(dpc245_runtime_on_can(&sr,&fast1));
        assert(dpc245_get_u16_le(&sr.settings,DPC245_OFS_MAX_SPEED_RAW)==300u);
        /* formula gives 3600 tenths km/h, AVG is clamped to MAX=300 */
        assert(dpc245_get_u16_le(&sr.settings,DPC245_OFS_AVG_SPEED_RAW)==300u);
        dpc245_set_u32_le(&sr.settings,DPC245_OFS_DISTANCE_AUX_RAW,5u);
        sr.previous_avg_distance_aux=0u;
        assert(dpc245_runtime_on_can(&sr,&fast1));
        /* 720*5/20 = 180 = 18.0 km/h, below MAX */
        assert(dpc245_get_u16_le(&sr.settings,DPC245_OFS_AVG_SPEED_RAW)==180u);

        /* Service threshold: bit/state becomes
           due at target and accumulated service distance is clamped to target. */
        dpc245_set_u32_le(&sr.settings,DPC245_OFS_SERVICE_TARGET_RAW,2500u);
        dpc245_set_u32_le(&sr.settings,DPC245_OFS_SERVICE_DISTANCE_RAW,2499u);
        assert(dpc245_runtime_on_can(&sr,&fast1));
        assert(!dpc245_runtime_service_due(&sr));
        dpc245_set_u32_le(&sr.settings,DPC245_OFS_SERVICE_DISTANCE_RAW,2501u);
        assert(dpc245_runtime_on_can(&sr,&fast1));
        assert(dpc245_runtime_service_due(&sr));
        assert(!dpc245_runtime_service_tip_active(&sr));
        dpc245_set_service_tips(&sr.settings,1u);
        assert(dpc245_runtime_service_tip_active(&sr));
        dpc245_set_service_tips(&sr.settings,0u);
        assert(dpc245_get_u32_le(&sr.settings,DPC245_OFS_SERVICE_DISTANCE_RAW)==2500u);

        /* Restore the pre-threshold value used by the Trip reset assertions. */
        dpc245_set_u32_le(&sr.settings,DPC245_OFS_SERVICE_DISTANCE_RAW,2010u);
        dpc245_runtime_trip_reset(&sr);
        assert(dpc245_get_u32_le(&sr.settings,DPC245_OFS_TRIP_DISTANCE_RAW)==0u);
        assert(dpc245_get_u32_le(&sr.settings,DPC245_OFS_TRIP_TIME_HALFSEC)==0u);
        assert(dpc245_get_u32_le(&sr.settings,DPC245_OFS_DISTANCE_AUX_RAW)==0u);
        assert(dpc245_get_u32_le(&sr.settings,DPC245_OFS_TIME_AUX_HALFSEC)==0u);
        assert(dpc245_get_u16_le(&sr.settings,DPC245_OFS_MAX_SPEED_RAW)==0u);
        assert(dpc245_get_u16_le(&sr.settings,DPC245_OFS_AVG_SPEED_RAW)==0u);
        assert(dpc245_get_u32_le(&sr.settings,DPC245_OFS_TOTAL_DISTANCE_RAW)==1010u);
        assert(dpc245_get_u32_le(&sr.settings,DPC245_OFS_SERVICE_DISTANCE_RAW)==2010u);

        dpc245_v3_hmi_stats_raw_t sraw={0}; dpc245_runtime_hmi_stats(&sr,&sraw);
        assert(sraw.raw10==1010u && sraw.raw14==0u && sraw.raw18==2010u);
    }

    /* Rider power moving average, freshness and calorie input. */
    {
        dpc245_settings_t ps; dpc245_settings_defaults(&ps);
        dpc245_runtime_t rt; dpc245_runtime_init(&rt,&ps);
        uint16_t w=0xFFFFu;

        /* No frame yet: unavailable. Poll alone does not invent a value. */
        assert(!dpc245_runtime_pedal_avg(&rt,&w));
        dpc245_runtime_pedal_poll(&rt,1000u);
        assert(!dpc245_runtime_pedal_avg(&rt,&w));

        /* Single sample (torque 2966, cadence 30 -> 250 W). */
        pedal_at(&rt,1000u,2966u,30u);
        assert(rt.pedal_power_w==250u);
        assert(dpc245_runtime_pedal_avg(&rt,&w) && w==250u);

        /* Partial window: mean of the samples present, rounded to nearest. */
        dpc245_runtime_init(&rt,&ps);
        pedal_at(&rt,3000u,1637u,30u);
        assert(dpc245_runtime_pedal_avg(&rt,&w) && w==100u);
        pedal_at(&rt,3000u,2523u,30u);
        assert(dpc245_runtime_pedal_avg(&rt,&w) && w==150u);
        pedal_at(&rt,3000u,3409u,30u);
        assert(dpc245_runtime_pedal_avg(&rt,&w) && w==200u);
        pedal_at(&rt,3000u,4296u,30u);
        assert(dpc245_runtime_pedal_avg(&rt,&w) && w==250u);
        pedal_at(&rt,3000u,5182u,30u);
        assert(dpc245_runtime_pedal_avg(&rt,&w) && w==300u);

        /* Six samples: the oldest leaves the five-sample window. */
        pedal_at(&rt,3000u,6068u,30u);
        assert(dpc245_runtime_pedal_avg(&rt,&w) && w==400u);

        /* Zero samples with cadence above zero stay in the window and lower
         * the mean; they do not reset it. */
        pedal_at(&rt,3000u,0u,30u);
        assert(dpc245_runtime_pedal_avg(&rt,&w) && w==360u);
        assert(rt.pedal_power_w==0u);

        /* Stopped crank: window cleared, value 0 and still valid. */
        pedal_at(&rt,3000u,5182u,0u);
        assert(rt.pedal_power_w==0u);
        assert(dpc245_runtime_pedal_avg(&rt,&w) && w==0u);
        assert(rt.pedal_sample_count==0u);

        /* Torque at or below the offset is a zero sample, not a reset. */
        pedal_at(&rt,3000u,750u,30u);
        assert(dpc245_runtime_pedal_avg(&rt,&w) && w==0u);
        pedal_at(&rt,3000u,1637u,30u);
        assert(dpc245_runtime_pedal_avg(&rt,&w) && w==50u);

        /* More than 5000 ms without a 0x3200 frame: invalid, window cleared.
         * The next frame is accepted again with a single-sample mean. */
        pedal_at(&rt,10000u,5182u,0u); /* clear the window */
        pedal_at(&rt,10000u,2966u,30u);
        dpc245_runtime_pedal_poll(&rt,15000u);
        assert(dpc245_runtime_pedal_avg(&rt,&w) && w==250u);
        dpc245_runtime_pedal_poll(&rt,15001u);
        assert(!dpc245_runtime_pedal_avg(&rt,&w));
        assert(rt.pedal_sample_count==0u);
        pedal_at(&rt,15002u,1637u,30u);
        assert(dpc245_runtime_pedal_avg(&rt,&w) && w==100u);

        /* Wrap-safe freshness across the 32-bit millisecond boundary. */
        dpc245_runtime_t rt2; dpc245_runtime_init(&rt2,&ps);
        const uint32_t base=0xFFFFFF00u;
        pedal_at(&rt2,base,2966u,30u);
        dpc245_runtime_pedal_poll(&rt2,base+5000u);
        assert(dpc245_runtime_pedal_avg(&rt2,&w) && w==250u);
        dpc245_runtime_pedal_poll(&rt2,base+5001u);
        assert(!dpc245_runtime_pedal_avg(&rt2,&w));
        pedal_at(&rt2,base+5001u,1637u,30u);
        assert(dpc245_runtime_pedal_avg(&rt2,&w) && w==100u);

        /* The displayed average saturates at 9999 W; the per-frame sample is
         * unchanged for the calorie integration. */
        pedal_at(&rt2,base+5001u,65000u,255u);
        assert(rt2.pedal_power_w==61624u);
        assert(dpc245_runtime_pedal_avg(&rt2,&w) && w==9999u);

        /* Trip reset clears the window and the average; validity follows
         * sample freshness. */
        dpc245_runtime_t rt3; dpc245_runtime_init(&rt3,&ps);
        pedal_at(&rt3,1000u,1637u,30u);
        pedal_at(&rt3,1000u,2523u,30u);
        assert(dpc245_runtime_pedal_avg(&rt3,&w) && w==150u);
        dpc245_runtime_trip_reset(&rt3);
        assert(rt3.pedal_sample_count==0u);
        assert(dpc245_runtime_pedal_avg(&rt3,&w) && w==0u);
        pedal_at(&rt3,1000u,3409u,30u);
        assert(dpc245_runtime_pedal_avg(&rt3,&w) && w==300u);

        /* Calories keep integrating the per-frame sample, not the average. */
        dpc245_runtime_t rt4; dpc245_runtime_init(&rt4,&ps);
        pedal_at(&rt4,1000u,1637u,30u);
        dpc245_runtime_tick_500ms(&rt4,true);
        assert(rt4.rider_energy_half_joule==100u);
        pedal_at(&rt4,1000u,3409u,30u);
        assert(dpc245_runtime_pedal_avg(&rt4,&w) && w==200u);
        dpc245_runtime_tick_500ms(&rt4,true);
        assert(rt4.rider_energy_half_joule==400u);
    }

    /* Persistent settings record and serial-memory journal. */
    {
        dpc245_settings_t ss;
        dpc245_settings_defaults(&ss);
        assert(dpc245_settings_validate(&ss));
        assert(ss.raw[0x06]==4u && ss.raw[0x07]==5u && ss.raw[0x08]==5u && ss.raw[0x0A]==3u);
        assert(dpc245_get_u32_le(&ss,0x1Cu)==500000u);
        assert(dpc245_unit(&ss)==0u);
        dpc245_set_unit(&ss,1u);
        dpc245_set_auto_off_min(&ss,10u);
        dpc245_set_brightness_level(&ss,2u);
        dpc245_set_service_tips(&ss,1u);
        dpc245_set_al_sensitivity(&ss,5u);
        dpc245_set_theme(&ss,DPC245_THEME_YELLOW);
        dpc245_set_language(&ss,4u);
        dpc245_settings_finalize(&ss);
        assert(dpc245_settings_validate(&ss));
        assert(dpc245_theme(&ss)==DPC245_THEME_YELLOW);
        ss.raw[0x0D]=1u;assert(dpc245_theme(&ss)==DPC245_THEME_STANDARD);
        ss.raw[0x0D]=2u;assert(dpc245_theme(&ss)==DPC245_THEME_YELLOW);
        dpc245_set_theme(&ss,DPC245_THEME_ARC);assert(dpc245_theme(&ss)==DPC245_THEME_ARC);
        dpc245_set_theme(&ss,DPC245_THEME_WEDGE);assert(dpc245_theme(&ss)==DPC245_THEME_WEDGE);
        dpc245_set_theme(&ss,DPC245_THEME_YELLOW);
        assert(dpc245_language(&ss)==4u);
        /* Status aliases resolve to the current fields. */
        assert(dpc245_theme(&ss)==DPC245_THEME_YELLOW);
        assert(dpc245_language(&ss)==4u);
        ss.raw[0x48]^=1u; assert(!dpc245_settings_validate(&ss)); ss.raw[0x48]^=1u;

        fake_mem_t fm; memset(&fm,0xff,sizeof(fm));
        dpc245_persist_io_t io={fm_read,fm_write,fm_erase,&fm};
        dpc245_settings_t loaded;
        assert(dpc245_persist_load(&io,&loaded)==0xffu); /* no record -> defaults */
        assert(dpc245_settings_validate(&loaded));
        loaded.raw[0x05]=1u; dpc245_settings_finalize(&loaded);
        uint8_t slot=0xffu; assert(dpc245_persist_save(&io,&loaded,&slot) && slot==0u);
        assert(fm.mem[0x300]==0xA5u && fm.mem[0x301]==0x5Au);
        dpc245_settings_t again; assert(dpc245_persist_load(&io,&again)==0u);
        assert(again.raw[0x05]==1u && dpc245_settings_validate(&again));
        again.raw[0x07]=7u; dpc245_settings_finalize(&again);
        assert(dpc245_persist_save(&io,&again,&slot) && slot==1u);
        assert(dpc245_persist_load(&io,&loaded)==1u && loaded.raw[0x07]==7u);
        /* Invalid newest record is skipped and the previous valid slot wins. */
        fm.mem[0x600+0x48]^=1u;
        assert(dpc245_persist_load(&io,&loaded)==0u && loaded.raw[0x05]==1u);
    }


    /* Physical key timing: 35 ms debounce, >=60 ms short and 4.01 s hold. */
    {
        dpc245_keys_t k;
        assert(dpc245_raw_key_code(DPC245_KEYS_RAW_DOWN)==1u);
        assert(dpc245_raw_key_code(DPC245_KEYS_RAW_UP)==2u);
        assert(dpc245_raw_key_code(DPC245_KEYS_RAW_UP|DPC245_KEYS_RAW_DOWN)==3u);
        assert(dpc245_raw_key_code(DPC245_KEYS_RAW_POWER)==4u);
        assert(dpc245_raw_key_code(DPC245_KEYS_RAW_POWER|DPC245_KEYS_RAW_DOWN)==8u);
        assert(dpc245_raw_key_code(DPC245_KEYS_RAW_POWER|DPC245_KEYS_RAW_UP|DPC245_KEYS_RAW_DOWN)==3u);
        assert(DPC245_KEY_SAMPLE_MS==10u && DPC245_KEY_DEBOUNCE_MS==35u);
        assert(DPC245_KEY_SHORT_MIN_MS==60u && DPC245_KEY_HOLD_MS==4010u);

        dpc245_keys_init(&k,0u,0u);
        uint32_t e;
        for(uint32_t t=10u;t<=4050u;t+=10u){
            e=dpc245_keys_update(&k,DPC245_KEYS_RAW_UP,t);
            if(t<4060u)assert((e&DPC245_KEY_EVENT_UP_HOLD)==0u);
        }
        e=dpc245_keys_update(&k,DPC245_KEYS_RAW_UP,4060u); assert(e&DPC245_KEY_EVENT_UP_HOLD);
        for(uint32_t t=4070u;t<=4110u;t+=10u){
            e=dpc245_keys_update(&k,0u,t);assert((e&DPC245_KEY_EVENT_UP_SHORT)==0u);
        }

        dpc245_keys_init(&k,0u,0u);
        for(uint32_t t=10u;t<=100u;t+=10u)
            assert(dpc245_keys_update(&k,DPC245_KEYS_RAW_POWER,t)==0u);
        for(uint32_t t=110u;t<150u;t+=10u)
            assert(dpc245_keys_update(&k,0u,t)==0u);
        e=dpc245_keys_update(&k,0u,150u); assert(e&DPC245_KEY_EVENT_POWER_SHORT);
        dpc245_keys_init(&k,0u,0u);
        for(uint32_t t=10u;t<=50u;t+=10u)
            assert(dpc245_keys_update(&k,DPC245_KEYS_RAW_POWER,t)==0u);
        for(uint32_t t=60u;t<=100u;t+=10u)
            assert(dpc245_keys_update(&k,0u,t)==0u);

        dpc245_keys_init(&k,0u,0u);
        uint8_t chord=DPC245_KEYS_RAW_UP|DPC245_KEYS_RAW_DOWN;
        for(uint32_t t=10u;t<550u;t+=10u)
            assert((dpc245_keys_update(&k,chord,t)&DPC245_KEY_EVENT_SETTINGS_CHORD)==0u);
        e=dpc245_keys_update(&k,chord,550u); assert(e&DPC245_KEY_EVENT_SETTINGS_CHORD);
        for(uint32_t t=560u;t<=600u;t+=10u){
            e=dpc245_keys_update(&k,0u,t);
            assert((e&(DPC245_KEY_EVENT_UP_SHORT|DPC245_KEY_EVENT_DOWN_SHORT))==0u);
        }

        dpc245_keys_init(&k,0u,0u);
        for(uint32_t t=10u;t<50u;t+=10u)
            assert(dpc245_keys_update(&k,DPC245_KEYS_RAW_DOWN,t)==0u);
        e=dpc245_keys_update(&k,DPC245_KEYS_RAW_DOWN,50u); assert(e&DPC245_KEY_EVENT_DOWN_PRESS);
        e=dpc245_keys_update(&k,0u,60u); assert(e==0u); /* one-sample release glitch */
        e=dpc245_keys_update(&k,DPC245_KEYS_RAW_DOWN,70u); assert(e==0u);
        for(uint32_t t=80u;t<4060u;t+=10u)
            assert((dpc245_keys_update(&k,DPC245_KEYS_RAW_DOWN,t)&DPC245_KEY_EVENT_DOWN_RELEASE)==0u);
        e=dpc245_keys_update(&k,DPC245_KEYS_RAW_DOWN,4060u); assert(e&DPC245_KEY_EVENT_DOWN_HOLD);
        for(uint32_t t=4070u;t<4110u;t+=10u)
            assert(dpc245_keys_update(&k,0u,t)==0u);
        e=dpc245_keys_update(&k,0u,4110u); assert(e&DPC245_KEY_EVENT_DOWN_RELEASE);
        assert((e&DPC245_KEY_EVENT_DOWN_SHORT)==0u);

        dpc245_keys_init(&k,0u,0u);
        uint8_t repeats=0u;
        for(uint32_t t=10u;t<=50u;t+=10u) e=dpc245_keys_update(&k,DPC245_KEYS_RAW_UP,t);
        assert(e&DPC245_KEY_EVENT_UP_PRESS);
        for(uint32_t t=60u;t<450u;t+=10u)
            assert((dpc245_keys_update(&k,DPC245_KEYS_RAW_UP,t)&DPC245_KEY_EVENT_UP_REPEAT)==0u);
        e=dpc245_keys_update(&k,DPC245_KEYS_RAW_UP,450u); assert(e&DPC245_KEY_EVENT_UP_REPEAT); ++repeats;
        e=dpc245_keys_update(&k,DPC245_KEYS_RAW_UP,570u); assert(e&DPC245_KEY_EVENT_UP_REPEAT); ++repeats;
        for(uint32_t t=580u;t<2050u;t+=10u)
            { e=dpc245_keys_update(&k,DPC245_KEYS_RAW_UP,t); if(e&DPC245_KEY_EVENT_UP_REPEAT)++repeats; }
        assert(e==0u);
        e=dpc245_keys_update(&k,DPC245_KEYS_RAW_UP,2050u); assert(e&DPC245_KEY_EVENT_UP_REPEAT_FAST); ++repeats;
        for(uint32_t t=2060u;t<=3050u;t+=10u)
            { e=dpc245_keys_update(&k,DPC245_KEYS_RAW_UP,t); if(e&DPC245_KEY_EVENT_UP_REPEAT_FAST)++repeats; }
        assert(repeats==40u);
        e=dpc245_keys_update(&k,0u,3060u); assert((e&DPC245_KEY_EVENT_UP_REPEAT_FAST)==0u);
    }

    /* Application-code translation is kept separate from physical timing. */
    {
        assert(dpc245_translate_appkey(1u,1u,false,true)==DPC245_APPKEY_DOWN_SHORT);
        assert(dpc245_translate_appkey(2u,1u,false,true)==DPC245_APPKEY_UP_SHORT);
        assert(dpc245_translate_appkey(4u,1u,false,true)==DPC245_APPKEY_POWER);
        assert(dpc245_translate_appkey(1u,0u,false,true)==DPC245_APPKEY_NONE);
        assert(dpc245_translate_appkey(1u,0u,true,true)==DPC245_APPKEY_DOWN_HOLD);
        assert(dpc245_translate_appkey(2u,0u,false,true)==DPC245_APPKEY_UP_HOLD);
        assert(dpc245_translate_appkey(3u,0u,false,true)==DPC245_APPKEY_CHORD);
        assert(dpc245_translate_appkey(3u,0u,false,false)==DPC245_APPKEY_NONE);
        assert(dpc245_translate_appkey(8u,0u,false,true)==DPC245_APPKEY_POWER_DOWN_COMBO);
        assert(dpc245_translate_appkey(5u,3u,false,true)==DPC245_APPKEY_VERY_LONG);
    }

    /* Repeat events affect edited values only and clamp accelerated scales. */
    {
        dpc245_settings_t rs; dpc245_settings_defaults(&rs);
        dpc245_app_t ra; dpc245_app_init(&ra,&rs);
        ra.screen=DPC245_APP_SCREEN_ADVANCED; ra.advanced.screen=EVIST_ADV_GRAPH;
        ra.advanced.edit=true; ra.advanced.working.graph_scale_w=250u;
        dpc245_app_event(&ra,DPC245_KEY_EVENT_UP_REPEAT_FAST,100u);
        assert(ra.advanced.working.graph_scale_w==375u);
        ra.advanced.working.graph_scale_w=3000u;
        dpc245_app_event(&ra,DPC245_KEY_EVENT_UP_REPEAT_FAST,140u);
        assert(ra.advanced.working.graph_scale_w==3000u);
        ra.advanced.edit=false; ra.advanced.screen=EVIST_ADV_MENU; ra.advanced.cursor=3u;
        dpc245_app_event(&ra,DPC245_KEY_EVENT_DOWN_REPEAT,180u);
        assert(ra.advanced.cursor==3u);
        ra.screen=DPC245_APP_SCREEN_DASHBOARD; ra.assist_level=2u;
        dpc245_app_event(&ra,DPC245_KEY_EVENT_DOWN_REPEAT,220u);
        assert(ra.assist_level==2u && !ra.walk);
        dpc245_app_event(&ra,DPC245_KEY_EVENT_UP_HOLD,260u);
        assert(ra.light_toggle_requested);
    }

    /* Standard Walk keeps the five-second short-click window and stays on during a held DOWN. */
    {
        dpc245_settings_t ss; dpc245_settings_defaults(&ss);
        dpc245_app_t sc; dpc245_app_init(&sc,&ss);
        assert(sc.screen==DPC245_APP_SCREEN_DASHBOARD && sc.assist_level==0u);
        dpc245_app_event(&sc,DPC245_KEY_EVENT_DOWN_PRESS,10u);
        assert(!sc.walk);
        dpc245_app_event(&sc,DPC245_KEY_EVENT_DOWN_RELEASE,80u);
        dpc245_app_event(&sc,DPC245_KEY_EVENT_DOWN_SHORT,80u);
        assert(sc.walk && sc.walk_armed && sc.walk_active && !sc.walk_quick);
        bafang_can_frame_t whb;
        assert(dpc245_build_periodic_frame(0x03106300u,&sc,false,0u,0u,&whb));
        assert(whb.data[1]==0x06u);
        dpc245_app_tick(&sc,5079u); assert(sc.walk);
        dpc245_app_tick(&sc,5080u); assert(!sc.walk && !sc.walk_armed);
        dpc245_app_event(&sc,DPC245_KEY_EVENT_DOWN_PRESS,6000u);
        dpc245_app_event(&sc,DPC245_KEY_EVENT_DOWN_RELEASE,6070u);
        dpc245_app_event(&sc,DPC245_KEY_EVENT_DOWN_SHORT,6070u);
        dpc245_app_event(&sc,DPC245_KEY_EVENT_DOWN_PRESS,6100u);
        for(uint32_t t=6100u;t<=36100u;t+=100u){
            if(t==10100u)dpc245_app_event(&sc,DPC245_KEY_EVENT_DOWN_HOLD,t);
            dpc245_app_tick(&sc,t);
            assert(sc.walk && sc.down_held && sc.assist_level==0u);
        }
        dpc245_app_event(&sc,DPC245_KEY_EVENT_DOWN_RELEASE,36110u);
        assert(!sc.walk && !sc.down_held && sc.assist_level==0u);
    }

    /* EVistDrive preference journal and advanced-settings round trip. */
    {
        memset(pref_mem,0xff,sizeof(pref_mem));
        dpc245_persist_io_t pio={pm_read,pm_write,pm_erase,0};
        evist_preferences_t p0,p1,p2;
        evist_preferences_defaults(&p0);
        assert(evist_preferences_validate(&p0));
        assert(!evist_preferences_load(&pio,&p1));
        /* Battery record in the encoding without BAR tag. */
        memcpy(&pref_mem[EVIST_PREF_REGION_BASE],"EVP1\xBA\xA9\xAF\xCE",8u);
        pref_mem[EVIST_PREF_SET_BASE]=0x55u;
        pref_mem[EVIST_PREF_SET_BASE+1u]=(uint8_t)(0x55u^0xC7u);
        assert(evist_preferences_load(&pio,&p1));
        assert(p1.battery_mask==(EVIST_BATTERY_SOC|EVIST_BATTERY_ICON));
        assert((p1.battery_mask&EVIST_BATTERY_BAR)==0u);
        memset(pref_mem,0xff,sizeof(pref_mem));
        evist_preferences_defaults(&p0);
        p1=p0;p1.assist_level=7u;p1.lower_panel=5u;p1.graph_scale_w=1750u;
        p1.battery_mask=EVIST_BATTERY_SOC|EVIST_BATTERY_ICON;p1.light_gain=8u;
        p1.light_dwell=6u;p1.light_threshold=3600u;p1.light_level=11u;
        p1.panel_mask=(uint16_t)((1u<<0)|(1u<<5)|(1u<<8));p1.quick_walk_enabled=true;
        p1.quick_walk_hold_steps=6u;
        assert(evist_preferences_save(&pio,&p0,&p1)==EVIST_PREF_SAVE_OK);
        assert(evist_preferences_load(&pio,&p2));
        assert(p2.battery_mask==(EVIST_BATTERY_SOC|EVIST_BATTERY_ICON)); /* prior encoding */
        assert((p2.battery_mask&EVIST_BATTERY_BAR)==0u); /* old record means BAR off */
        p1=p2;p1.battery_mask|=EVIST_BATTERY_BAR;
        assert(evist_preferences_save(&pio,&p2,&p1)==EVIST_PREF_SAVE_OK);
        assert(evist_preferences_load(&pio,&p2));
        assert(p2.assist_level==7u && p2.lower_panel==5u && p2.graph_scale_w==1750u);
        assert(p2.battery_mask==(EVIST_BATTERY_SOC|EVIST_BATTERY_ICON|EVIST_BATTERY_BAR));
        assert(p2.light_gain==8u && p2.light_dwell==6u && p2.light_threshold==3600u && p2.light_level==11u);
        assert(p2.panel_mask==p1.panel_mask && p2.quick_walk_enabled && p2.quick_walk_hold_steps==6u);
        /* Every earlier threshold value 0x59..0x5F remains a threshold record. */
        {
            uint32_t a=EVIST_PREF_SET_BASE;
            while(pref_mem[a]!=0xFFu || pref_mem[a+1u]!=0xFFu)a+=2u;
            for(uint8_t c=0x59u;c<=0x5Fu;++c){
                pref_mem[a]=c;pref_mem[a+1u]=(uint8_t)(c^0xC7u);
                assert(evist_preferences_load(&pio,&p2));
                assert(p2.light_threshold==(uint16_t)(1800u+(uint16_t)(c-0x59u)*100u));
                a+=2u;
            }
            pref_mem[a]=0x79u;pref_mem[a+1u]=(uint8_t)(0x79u^0xC7u);
            assert(evist_preferences_load(&pio,&p2) && p2.light_threshold==5000u);
        }

        evist_advanced_t adv;evist_advanced_open(&adv,&p2,5u);
        assert(evist_advanced_row_count(&adv)==12u);
        adv.screen=EVIST_ADV_BATTERY;
        assert(evist_advanced_row_count(&adv)==6u);
        {
            static const uint8_t battery_bits[4]={EVIST_BATTERY_ICON,EVIST_BATTERY_SOC,EVIST_BATTERY_VOLT,EVIST_BATTERY_BAR};
            uint8_t original=adv.working.battery_mask;
            for(uint8_t i=0u;i<4u;++i){
                adv.cursor=i;evist_advanced_key(&adv,EVIST_ADV_KEY_POWER);
                evist_advanced_key(&adv,EVIST_ADV_KEY_UP);evist_advanced_key(&adv,EVIST_ADV_KEY_POWER);
                assert(adv.working.battery_mask==(uint8_t)(original^battery_bits[i]));
                evist_advanced_key(&adv,EVIST_ADV_KEY_POWER);
                evist_advanced_key(&adv,EVIST_ADV_KEY_UP);evist_advanced_key(&adv,EVIST_ADV_KEY_POWER);
                assert(adv.working.battery_mask==original);
            }
        }
        adv.cursor=3u;evist_advanced_key(&adv,EVIST_ADV_KEY_POWER);
        evist_advanced_key(&adv,EVIST_ADV_KEY_UP);
        evist_advanced_key(&adv,EVIST_ADV_KEY_POWER);
        assert((adv.working.battery_mask&EVIST_BATTERY_BAR)==0u);
        adv.cursor=4u;evist_advanced_key(&adv,EVIST_ADV_KEY_POWER);
        assert(adv.preferences_changed);
        adv.screen=EVIST_ADV_MENU;adv.preferences_changed=false;adv.cursor=0u;
        for(unsigned i=0u;i<7u;i++)evist_advanced_key(&adv,EVIST_ADV_KEY_DOWN);
        evist_advanced_key(&adv,EVIST_ADV_KEY_POWER);
        assert(adv.screen==EVIST_ADV_PANELS);
        /* A panel mask must never be allowed to become empty. */
        adv.working.panel_mask=1u;adv.cursor=0u;evist_advanced_key(&adv,EVIST_ADV_KEY_POWER);
        evist_advanced_key(&adv,EVIST_ADV_KEY_DOWN);
        assert(adv.working.panel_mask==1u);

        /* DIAG is read-only: pages wrap in both directions, POWER returns to the menu. */
        adv.edit=false;adv.cursor=(uint8_t)(evist_advanced_row_count(&adv)-1u);
        evist_advanced_key(&adv,EVIST_ADV_KEY_POWER);
        assert(adv.screen==EVIST_ADV_MENU);
        adv.cursor=10u;evist_advanced_key(&adv,EVIST_ADV_KEY_POWER);
        assert(adv.screen==EVIST_ADV_DIAG && adv.diag_page==0u);
        evist_advanced_key(&adv,EVIST_ADV_KEY_UP);
        assert(adv.diag_page==6u);
        evist_advanced_key(&adv,EVIST_ADV_KEY_DOWN);
        assert(adv.diag_page==0u);
        assert(!adv.exit_requested && !adv.busy && adv.request==EVIST_ADV_REQ_NONE);
        evist_advanced_key(&adv,EVIST_ADV_KEY_POWER);
        assert(adv.screen==EVIST_ADV_MENU && !adv.exit_requested);

        /* EXIT is the last menu row (index 11); it still requests leaving the page. */
        adv.cursor=11u;evist_advanced_key(&adv,EVIST_ADV_KEY_POWER);
        assert(adv.exit_requested);

        /* LIGHT GAIN reaches the runtime before SAVE; BACK restores the committed value. */
        evist_advanced_t lt;evist_advanced_open(&lt,&p2,5u);
        evist_preferences_t rt;
        lt.cursor=6u;evist_advanced_key(&lt,EVIST_ADV_KEY_POWER);
        assert(lt.screen==EVIST_ADV_LIGHT);
        assert(!evist_advanced_take_light_runtime(&lt,&rt));
        lt.cursor=1u;evist_advanced_key(&lt,EVIST_ADV_KEY_POWER);
        evist_advanced_key(&lt,EVIST_ADV_KEY_UP);
        evist_advanced_key(&lt,EVIST_ADV_KEY_POWER);
        assert(evist_advanced_take_light_runtime(&lt,&rt));
        assert(rt.light_gain==(uint8_t)(p2.light_gain+1u) && lt.committed.light_gain==p2.light_gain);
        assert(!evist_advanced_take_light_runtime(&lt,&rt));
        lt.cursor=(uint8_t)(evist_advanced_row_count(&lt)-1u);
        evist_advanced_key(&lt,EVIST_ADV_KEY_POWER);
        assert(lt.screen==EVIST_ADV_MENU);
        assert(evist_advanced_take_light_runtime(&lt,&rt));
        assert(rt.light_gain==p2.light_gain);

        /* Idle exit from LIGHT discards the preview and publishes the committed runtime config. */
        dpc245_settings_t idle_settings;dpc245_settings_defaults(&idle_settings);
        dpc245_app_t idle_app;dpc245_app_init(&idle_app,&idle_settings);
        dpc245_app_apply_preferences(&idle_app,&p2);
        idle_app.screen=DPC245_APP_SCREEN_ADVANCED;
        idle_app.advanced.screen=EVIST_ADV_LIGHT;
        idle_app.advanced.working.light_gain=(uint8_t)(p2.light_gain+1u);
        idle_app.advanced.light_runtime_dirty=true;
        assert(evist_advanced_take_light_runtime(&idle_app.advanced,&rt));
        assert(rt.light_gain==(uint8_t)(p2.light_gain+1u));
        dpc245_app_tick(&idle_app,30000u);
        assert(idle_app.screen==DPC245_APP_SCREEN_DASHBOARD);
        assert(evist_advanced_take_light_runtime(&idle_app.advanced,&rt));
        assert(rt.light_gain==p2.light_gain);
        assert(!evist_advanced_take_preferences(&idle_app.advanced,&rt));
        assert(idle_app.advanced.request==EVIST_ADV_REQ_NONE && !idle_app.advanced.busy);
    }

    /* Configured Quick Walk is a separate EVistDrive dead-man shortcut. */
    {
        dpc245_settings_t s0;dpc245_settings_defaults(&s0);dpc245_app_t a0;dpc245_app_init(&a0,&s0);
        evist_preferences_t p;evist_preferences_defaults(&p);p.assist_level=2u;p.quick_walk_enabled=true;p.quick_walk_hold_steps=4u;
        dpc245_app_apply_preferences(&a0,&p);
        dpc245_app_event(&a0,DPC245_KEY_EVENT_DOWN_PRESS,100u);
        assert(a0.down_held && a0.walk_armed && a0.walk_quick && !a0.walk_active && !a0.walk);
        dpc245_app_event(&a0,DPC245_KEY_EVENT_DOWN_RELEASE,101u);
        assert(!a0.down_held && !a0.walk && !a0.walk_active && !a0.walk_armed);
        dpc245_app_event(&a0,DPC245_KEY_EVENT_DOWN_PRESS,200u);
        dpc245_app_tick(&a0,2199u);assert(!a0.walk_active);
        dpc245_app_tick(&a0,2200u);assert(a0.walk_active && a0.walk);
        for(uint32_t t=2300u;t<=30200u;t+=100u){
            dpc245_app_tick(&a0,t);
            assert(a0.walk && a0.down_held && a0.assist_level==2u);
        }
        assert(a0.assist_level==2u); /* shortcut does not destroy the selected assist level */
        dpc245_app_event(&a0,DPC245_KEY_EVENT_DOWN_RELEASE,30201u);
        assert(!a0.down_held && !a0.walk && !a0.walk_active && !a0.walk_armed && !a0.walk_quick);
        a0.ride_confirm_pending=true;
        dpc245_app_event(&a0,DPC245_KEY_EVENT_DOWN_PRESS,2300u);
        assert(a0.down_held); /* updated before the pending RIDE combo can return early */
        dpc245_app_event(&a0,DPC245_KEY_EVENT_DOWN_RELEASE,2301u);
        assert(!a0.down_held);
    }

    /* Controller configuration transactions: tests/test_config.c. */

    /* RTC register model and HMI-local CAN write/control commands. */
    {
        rtc_fake_t rf={{0},0u};
        dpc245_rtc_io_t rio={&rf,rtc_fake_read,rtc_fake_write};
        rf.r[DPC245_RTC_REG_WP]=DPC245_RTC_WP_ENABLE;
        rf.r[DPC245_RTC_REG_SECONDS]=dpc245_rtc_bcd_encode(3u);
        rf.r[DPC245_RTC_REG_MINUTES]=dpc245_rtc_bcd_encode(2u);
        rf.r[DPC245_RTC_REG_HOURS]=dpc245_rtc_bcd_encode(1u);
        rf.r[DPC245_RTC_REG_DATE]=dpc245_rtc_bcd_encode(7u);
        rf.r[DPC245_RTC_REG_MONTH]=dpc245_rtc_bcd_encode(9u);
        rf.r[DPC245_RTC_REG_WEEKDAY]=dpc245_rtc_bcd_encode(1u);
        rf.r[DPC245_RTC_REG_YEAR]=dpc245_rtc_bcd_encode(26u);
        dpc245_rtc_datetime_t dt;
        assert(dpc245_rtc_read(&rio,&dt));
        assert(dt.hour==1u&&dt.minute==2u&&dt.second==3u&&dt.date==7u&&dt.month==9u&&dt.year==26u);
        assert(dpc245_rtc_set_time(&rio,12u,34u,56u));
        assert(rf.r[DPC245_RTC_REG_WP]==DPC245_RTC_WP_ENABLE);
        assert(rf.r[DPC245_RTC_REG_HOURS]==0x12u&&rf.r[DPC245_RTC_REG_MINUTES]==0x34u&&rf.r[DPC245_RTC_REG_SECONDS]==0x56u);
        assert(!dpc245_rtc_set_time(&rio,24u,0u,0u));

        dpc245_settings_t ss; dpc245_settings_defaults(&ss);
        dpc245_runtime_t rt; dpc245_runtime_init(&rt,&ss);
        dpc245_hmi_control_result_t cr;
        bafang_can_frame_t wr={0};
        wr.id=bafang_can_id_encode((bafang_can_id_t){.src=BAFANG_NODE_BESST,.dst=BAFANG_NODE_HMI,.type=BAFANG_CAN_OP_WRITE,.cmd=0x62u,.sub=0x01u});
        wr.dlc=3u;wr.data[0]=0x7Bu;wr.data[1]=0u;wr.data[2]=0u;
        assert(dpc245_hmi_control_process(&wr,&rt,&rio,&cr));
        assert(cr.handled&&cr.have_reply&&cr.settings_dirty&&cr.reply.dlc==3u);
        assert(dpc245_get_u32_le(&rt.settings,DPC245_OFS_TOTAL_DISTANCE_RAW)==12300u);
        wr.id=bafang_can_id_encode((bafang_can_id_t){.src=BAFANG_NODE_BESST,.dst=BAFANG_NODE_HMI,.type=BAFANG_CAN_OP_WRITE,.cmd=0x62u,.sub=0x02u});
        wr.data[0]=9u;wr.data[1]=8u;wr.data[2]=7u;
        assert(dpc245_hmi_control_process(&wr,&rt,&rio,&cr)&&cr.rtc_updated);
        assert(rf.r[DPC245_RTC_REG_HOURS]==0x09u&&rf.r[DPC245_RTC_REG_MINUTES]==0x08u&&rf.r[DPC245_RTC_REG_SECONDS]==0x07u);
        wr.id=bafang_can_id_encode((bafang_can_id_t){.src=BAFANG_NODE_BESST,.dst=BAFANG_NODE_HMI,.type=BAFANG_CAN_OP_WRITE,.cmd=0x63u,.sub=0x0Bu});
        wr.data[0]=0x10u;wr.data[1]=0x27u;wr.data[2]=0u; /* 10000 */
        assert(dpc245_hmi_control_process(&wr,&rt,&rio,&cr)&&cr.settings_dirty);
        assert(dpc245_get_u32_le(&rt.settings,DPC245_OFS_SERVICE_TARGET_RAW)==100000u);
        wr.data[0]=0x0Fu;assert(!dpc245_hmi_control_process(&wr,&rt,&rio,&cr));
    }

    /* Boot splash: the built-in RLE must decode to the splash image (CRC32 pinned
     * from the 320x170 RGB565 asset) and malformed streams must fail. */
    {
        uint32_t crc=0xFFFFFFFFu;uint32_t pixels=0u;
        struct splash_crc_state st={&crc,&pixels};
        bool ok=boot_splash_emit_all(splash_crc_emit,&st);
        assert(ok);
        assert(pixels==320u*170u);
        assert((crc^0xFFFFFFFFu)==0x693DF89Au);
        const uint8_t bad_zero[3]={0x00u,0x00u,0x00u};
        const uint8_t bad_short[3]={0x01u,0x00u,0x00u};
        const uint8_t bad_len[4]={0x01u,0x00u,0x00u,0x00u};
        assert(!boot_splash_decode(bad_zero,3u,splash_crc_emit,&st));
        assert(!boot_splash_decode(bad_short,3u,splash_crc_emit,&st));
        assert(!boot_splash_decode(bad_len,4u,splash_crc_emit,&st));
    }

    /* RIDE MODE record: record fields (magic, version, mode, speed LE, XOR). */
    {
        /* Explicit 25.0 km/h: the bytes below pin the record format, not the default. */
        evist_ride_cfg_t legal = RIDE_CFG(EVIST_RIDE_LEGAL, 2500u);
        evist_ride_cfg_t off = RIDE_CFG(EVIST_RIDE_OFFROAD, 2500u);
        evist_ride_cfg_t back;
        uint8_t rec[EVIST_RIDE_REC_SIZE];
        assert(evist_ride_target_x100(&legal) == 2500u);
        assert(evist_ride_target_x100(&off) == 2500u);
        off.offroad_x100 = 3000u;
        assert(evist_ride_target_x100(&off) == 3000u);

        assert(evist_ride_record_encode(&legal, rec));
        /* The record keeps the offroad speed in the record even in LEGAL mode. */
        assert(rec[0] == 0xACu && rec[1] == 0x01u && rec[2] == 0u && rec[3] == 0xC4u && rec[4] == 0x09u && rec[5] == 0x60u);
        assert(evist_ride_record_decode(rec, &back) && back.mode == EVIST_RIDE_LEGAL);

        evist_ride_cfg_t off25 = RIDE_CFG(EVIST_RIDE_OFFROAD, 2500u);
        assert(evist_ride_record_encode(&off25, rec));
        assert(rec[2] == 1u && rec[3] == 0xC4u && rec[4] == 0x09u && rec[5] == 0x61u);
        assert(evist_ride_record_decode(rec, &back) && back.mode == EVIST_RIDE_OFFROAD && back.offroad_x100 == 2500u);

        /* Out-of-range or off-step speeds and bad mode values are refused both ways. */
        evist_ride_cfg_t bad = RIDE_CFG(EVIST_RIDE_OFFROAD, 1025u);
        assert(!evist_ride_record_encode(&bad, rec));
        bad.offroad_x100 = 999u;   assert(!evist_ride_record_encode(&bad, rec));
        bad.offroad_x100 = 7050u;  assert(!evist_ride_record_encode(&bad, rec));
        bad.offroad_x100 = 2500u; bad.mode = (evist_ride_mode_t)2; assert(!evist_ride_record_encode(&bad, rec));
        assert(evist_ride_speed_valid(1000u) && evist_ride_speed_valid(7000u) && !evist_ride_speed_valid(7050u));

        /* Corrupt CRC is rejected. */
        assert(evist_ride_record_encode(&off25, rec));
        rec[5] ^= 0x01u;
        assert(!evist_ride_record_decode(rec, &back));

        /* Log scan: torn record skipped, newest valid wins, blank ends the area. */
        uint8_t log[4 * EVIST_RIDE_REC_SIZE];
        evist_ride_cfg_t old_off = RIDE_CFG(EVIST_RIDE_OFFROAD, 3000u);
        evist_ride_record_encode(&old_off, log);
        evist_ride_record_encode(&off25, log + 6);
        log[6 + 3] ^= 0x10u;                 /* torn copy of the second record */
        evist_ride_record_encode(&legal, log + 12);
        for (size_t i = 18; i < 24; ++i) log[i] = 0xFFu;  /* blank tail */
        evist_ride_cfg_t got;
        assert(evist_ride_log_scan(log, 4u, &got));
        assert(got.mode == EVIST_RIDE_LEGAL);
        /* Second record is torn, so the older valid 3000 record is the newest usable one. */
        assert(evist_ride_log_scan(log, 2u, &got) && got.mode == EVIST_RIDE_OFFROAD && got.offroad_x100 == 3000u);
        uint8_t blank[2 * EVIST_RIDE_REC_SIZE];
        for (size_t i = 0; i < sizeof(blank); ++i) blank[i] = 0xFFu;
        assert(!evist_ride_log_scan(blank, 2u, &got) && got.mode == EVIST_RIDE_LEGAL && got.offroad_x100 == EVIST_RIDE_OFFROAD_DEFAULT);
    }

    /* Menu model: POWER on a row opens it, arrows change the value, POWER returns
     * to the same row so browsing continues there (DISPLAY, THEMES and ADV SET). */
    {
        dpc245_settings_t st;
        dpc245_settings_defaults(&st);
        static dpc245_app_t ap;
        dpc245_app_init(&ap, &st);

        ap.screen = DPC245_APP_SCREEN_DISPLAY_MENU;
        ap.selection = 8u;                                   /* AUTO OFF */
        dpc245_app_key(&ap, DPC245_APP_KEY_POWER);
        assert(ap.screen == DPC245_APP_SCREEN_EDIT_AUTO_OFF);
        dpc245_app_key(&ap, DPC245_APP_KEY_UP);
        dpc245_app_key(&ap, DPC245_APP_KEY_POWER);
        assert(ap.screen == DPC245_APP_SCREEN_DISPLAY_MENU && ap.selection == 8u);
        assert(dpc245_app_take_settings_dirty(&ap));

        ap.screen = DPC245_APP_SCREEN_DISPLAY_MENU;
        ap.selection = 6u;
        dpc245_app_key(&ap, DPC245_APP_KEY_POWER);
        assert(ap.screen == DPC245_APP_SCREEN_THEMES_MENU && ap.selection == 0u);
        ap.selection = 2u;
        dpc245_app_key(&ap, DPC245_APP_KEY_POWER);
        assert(ap.screen == DPC245_APP_SCREEN_THEMES_MENU && ap.pending_theme == 2u);
        assert(dpc245_theme(&ap.runtime.settings) == 0u && !ap.settings_dirty);
        ap.selection = 4u;
        dpc245_app_key(&ap, DPC245_APP_KEY_POWER);
        assert(ap.screen == DPC245_APP_SCREEN_THEMES_MENU && ap.selection == 4u);
        assert(dpc245_theme(&ap.runtime.settings) == 2u && ap.settings_dirty);
        assert(ap.stored_theme==0u && ap.theme_save_pending);
        assert(strcmp(dpc245_app_theme_row_value(&ap,0u),"ACTIVE")==0);
        assert(strcmp(dpc245_app_theme_row_value(&ap,2u),"SELECTED")==0);
        assert(dpc245_app_take_settings_dirty(&ap));
        dpc245_app_theme_store_result(&ap,false,2u);
        assert(ap.stored_theme==0u && ap.theme_save_failed && !ap.theme_save_pending);
        dpc245_app_key(&ap, DPC245_APP_KEY_POWER);
        assert(ap.settings_dirty && ap.theme_save_pending);
        assert(strcmp(ap.theme_status,"SAVE IN PROGRESS")==0);
        assert(strcmp(dpc245_app_theme_row_value(&ap,0u),"ACTIVE")==0);
        assert(strcmp(dpc245_app_theme_row_value(&ap,2u),"SELECTED")==0);
        assert(dpc245_app_take_settings_dirty(&ap));
        dpc245_app_theme_store_result(&ap,true,2u);
        assert(ap.stored_theme==2u && !ap.theme_save_pending && !ap.theme_save_failed);
        assert(strcmp(dpc245_app_theme_row_value(&ap,2u),"ACTIVE")==0);
        dpc245_app_key(&ap,DPC245_APP_KEY_POWER);
        assert(!ap.settings_dirty && strcmp(ap.theme_status,"NO CHANGE")==0);
        ap.selection = 3u;
        dpc245_app_key(&ap, DPC245_APP_KEY_POWER);
        ap.selection = 5u;
        dpc245_app_key(&ap, DPC245_APP_KEY_POWER);
        assert(ap.screen == DPC245_APP_SCREEN_DISPLAY_MENU && ap.selection == 6u);
        assert(dpc245_theme(&ap.runtime.settings) == 2u);
        dpc245_app_key(&ap,DPC245_APP_KEY_POWER);
        assert(ap.screen==DPC245_APP_SCREEN_THEMES_MENU && ap.selection==2u && ap.pending_theme==2u);
        ap.selection=3u;dpc245_app_key(&ap,DPC245_APP_KEY_POWER);
        dpc245_app_tick(&ap,30000u);
        assert(ap.screen==DPC245_APP_SCREEN_DASHBOARD && dpc245_theme(&ap.runtime.settings)==2u);
        assert(ap.pending_theme==2u && !ap.settings_dirty);

        dpc245_app_init(&ap,&st);
        ap.screen=DPC245_APP_SCREEN_DISPLAY_MENU;ap.selection=6u;
        dpc245_app_key(&ap,DPC245_APP_KEY_POWER);
        ap.selection=2u;dpc245_app_key(&ap,DPC245_APP_KEY_POWER);
        ap.selection=4u;dpc245_app_key(&ap,DPC245_APP_KEY_POWER);
        assert(dpc245_app_take_settings_dirty(&ap));
        dpc245_app_theme_store_result(&ap,false,2u);
        ap.selection=0u;dpc245_app_key(&ap,DPC245_APP_KEY_POWER);
        ap.selection=4u;dpc245_app_key(&ap,DPC245_APP_KEY_POWER);
        assert(ap.settings_dirty && strcmp(ap.theme_status,"SAVE IN PROGRESS")==0);
        assert(ap.stored_theme==0u && ap.theme_save_pending);
        assert(dpc245_app_take_settings_dirty(&ap));
        dpc245_app_theme_store_result(&ap,true,0u);
        assert(!ap.theme_save_failed && !ap.theme_save_pending);

        /* Leaving THEMES after a failed write restores the confirmed runtime theme. */
        for (unsigned idle_exit = 0u; idle_exit < 2u; ++idle_exit) {
            dpc245_app_init(&ap,&st);
            ap.screen=DPC245_APP_SCREEN_DISPLAY_MENU;ap.selection=6u;
            dpc245_app_key(&ap,DPC245_APP_KEY_POWER);
            ap.selection=2u;dpc245_app_key(&ap,DPC245_APP_KEY_POWER);
            ap.selection=4u;dpc245_app_key(&ap,DPC245_APP_KEY_POWER);
            assert(dpc245_app_take_settings_dirty(&ap));
            dpc245_app_theme_store_result(&ap,false,2u);
            assert(dpc245_theme(&ap.runtime.settings)==2u && ap.stored_theme==0u);
            assert(ap.theme_save_failed && !ap.theme_save_pending && !ap.settings_dirty);
            if (idle_exit) dpc245_app_tick(&ap,30000u);
            else {
                ap.selection=5u;
                dpc245_app_key(&ap,DPC245_APP_KEY_POWER);
            }
            assert(ap.screen==(idle_exit?DPC245_APP_SCREEN_DASHBOARD:DPC245_APP_SCREEN_DISPLAY_MENU));
            assert(dpc245_theme(&ap.runtime.settings)==0u && dpc245_settings_validate(&ap.runtime.settings));
            assert(ap.pending_theme==0u && ap.stored_theme==0u && !ap.theme_save_failed);
            assert(ap.theme_status==0);
            assert(!ap.settings_dirty && !dpc245_app_take_settings_dirty(&ap));
        }

        /* Exiting while a write is in progress leaves its result path intact. */
        for (unsigned idle_exit = 0u; idle_exit < 2u; ++idle_exit) {
            dpc245_app_init(&ap,&st);
            ap.screen=DPC245_APP_SCREEN_DISPLAY_MENU;ap.selection=6u;
            dpc245_app_key(&ap,DPC245_APP_KEY_POWER);
            ap.selection=2u;dpc245_app_key(&ap,DPC245_APP_KEY_POWER);
            ap.selection=4u;dpc245_app_key(&ap,DPC245_APP_KEY_POWER);
            assert(dpc245_app_take_settings_dirty(&ap));
            if (idle_exit) dpc245_app_tick(&ap,30000u);
            else {
                ap.selection=5u;
                dpc245_app_key(&ap,DPC245_APP_KEY_POWER);
            }
            assert(dpc245_theme(&ap.runtime.settings)==2u && ap.theme_save_pending && !ap.settings_dirty);
            dpc245_app_theme_store_result(&ap,true,2u);
            assert(ap.stored_theme==2u && !ap.theme_save_pending && !ap.theme_save_failed);
        }

        evist_preferences_t pref;
        evist_preferences_defaults(&pref);
        evist_advanced_t av;
        evist_advanced_open(&av, &pref, 5u);
        for (unsigned i = 0u; i < 8u; ++i) evist_advanced_key(&av, EVIST_ADV_KEY_DOWN);
        evist_advanced_key(&av, EVIST_ADV_KEY_POWER);        /* QUICK WALK, row 8 */
        assert(av.screen == EVIST_ADV_QUICK_WALK);
        av.edit = false;
        av.cursor = (uint8_t)(evist_advanced_row_count(&av) - 1u);   /* BACK */
        evist_advanced_key(&av, EVIST_ADV_KEY_POWER);
        assert(av.screen == EVIST_ADV_MENU && av.cursor == 8u);
    }

    /* Main-loop timing statistics. */
    {
        evist_loop_stats_t ls;
        evist_loop_stats_init(&ls, 5000u);
        evist_loop_stats_add(&ls, 300u, 5100u);
        evist_loop_stats_add(&ls, 4200u, 5500u);         /* long pass, e.g. a full frame */
        evist_loop_stats_add(&ls, 250u, 5900u);
        assert(ls.max_us == 4200u && ls.last_window_max_us == 0u);   /* first second not complete */
        evist_loop_stats_add(&ls, 280u, 6000u);          /* window closes at 1000 ms */
        assert(ls.last_window_max_us == 4200u && ls.window_max_us == 0u);
        evist_loop_stats_add(&ls, 900u, 6400u);
        evist_loop_stats_add(&ls, 310u, 7000u);          /* next window: max 900 */
        assert(ls.last_window_max_us == 900u && ls.max_us == 4200u);
        evist_loop_stats_add(&ls, 100u, 7000u + 1000u);  /* idle second */
        assert(ls.last_window_max_us == 100u);
        /* SysTick at 120 MHz (RVR 119999), counting down. */
        const uint32_t rvr = 119999u;
        assert(evist_micros_compose(10u, rvr, rvr, false) == 10000u);          /* start of ms 10 */
        assert(evist_micros_compose(10u, 60000u, rvr, false) == 10499u);       /* mid ms 10 */
        assert(evist_micros_compose(10u, 0u, rvr, false) == 10999u);           /* end of ms 10 */
        /* reloaded, interrupt still pending: CVR belongs to ms 11, not back to 10.0 */
        assert(evist_micros_compose(10u, rvr - 5u, rvr, true) == 11000u);
        /* pending but CVR still low: reload happened after the read, stays in ms 10 */
        assert(evist_micros_compose(10u, 3u, rvr, true) == 10999u);
        /* without the pending-tick correction this sample would go ~1 ms backwards */
        assert(evist_micros_compose(10u, rvr - 5u, rvr, true) > evist_micros_compose(10u, 0u, rvr, false));
        /* HSI 8 MHz reload */
        assert(evist_micros_compose(2u, 3999u, 7999u, false) == 2500u);

        /* A pass longer than the window closes one window holding it,
         * and the next window starts at the end of that pass. */
        evist_loop_stats_init(&ls, 20000u);
        evist_loop_stats_add(&ls, 200u, 20100u);
        evist_loop_stats_add(&ls, 1500000u, 21600u);     /* 1.5 s pass */
        assert(ls.last_window_max_us == 1500000u && ls.max_us == 1500000u);
        assert(ls.window_start_ms == 21600u && ls.window_max_us == 0u);
        evist_loop_stats_add(&ls, 400u, 22000u);
        evist_loop_stats_add(&ls, 350u, 22600u);         /* closes the next window */
        assert(ls.last_window_max_us == 400u);
        evist_loop_stats_add(&ls, 3000000u, 25600u);     /* 3 s pass, two windows long */
        assert(ls.last_window_max_us == 3000000u && ls.window_start_ms == 25600u);
        /* millisecond counter wrap keeps windows working */
        evist_loop_stats_init(&ls, 0xFFFFFF00u);
        evist_loop_stats_add(&ls, 700u, 0x000002F0u);
        assert(ls.last_window_max_us == 700u);
    }

    puts("all core tests passed");
    return 0;
}
