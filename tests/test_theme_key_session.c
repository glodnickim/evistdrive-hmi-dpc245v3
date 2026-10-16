/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_app.h"
#include "dpc245_key_sampler.h"
#include "dpc245_buttons.h"
#include "dpc245_tx.h"
#include "native_store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define MEMORY_BYTES 0x200000u
static uint8_t memory[MEMORY_BYTES];
static unsigned wait_steps;
static bool always_busy;
static bool write_fails;
static unsigned reads, writes, erases, polls;
static unsigned polls_in_pass;
volatile uint32_t evist_ms_ticks;

static bool read_memory(void *ctx,uint32_t address,uint8_t *dst,size_t n){
    (void)ctx;reads++;
    if(address>MEMORY_BYTES || n>MEMORY_BYTES-address)return false;
    memcpy(dst,memory+address,n);return true;
}
static bool write_memory(void *ctx,uint32_t address,const uint8_t *src,size_t n){
    (void)ctx;writes++;
    if(write_fails || address>MEMORY_BYTES || n>MEMORY_BYTES-address)return false;
    for(size_t i=0u;i<n;i++)memory[address+i]&=src[i];
    return true;
}
static bool erase_start(void *ctx,uint32_t address){
    (void)ctx;erases++;
    if(address>MEMORY_BYTES-EVIST_NATIVE_PAGE_BYTES)return false;
    memset(memory+address,0xFFu,EVIST_NATIVE_PAGE_BYTES);
    return true;
}
static int ready(void *ctx){
    (void)ctx;polls++;polls_in_pass++;
    if(always_busy)return 0;
    if(wait_steps){wait_steps--;return 0;}
    return 1;
}
static dpc245_persist_io_t memory_io={read_memory,write_memory,0,0};
static evist_native_io_t store_io={&memory_io,erase_start,ready,0};

static void dispatch(dpc245_app_t *app,uint32_t mask,uint32_t now){
    static const dpc245_key_event_t order[]={
        DPC245_KEY_EVENT_SETTINGS_CHORD,DPC245_KEY_EVENT_UP_PRESS,
        DPC245_KEY_EVENT_UP_HOLD,DPC245_KEY_EVENT_POWER_DOWN_HOLD,
        DPC245_KEY_EVENT_DOWN_PRESS,DPC245_KEY_EVENT_UP_REPEAT,
        DPC245_KEY_EVENT_DOWN_REPEAT,DPC245_KEY_EVENT_UP_REPEAT_FAST,
        DPC245_KEY_EVENT_DOWN_REPEAT_FAST,DPC245_KEY_EVENT_DOWN_HOLD,
        DPC245_KEY_EVENT_DOWN_RELEASE,DPC245_KEY_EVENT_UP_SHORT,
        DPC245_KEY_EVENT_DOWN_SHORT,DPC245_KEY_EVENT_POWER_SHORT
    };
    for(size_t i=0u;i<sizeof order/sizeof order[0];i++)
        if(mask & order[i])dpc245_app_event(app,order[i],now);
}

static void button_tick(uint8_t raw,uint32_t now_ms){
    dpc245_buttons_host_raw_set(raw);
    evist_ms_ticks=now_ms;
    dpc245_buttons_tick_1ms();
}

static void button_foreground_pass(dpc245_app_t *app,uint32_t now_ms,bool gated){
    if(dpc245_buttons_take_released()&DPC245_KEY_DOWN)
        dpc245_app_down_state(app,false,now_ms);
    dpc245_key_sample_event_t event;
    if(!gated)while(dpc245_buttons_key_event(&event)){
        uint32_t key_time=dpc245_key_time_for_pass(event.time_ms,now_ms);
        dispatch(app,event.events,key_time);
    }
    dpc245_app_down_state(app,(dpc245_buttons_state()&DPC245_KEY_DOWN)!=0u,now_ms);
    dpc245_app_tick(app,now_ms);
}

static void assert_can_walk(const dpc245_app_t *app,bool expected){
    bafang_can_frame_t heartbeat;
    assert(dpc245_build_periodic_frame(0x03106300u,app,false,0u,1u,&heartbeat));
    assert((heartbeat.data[1]==0x06u)==expected);
}

static void ride_key_span(dpc245_app_t *app,uint8_t raw,uint32_t first_ms,uint32_t last_ms){
    for(uint32_t ms=first_ms;ms<=last_ms;ms++){
        button_tick(raw,ms);
        if(ms%10u==0u)button_foreground_pass(app,ms,false);
    }
}

static void ride_keys_init(dpc245_app_t *app,dpc245_key_sampler_t *keys,
                           const dpc245_settings_t *settings){
    dpc245_app_init(app,settings);
    assert(app->screen==DPC245_APP_SCREEN_DASHBOARD && app->assist_level==0u);
    dpc245_buttons_init();
    dpc245_buttons_key_sampler_start(keys,0u);
}

static void test_ride_combo_physical_keys(void){
    dpc245_settings_t settings;
    dpc245_settings_defaults(&settings);
    dpc245_app_t app;
    dpc245_key_sampler_t keys;

    ride_keys_init(&app,&keys,&settings);
    ride_key_span(&app,DPC245_KEY_UP,1u,100u);
    ride_key_span(&app,0u,101u,220u);
    assert(app.ride_combo_step==1u && app.assist_level==1u);
    ride_key_span(&app,DPC245_KEY_DOWN,221u,320u);
    ride_key_span(&app,0u,321u,440u);
    assert(app.ride_combo_step==2u && app.assist_level==0u && !app.walk);
    ride_key_span(&app,DPC245_KEY_UP,441u,540u);
    ride_key_span(&app,0u,541u,660u);
    assert(app.ride_confirm_pending && app.assist_level==0u);
    ride_key_span(&app,DPC245_KEY_POWER,661u,760u);
    ride_key_span(&app,0u,761u,860u);
    assert(!app.ride_confirm_pending && app.ride_apply_requested && app.ride_combo_apply);
    assert(app.ride_pending_session==DPC245_RIDE_SESSION_OFFROAD);
    assert(!keys.overflow);

    ride_keys_init(&app,&keys,&settings);
    ride_key_span(&app,DPC245_KEY_UP,1u,100u);
    ride_key_span(&app,0u,101u,920u);
    ride_key_span(&app,DPC245_KEY_DOWN,921u,1020u);
    ride_key_span(&app,0u,1021u,1810u);
    ride_key_span(&app,DPC245_KEY_UP,1811u,1910u);
    ride_key_span(&app,0u,1911u,2010u);
    assert(!app.ride_confirm_pending && !app.ride_apply_requested);
    assert(!keys.overflow);

    ride_keys_init(&app,&keys,&settings);
    ride_key_span(&app,DPC245_KEY_UP,1u,4200u);
    ride_key_span(&app,0u,4201u,4320u);
    assert(app.light_toggle_requested && app.ride_combo_step==0u);
    ride_key_span(&app,DPC245_KEY_DOWN,4321u,4420u);
    ride_key_span(&app,0u,4421u,4540u);
    ride_key_span(&app,DPC245_KEY_UP,4541u,4640u);
    ride_key_span(&app,0u,4641u,4760u);
    assert(!app.ride_confirm_pending && !app.ride_apply_requested);
    assert(!keys.overflow);

    ride_keys_init(&app,&keys,&settings);
    ride_key_span(&app,DPC245_KEY_UP,1u,100u);
    ride_key_span(&app,0u,101u,220u);
    ride_key_span(&app,DPC245_KEY_DOWN,221u,320u);
    ride_key_span(&app,0u,321u,440u);
    ride_key_span(&app,DPC245_KEY_UP,441u,540u);
    ride_key_span(&app,0u,541u,660u);
    assert(app.ride_confirm_pending && app.assist_level==0u);
    ride_key_span(&app,DPC245_KEY_UP,661u,720u);
    assert(!app.ride_confirm_pending && app.assist_level==0u);
    ride_key_span(&app,0u,721u,820u);
    assert(app.assist_level==1u && !app.ride_apply_requested);
    puts("physical RIDE MODE keys: confirmation, timeout, light hold and UP cancellation passed");
}

static void test_key_time_for_pass(void){
    assert(dpc245_key_time_for_pass(999u,1000u)==999u);
    assert(dpc245_key_time_for_pass(1000u,1000u)==1000u);
    assert(dpc245_key_time_for_pass(1001u,1000u)==1000u);
    assert(dpc245_key_time_for_pass(0u,UINT32_MAX)==UINT32_MAX);
    assert(dpc245_key_time_for_pass(UINT32_MAX,0u)==UINT32_MAX);
}

static void test_sample_during_foreground_pass(void){
    dpc245_settings_t settings;
    dpc245_settings_defaults(&settings);
    dpc245_app_t app;
    dpc245_app_init(&app,&settings);
    dpc245_app_event(&app,DPC245_KEY_EVENT_SETTINGS_CHORD,1000u);
    assert(app.screen==DPC245_APP_SCREEN_ROOT_MENU);
    dpc245_buttons_init();
    dpc245_key_sampler_t keys;
    dpc245_buttons_key_sampler_start(&keys,1000u);
    /* Foreground captured 1000 ms; the interrupt then sampled a new key. */
    for(uint32_t ms=1001u;ms<=1100u;ms++)button_tick(DPC245_KEY_UP,ms);
    button_foreground_pass(&app,1000u,false);
    assert(app.key_events>=2u);
    assert(app.last_key_event_ms==1000u);
    assert(app.screen==DPC245_APP_SCREEN_ROOT_MENU);
}

static void test_down_release_latch_between_passes(void){
    dpc245_settings_t settings;
    dpc245_settings_defaults(&settings);
    const uint32_t gaps[]={1u,2u,3u,20u};
    for(size_t gap=0u;gap<sizeof gaps/sizeof gaps[0];gap++){
        uint32_t gap_ms=gaps[gap];
        dpc245_app_t app;
        dpc245_app_init(&app,&settings);
        app.assist_level=2u;
        app.preferences.quick_walk_enabled=true;
        app.preferences.quick_walk_hold_steps=1u;
        dpc245_buttons_init();
        dpc245_key_sampler_t keys;
        dpc245_buttons_key_sampler_start(&keys,0u);
        for(uint32_t ms=1u;ms<=700u;ms++)button_tick(DPC245_KEY_DOWN,ms);
        button_foreground_pass(&app,700u,false);
        assert(app.walk && app.walk_quick && app.walk_active);
        assert_can_walk(&app,true);
        for(uint32_t ms=701u;ms<=760u;ms++)
            button_tick(ms<=700u+gap_ms?0u:DPC245_KEY_DOWN,ms);
        assert((dpc245_buttons_state()&DPC245_KEY_DOWN)!=0u);
        button_foreground_pass(&app,760u,true);
        assert(dpc245_buttons_take_released()==0u);
        if(gap_ms<=3u){
            assert(app.walk && app.walk_active);
            assert_can_walk(&app,true);
            continue;
        }
        assert(!app.walk && !app.walk_active && !app.walk_armed && !app.down_held);
        assert_can_walk(&app,false);
        assert(app.suppress_down_short_once && app.assist_level==2u);
        dpc245_key_sample_event_t event;
        while(dpc245_buttons_key_event(&event))
            assert((event.events&(DPC245_KEY_EVENT_DOWN_RELEASE|DPC245_KEY_EVENT_DOWN_PRESS))==0u);
        for(uint32_t ms=761u;ms<=1200u;ms++)button_tick(DPC245_KEY_DOWN,ms);
        button_foreground_pass(&app,1200u,false);
        assert(!app.walk && !app.walk_active && app.assist_level==2u);
        assert_can_walk(&app,false);
        for(uint32_t ms=1201u;ms<=1320u;ms++)
            button_tick(ms<=1260u?0u:DPC245_KEY_DOWN,ms);
        button_foreground_pass(&app,1320u,false);
        assert(!app.walk && app.walk_quick && !app.walk_active);
        assert_can_walk(&app,false);
        assert(app.assist_level==2u && !app.suppress_down_short_once);
        for(uint32_t ms=1321u;ms<=1810u;ms++)button_tick(DPC245_KEY_DOWN,ms);
        button_foreground_pass(&app,1810u,false);
        assert(app.walk && app.walk_active);
        assert_can_walk(&app,true);
        puts("20 ms DOWN release and re-press between passes clears CAN Walk; fresh hold reactivates");
    }
    puts("1-3 ms DOWN glitches leave CAN Walk request active");
}

static void enter_themes(dpc245_app_t *app){
    dpc245_app_event(app,DPC245_KEY_EVENT_SETTINGS_CHORD,1000u);
    assert(app->screen==DPC245_APP_SCREEN_ROOT_MENU);
    dpc245_app_key(app,DPC245_APP_KEY_POWER);
    assert(app->screen==DPC245_APP_SCREEN_DISPLAY_MENU);
    for(unsigned i=0u;i<6u;i++)dpc245_app_key(app,DPC245_APP_KEY_DOWN);
    dpc245_app_key(app,DPC245_APP_KEY_POWER);
    assert(app->screen==DPC245_APP_SCREEN_THEMES_MENU);
}

static unsigned replay_taps(dpc245_app_t *app,unsigned pass_ms){
    dpc245_key_sampler_t sampled;
    dpc245_keys_t loop_sampled;
    dpc245_key_sampler_init(&sampled,0u,1000u);
    dpc245_keys_init(&loop_sampled,0u,1000u);
    unsigned old_shorts=0u,new_shorts=0u;
    for(unsigned t=1u;t<=650u;t++){
        unsigned tap=t-1u;
        uint8_t raw=0u;
        if(tap<90u)raw=DPC245_KEYS_RAW_DOWN;
        else if(tap>=180u && tap<270u)raw=DPC245_KEYS_RAW_POWER;
        else if(tap>=360u && tap<450u)raw=DPC245_KEYS_RAW_UP;
        dpc245_key_sampler_tick(&sampled,raw,1000u+t);
        if(t%pass_ms==0u){
            uint32_t old=dpc245_keys_update(&loop_sampled,raw,1000u+t);
            old_shorts+=(old&DPC245_KEY_EVENT_DOWN_SHORT)!=0u;
            old_shorts+=(old&DPC245_KEY_EVENT_POWER_SHORT)!=0u;
            old_shorts+=(old&DPC245_KEY_EVENT_UP_SHORT)!=0u;
            dpc245_key_sample_event_t event;
            while(dpc245_key_sampler_pop(&sampled,&event)){
                new_shorts+=(event.events&DPC245_KEY_EVENT_DOWN_SHORT)!=0u;
                new_shorts+=(event.events&DPC245_KEY_EVENT_POWER_SHORT)!=0u;
                new_shorts+=(event.events&DPC245_KEY_EVENT_UP_SHORT)!=0u;
                dispatch(app,event.events,event.time_ms);
            }
        }
    }
    assert(new_shorts==3u && !sampled.overflow);
    assert(app->screen==DPC245_APP_SCREEN_DISPLAY_MENU && app->selection==5u);
    printf("pass=%u ms: loop shorts=%u, tick shorts=%u\n",pass_ms,old_shorts,new_shorts);
    return old_shorts;
}

static void exercise_theme(uint8_t theme,bool slow,bool failure){
    memset(memory,0xFFu,sizeof memory);
    wait_steps=0u;write_fails=false;always_busy=false;
    reads=writes=erases=polls=0u;
    evist_native_store_t store;
    evist_native_values_t values;
    assert(evist_native_store_init(&store,&store_io,&values));
    assert(evist_native_store_begin(&store,&values));
    for(unsigned i=0u;store.busy && i<20u;i++)
        assert(evist_native_store_step(&store,&store_io,10u*i)!=EVIST_NATIVE_INVALID);
    assert(store.valid && !store.busy);

    dpc245_app_t app;
    dpc245_app_init(&app,&values.hmi);
    enter_themes(&app);
    for(uint8_t i=0u;i<theme;i++)dpc245_app_key(&app,DPC245_APP_KEY_DOWN);
    dpc245_app_key(&app,DPC245_APP_KEY_POWER);
    for(uint8_t i=theme;i<4u;i++)dpc245_app_key(&app,DPC245_APP_KEY_DOWN);
    dpc245_app_key(&app,DPC245_APP_KEY_POWER);
    assert(app.theme_save_pending && dpc245_app_take_settings_dirty(&app));
    values.hmi=app.runtime.settings;
    assert(evist_native_store_begin(&store,&values));
    if(slow){
        memory[EVIST_NATIVE_FIRST+EVIST_NATIVE_PAGE_BYTES]=0u;
        wait_steps=16u;
    }
    if(failure)write_fails=true;
    unsigned steps=0u;
    evist_native_result_t result=EVIST_NATIVE_BUSY;
    while(store.busy && steps<64u){result=evist_native_store_step(&store,&store_io,10u*steps);steps++;}
    assert(!store.busy);
    assert(result==(failure?EVIST_NATIVE_WRITE_FAILED:EVIST_NATIVE_OK));
    dpc245_app_theme_store_result(&app,!failure,theme);
    assert(!app.theme_save_pending);
    unsigned old=replay_taps(&app,slow?130u:10u);
    assert(old==(slow?0u:3u));
    dpc245_app_key(&app,DPC245_APP_KEY_DOWN);
    dpc245_app_key(&app,DPC245_APP_KEY_POWER);
    assert(app.screen==DPC245_APP_SCREEN_THEMES_MENU);
    while(app.selection!=5u)dpc245_app_key(&app,DPC245_APP_KEY_DOWN);
    dpc245_app_key(&app,DPC245_APP_KEY_POWER);
    assert(app.screen==DPC245_APP_SCREEN_DISPLAY_MENU);
    while(app.selection!=9u)dpc245_app_key(&app,DPC245_APP_KEY_DOWN);
    dpc245_app_key(&app,DPC245_APP_KEY_POWER);
    assert(app.screen==DPC245_APP_SCREEN_ROOT_MENU);
    dpc245_app_key(&app,DPC245_APP_KEY_DOWN);
    dpc245_app_key(&app,DPC245_APP_KEY_POWER);
    assert(app.screen==DPC245_APP_SCREEN_BIKE_MENU);
    while(app.selection!=6u)dpc245_app_key(&app,DPC245_APP_KEY_DOWN);
    dpc245_app_key(&app,DPC245_APP_KEY_POWER);
    assert(app.screen==DPC245_APP_SCREEN_ROOT_MENU);
    dpc245_app_key(&app,DPC245_APP_KEY_DOWN);
    dpc245_app_key(&app,DPC245_APP_KEY_POWER);
    assert(app.screen==DPC245_APP_SCREEN_INFO_MENU);
    printf("theme=%u slow=%u failure=%u steps=%u reads=%u writes=%u erases=%u polls=%u\n",
           theme,slow,failure,steps,reads,writes,erases,polls);
    evist_native_store_t reboot;
    evist_native_values_t loaded;
    assert(evist_native_store_init(&reboot,&store_io,&loaded));
    assert(dpc245_theme(&loaded.hmi)==(failure?DPC245_THEME_STANDARD:theme));
    dpc245_app_t fresh;
    dpc245_app_init(&fresh,&loaded.hmi);
    enter_themes(&fresh);
    fresh.selection=4u;
    assert(!fresh.theme_save_pending && fresh.stored_theme==dpc245_theme(&loaded.hmi));
    assert(replay_taps(&fresh,10u)==3u);
}

typedef struct {
    dpc245_app_t app,old_app;
    dpc245_key_sampler_t sampled;
    dpc245_keys_t old_keys;
    evist_native_store_t store;
    evist_native_values_t values;
    uint32_t now_ms,next_pass_ms;
    unsigned old_shorts,new_shorts,store_steps,frames_while_busy;
    uint8_t frame_rows;
    bool frame_active,redraw_pending,navigation_frame_pending;
    evist_native_result_t result;
} session_t;

static void session_pass(session_t *s,uint8_t raw){
    uint32_t old=dpc245_keys_update(&s->old_keys,raw,s->now_ms);
    if(old){
        s->old_shorts+=!!(old&DPC245_KEY_EVENT_DOWN_SHORT);
        s->old_shorts+=!!(old&DPC245_KEY_EVENT_UP_SHORT);
        s->old_shorts+=!!(old&DPC245_KEY_EVENT_POWER_SHORT);
        dispatch(&s->old_app,old,s->now_ms);
    }
    bool nav=false;
    dpc245_key_sample_event_t event;
    while(!s->navigation_frame_pending && dpc245_key_sampler_pop(&s->sampled,&event)){
        uint32_t mask=event.events;
        s->redraw_pending=true;
        s->new_shorts+=!!(mask&DPC245_KEY_EVENT_DOWN_SHORT);
        s->new_shorts+=!!(mask&DPC245_KEY_EVENT_UP_SHORT);
        s->new_shorts+=!!(mask&DPC245_KEY_EVENT_POWER_SHORT);
        nav|=(mask&(DPC245_KEY_EVENT_DOWN_SHORT|DPC245_KEY_EVENT_UP_SHORT|
                    DPC245_KEY_EVENT_POWER_SHORT|DPC245_KEY_EVENT_SETTINGS_CHORD))!=0u;
        dispatch(&s->app,mask,event.time_ms);
        if(nav){s->navigation_frame_pending=true;break;}
    }
    dpc245_app_down_state(&s->app,(s->sampled.keys.sampled_raw&DPC245_KEYS_RAW_DOWN)!=0u,s->now_ms);
    if(nav)s->frame_active=false;
    polls_in_pass=0u;
    bool was_busy=s->store.busy;
    if(was_busy){
        s->result=evist_native_store_step(&s->store,&store_io,s->now_ms);
        s->store_steps++;
        if(s->result!=EVIST_NATIVE_BUSY){
            dpc245_app_theme_store_result(&s->app,s->result==EVIST_NATIVE_OK,
                                           dpc245_theme(&s->store.target.hmi));
            s->app.theme_status=evist_native_result_text(s->result);
        }
    }
    if(!s->frame_active&&s->redraw_pending){
        s->frame_active=true;s->frame_rows=0u;s->redraw_pending=false;
    }
    if(s->frame_active){
        s->frame_rows=(uint8_t)(s->frame_rows+3u);
        if(s->frame_rows>=17u){
            s->frame_active=false;
            s->navigation_frame_pending=false;
            if(was_busy)s->frames_while_busy++;
        }
    }
    /* Each modeled status poll occupies one slow foreground pass. */
    s->next_pass_ms=s->now_ms+10u+(polls_in_pass?120u:0u);
}

static void session_run(session_t *s,uint8_t raw,unsigned duration_ms){
    for(unsigned i=0u;i<duration_ms;i++){
        s->now_ms++;
        dpc245_key_sampler_tick(&s->sampled,raw,s->now_ms);
        if(s->now_ms>=s->next_pass_ms)session_pass(s,raw);
    }
}

static void session_tap(session_t *s,uint8_t key){
    session_run(s,key,90u);
    session_run(s,0u,90u);
}

static void test_coupled_session(uint8_t theme,bool fail,bool stuck){
    memset(memory,0xFFu,sizeof memory);
    wait_steps=0u;always_busy=false;write_fails=false;
    reads=writes=erases=polls=polls_in_pass=0u;
    session_t s={0};
    assert(evist_native_store_init(&s.store,&store_io,&s.values));
    assert(evist_native_store_begin(&s.store,&s.values));
    for(unsigned i=0u;s.store.busy&&i<20u;i++)
        (void)evist_native_store_step(&s.store,&store_io,10u*i);
    assert(s.store.valid&&!s.store.busy);
    dpc245_app_init(&s.app,&s.values.hmi);
    dpc245_app_init(&s.old_app,&s.values.hmi);
    s.now_ms=1000u;s.next_pass_ms=1010u;
    dpc245_key_sampler_init(&s.sampled,0u,s.now_ms);
    dpc245_keys_init(&s.old_keys,0u,s.now_ms);
    session_run(&s,DPC245_KEYS_RAW_DOWN|DPC245_KEYS_RAW_UP,600u);
    session_run(&s,0u,100u);
    assert(s.app.screen==DPC245_APP_SCREEN_ROOT_MENU &&
           s.old_app.screen==DPC245_APP_SCREEN_ROOT_MENU);
    session_tap(&s,DPC245_KEYS_RAW_POWER);
    assert(s.app.screen==DPC245_APP_SCREEN_DISPLAY_MENU &&
           s.old_app.screen==DPC245_APP_SCREEN_DISPLAY_MENU);
    for(unsigned i=0u;i<6u;i++)session_tap(&s,DPC245_KEYS_RAW_DOWN);
    session_tap(&s,DPC245_KEYS_RAW_POWER);
    assert(s.app.screen==DPC245_APP_SCREEN_THEMES_MENU);
    for(unsigned i=0u;i<theme;i++)session_tap(&s,DPC245_KEYS_RAW_DOWN);
    if(theme){
        session_tap(&s,DPC245_KEYS_RAW_POWER);
        for(unsigned i=theme;i<4u;i++)session_tap(&s,DPC245_KEYS_RAW_DOWN);
        session_tap(&s,DPC245_KEYS_RAW_POWER);
        assert(s.app.theme_save_pending && dpc245_app_take_settings_dirty(&s.app));
        s.values.hmi=s.app.runtime.settings;
        assert(evist_native_store_begin(&s.store,&s.values));
        memory[EVIST_NATIVE_FIRST+EVIST_NATIVE_PAGE_BYTES]=0u;
        wait_steps=16u;always_busy=stuck;write_fails=fail;
    }else{
        for(unsigned i=0u;i<4u;i++)session_tap(&s,DPC245_KEYS_RAW_DOWN);
    }
    assert(s.app.selection==4u && s.old_app.selection==4u);
    s.frame_active=true;s.frame_rows=0u;
    s.old_shorts=s.new_shorts=0u;
    session_run(&s,0u,100u);
    bool busy_before=s.store.busy;
    bool pending_before=s.app.theme_save_pending;
    session_tap(&s,DPC245_KEYS_RAW_DOWN);
    session_tap(&s,DPC245_KEYS_RAW_POWER);
    session_tap(&s,DPC245_KEYS_RAW_UP);
    session_run(&s,0u,150u);
    for(unsigned i=0u;s.new_shorts<3u&&i<60u;i++)session_run(&s,0u,130u);
    assert(s.new_shorts==3u);
    assert(s.app.screen==DPC245_APP_SCREEN_DISPLAY_MENU&&s.app.selection==5u);
    if(theme)assert(busy_before&&pending_before);
    if(theme && (fail||stuck||wait_steps))assert(s.old_shorts<3u);
    if(theme){
        if(stuck){
            assert(s.store.phase==3u);
            uint32_t deadline=s.store.wait_started_ms+5000u;
            assert(s.now_ms<deadline-1u);
            session_run(&s,0u,deadline-1u-s.now_ms);
            assert(s.store.busy && s.app.theme_save_pending &&
                   s.app.screen==DPC245_APP_SCREEN_DISPLAY_MENU && s.app.selection==5u);
            printf("busy store before deadline: now=%u busy=%u pending=%u screen=%u selection=%u frames=%u\n",
                   s.now_ms,s.store.busy,s.app.theme_save_pending,s.app.screen,
                   s.app.selection,s.frames_while_busy);
            session_run(&s,0u,200u);
            assert(!s.store.busy && !s.app.theme_save_pending &&
                   s.result==EVIST_NATIVE_ERASE_FAILED &&
                   strcmp(s.app.theme_status,"ERASE ERROR - CHECK SAVE")==0);
            printf("busy store after deadline: now=%u busy=%u pending=%u screen=%u selection=%u result=%u\n",
                   s.now_ms,s.store.busy,s.app.theme_save_pending,s.app.screen,
                   s.app.selection,s.result);
        }else session_run(&s,0u,6500u);
        assert(!s.store.busy&&!s.app.theme_save_pending);
        assert(s.result==(fail?EVIST_NATIVE_WRITE_FAILED:stuck?EVIST_NATIVE_ERASE_FAILED:EVIST_NATIVE_OK));
        assert(s.frames_while_busy>0u);
    }else assert(s.old_shorts==3u);
    printf("coupled theme=%u fail=%u stuck=%u: before busy=%u pending=%u; after busy=%u pending=%u screen=%u selection=%u old=%u tick=%u steps=%u polls=%u frames=%u\n",
           theme,fail,stuck,busy_before,pending_before,s.store.busy,s.app.theme_save_pending,
           s.app.screen,s.app.selection,s.old_shorts,s.new_shorts,s.store_steps,polls,s.frames_while_busy);
    if(theme){
        evist_native_store_t reboot;
        evist_native_values_t loaded;
        assert(evist_native_store_init(&reboot,&store_io,&loaded));
        dpc245_app_t fresh;
        dpc245_app_init(&fresh,&loaded.hmi);
        enter_themes(&fresh);fresh.selection=4u;
        assert(replay_taps(&fresh,10u)==3u);
    }
}

static void test_walk_release_overflow(void){
    dpc245_settings_t settings;
    dpc245_settings_defaults(&settings);
    dpc245_app_t app;
    dpc245_app_init(&app,&settings);
    app.assist_level=1u;
    app.preferences.quick_walk_enabled=true;
    app.preferences.quick_walk_hold_steps=1u;
    dpc245_key_sampler_t keys;
    dpc245_key_sampler_init(&keys,0u,0u);
    for(uint32_t ms=1u;ms<=100u;ms++)
        dpc245_key_sampler_tick(&keys,DPC245_KEYS_RAW_DOWN,ms);
    dpc245_key_sample_event_t event;
    while(dpc245_key_sampler_pop(&keys,&event))dispatch(&app,event.events,event.time_ms);
    dpc245_app_tick(&app,600u);
    assert(app.walk && app.walk_quick);
    for(uint32_t ms=101u;ms<=6100u;ms++)
        dpc245_key_sampler_tick(&keys,DPC245_KEYS_RAW_DOWN,ms);
    for(uint32_t ms=6101u;ms<=6200u;ms++)dpc245_key_sampler_tick(&keys,0u,ms);
    assert(keys.overflow && app.walk);
    unsigned release_count=0u;
    while(dpc245_key_sampler_pop(&keys,&event)){
        if(event.events&DPC245_KEY_EVENT_DOWN_RELEASE)release_count++;
        dispatch(&app,event.events,event.time_ms);
    }
    assert(release_count==1u && !app.walk && !app.down_held);

    /* A full critical queue uses the emergency event slot; physical state is
     * sufficient to clear Walk even before the stalled consumer drains it. */
    dpc245_key_sampler_init(&keys,DPC245_KEYS_RAW_DOWN,0u);
    for(unsigned i=0u;i<DPC245_KEY_SAMPLER_CAPACITY-1u;i++)
        keys.queue[i]=(dpc245_key_sample_event_t){DPC245_KEY_EVENT_DOWN_PRESS,10u};
    keys.write=DPC245_KEY_SAMPLER_CAPACITY-1u;
    app.walk=app.walk_active=app.walk_quick=app.down_held=true;
    for(uint32_t ms=1u;ms<=100u;ms++)dpc245_key_sampler_tick(&keys,0u,ms);
    assert(keys.pending_down_release && keys.navigation_suppressed);
    dpc245_app_down_state(&app,false,100u);
    assert(!app.walk && !app.down_held);
    while(dpc245_key_sampler_pop(&keys,&event))dispatch(&app,event.events,event.time_ms);
    dpc245_app_down_state(&app,false,100u);
    assert(!app.walk && !app.down_held);

    dpc245_app_init(&app,&settings);
    dpc245_app_event(&app,DPC245_KEY_EVENT_DOWN_SHORT,100u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_DOWN_PRESS,200u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_DOWN_HOLD,4210u);
    assert(app.walk && app.walk_hold_latched);
    dpc245_app_down_state(&app,false,4220u);
    assert(!app.walk && !app.down_held);

    dpc245_app_init(&app,&settings);
    dpc245_app_event(&app,DPC245_KEY_EVENT_DOWN_SHORT,100u);
    dpc245_app_event(&app,DPC245_KEY_EVENT_DOWN_PRESS,200u);
    dpc245_app_down_state(&app,false,250u);
    assert(app.walk && app.walk_active && !app.down_held);
    puts("Quick and standard Walk release survive a stalled consumer and a full queue");
}

static void test_full_queue_navigation_recovery(void){
    dpc245_key_sampler_t keys;
    dpc245_key_sampler_init(&keys,0u,0u);
    for(unsigned i=0u;i<DPC245_KEY_SAMPLER_CAPACITY-1u;i++)
        keys.queue[i]=(dpc245_key_sample_event_t){DPC245_KEY_EVENT_DOWN_PRESS,10u};
    keys.write=DPC245_KEY_SAMPLER_CAPACITY-1u;
    for(uint32_t ms=1u;ms<=90u;ms++)dpc245_key_sampler_tick(&keys,DPC245_KEYS_RAW_POWER,ms);
    for(uint32_t ms=91u;ms<=180u;ms++)dpc245_key_sampler_tick(&keys,0u,ms);
    for(uint32_t ms=181u;ms<=270u;ms++)dpc245_key_sampler_tick(&keys,DPC245_KEYS_RAW_UP,ms);
    for(uint32_t ms=271u;ms<=360u;ms++)dpc245_key_sampler_tick(&keys,0u,ms);
    for(uint32_t ms=361u;ms<=450u;ms++)dpc245_key_sampler_tick(&keys,DPC245_KEYS_RAW_DOWN,ms);
    for(uint32_t ms=451u;ms<=540u;ms++)dpc245_key_sampler_tick(&keys,0u,ms);
    assert(keys.overflow && keys.navigation_suppressed && keys.pending_down_release);
    dpc245_key_sample_event_t event;
    unsigned release_count=0u,navigation_count=0u;
    while(dpc245_key_sampler_pop(&keys,&event)){
        release_count+=!!(event.events&DPC245_KEY_EVENT_DOWN_RELEASE);
        navigation_count+=!!(event.events&(DPC245_KEY_EVENT_POWER_SHORT|
            DPC245_KEY_EVENT_UP_SHORT|DPC245_KEY_EVENT_DOWN_SHORT));
    }
    assert(release_count==1u && navigation_count==0u && !keys.navigation_suppressed);
    for(uint32_t ms=541u;ms<=630u;ms++)dpc245_key_sampler_tick(&keys,DPC245_KEYS_RAW_POWER,ms);
    for(uint32_t ms=631u;ms<=720u;ms++)dpc245_key_sampler_tick(&keys,0u,ms);
    navigation_count=0u;
    while(dpc245_key_sampler_pop(&keys,&event))
        navigation_count+=!!(event.events&DPC245_KEY_EVENT_POWER_SHORT);
    assert(navigation_count==1u);
    puts("full queue discards mixed navigation, keeps DOWN release, then resynchronizes");
}

typedef struct {
    dpc245_app_t app;
    dpc245_key_sampler_t keys;
    uint32_t now_ms,delivered_mask[16],delivered_time[16];
    unsigned delivered,completed,rows;
    bool gated,physical_down;
} gated_app_t;

static void gated_app_init(gated_app_t *g,const dpc245_settings_t *settings,uint32_t now_ms){
    memset(g,0,sizeof *g);
    dpc245_app_init(&g->app,settings);
    dpc245_key_sampler_init(&g->keys,0u,now_ms);
    g->now_ms=now_ms;
}

static void gated_app_enqueue(gated_app_t *g,uint32_t mask,uint32_t time_ms){
    uint8_t next=(uint8_t)((g->keys.write+1u)%DPC245_KEY_SAMPLER_CAPACITY);
    assert(next!=g->keys.read);
    g->keys.queue[g->keys.write]=(dpc245_key_sample_event_t){mask,time_ms};
    g->keys.write=next;
}

static void gated_app_pass(gated_app_t *g){
    g->now_ms+=13u;
    dpc245_key_sample_event_t event;
    while(!g->gated && dpc245_key_sampler_pop(&g->keys,&event)){
        if(event.time_ms>g->now_ms)g->now_ms=event.time_ms;
        assert(g->delivered<16u);
        if(g->delivered)assert(event.time_ms>=g->delivered_time[g->delivered-1u]);
        g->delivered_mask[g->delivered]=event.events;
        g->delivered_time[g->delivered]=event.time_ms;
        g->delivered++;
        dispatch(&g->app,event.events,event.time_ms);
        if(event.events&(DPC245_KEY_EVENT_UP_SHORT|DPC245_KEY_EVENT_DOWN_SHORT|
                         DPC245_KEY_EVENT_POWER_SHORT|DPC245_KEY_EVENT_SETTINGS_CHORD)){
            g->gated=true;
            g->rows=0u;
            break;
        }
    }
    dpc245_app_down_state(&g->app,g->physical_down,g->now_ms);
    dpc245_app_tick(&g->app,g->now_ms);
    if(g->gated){
        g->rows+=3u;
        if(g->rows>=17u){g->gated=false;g->completed++;}
    }
}

static void gated_app_finish_frame(gated_app_t *g,unsigned delivered){
    assert(g->gated && g->delivered==delivered);
    for(unsigned i=0u;i<5u;i++){
        gated_app_pass(g);
        assert(g->delivered==delivered);
    }
    assert(!g->gated);
}

static void test_gated_app_order(void){
    dpc245_settings_t settings;
    dpc245_settings_defaults(&settings);
    gated_app_t g;
    for(unsigned active=0u;active<2u;active++){
        gated_app_init(&g,&settings,active?1500u:300u);
        g.app.assist_level=active?2u:1u;
        g.app.preferences.quick_walk_enabled=true;
        g.app.preferences.quick_walk_hold_steps=1u;
        dpc245_app_event(&g.app,DPC245_KEY_EVENT_DOWN_PRESS,100u);
        if(active){dpc245_app_tick(&g.app,700u);assert(g.app.walk && g.app.walk_active);}
        gated_app_enqueue(&g,DPC245_KEY_EVENT_POWER_SHORT,active?800u:200u);
        gated_app_enqueue(&g,DPC245_KEY_EVENT_DOWN_RELEASE|
                             DPC245_KEY_EVENT_DOWN_SHORT,active?1500u:300u);
        gated_app_enqueue(&g,DPC245_KEY_EVENT_DOWN_PRESS,active?1700u:450u);
        gated_app_pass(&g);
        assert(g.gated && g.delivered==1u);
        assert(!g.app.walk && !g.app.down_held);
        assert(g.app.suppress_down_short_once==(active!=0u));
        gated_app_finish_frame(&g,1u);
        gated_app_pass(&g);
        assert(g.gated && g.delivered==2u);
        assert(g.app.assist_level==(active?2u:0u) && !g.app.walk);
        assert(!g.app.suppress_down_short_once);
        gated_app_finish_frame(&g,2u);
        g.physical_down=true;
        gated_app_pass(&g);
        assert(g.delivered==3u && g.app.assist_level==(active?2u:0u));
        assert(g.app.walk_quick==(active!=0u) && !g.app.walk);
        assert(g.delivered_mask[0]==DPC245_KEY_EVENT_POWER_SHORT &&
               g.delivered_mask[1]==(DPC245_KEY_EVENT_DOWN_RELEASE|DPC245_KEY_EVENT_DOWN_SHORT) &&
               g.delivered_mask[2]==DPC245_KEY_EVENT_DOWN_PRESS);
        assert(g.delivered_time[0]<g.delivered_time[1] &&
               g.delivered_time[1]<g.delivered_time[2]);
    }
    puts("gated Quick Walk release, short and new press stay ordered for early and active holds");

    gated_app_init(&g,&settings,0u);
    g.app.screen=DPC245_APP_SCREEN_DISPLAY_MENU;
    g.app.selection=7u;
    uint8_t auto_off_before=dpc245_auto_off_min(&g.app.runtime.settings);
    const uint32_t edit_events[]={DPC245_KEY_EVENT_DOWN_SHORT,DPC245_KEY_EVENT_POWER_SHORT,
        DPC245_KEY_EVENT_UP_PRESS,DPC245_KEY_EVENT_UP_REPEAT,
        DPC245_KEY_EVENT_UP_REPEAT_FAST,DPC245_KEY_EVENT_UP_SHORT};
    for(unsigned i=0u;i<6u;i++)gated_app_enqueue(&g,edit_events[i],100u+100u*i);
    gated_app_pass(&g);
    assert(g.app.selection==8u && g.delivered==1u);
    gated_app_finish_frame(&g,1u);
    gated_app_pass(&g);
    assert(g.app.screen==DPC245_APP_SCREEN_EDIT_AUTO_OFF && g.delivered==2u);
    gated_app_finish_frame(&g,2u);
    assert(dpc245_auto_off_min(&g.app.runtime.settings)==auto_off_before);
    gated_app_pass(&g);
    assert(g.delivered==6u && dpc245_auto_off_min(&g.app.runtime.settings)==
           (uint8_t)((auto_off_before+2u)%11u));
    for(unsigned i=0u;i<6u;i++)assert(g.delivered_mask[i]==edit_events[i]);
    puts("editor transition delivers press, repeats and short after the displayed edit frame");

    gated_app_init(&g,&settings,0u);
    gated_app_enqueue(&g,DPC245_KEY_EVENT_POWER_SHORT,100u);
    gated_app_enqueue(&g,DPC245_KEY_EVENT_SETTINGS_CHORD,280u);
    gated_app_enqueue(&g,DPC245_KEY_EVENT_UP_HOLD,900u);
    gated_app_pass(&g);
    gated_app_finish_frame(&g,1u);
    assert(!g.app.light_toggle_requested);
    gated_app_pass(&g);
    assert(g.app.screen==DPC245_APP_SCREEN_ROOT_MENU);
    gated_app_finish_frame(&g,2u);
    gated_app_pass(&g);
    assert(g.delivered==3u && !g.app.light_toggle_requested &&
           g.delivered_mask[1]==DPC245_KEY_EVENT_SETTINGS_CHORD &&
           g.delivered_mask[2]==DPC245_KEY_EVENT_UP_HOLD);
    puts("chord precedes UP hold while queued behind a navigation frame");
}

static void test_gate_capacity_normal_rate(void){
    dpc245_key_sampler_t keys;
    dpc245_key_sampler_init(&keys,0u,0u);
    unsigned gate_passes=0u,shorts=0u,peak=0u;
    for(uint32_t ms=1u;ms<=18500u;ms++){
        uint8_t raw=ms<=18000u && ((ms-1u)%180u)<90u?DPC245_KEYS_RAW_DOWN:0u;
        dpc245_key_sampler_tick(&keys,raw,ms);
        unsigned used=(keys.write+DPC245_KEY_SAMPLER_CAPACITY-keys.read)%
                      DPC245_KEY_SAMPLER_CAPACITY;
        if(used>peak)peak=used;
        if(ms%13u!=0u)continue;
        if(gate_passes){gate_passes--;continue;}
        dpc245_key_sample_event_t event;
        while(dpc245_key_sampler_pop(&keys,&event)){
            if(event.events&DPC245_KEY_EVENT_DOWN_SHORT){shorts++;gate_passes=5u;break;}
        }
    }
    assert(shorts==100u && !keys.overflow && peak<8u);
    printf("normal gate capacity: 100 taps at 180 ms, peak queue=%u of %u, overflow=0\n",
           peak,DPC245_KEY_SAMPLER_CAPACITY-1u);
}

static void test_quick_walk_short_suppression(void){
    dpc245_settings_t settings;
    dpc245_settings_defaults(&settings);
    for(unsigned variant=0u;variant<3u;variant++){
        dpc245_app_t app;
        dpc245_app_init(&app,&settings);
        app.assist_level=variant==2u?0u:2u;
        app.preferences.quick_walk_enabled=true;
        app.preferences.quick_walk_hold_steps=1u;
        dpc245_keys_t keys;
        dpc245_keys_init(&keys,0u,0u);
        uint32_t held_ms=variant==0u?1500u:300u;
        for(uint32_t ms=1u;ms<=held_ms;ms++){
            dispatch(&app,dpc245_keys_update(&keys,DPC245_KEYS_RAW_DOWN,ms),ms);
            dpc245_app_tick(&app,ms);
        }
        if(variant==0u)assert(app.walk_quick && app.walk_active && app.walk);
        else assert(!app.walk);
        for(uint32_t ms=held_ms+1u;ms<=held_ms+100u;ms++){
            if(ms==held_ms+10u)dpc245_app_down_state(&app,false,ms);
            dispatch(&app,dpc245_keys_update(&keys,0u,ms),ms);
            dpc245_app_tick(&app,ms);
        }
        if(variant==0u)assert(app.assist_level==2u && !app.walk);
        else if(variant==1u)assert(app.assist_level==1u && !app.walk);
        else assert(app.assist_level==0u && app.walk && app.walk_active);
    }
    puts("Quick Walk active at 1.5 s suppresses short; early release lowers assist; standard Walk remains");
}

int main(void){
    test_key_time_for_pass();
    test_ride_combo_physical_keys();
    test_sample_during_foreground_pass();
    test_down_release_latch_between_passes();
    test_walk_release_overflow();
    test_full_queue_navigation_recovery();
    test_gated_app_order();
    test_gate_capacity_normal_rate();
    test_quick_walk_short_suppression();
    dpc245_settings_t defaults;
    dpc245_settings_defaults(&defaults);
    dpc245_app_t baseline;
    dpc245_app_init(&baseline,&defaults);
    enter_themes(&baseline);
    baseline.selection=4u;
    assert(replay_taps(&baseline,10u)==3u);
    test_coupled_session(DPC245_THEME_STANDARD,false,false);
    test_coupled_session(DPC245_THEME_ARC,false,false);
    test_coupled_session(DPC245_THEME_WEDGE,false,false);
    test_coupled_session(DPC245_THEME_WEDGE,true,false);
    test_coupled_session(DPC245_THEME_WEDGE,false,true);
    exercise_theme(DPC245_THEME_ARC,false,false);
    exercise_theme(DPC245_THEME_ARC,true,false);
    exercise_theme(DPC245_THEME_WEDGE,true,false);
    exercise_theme(DPC245_THEME_WEDGE,true,true);
    puts("theme key session passed");
    return 0;
}
