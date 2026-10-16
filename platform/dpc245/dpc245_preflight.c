/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_preflight.h"
#include "bxcan_raw.h"
#include "dpc245_ble_proto.h"
#include "dpc245_ble_uart.h"
#include "dpc245_buttons.h"
#include "dpc245_can_init.h"
#include "dpc245_recovery.h"
#include "dpc245_recovery_trigger.h"
#include "dpc245_update.h"

/* Upper bound for the BLE-path coordination frame to leave the mailbox. The
 * mailbox must drain before update entry. */
#define PREFLIGHT_TX_DRAIN_SPIN 600000u

static dpc245_ble_parser_t s_parser;
static dpc245_ble_frame_t s_frame;
static dpc245_recovery_chord_t s_chord;

static void handoff_after_ble_request(void) {
    /* BLE entry broadcasts 0x13FF3005 # 00 and drains CAN TX
     * before invalidating the application marker. */
    bafang_can_frame_t coordination;
    dpc245_update_t update;
    dpc245_update_init(&update);
    if (dpc245_update_request(&update) && dpc245_update_next_can(&update, &coordination)) {
        if (dpc245_can_transmit(&coordination)) {
            for (uint32_t n = 0u; n < PREFLIGHT_TX_DRAIN_SPIN && !dpc245_can_tx_idle(); ++n) {}
        }
    }
    dpc245_enter_resident_bootloader();
}

void dpc245_preflight_begin(dpc245_preflight_clock_t clock, dpc245_preflight_stats_t *stats) {
    dpc245_ble_parser_init(&s_parser);
    dpc245_recovery_chord_init(&s_chord);
    bool can_ok;
    if (clock == DPC245_PREFLIGHT_CLOCK_HSI) {
        can_ok = dpc245_can_init_250k_hsi();
        dpc245_ble_uart_init_hsi();
    } else {
        can_ok = dpc245_can_init_250k();
        dpc245_ble_uart_init();
    }
    if (stats) {
        stats->can_ok = can_ok;
        stats->can_frames = 0u;
        stats->ble_bytes = 0u;
        stats->ble_frames = 0u;
    }
}

void dpc245_preflight_poll(uint32_t now_ms, dpc245_preflight_stats_t *stats) {
    bafang_can_frame_t f;
    while (dpc245_can_receive_poll(&f)) {
        if (stats) ++stats->can_frames;
        /* CAN entry sends no reply before invalidating the marker. */
        if (dpc245_is_can_update_request(&f)) dpc245_enter_resident_bootloader();
    }

    dpc245_ble_uart_poll();
    uint8_t byte;
    while (dpc245_ble_uart_read_byte(&byte)) {
        if (stats) ++stats->ble_bytes;
        if (dpc245_ble_parser_feed(&s_parser, byte, &s_frame)) {
            if (stats) ++stats->ble_frames;
            if (dpc245_is_ble_update_request(&s_frame)) handoff_after_ble_request();
        }
    }

    if (dpc245_recovery_chord_update(&s_chord, dpc245_buttons_raw(), now_ms))
        dpc245_enter_resident_bootloader();
}

void dpc245_preflight_run(uint32_t window_ms, dpc245_millis_fn millis,
                          dpc245_preflight_stats_t *stats) {
    const uint32_t start = millis();
    for (;;) {
        const uint32_t now = millis();
        if (window_ms != DPC245_PREFLIGHT_FOREVER && (uint32_t)(now - start) >= window_ms) return;
        dpc245_preflight_poll(now, stats);
    }
}
