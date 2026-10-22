/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "native_store.h"
#include "app_config.h"

#define RECORD_CRC 188u
#define RECORD_PAYLOAD 12u
#define HMI_OFFSET 29u
#define RIDE_OFFSET 102u
#define MOTOR_LIMIT_OFFSET 109u
#define CTRL_LIMIT_OFFSET 110u
#define FIRMWARE_VERSION_OFFSET 111u
#define FIRMWARE_VERSION_BYTES 32u
#define ASSIST_MASK_OFFSET 143u
_Static_assert(sizeof(EVISTDRIVE_HMI_VERSION) <= FIRMWARE_VERSION_BYTES,
               "firmware version exceeds native record field");
_Static_assert(ASSIST_MASK_OFFSET >= FIRMWARE_VERSION_OFFSET + FIRMWARE_VERSION_BYTES &&
               ASSIST_MASK_OFFSET < RECORD_CRC, "assist mask overlaps native record fields");
/* An erase that reports busy for five seconds is treated as failed. */
#define ERASE_WAIT_LIMIT_MS 5000u

enum { STEP_CHECK = 1, STEP_ERASE_START, STEP_ERASE_WAIT,
       STEP_CONFIRM, STEP_WRITE, STEP_VERIFY, STEP_RETIRE_START,
       STEP_RETIRE_WAIT, STEP_RETIRE_CONFIRM };

static uint32_t page_address(uint16_t page) {
    return EVIST_NATIVE_FIRST + (uint32_t)page * EVIST_NATIVE_PAGE_BYTES;
}

static void put16(uint8_t *b, uint16_t v) { b[0]=(uint8_t)v; b[1]=(uint8_t)(v>>8); }
static uint16_t get16(const uint8_t *b) { return (uint16_t)(b[0]|((uint16_t)b[1]<<8)); }
static void put32(uint8_t *b, uint32_t v) {
    for (unsigned i=0u;i<4u;++i) b[i]=(uint8_t)(v>>(8u*i));
}
static uint32_t get32(const uint8_t *b) {
    return (uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);
}

static uint32_t crc32(const uint8_t *b, size_t n) {
    uint32_t crc=0xFFFFFFFFu;
    for(size_t i=0u;i<n;++i){
        crc^=b[i];
        for(unsigned bit=0u;bit<8u;++bit)
            crc=(crc>>1)^((crc&1u)?0xEDB88320u:0u);
    }
    return ~crc;
}

static bool values_valid(const evist_native_values_t *v) {
    return v && evist_preferences_validate(&v->preferences) &&
           dpc245_settings_validate(&v->hmi) && evist_ride_cfg_valid(&v->ride);
}

static void encode(uint8_t out[EVIST_NATIVE_RECORD_BYTES],
                   const evist_native_values_t *v, uint32_t sequence) {
    for(unsigned i=0u;i<EVIST_NATIVE_RECORD_BYTES;++i) out[i]=0xFFu;
    out[0]='E';out[1]='V';out[2]='N';out[3]='1';
    put16(out+4u,1u);put16(out+6u,EVIST_NATIVE_RECORD_BYTES);
    put32(out+8u,sequence);
    const evist_preferences_t *p=&v->preferences;
    uint8_t *q=out+RECORD_PAYLOAD;
    q[0]=p->assist_level;q[1]=p->lower_panel;q[2]=(uint8_t)p->temp_view;
    q[3]=(uint8_t)p->firmware_profile;put16(q+4u,p->graph_scale_w);
    q[6]=p->battery_mask;q[7]=(uint8_t)p->light_mode;q[8]=p->light_gain;
    q[9]=p->light_dwell;put16(q+10u,p->light_threshold);
    q[12]=p->light_level;put16(q+13u,p->panel_mask);
    q[15]=p->quick_walk_enabled?1u:0u;q[16]=p->quick_walk_hold_steps;
    out[ASSIST_MASK_OFFSET]=p->assist_mode_mask;
    for(unsigned i=0u;i<DPC245_SETTINGS_SIZE;++i)out[HMI_OFFSET+i]=v->hmi.raw[i];
    q=out+RIDE_OFFSET;
    q[0]=(uint8_t)v->ride.mode;q[1]=(uint8_t)v->ride.start;
    q[2]=(uint8_t)v->ride.last_mode;
    put16(q+3u,v->ride.legal_x100);put16(q+5u,v->ride.offroad_x100);
    out[MOTOR_LIMIT_OFFSET]=p->motor_temp_limit_c;
    out[CTRL_LIMIT_OFFSET]=p->ctrl_temp_limit_c;
    for(unsigned i=0u;i<sizeof(EVISTDRIVE_HMI_VERSION);++i)
        out[FIRMWARE_VERSION_OFFSET+i]=(uint8_t)EVISTDRIVE_HMI_VERSION[i];
    put32(out+RECORD_CRC,crc32(out,RECORD_CRC));
}

static bool current_firmware_version(const uint8_t in[EVIST_NATIVE_RECORD_BYTES]) {
    for(unsigned i=0u;i<sizeof(EVISTDRIVE_HMI_VERSION);++i)
        if(in[FIRMWARE_VERSION_OFFSET+i]!=(uint8_t)EVISTDRIVE_HMI_VERSION[i])return false;
    return true;
}

static void reset_options(evist_native_values_t *v) {
    dpc245_settings_t defaults;
    dpc245_settings_defaults(&defaults);
    /* Copy only selectors. Accumulated values, controller mirrors and unknown
     * bytes in the HMI record remain exactly as loaded. */
    static const uint8_t option_offsets[]={0x05u,0x06u,0x07u,0x09u,0x0Au,
                                           0x0Du,0x0Eu,0x2Eu};
    for(unsigned i=0u;i<sizeof option_offsets;++i){
        uint8_t offset=option_offsets[i];
        v->hmi.raw[offset]=defaults.raw[offset];
    }
    dpc245_settings_finalize(&v->hmi);
    evist_preferences_defaults(&v->preferences);
    v->ride=evist_ride_defaults();
}

static bool decode(const uint8_t in[EVIST_NATIVE_RECORD_BYTES],
                   evist_native_values_t *v, uint32_t *sequence) {
    if(in[0]!='E'||in[1]!='V'||in[2]!='N'||in[3]!='1'||
       get16(in+4u)!=1u||get16(in+6u)!=EVIST_NATIVE_RECORD_BYTES||
       get32(in+RECORD_CRC)!=crc32(in,RECORD_CRC))return false;
    evist_native_values_t x;
    const uint8_t *q=in+RECORD_PAYLOAD;
    x.preferences.assist_level=q[0];x.preferences.lower_panel=q[1];
    x.preferences.temp_view=(evist_temp_view_t)q[2];
    x.preferences.firmware_profile=(evist_firmware_profile_t)q[3];
    x.preferences.graph_scale_w=get16(q+4u);x.preferences.battery_mask=q[6];
    x.preferences.light_mode=(evist_pref_light_mode_t)q[7];
    x.preferences.light_gain=q[8];x.preferences.light_dwell=q[9];
    x.preferences.light_threshold=get16(q+10u);x.preferences.light_level=q[12];
    x.preferences.panel_mask=get16(q+13u);x.preferences.quick_walk_enabled=q[15]!=0u;
    x.preferences.quick_walk_hold_steps=q[16];
    x.preferences.assist_mode_mask=in[ASSIST_MASK_OFFSET]==0xFFu?
        EVIST_ASSIST_MODE_MASK_ALL:in[ASSIST_MASK_OFFSET];
    x.preferences.motor_temp_limit_c=in[MOTOR_LIMIT_OFFSET]==0xFFu?0u:in[MOTOR_LIMIT_OFFSET];
    x.preferences.ctrl_temp_limit_c=in[CTRL_LIMIT_OFFSET]==0xFFu?0u:in[CTRL_LIMIT_OFFSET];
    for(unsigned i=0u;i<DPC245_SETTINGS_SIZE;++i)x.hmi.raw[i]=in[HMI_OFFSET+i];
    q=in+RIDE_OFFSET;
    x.ride.mode=(evist_ride_mode_t)q[0];x.ride.start=(evist_ride_start_t)q[1];
    x.ride.last_mode=(evist_ride_mode_t)q[2];
    x.ride.legal_x100=get16(q+3u);x.ride.offroad_x100=get16(q+5u);
    if(!values_valid(&x))return false;
    *v=x;if(sequence)*sequence=get32(in+8u);return true;
}

static int blank_page(const dpc245_persist_io_t *io,uint16_t page) {
    uint8_t b[64];
    for(unsigned part=0u;part<4u;++part){
        if(!io->read(io->ctx,page_address(page)+part*64u,b,sizeof b))return -1;
        for(unsigned i=0u;i<sizeof b;++i)if(b[i]!=0xFFu)return 0;
    }
    return 1;
}

static bool newer(uint32_t a,uint32_t b){return (int32_t)(a-b)>0;}

static bool scan_native_pages(evist_native_store_t *s,const dpc245_persist_io_t *mem){
    uint8_t magic[4],rec[EVIST_NATIVE_RECORD_BYTES];
    for(uint16_t page=0u;page<EVIST_NATIVE_PAGE_COUNT;++page){
        if(!mem->read(mem->ctx,page_address(page),magic,sizeof magic))return false;
        if(magic[0]!='E'||magic[1]!='V'||magic[2]!='N'||magic[3]!='1')continue;
        if(!mem->read(mem->ctx,page_address(page),rec,sizeof rec))return false;
        evist_native_values_t candidate;uint32_t sequence;
        if(!decode(rec,&candidate,&sequence))continue;
        if(!s->valid||newer(sequence,s->sequence)){
            if(s->valid){s->previous=s->page;s->previous_sequence=s->sequence;s->have_previous=true;}
            s->saved=candidate;s->sequence=sequence;s->page=page;s->valid=true;
            s->options_reset=!current_firmware_version(rec);
        }else if(sequence!=s->sequence&&(!s->have_previous||newer(sequence,s->previous_sequence))){
            s->previous=page;s->previous_sequence=sequence;s->have_previous=true;
        }
    }
    return true;
}

bool evist_native_store_init(evist_native_store_t *s,const evist_native_io_t *io,
                             evist_native_values_t *loaded){
    if(!s||!loaded)return false;
    evist_preferences_defaults(&s->saved.preferences);
    dpc245_settings_defaults(&s->saved.hmi);
    s->saved.ride=evist_ride_defaults();
    s->target=s->saved;s->sequence=0u;s->previous_sequence=0u;
    s->page=0u;s->previous=0u;s->candidate=0u;
    s->phase=0u;s->scan_part=0u;s->valid=false;s->have_previous=false;
    s->wait_started_ms=0u;
    s->imported=false;s->options_reset=false;s->busy=false;s->known=false;
    *loaded=s->saved;
    if(!io||!io->memory||!io->memory->read)return false;
    if(!scan_native_pages(s,io->memory))return false;
    if(s->valid){
        s->known=true;*loaded=s->saved;
        if(s->options_reset)reset_options(loaded);
        return true;
    }
    uint8_t old_header[4];
    if(!io->memory->read(io->memory->ctx,EVIST_PREF_REGION_BASE,old_header,sizeof old_header))return false;
    bool have_legacy_preferences=old_header[0]=='E'&&old_header[1]=='V'&&
                                 old_header[2]=='P'&&old_header[3]=='1';
    if(have_legacy_preferences){
        if(!evist_preferences_load(io->memory,&s->saved.preferences))return false;
    }
    bool have_hmi;
    if(!dpc245_persist_load_checked(io->memory,&s->saved.hmi,&have_hmi))return false;
    if(!evist_ride_store_load_checked(io->memory,&s->saved.ride))return false;
    evist_ride_cfg_t ride_defaults=evist_ride_defaults();
    bool have_legacy_ride=s->saved.ride.mode!=ride_defaults.mode||
                          s->saved.ride.start!=ride_defaults.start||
                          s->saved.ride.last_mode!=ride_defaults.last_mode||
                          s->saved.ride.legal_x100!=ride_defaults.legal_x100||
                          s->saved.ride.offroad_x100!=ride_defaults.offroad_x100;
    /* Prefer a fully free unit for the first image. */
    uint8_t magic[4];
    for(uint16_t page=0u;page<EVIST_NATIVE_PAGE_COUNT;++page){
        if(!io->memory->read(io->memory->ctx,page_address(page),magic,1u))return false;
        if(magic[0]!=0xFFu)continue;
        int blank=blank_page(io->memory,page);
        if(blank<0)return false;
        if(blank==1){s->candidate=page;s->imported=true;break;}
    }
    if(!s->imported){s->candidate=0u;s->imported=true;}
    s->known=true;*loaded=s->saved;
    s->options_reset=have_legacy_preferences||have_hmi||have_legacy_ride;
    if(s->options_reset)reset_options(loaded);
    return true;
}

bool evist_native_store_begin(evist_native_store_t *s,const evist_native_values_t *target){
    if(!s||!s->known||s->busy||!target)return false;
    evist_native_values_t normalized=*target;
    dpc245_settings_finalize(&normalized.hmi);
    if(!values_valid(&normalized))return false;
    s->target=normalized;s->candidate=s->valid?(uint16_t)((s->page+1u)%EVIST_NATIVE_PAGE_COUNT):s->candidate;
    s->scan_part=0u;s->phase=STEP_CHECK;s->busy=true;return true;
}

static evist_native_result_t finish(evist_native_store_t *s,evist_native_result_t result){
    s->busy=false;s->phase=0u;return result;
}

evist_native_result_t evist_native_store_step(evist_native_store_t *s,const evist_native_io_t *io,
                                               uint32_t now_ms){
    if(!s||!s->busy||!io||!io->memory||!io->memory->read||!io->memory->write||
       !io->erase_start||!io->ready)return EVIST_NATIVE_INVALID;
    const dpc245_persist_io_t *mem=io->memory;
    uint32_t addr=page_address(s->candidate);
    if(s->phase==STEP_CHECK){
        uint8_t b[64];
        if(!mem->read(mem->ctx,addr+(uint32_t)s->scan_part*64u,b,sizeof b))
            return finish(s,EVIST_NATIVE_NO_MEMORY);
        for(unsigned i=0u;i<sizeof b;++i)if(b[i]!=0xFFu){s->phase=STEP_ERASE_START;break;}
        if(s->phase==STEP_CHECK&&++s->scan_part==4u)s->phase=STEP_WRITE;
    }else if(s->phase==STEP_ERASE_START){
        if(!io->erase_start(io->ctx,addr))return finish(s,EVIST_NATIVE_ERASE_FAILED);
        s->wait_started_ms=now_ms;
        s->phase=STEP_ERASE_WAIT;
    }else if(s->phase==STEP_ERASE_WAIT){
        int ready=io->ready(io->ctx);
        if(ready<0)return finish(s,EVIST_NATIVE_ERASE_FAILED);
        if(ready==1){s->scan_part=0u;s->phase=STEP_CONFIRM;}
        else if((uint32_t)(now_ms-s->wait_started_ms)>=ERASE_WAIT_LIMIT_MS)
            return finish(s,EVIST_NATIVE_ERASE_FAILED);
    }else if(s->phase==STEP_CONFIRM){
        uint8_t b[64];
        if(!mem->read(mem->ctx,addr+(uint32_t)s->scan_part*64u,b,sizeof b))
            return finish(s,EVIST_NATIVE_NO_MEMORY);
        for(unsigned i=0u;i<sizeof b;++i)if(b[i]!=0xFFu)return finish(s,EVIST_NATIVE_ERASE_FAILED);
        if(++s->scan_part==4u)s->phase=STEP_WRITE;
    }else if(s->phase==STEP_WRITE){
        uint8_t rec[EVIST_NATIVE_RECORD_BYTES];
        encode(rec,&s->target,s->valid?s->sequence+1u:1u);
        if(!mem->write(mem->ctx,addr,rec,sizeof rec))return finish(s,EVIST_NATIVE_WRITE_FAILED);
        s->phase=STEP_VERIFY;
    }else if(s->phase==STEP_VERIFY){
        uint8_t rec[EVIST_NATIVE_RECORD_BYTES],expected[EVIST_NATIVE_RECORD_BYTES];
        encode(expected,&s->target,s->valid?s->sequence+1u:1u);
        if(!mem->read(mem->ctx,addr,rec,sizeof rec))return finish(s,EVIST_NATIVE_VERIFY_FAILED);
        for(unsigned i=0u;i<sizeof rec;++i)if(rec[i]!=expected[i])return finish(s,EVIST_NATIVE_VERIFY_FAILED);
        uint32_t seq;
        if(!decode(rec,&s->saved,&seq))return finish(s,EVIST_NATIVE_VERIFY_FAILED);
        uint16_t retired=s->previous;
        bool had_retired=s->have_previous;
        if(s->valid){s->previous=s->page;s->previous_sequence=s->sequence;s->have_previous=true;}
        s->sequence=seq;s->page=s->candidate;s->valid=true;
        s->candidate=retired;
        s->phase=had_retired?STEP_RETIRE_START:0u;
        if(!had_retired)return finish(s,EVIST_NATIVE_OK);
    }else if(s->phase==STEP_RETIRE_START){
        if(!io->erase_start(io->ctx,page_address(s->candidate)))return finish(s,EVIST_NATIVE_ERASE_FAILED);
        s->wait_started_ms=now_ms;
        s->phase=STEP_RETIRE_WAIT;
    }else if(s->phase==STEP_RETIRE_WAIT){
        int ready=io->ready(io->ctx);
        if(ready<0)return finish(s,EVIST_NATIVE_ERASE_FAILED);
        if(ready==1){s->scan_part=0u;s->phase=STEP_RETIRE_CONFIRM;}
        else if((uint32_t)(now_ms-s->wait_started_ms)>=ERASE_WAIT_LIMIT_MS)
            return finish(s,EVIST_NATIVE_ERASE_FAILED);
    }else if(s->phase==STEP_RETIRE_CONFIRM){
        uint8_t b[64];
        if(!mem->read(mem->ctx,addr+(uint32_t)s->scan_part*64u,b,sizeof b))
            return finish(s,EVIST_NATIVE_NO_MEMORY);
        for(unsigned i=0u;i<sizeof b;++i)if(b[i]!=0xFFu)return finish(s,EVIST_NATIVE_ERASE_FAILED);
        if(++s->scan_part==4u)return finish(s,EVIST_NATIVE_OK);
    }else return finish(s,EVIST_NATIVE_INVALID);
    return EVIST_NATIVE_BUSY;
}

const char *evist_native_result_text(evist_native_result_t r){
    switch(r){
    case EVIST_NATIVE_OK:return "SAVED";
    case EVIST_NATIVE_BUSY:return "SAVE IN PROGRESS";
    case EVIST_NATIVE_NO_MEMORY:return "NO MEMORY - NOT SAVED";
    case EVIST_NATIVE_ERASE_FAILED:return "ERASE ERROR - CHECK SAVE";
    case EVIST_NATIVE_WRITE_FAILED:return "WRITE ERROR - NOT SAVED";
    case EVIST_NATIVE_VERIFY_FAILED:return "VERIFY FAILED - NOT SAVED";
    default:return "INVALID - NOT SAVED";
    }
}
