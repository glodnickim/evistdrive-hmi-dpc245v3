/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Persistence I/O and error regression. */
#define main canonical_config_main
#include "test_config.c"
#undef main
#include "dpc245_ble_app.h"
#include "dpc245_keys.h"
#include "dpc245_tx.h"
#include "dpc245_v3_profile.h"
#include "dpc245_spi_mem.h"
#include "dpc245_clock.h"

static bool async_failed_program(void *ctx,uint32_t a,const uint8_t *data,size_t n) {
    (void)ctx;(void)a;(void)data;++f_writes;f_write_bytes+=n;return false;
}
static size_t worst_step;
static unsigned steps;

/* Explicit pessimistic elapsed MODEL, not a measured target WCET.
 * Every step charges the ENTIRE shared flag/status allowance, including
 * successful polls: 64 CPU cycles/check, 128 cycles/status bookkeeping,
 * 100000 cycles of other store bookkeeping, all status-inclusive wire time,
 * plus 1 ms of harness service work. At the configured 120 MHz clock.
 * No register/bus/interrupt latency is proven by those modeling assumptions. */
#define MODEL_FLAG_CYCLES 64u
#define MODEL_STATUS_CYCLES 128u
#define MODEL_STORE_CYCLES 100000u
#define MODEL_SERVICE_GAP_MS 80u
static uint32_t model_now, last_service, max_service_gap;
static uint32_t max_modeled_step_us;
static unsigned service_calls, ble_replies, key_releases, heartbeats, can_errors;
static uint32_t last_heartbeat;
static bool starve_control;
static evist_ctrl_txn_t service_txn;
static dpc245_app_t service_app;
static evist_can_router_t service_router;
static dpc245_ble_app_t service_ble;
static dpc245_hmi_identity_records_t service_identity;
static dpc245_update_t service_update;
static dpc245_keys_t service_keys;
static dpc245_v3_tx_schedule_t service_schedule;

static uint32_t model_step_us(size_t status_inclusive_bytes) {
    uint64_t cycles=(uint64_t)DPC245_SPI_STEP_FLAG_POLLS*MODEL_FLAG_CYCLES
        +(uint64_t)DPC245_SPI_STEP_STATUS_POLLS*MODEL_STATUS_CYCLES+MODEL_STORE_CYCLES;
    uint64_t cpu_us=(cycles*1000000u+DPC245_SYSCLK_HZ-1u)/DPC245_SYSCLK_HZ;
    uint64_t wire_us=((uint64_t)status_inclusive_bytes*8000000u+937499u)/937500u;
    uint32_t us=(uint32_t)(cpu_us+wire_us+1000u);
    if(us>max_modeled_step_us)max_modeled_step_us=us;
    return us;
}

/* Real host service modules used by foreground main; IO boundaries are
 * modeled. A separate preprocessed-main guard ties these calls to main's
 * one-step integration and detects synchronous batching/removing services. */
static void service_foreground(void) {
    uint32_t gap=model_now-last_service;
    if(gap>max_service_gap)max_service_gap=gap;
    check(gap<=MODEL_SERVICE_GAP_MS,"service: CAN/BLE/keys/heartbeat opportunity within declared elapsed model");
    last_service=model_now;++service_calls;
    bafang_can_frame_t tx;
    (void)pump(&service_txn,model_now,&tx);
    evist_txn_kind_t kind;evist_txn_result_t result;uint8_t data[64];
    if(evist_ctrl_txn_take_result(&service_txn,&kind,&result,data)){
        check(result!=EVIST_TXN_OK,"service: real CAN transaction timeout surfaces");
        ++can_errors;
    }
    uint8_t wire[DPC245_BLE_MAX_WIRE];
    size_t n=dpc245_ble_frame_build(wire,sizeof wire,0x11u,0x10u,1u,0x59u,NULL,0u);
    dpc245_ble_parser_t parser;dpc245_ble_parser_init(&parser);
    dpc245_ble_frame_t frame;dpc245_ble_app_result_t reply;
    bool handled=false;
    for(size_t i=0u;i<n;++i)if(dpc245_ble_parser_feed(&parser,wire[i],&frame))
        handled=dpc245_ble_app_handle(&service_ble,&frame,&service_app,&service_router,
            &service_identity,false,&service_update,&reply);
    check(handled&&reply.have_tx&&reply.tx_len==10u,"service: real BLE parser/dispatcher produces read reply");
    if(handled&&reply.have_tx)++ble_replies;
    uint8_t raw=(model_now%1000u)<400u?DPC245_KEYS_RAW_DOWN:0u;
    uint32_t keys=dpc245_keys_update(&service_keys,raw,model_now);
    if(keys&DPC245_KEY_EVENT_DOWN_RELEASE)++key_releases;
    dpc245_app_tick(&service_app,model_now);
    uint32_t due=dpc245_v3_tx_schedule_poll(&service_schedule,model_now);
    for(size_t i=0u;i<dpc245_v3_periodic_can_count;++i)
        if((due&(1u<<i))!=0u&&dpc245_v3_periodic_can[i].can_id==0x03106300u){
            check(dpc245_build_periodic_frame(0x03106300u,&service_app,false,0u,1u,&tx),
                "service: real heartbeat scheduler/frame serializer");
            if(heartbeats!=0u)check(model_now-last_heartbeat<=132u+2u*MODEL_SERVICE_GAP_MS,
                "service: heartbeat emission model accounts for scheduler quantization");
            last_heartbeat=model_now;++heartbeats;
        }
}
static void service_init(void) {
    dpc245_settings_t settings;dpc245_settings_defaults(&settings);
    dpc245_app_init(&service_app,&settings);
    evist_can_router_init(&service_router);dpc245_ble_app_init(&service_ble);
    dpc245_hmi_identity_records_init(&service_identity);dpc245_update_init(&service_update);
    dpc245_keys_init(&service_keys,0u,0u);dpc245_v3_tx_schedule_init(&service_schedule,0u);
    evist_ctrl_txn_init(&service_txn);
    bafang_can_frame_t tx;
    check(evist_ctrl_txn_read_wheel(&service_txn,0u)&&pump(&service_txn,0u,&tx),"service: initial real CAN READ");
    service_foreground();
}
static void service_after_step(size_t bound) {
    model_now+=(model_step_us(bound)+999u)/1000u;
    if(!starve_control)service_foreground();
}

static evist_pref_save_result_t run_job(evist_pref_store_t *s, const dpc245_persist_io_t *io) {
    evist_pref_save_result_t result = EVIST_PREF_SAVE_IN_PROGRESS;
    unsigned iteration = 0u;
    while (evist_pref_store_busy(s) && iteration++ < 2048u) {
        size_t bytes0 = f_spi_bytes();
        unsigned reads0 = f_reads, writes0 = f_writes;
        result = evist_pref_store_save_step(s, io);
        /* Extra RDSR before every read/program, beyond the normal wire sequence.
         * Add the entire shared 256-poll allowance as a conservative bound.
         * This is wire workload, not CPU or whole-application timing proof. */
        size_t bytes = f_spi_bytes() - bytes0 + 2u * (f_reads - reads0) + 2u * (f_writes - writes0) + 512u;
        if (bytes > worst_step) worst_step = bytes;
        check(bytes <= SAVE_SPI_BUDGET, "async: shared-status-inclusive wire bound per service step");
        check(bytes * 8u <= 93750u, "async: nominal wire gap below 100ms heartbeat");
        ++steps; service_after_step(bytes);
    }
    if(starve_control)service_foreground();
    check(!evist_pref_store_busy(s), "async: finite job completes within scan/record bound");
    return result;
}

static void test_async_fault_relearn(void) {
    dpc245_persist_io_t io = {f_read, f_write, NULL, NULL};
    evist_pref_store_t s;
    evist_preferences_t loaded, after, reboot;
    mem_reset(true);
    for (uint32_t a = EVIST_PREF_SET_BASE; a < EVIST_PREF_SET_END; a += 2u) put_code(a, 0x09u);
    f_reset(); evist_pref_store_init(&s, &io, &loaded);
    unsigned load_reads = f_reads;
    after = loaded; after.assist_level = 1u;
    s.known = false;
    f_reset(); f_fail_at = load_reads + 1u; /* the first precheck read fails */
    check(evist_pref_store_begin(&s, &after), "async: explicit request accepted");
    unsigned before = steps;
    evist_pref_save_result_t result = run_job(&s, &io);
    printf("async failure: result=%s reads=%u expected=%u steps=%u known=%u\n",
        evist_pref_save_text(result), f_reads, load_reads + 1u, steps - before, (unsigned)s.known);
    check(f_reads == load_reads + 1u, "async: relearn then first precheck read fails, no second scan");
    check(result == EVIST_PREF_SAVE_NO_MEMORY && !s.known && f_writes == 0u, "async: fault never SAVED, snapshot UNKNOWN");
    f_reset();
    check(evist_pref_store_begin(&s, &after), "async: explicit retry accepted");
    check(run_job(&s, &io) == EVIST_PREF_SAVE_OK, "async: IO available again saves");
    check(evist_preferences_load(&io, &reboot) && same_prefs(&reboot, &after), "async: reboot matches saved target");
}

static void test_async_idle_target_and_maximum(void) {
    dpc245_persist_io_t io = {f_read, f_write, NULL, NULL};
    evist_pref_store_t s;
    evist_preferences_t p, a, b, reboot;
    mem_reset(false); f_reset(); evist_pref_store_init(&s, &io, &p);
    f_reset();
    check(evist_pref_store_save_step(&s, &io) == EVIST_PREF_SAVE_INVALID && f_reads == 0u && f_writes == 0u,
          "idle: no unsolicited IO or SAVED");
    a=p; a.assist_level=9u; a.lower_panel=8u; a.firmware_profile=EVIST_PROFILE_FAKE_TAXI;
    a.graph_scale_w=3000u; a.battery_mask=EVIST_BATTERY_ICON; a.light_mode=EVIST_PREF_LIGHT_MANUAL;
    a.light_gain=9u; a.light_dwell=30u; a.light_threshold=5000u; a.light_level=15u;
    a.panel_mask=1u; a.quick_walk_enabled=true; a.quick_walk_hold_steps=10u;
    b=p; b.light_gain=6u;
    evist_preferences_t wanted=a;
    check(evist_pref_store_begin(&s, &a), "maximum: explicit request accepted");
    a.light_gain=1u; /* caller's buffer no longer owns the target */
    check(!evist_pref_store_begin(&s, &b), "active target refuses overwrite by another begin");
    check(evist_pref_store_save(&s, &io, &b) == EVIST_PREF_SAVE_IN_PROGRESS, "sync helper cannot overwrite active target");
    check(run_job(&s, &io) == EVIST_PREF_SAVE_OK, "maximum: bounded steps finish full edit");
    check(evist_preferences_load(&io, &reboot) && same_prefs(&reboot, &wanted), "maximum: immutable target is what reboot loads");
    check(evist_pref_store_begin(&s, &b) && run_job(&s, &io) == EVIST_PREF_SAVE_OK, "later target saves after active job");
    check(evist_preferences_load(&io, &reboot) && same_prefs(&reboot, &b), "later target survives reboot");
    b.light_gain=0u; f_reset();
    check(!evist_pref_store_begin(&s, &b) && !evist_pref_store_busy(&s) && f_writes==0u, "invalid target rejected without IO");
}

static void test_async_blank_and_persistent_error(void) {
    dpc245_persist_io_t io = {f_read, f_write, NULL, NULL};
    evist_pref_store_t s;
    evist_preferences_t p, after, reboot;
    mem_reset(false); f_reset(); evist_pref_store_init(&s, &io, &p);
    s.known=false; after=p; after.light_gain=6u;
    f_reset(); f_fail_from=1u;
    check(evist_pref_store_begin(&s, &after) && run_job(&s, &io)==EVIST_PREF_SAVE_NO_MEMORY,
          "persistent read error: fails honestly and completes");
    check(!s.known && f_writes==0u, "persistent read error: UNKNOWN, no program");
    f_reset();
    check(evist_pref_store_begin(&s, &after) && run_job(&s, &io)==EVIST_PREF_SAVE_OK,
          "blank unknown: entire erased region proved one chunk per step then saves");
    check(evist_preferences_load(&io, &reboot) && same_prefs(&reboot, &after), "blank retry: reboot matches");
    f_reset(); after.light_gain=7u; io.write=async_failed_program;
    check(evist_pref_store_begin(&s, &after) && run_job(&s, &io)==EVIST_PREF_SAVE_WRITE_FAILED,
          "failed program never becomes SAVED");
    check(!s.known, "failed program invalidates snapshot");
}

static void test_async_full_pair_and_can_deadline(void) {
    dpc245_persist_io_t io={f_read,f_write,NULL,NULL};
    evist_pref_store_t s; evist_preferences_t p,after,reboot;
    mem_reset(true);
    uint32_t count=(EVIST_PREF_RIDE_BASE-EVIST_PREF_PAIR_BASE)/2u;
    for(uint32_t i=0u;i<count;++i)put_pair(i,0u,0u);
    f_reset();evist_pref_store_init(&s,&io,&p);s.known=false;after=p;after.assist_level=1u;
    f_reset();check(evist_pref_store_begin(&s,&after)&&run_job(&s,&io)==EVIST_PREF_SAVE_LOG_FULL,
        "full pair journal: bounded recovery, LOG_FULL without writing RIDE");
    check(f_writes==0u&&s.known&&evist_preferences_load(&io,&reboot)&&same_prefs(&p,&reboot),
        "full pair journal: snapshot exact, no hidden write");

    /* A real controller transaction keeps ticking during worst-bound modeled
     * blank-region recovery. No reply is delivered: timeout must surface while
     * persistence is still busy, rather than waiting for its full scan. */
    mem_reset(false);f_reset();evist_pref_store_init(&s,&io,&p);s.known=false;after=p;after.light_gain=6u;
    check(evist_pref_store_begin(&s,&after),"concurrent service: begin blank recovery");
    evist_ctrl_txn_t txn;evist_ctrl_txn_init(&txn);bafang_can_frame_t tx;
    check(evist_ctrl_txn_read_wheel(&txn,0u)&&pump(&txn,0u,&tx),"concurrent service: wheel READ sent");
    uint32_t now=0u;bool timeout_while_busy=false;
    unsigned service_iterations=0u;
    while(evist_pref_store_busy(&s) && service_iterations++<2048u){
        (void)pump(&txn,now,&tx);
        evist_txn_kind_t kind;evist_txn_result_t result;uint8_t data[64];
        if(evist_ctrl_txn_take_result(&txn,&kind,&result,data)){
            timeout_while_busy=true;
            check(result!=EVIST_TXN_OK,"concurrent service: missing controller response is an error");
        }
        ++steps;
        size_t before=f_spi_bytes();unsigned reads=f_reads,writes=f_writes;
        (void)evist_pref_store_save_step(&s,&io);
        size_t bound=f_spi_bytes()-before+2u*(f_reads-reads)+2u*(f_writes-writes)+512u;
        check(bound<=SAVE_SPI_BUDGET,"concurrent service: per-step wire bound");
        service_after_step(bound);
        now+=(model_step_us(bound)+999u)/1000u;
    }
    if(starve_control)service_foreground();
    check(!evist_pref_store_busy(&s),"concurrent service: finite persistence job");
    check(timeout_while_busy,"controller deadline processed before long persistence job finishes");
    printf("concurrent CAN timeout serviced during persistence; modeled job duration=%u ms\n",now);
}

int main(int argc,char **argv) {
    starve_control=argc==2&&strcmp(argv[1],"--starve")==0;
    service_init();
    test_async_fault_relearn();
    test_async_idle_target_and_maximum();
    test_async_blank_and_persistent_error();
    test_async_full_pair_and_can_deadline();
    check(ble_replies==service_calls&&key_releases>0u&&heartbeats>0u&&can_errors>0u,
        "real service outcomes seen during persistence, including CAN timeout");
    printf("MODEL configured CPU=%u Hz flag_polls=%u status_polls=%u flag_cycles=%u status_cycles=%u store_cycles=%u\n",
        DPC245_SYSCLK_HZ,DPC245_SPI_STEP_FLAG_POLLS,DPC245_SPI_STEP_STATUS_POLLS,MODEL_FLAG_CYCLES,MODEL_STATUS_CYCLES,MODEL_STORE_CYCLES);
    printf("MODEL max_step=%u us max_service_gap=%u ms budget=%u ms BLE_replies=%u keys_releases=%u heartbeats=%u CAN_errors=%u\n",
        max_modeled_step_us,max_service_gap,MODEL_SERVICE_GAP_MS,ble_replies,key_releases,heartbeats,can_errors);
    printf("async steps=%u worst_status_inclusive_bound=%zu bytes nominal_wire_gap=%.3f ms failures=%u\n",
        steps,worst_step,(double)worst_step*8.0/937.5,failures);
    puts("SCOPE: real host services + declared CPU/status/wire elapsed model; physical CPU/SPI/full-main timing and HW NOT_RUN");
    return failures?1:0;
}
