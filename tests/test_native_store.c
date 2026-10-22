/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "native_store.h"
#include "app_config.h"
#include "dpc245_spi_mem.h"
#include "dpc245_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEM_BYTES 0x200000u
static uint8_t memory[MEM_BYTES], saved_memory[MEM_BYTES];
static uint32_t wear[EVIST_NATIVE_PAGE_COUNT];
static uint32_t pending_erase;
static unsigned erase_ticks, erase_calls, write_calls;
static bool fail_read, fail_erase, fail_write;
static bool stuck_ready;
static uint32_t fail_read_address;
static unsigned tear_bytes;
static unsigned failures;
static bool step_active;
static size_t step_bytes,max_step_bytes;
static unsigned step_programs,step_erases,step_polls;
static uint32_t step_time_ms;

static void check(bool okay,const char *name){
    if(!okay){++failures;fprintf(stderr,"FAIL %s\n",name);}
}
static bool mread(void *ctx,uint32_t address,uint8_t *dst,size_t n){
    (void)ctx;
    if(step_active)step_bytes+=n+5u;
    if(fail_read||address==fail_read_address||address>MEM_BYTES||n>MEM_BYTES-address)return false;
    memcpy(dst,memory+address,n);return true;
}
static bool mwrite(void *ctx,uint32_t address,const uint8_t *src,size_t n){
    (void)ctx;++write_calls;
    if(step_active){step_bytes+=n+7u;++step_programs;}
    if(fail_write||address>MEM_BYTES||n>MEM_BYTES-address)return false;
    size_t count=tear_bytes&&tear_bytes<n?tear_bytes:n;
    for(size_t i=0u;i<count;++i)memory[address+i]&=src[i];
    return count==n;
}
static bool merase(void *ctx,uint32_t address){
    (void)ctx;++erase_calls;
    if(step_active){step_bytes+=6u;++step_erases;}
    if(fail_erase||!dpc245_spi_mem_erase_allowed(address)||erase_ticks)return false;
    pending_erase=address;erase_ticks=2u;return true;
}
static int mready(void *ctx){
    (void)ctx;
    if(step_active){step_bytes+=2u;++step_polls;}
    if(stuck_ready)return 0;
    if(!erase_ticks)return 1;
    if(--erase_ticks)return 0;
    memset(memory+pending_erase,0xFFu,EVIST_NATIVE_PAGE_BYTES);
    if(pending_erase>=EVIST_NATIVE_FIRST&&pending_erase<=EVIST_NATIVE_LAST)
        ++wear[(pending_erase-EVIST_NATIVE_FIRST)/EVIST_NATIVE_PAGE_BYTES];
    return 1;
}
static void power_cut(void){while(erase_ticks)mready(NULL);}
static dpc245_persist_io_t memio={mread,mwrite,NULL,NULL};
static evist_native_io_t io={&memio,merase,mready,NULL};
static void reset_memory(void){
    memset(memory,0xFF,sizeof memory);memset(wear,0,sizeof wear);
    pending_erase=0u;erase_ticks=erase_calls=write_calls=0u;
    fail_read=fail_erase=fail_write=false;tear_bytes=0u;
    stuck_ready=false;
    fail_read_address=UINT32_MAX;
    step_active=false;step_bytes=max_step_bytes=0u;
    step_time_ms=0u;
}
static evist_native_result_t checked_step(evist_native_store_t *store){
    step_active=true;step_bytes=0u;step_programs=step_erases=step_polls=0u;
    step_time_ms+=10u;
    evist_native_result_t result=evist_native_store_step(store,&io,step_time_ms);
    step_active=false;
    if(step_bytes>max_step_bytes)max_step_bytes=step_bytes;
    check(step_programs<=1u&&step_erases<=1u&&step_polls<=1u,
          "one SPI operation class per step");
    check(step_bytes+512u<=770u,"status-inclusive wire allowance");
    return result;
}
static evist_native_result_t finish_job(evist_native_store_t *store){
    evist_native_result_t result=EVIST_NATIVE_BUSY;
    for(unsigned step=0u;step<64u&&store->busy;++step)
        result=checked_step(store);
    check(!store->busy,"bounded job");return result;
}
static bool same_values(const evist_native_values_t *a,const evist_native_values_t *b){
    return a->preferences.assist_level==b->preferences.assist_level&&
           a->preferences.assist_mode_mask==b->preferences.assist_mode_mask&&
           a->preferences.lower_panel==b->preferences.lower_panel&&
           a->preferences.light_gain==b->preferences.light_gain&&
           a->preferences.temp_view==b->preferences.temp_view&&
           a->preferences.motor_temp_limit_c==b->preferences.motor_temp_limit_c&&
           a->preferences.ctrl_temp_limit_c==b->preferences.ctrl_temp_limit_c&&
           a->preferences.panel_mask==b->preferences.panel_mask&&
           dpc245_unit(&a->hmi)==dpc245_unit(&b->hmi)&&
           dpc245_get_u32_le(&a->hmi,DPC245_OFS_TRIP_DISTANCE_RAW)==
           dpc245_get_u32_le(&b->hmi,DPC245_OFS_TRIP_DISTANCE_RAW)&&
           a->ride.mode==b->ride.mode&&a->ride.start==b->ride.start&&
           a->ride.legal_x100==b->ride.legal_x100&&
           a->ride.offroad_x100==b->ride.offroad_x100;
}
static void first_record(evist_native_store_t *store,evist_native_values_t *base){
    check(evist_native_store_init(store,&io,base),"empty init");
    check(!store->valid&&store->imported,"empty needs first image");
    check(evist_native_store_begin(store,base),"first begin");
    check(finish_job(store)==EVIST_NATIVE_OK,"first save");
}
static void test_many_saves(void){
    reset_memory();evist_native_store_t store;evist_native_values_t value,reloaded;
    first_record(&store,&value);
    for(unsigned i=0u;i<3000u;++i){
        value.preferences.assist_level=(uint8_t)(i%10u);
        value.preferences.assist_mode_mask=(uint8_t)(i&EVIST_ASSIST_MODE_MASK_ALL);
        value.preferences.light_gain=(uint8_t)(1u+i%9u);
        dpc245_set_u32_le(&value.hmi,DPC245_OFS_TRIP_DISTANCE_RAW,i*10u);
        check(evist_native_store_begin(&store,&value),"many begin");
        if(finish_job(&store)!=EVIST_NATIVE_OK){check(false,"many save");break;}
    }
    evist_native_store_t reboot;
    check(evist_native_store_init(&reboot,&io,&reloaded)&&same_values(&value,&reloaded),
          "many reload");
    uint32_t min=0xFFFFFFFFu,max=0u;
    for(unsigned i=0u;i<EVIST_NATIVE_PAGE_COUNT;++i){
        if(wear[i]<min)min=wear[i];if(wear[i]>max)max=wear[i];
    }
    check(max-min<=1u,"erase wear spread");
    printf("many saves=3000 pages=%u erase_min=%lu erase_max=%lu\n",
           (unsigned)EVIST_NATIVE_PAGE_COUNT,(unsigned long)min,(unsigned long)max);
}
static void test_cuts_and_faults(void){
    reset_memory();evist_native_store_t store;evist_native_values_t base,want,loaded;
    first_record(&store,&base);
    memcpy(saved_memory,memory,sizeof memory);
    want=base;want.preferences.light_gain=7u;want.ride.mode=EVIST_RIDE_OFFROAD;
    want.ride.offroad_x100=3000u;
    unsigned cuts=0u;
    for(unsigned cut=0u;cut<20u;++cut){
        memcpy(memory,saved_memory,sizeof memory);power_cut();
        check(evist_native_store_init(&store,&io,&loaded),"cut setup");
        check(evist_native_store_begin(&store,&want),"cut begin");
        for(unsigned i=0u;i<cut&&store.busy;++i)(void)checked_step(&store);
        power_cut();
        evist_native_store_t reboot;
        check(evist_native_store_init(&reboot,&io,&loaded),"cut reload");
        check(same_values(&loaded,&base)||same_values(&loaded,&want),"cut retains valid value");
        ++cuts;
    }
    memcpy(memory,saved_memory,sizeof memory);
    check(evist_native_store_init(&store,&io,&loaded),"tear setup");
    want.preferences.light_gain=8u;
    tear_bytes=40u;
    check(evist_native_store_begin(&store,&want),"tear begin");
    check(finish_job(&store)==EVIST_NATIVE_WRITE_FAILED,"torn write reported");
    tear_bytes=0u;
    check(evist_native_store_init(&store,&io,&loaded)&&same_values(&loaded,&base),
          "torn write keeps prior");
    memcpy(memory,saved_memory,sizeof memory);
    check(evist_native_store_init(&store,&io,&loaded),"CRC setup");
    check(evist_native_store_begin(&store,&want),"CRC begin");
    while(store.busy&&store.phase!=6u)(void)checked_step(&store);
    /* Flip a payload bit before retiring the prior image. */
    memory[EVIST_NATIVE_FIRST+EVIST_NATIVE_PAGE_BYTES+20u]^=1u;
    evist_native_store_t reboot;
    check(evist_native_store_init(&reboot,&io,&loaded)&&same_values(&loaded,&base),
          "CRC fault keeps prior");
    memcpy(memory,saved_memory,sizeof memory);
    check(evist_native_store_init(&store,&io,&loaded),"erase setup");
    memory[EVIST_NATIVE_FIRST+EVIST_NATIVE_PAGE_BYTES]=0u;
    fail_erase=true;
    check(evist_native_store_begin(&store,&want),"erase begin");
    check(finish_job(&store)==EVIST_NATIVE_ERASE_FAILED,"erase failure reported");
    fail_erase=false;
    check(evist_native_store_init(&reboot,&io,&loaded)&&same_values(&loaded,&base),
          "erase failure keeps prior");
    fail_read=true;
    check(!evist_native_store_init(&reboot,&io,&loaded),"read failure cannot mean blank");
    fail_read=false;
    check(evist_native_store_init(&reboot,&io,&loaded),"step read setup");
    check(evist_native_store_begin(&reboot,&want),"step read begin");
    unsigned erases_before=erase_calls;
    fail_read=true;
    check(checked_step(&reboot)==EVIST_NATIVE_NO_MEMORY&&
          erase_calls==erases_before,"step read failure cannot trigger erase");
    fail_read=false;
    check(!dpc245_spi_mem_erase_allowed(EVIST_NATIVE_FIRST-1u)&&
          !dpc245_spi_mem_erase_allowed(0x001FFF00u)&&
          !dpc245_spi_mem_erase_allowed(0x00000300u)&&
          dpc245_spi_mem_erase_allowed(EVIST_NATIVE_FIRST),
          "erase guard rejects outside and unaligned");
    printf("power cuts=%u torn/CRC/erase/read/guard checked\n",cuts);
}
static void test_dirty_target_and_retirement(void){
    reset_memory();evist_native_store_t store;evist_native_values_t first,second,third,loaded;
    first_record(&store,&first);
    second=first;second.preferences.light_gain=6u;
    check(evist_native_store_begin(&store,&second),"second begin");
    check(finish_job(&store)==EVIST_NATIVE_OK,"second save");
    uint32_t newest=EVIST_NATIVE_FIRST+(uint32_t)store.page*EVIST_NATIVE_PAGE_BYTES;
    memory[newest+20u]^=1u;
    evist_native_store_t reboot;
    check(evist_native_store_init(&reboot,&io,&loaded)&&same_values(&loaded,&first),
          "CRC fault on newest returns prior good image");
    memory[newest+20u]^=1u;
    uint32_t target=EVIST_NATIVE_FIRST+(uint32_t)(store.page+1u)*EVIST_NATIVE_PAGE_BYTES;
    memory[target]=0u;
    memcpy(saved_memory,memory,sizeof memory);
    third=second;third.preferences.light_gain=9u;
    unsigned cuts=0u;
    for(unsigned cut=0u;cut<30u;++cut){
        memcpy(memory,saved_memory,sizeof memory);
        check(evist_native_store_init(&store,&io,&loaded),"dirty cut setup");
        check(evist_native_store_begin(&store,&third),"dirty cut begin");
        for(unsigned i=0u;i<cut&&store.busy;++i)(void)checked_step(&store);
        power_cut();
        check(evist_native_store_init(&reboot,&io,&loaded),"dirty cut reload");
        check(same_values(&loaded,&second)||same_values(&loaded,&third),
              "dirty cut retains newest good image");
        ++cuts;
    }
    memcpy(memory,saved_memory,sizeof memory);
    check(evist_native_store_init(&store,&io,&loaded),"retire setup");
    check(evist_native_store_begin(&store,&third),"retire begin");
    while(store.busy&&store.phase!=7u)(void)checked_step(&store);
    fail_erase=true;
    check(checked_step(&store)==EVIST_NATIVE_ERASE_FAILED,
          "retire erase failure reported");
    fail_erase=false;
    check(evist_native_store_init(&reboot,&io,&loaded)&&same_values(&loaded,&third),
          "retire erase failure keeps new image");
    printf("dirty-target cuts=%u prior-image CRC and retirement checked\n",cuts);
}
static void check_reset_values(const evist_native_values_t *before,
                               const evist_native_values_t *after);
static void test_import(void){
    reset_memory();
    const uint8_t header[8]={'E','V','P','1',0xBAu,0xA9u,0xAFu,0xCEu};
    memcpy(memory+EVIST_PREF_REGION_BASE,header,sizeof header);
    memory[EVIST_PREF_PAIR_BASE]=0x13u;
    memory[EVIST_PREF_PAIR_BASE+1u]=0x13u^0x5Au;
    memory[EVIST_PREF_SET_BASE]=0x0Au;
    memory[EVIST_PREF_SET_BASE+1u]=0x0Au^0xC7u;
    evist_ride_cfg_t ride=evist_ride_defaults();
    ride.mode=EVIST_RIDE_OFFROAD;ride.offroad_x100=3500u;
    ride.legal_x100=2500u;ride.start=EVIST_RIDE_START_LAST;
    uint8_t old_ride[EVIST_RIDE_REC_SIZE];
    check(evist_ride_record_v2_encode(&ride,old_ride),"fixture ride encode");
    memcpy(memory+EVIST_PREF_RIDE_BASE,old_ride,sizeof old_ride);
    dpc245_settings_t hmi;dpc245_settings_defaults(&hmi);
    dpc245_set_unit(&hmi,DPC245_UNIT_IMPERIAL);dpc245_settings_finalize(&hmi);
    memory[DPC245_JOURNAL_MARKER_BASE]=0xA5u;
    memory[DPC245_JOURNAL_MARKER_BASE+1u]=0x5Au;
    memcpy(memory+DPC245_JOURNAL_RECORD_BASE,hmi.raw,DPC245_SETTINGS_SIZE);
    evist_native_store_t store,reboot;evist_native_values_t value,loaded;
    unsigned erases_before=erase_calls;
    fail_read_address=EVIST_PREF_RIDE_BASE;
    check(!evist_native_store_init(&store,&io,&value)&&erase_calls==erases_before,
          "ride import read failure cannot start migration");
    fail_read_address=UINT32_MAX;
    check(evist_native_store_init(&store,&io,&value),"import init");
    check(store.options_reset&&store.saved.preferences.assist_level==3u&&
          store.saved.preferences.lower_panel==3u&&
          store.saved.preferences.light_gain==6u&&dpc245_unit(&store.saved.hmi)==1u&&
          store.saved.ride.offroad_x100==3500u,"settings-log values decoded");
    check_reset_values(&store.saved,&value);
    memcpy(saved_memory,memory,sizeof memory);
    for(unsigned cut=0u;cut<12u;++cut){
        memcpy(memory,saved_memory,sizeof memory);
        check(evist_native_store_init(&store,&io,&loaded),"import cut setup");
        check(evist_native_store_begin(&store,&loaded),"import cut begin");
        for(unsigned step=0u;step<cut&&store.busy;++step)(void)checked_step(&store);
        power_cut();
        evist_native_store_t after_cut;
        check(evist_native_store_init(&after_cut,&io,&loaded)&&same_values(&loaded,&value),
              "import cut retains source values");
    }
    memcpy(memory,saved_memory,sizeof memory);
    check(evist_native_store_init(&store,&io,&loaded),"import final setup");
    check(evist_native_store_begin(&store,&value),"import begin");
    check(finish_job(&store)==EVIST_NATIVE_OK,"import write");
    check(evist_native_store_init(&reboot,&io,&loaded)&&same_values(&loaded,&value),
          "import reload");
    memcpy(memory+EVIST_NATIVE_FIRST,saved_memory+EVIST_NATIVE_FIRST,EVIST_NATIVE_PAGE_BYTES);
    check(evist_ride_record_encode(&ride,old_ride),"fixture ride v1 encode");
    memcpy(memory+EVIST_PREF_RIDE_BASE,old_ride,sizeof old_ride);
    check(evist_native_store_init(&reboot,&io,&loaded)&&reboot.options_reset&&
          reboot.saved.ride.mode==EVIST_RIDE_OFFROAD&&
          reboot.saved.ride.offroad_x100==3500u&&
          loaded.ride.mode==EVIST_RIDE_LEGAL,"ride v1 import reset");
    for(uint16_t page=0u;page<EVIST_NATIVE_PAGE_COUNT;++page)
        memory[EVIST_NATIVE_FIRST+(uint32_t)page*EVIST_NATIVE_PAGE_BYTES]=0u;
    check(evist_native_store_init(&store,&io,&loaded)&&store.known&&store.imported&&
          !store.valid&&store.candidate==0u&&store.options_reset&&
          loaded.preferences.assist_level==0u&&
          dpc245_unit(&loaded.hmi)==DPC245_UNIT_METRIC&&
          loaded.ride.mode==EVIST_RIDE_LEGAL,"full ring resets settings-log options and selects page zero");
    evist_native_values_t full_import=loaded;
    unsigned erases_before_full=erase_calls;
    check(evist_native_store_begin(&store,&full_import),"full ring begin");
    check(finish_job(&store)==EVIST_NATIVE_OK&&erase_calls==erases_before_full+1u,
          "full ring first save erases page zero");
    check(evist_native_store_init(&reboot,&io,&loaded)&&same_values(&loaded,&full_import),
          "full ring reloads imported values");
    printf("import pair/preferences/settings/ride v1/v2 and 12 cuts checked\n");
}
static void record_crc(uint32_t address){
    uint32_t crc=0xFFFFFFFFu;
    for(unsigned i=0u;i<188u;++i){
        crc^=memory[address+i];
        for(unsigned bit=0u;bit<8u;++bit)crc=(crc>>1)^((crc&1u)?0xEDB88320u:0u);
    }
    crc=~crc;
    for(unsigned i=0u;i<4u;++i)memory[address+188u+i]=(uint8_t)(crc>>(8u*i));
}
static bool reset_hmi_byte(unsigned i){
    return i==0x05u||i==0x06u||i==0x07u||i==0x09u||i==0x0Au||
           i==0x0Du||i==0x0Eu||i==0x2Eu;
}
static void check_reset_values(const evist_native_values_t *before,
                               const evist_native_values_t *after){
    evist_native_values_t defaults;
    evist_preferences_defaults(&defaults.preferences);
    dpc245_settings_defaults(&defaults.hmi);
    defaults.ride=evist_ride_defaults();
    const evist_preferences_t *p=&after->preferences,*d=&defaults.preferences;
    check(p->assist_level==d->assist_level&&p->lower_panel==d->lower_panel&&
          p->assist_mode_mask==d->assist_mode_mask&&
          p->temp_view==d->temp_view&&p->motor_temp_limit_c==d->motor_temp_limit_c&&
          p->ctrl_temp_limit_c==d->ctrl_temp_limit_c&&
          p->firmware_profile==d->firmware_profile&&p->graph_scale_w==d->graph_scale_w&&
          p->battery_mask==d->battery_mask&&p->light_mode==d->light_mode&&
          p->light_gain==d->light_gain&&p->light_dwell==d->light_dwell&&
          p->light_threshold==d->light_threshold&&p->light_level==d->light_level&&
          p->panel_mask==d->panel_mask&&p->quick_walk_enabled==d->quick_walk_enabled&&
          p->quick_walk_hold_steps==d->quick_walk_hold_steps,
          "firmware change resets every preference");
    check(after->ride.mode==defaults.ride.mode&&after->ride.start==defaults.ride.start&&
          after->ride.last_mode==defaults.ride.last_mode&&
          after->ride.legal_x100==defaults.ride.legal_x100&&
          after->ride.offroad_x100==defaults.ride.offroad_x100,
          "firmware change resets ride configuration");
    for(unsigned i=0u;i<DPC245_SETTINGS_SIZE;++i){
        if(i==DPC245_SETTINGS_CHECKSUM_OFFSET)continue;
        check(after->hmi.raw[i]==(reset_hmi_byte(i)?defaults.hmi.raw[i]:before->hmi.raw[i]),
              "firmware change preserves every non-option HMI byte");
    }
    check(dpc245_settings_validate(&after->hmi),"reset HMI checksum valid");
}
static void test_settings_log_upgrade(void){
    reset_memory();
    const uint8_t header[8]={'E','V','P','1',0xBAu,0xA9u,0xAFu,0xCEu};
    memcpy(memory+EVIST_PREF_REGION_BASE,header,sizeof header);
    memory[EVIST_PREF_PAIR_BASE]=0x13u;
    memory[EVIST_PREF_PAIR_BASE+1u]=0x13u^0x5Au;
    memory[EVIST_PREF_SET_BASE]=0x0Au;
    memory[EVIST_PREF_SET_BASE+1u]=0x0Au^0xC7u;
    evist_ride_cfg_t ride=evist_ride_defaults();
    ride.mode=EVIST_RIDE_OFFROAD;
    ride.start=EVIST_RIDE_START_OFFROAD;
    ride.last_mode=EVIST_RIDE_OFFROAD;
    ride.offroad_x100=3500u;
    uint8_t old_ride[EVIST_RIDE_REC_SIZE];
    check(evist_ride_record_v2_encode(&ride,old_ride),"upgrade ride fixture");
    memcpy(memory+EVIST_PREF_RIDE_BASE,old_ride,sizeof old_ride);
    dpc245_settings_t hmi;dpc245_settings_defaults(&hmi);
    hmi.raw[0x05u]=1u; /* unit */
    hmi.raw[0x06u]=2u; /* brightness */
    hmi.raw[0x07u]=8u; /* auto-off */
    hmi.raw[0x09u]=1u; /* service tips */
    hmi.raw[0x0Au]=7u; /* light sensitivity */
    hmi.raw[0x0Bu]=1u; /* preserved binary field */
    hmi.raw[0x0Cu]=0u; /* preserved binary field */
    hmi.raw[0x0Du]=3u; /* theme */
    hmi.raw[0x0Eu]=2u; /* language */
    hmi.raw[0x2Eu]=1u; /* BLE mode */
    dpc245_set_u32_le(&hmi,DPC245_OFS_TOTAL_DISTANCE_RAW,7654321u);
    dpc245_set_u32_le(&hmi,DPC245_OFS_TRIP_TIME_HALFSEC,12345u);
    dpc245_set_u32_le(&hmi,DPC245_OFS_SERVICE_DISTANCE_RAW,123456u);
    hmi.raw[DPC245_ERROR_HISTORY_OFFSET]=0x42u;
    hmi.raw[DPC245_ERROR_HISTORY_COUNT_OFFSET]=1u;
    dpc245_settings_finalize(&hmi);
    memory[DPC245_JOURNAL_MARKER_BASE]=0xA5u;
    memory[DPC245_JOURNAL_MARKER_BASE+1u]=0x5Au;
    memcpy(memory+DPC245_JOURNAL_RECORD_BASE,hmi.raw,DPC245_SETTINGS_SIZE);
    evist_native_store_t store,reboot;
    evist_native_values_t loaded,source;
    unsigned writes_before=write_calls,erases_before=erase_calls;
    check(evist_native_store_init(&store,&io,&loaded)&&!store.valid&&
          store.imported&&store.options_reset&&write_calls==writes_before&&
          erase_calls==erases_before,"settings-log upgrade resets in RAM without flash writes");
    source=store.saved;
    check(memcmp(source.hmi.raw,hmi.raw,DPC245_SETTINGS_SIZE)==0,
          "settings-log HMI source loaded intact");
    check_reset_values(&source,&loaded);
    check(loaded.hmi.raw[0x0Bu]==1u&&loaded.hmi.raw[0x0Cu]==0u&&
          loaded.hmi.raw[0x0Eu]==0u,"settings-log kept flags and reset language");
    check(evist_native_store_begin(&store,&loaded)&&finish_job(&store)==EVIST_NATIVE_OK&&
          write_calls==writes_before+1u&&erase_calls==erases_before,
          "settings-log upgrade takes one normal first save");
    uint32_t addr=EVIST_NATIVE_FIRST+(uint32_t)store.page*EVIST_NATIVE_PAGE_BYTES;
    check(memcmp(memory+addr+111u,EVISTDRIVE_HMI_VERSION,
                 sizeof(EVISTDRIVE_HMI_VERSION))==0,"upgraded record stamped current version");
    check(evist_native_store_init(&reboot,&io,&loaded)&&reboot.valid&&
          !reboot.options_reset,"upgraded record does not reset again");
    check_reset_values(&source,&loaded);
    puts("settings-log upgrade resets options and retains counters/history");
}
static void test_version_reset(void){
    reset_memory();evist_native_store_t store,reboot;
    evist_native_values_t value,loaded;
    check(evist_native_store_init(&store,&io,&value)&&!store.valid&&
          !store.options_reset&&store.imported,"blank memory keeps first-start behavior");
    first_record(&store,&value);
    value.preferences.assist_level=7u;
    value.preferences.light_gain=8u;
    value.preferences.motor_temp_limit_c=110u;
    value.ride.mode=EVIST_RIDE_OFFROAD;
    value.ride.offroad_x100=3500u;
    dpc245_set_unit(&value.hmi,1u);
    dpc245_set_brightness_level(&value.hmi,2u);
    dpc245_set_auto_off_min(&value.hmi,8u);
    dpc245_set_service_tips(&value.hmi,1u);
    dpc245_set_al_sensitivity(&value.hmi,7u);
    dpc245_set_theme(&value.hmi,2u);
    dpc245_set_language(&value.hmi,2u);
    value.hmi.raw[0x0Bu]=1u;
    value.hmi.raw[0x0Cu]=0u;
    value.hmi.raw[0x2Eu]=1u;
    dpc245_set_u32_le(&value.hmi,DPC245_OFS_TOTAL_DISTANCE_RAW,7654321u);
    dpc245_set_u32_le(&value.hmi,DPC245_OFS_TRIP_TIME_HALFSEC,12345u);
    value.hmi.raw[DPC245_ERROR_HISTORY_OFFSET]=0x42u;
    value.hmi.raw[DPC245_ERROR_HISTORY_COUNT_OFFSET]=1u;
    dpc245_settings_finalize(&value.hmi);
    check(evist_native_store_begin(&store,&value)&&finish_job(&store)==EVIST_NATIVE_OK,
          "custom record saved");
    uint32_t newest=EVIST_NATIVE_FIRST+(uint32_t)store.page*EVIST_NATIVE_PAGE_BYTES;
    check(memcmp(memory+newest+111u,EVISTDRIVE_HMI_VERSION,
                 sizeof(EVISTDRIVE_HMI_VERSION))==0,"record contains firmware version");
    unsigned writes_before=write_calls;
    check(evist_native_store_init(&reboot,&io,&loaded)&&!reboot.options_reset&&
          same_values(&value,&loaded)&&
          memcmp(value.hmi.raw,loaded.hmi.raw,DPC245_SETTINGS_SIZE)==0&&
          write_calls==writes_before,
          "same version loads unchanged without save");
    for(unsigned kind=0u;kind<2u;++kind){
        if(kind==0u)memset(memory+newest+111u,0xFFu,32u);
        else {memset(memory+newest+111u,0xFFu,32u);
              memcpy(memory+newest+111u,"10.18",6u);}
        record_crc(newest);
        writes_before=write_calls;
        check(evist_native_store_init(&reboot,&io,&loaded)&&reboot.options_reset&&
              write_calls==writes_before,"older version loads without writing at init");
        check_reset_values(&value,&loaded);
        if(kind==0u){
            fail_write=true;
            check(evist_native_store_begin(&reboot,&loaded)&&
                  finish_job(&reboot)==EVIST_NATIVE_WRITE_FAILED&&
                  !reboot.busy,"reset save failure ends without blocking");
            fail_write=false;
            check(evist_native_store_init(&reboot,&io,&loaded)&&reboot.options_reset,
                  "next boot retries after failed reset save");
            check_reset_values(&value,&loaded);
        }else{
            check(evist_native_store_begin(&reboot,&loaded)&&
                  finish_job(&reboot)==EVIST_NATIVE_OK,"reset saved through normal path");
            uint32_t updated=EVIST_NATIVE_FIRST+(uint32_t)reboot.page*EVIST_NATIVE_PAGE_BYTES;
            check(memcmp(memory+updated+111u,EVISTDRIVE_HMI_VERSION,
                         sizeof(EVISTDRIVE_HMI_VERSION))==0,"reset writes current version");
            check(evist_native_store_init(&store,&io,&loaded)&&!store.options_reset,
                  "saved version does not reset again");
            check_reset_values(&value,&loaded);
        }
    }
    puts("blank, old, different and same firmware versions; save failure checked");
}
static void test_temperature_limits(void){
    reset_memory();evist_native_store_t store;evist_native_values_t value,loaded;
    first_record(&store,&value);
    value.preferences.motor_temp_limit_c=120u;value.preferences.ctrl_temp_limit_c=95u;
    check(evist_native_store_begin(&store,&value),"temperature limits begin");
    check(finish_job(&store)==EVIST_NATIVE_OK,"temperature limits save");
    uint32_t newest=EVIST_NATIVE_FIRST+(uint32_t)store.page*EVIST_NATIVE_PAGE_BYTES;
    check(memory[newest+109u]==120u&&memory[newest+110u]==95u,"temperature limit bytes");
    evist_native_store_t reboot;
    check(evist_native_store_init(&reboot,&io,&loaded)&&same_values(&value,&loaded),
          "temperature limits round trip");
    memory[newest+109u]=0xFFu;memory[newest+110u]=0xFFu;record_crc(newest);
    check(evist_native_store_init(&reboot,&io,&loaded)&&loaded.preferences.motor_temp_limit_c==0u&&
          loaded.preferences.ctrl_temp_limit_c==0u,"earlier record defaults limits OFF");
    memory[newest+109u]=39u;record_crc(newest);
    check(evist_native_store_init(&reboot,&io,&loaded)&&reboot.page!=store.page&&
          loaded.preferences.motor_temp_limit_c==0u,"invalid limit record rejected");
    puts("temperature limit store round trip, earlier image, invalid value checked");
}
static void test_erase_wait_deadline(void){
    reset_memory();
    evist_native_store_t store;evist_native_values_t value;
    first_record(&store,&value);
    memory[EVIST_NATIVE_FIRST+EVIST_NATIVE_PAGE_BYTES]=0u;
    check(evist_native_store_begin(&store,&value),"erase timeout begin");
    while(store.busy&&store.phase!=3u)(void)checked_step(&store);
    stuck_ready=true;
    evist_native_result_t result=EVIST_NATIVE_BUSY;
    for(unsigned i=0u;i<600u&&store.busy;i++)result=checked_step(&store);
    check(result==EVIST_NATIVE_ERASE_FAILED&&!store.busy,"erase wait deadline");
    stuck_ready=false;

    reset_memory();first_record(&store,&value);
    check(evist_native_store_begin(&store,&value),"retire setup begin");
    check(finish_job(&store)==EVIST_NATIVE_OK,"retire setup save");
    check(evist_native_store_begin(&store,&value),"retire timeout begin");
    while(store.busy&&store.phase!=8u)(void)checked_step(&store);
    stuck_ready=true;result=EVIST_NATIVE_BUSY;
    for(unsigned i=0u;i<600u&&store.busy;i++)result=checked_step(&store);
    check(result==EVIST_NATIVE_ERASE_FAILED&&!store.busy,"retire wait deadline");
    printf("erase and retire waits fail within 5000 ms of busy status\n");
}
int main(void){
    test_settings_log_upgrade();
    test_version_reset();
    test_erase_wait_deadline();
    test_temperature_limits();
    test_many_saves();test_cuts_and_faults();test_dirty_target_and_retirement();test_import();
    printf("step max wire=%lu B, with full status allowance=%lu B\n",
           (unsigned long)max_step_bytes,(unsigned long)(max_step_bytes+512u));
    printf("native store issues=%u\n",failures);
    return failures?1:0;
}
