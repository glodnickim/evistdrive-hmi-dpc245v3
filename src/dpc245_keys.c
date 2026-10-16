/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_keys.h"

uint8_t dpc245_raw_key_code(uint8_t raw_mask) {
    const uint8_t down = DPC245_KEYS_RAW_DOWN;
    const uint8_t up = DPC245_KEYS_RAW_UP;
    const uint8_t power = DPC245_KEYS_RAW_POWER;
    if ((raw_mask & (uint8_t)(up | down)) == (uint8_t)(up | down)) return 3u;
    if ((raw_mask & (uint8_t)(power | down)) == (uint8_t)(power | down)) return 8u;
    if ((raw_mask & up) != 0u) return 2u;
    if ((raw_mask & down) != 0u) return 1u;
    if ((raw_mask & power) != 0u) return 4u;
    return 0u;
}

uint8_t dpc245_translate_appkey(uint8_t raw_key_code,
                               uint8_t event_class,
                               bool walk_armed,
                               bool chord_allowed) {
    if (event_class == 1u) {
        if (raw_key_code == 1u) return DPC245_APPKEY_DOWN_SHORT;
        if (raw_key_code == 2u) return DPC245_APPKEY_UP_SHORT;
        if (raw_key_code == 4u) return DPC245_APPKEY_POWER;
        return DPC245_APPKEY_NONE;
    }
    if (event_class == 0u) {
        if (raw_key_code == 1u)
            return walk_armed ? DPC245_APPKEY_DOWN_HOLD : DPC245_APPKEY_NONE;
        if (raw_key_code == 2u) return DPC245_APPKEY_UP_HOLD;
        if (raw_key_code == 3u)
            return chord_allowed ? DPC245_APPKEY_CHORD : DPC245_APPKEY_NONE;
        if (raw_key_code == 8u) return DPC245_APPKEY_POWER_DOWN_COMBO;
        return DPC245_APPKEY_NONE;
    }
    if (event_class == 3u && raw_key_code == 5u)
        return DPC245_APPKEY_VERY_LONG;
    return DPC245_APPKEY_NONE;
}

static uint32_t elapsed(uint32_t now, uint32_t then) {
    return (uint32_t)(now - then);
}

static uint32_t released_event(uint8_t code, uint32_t held_ms, bool suppress_short) {
    if (suppress_short || held_ms < DPC245_KEY_SHORT_MIN_MS || held_ms >= DPC245_KEY_HOLD_MS)
        return 0u;
    if (code == 1u) return DPC245_KEY_EVENT_DOWN_SHORT;
    if (code == 2u) return DPC245_KEY_EVENT_UP_SHORT;
    if (code == 4u) return DPC245_KEY_EVENT_POWER_SHORT;
    return 0u;
}

void dpc245_keys_init(dpc245_keys_t *k, uint8_t raw, uint32_t now_ms) {
    if (!k) return;
    k->sampled_raw = raw;
    k->candidate_raw = raw;
    k->active_code = dpc245_raw_key_code(raw);
    k->last_sample_ms = now_ms;
    k->candidate_since_ms = now_ms;
    k->pressed_ms = now_ms;
    k->chord_started_ms = now_ms;
    k->repeat_deadline_ms = now_ms + 400u;
    k->hold_sent = false;
    k->repeat_fast = false;
    k->chord_sent = false;
    k->suppress_short = false;
}

uint32_t dpc245_keys_update(dpc245_keys_t *k, uint8_t raw, uint32_t now_ms) {
    if (!k) return 0u;
    if (elapsed(now_ms, k->last_sample_ms) < DPC245_KEY_SAMPLE_MS) return 0u;
    k->last_sample_ms = now_ms;

    if (raw != k->candidate_raw) {
        k->candidate_raw = raw;
        k->candidate_since_ms = now_ms;
    }
    if (k->candidate_raw != k->sampled_raw &&
        elapsed(now_ms, k->candidate_since_ms) < DPC245_KEY_DEBOUNCE_MS)
        raw = k->sampled_raw;
    else
        raw = k->candidate_raw;

    uint32_t ev = 0u;
    const uint8_t new_code = dpc245_raw_key_code(raw);
    const uint8_t old_code = k->active_code;

    if (new_code != old_code) {
        if (old_code != 0u) {
            const uint32_t held_ms = elapsed(now_ms, k->pressed_ms);
            ev |= released_event(old_code, held_ms, k->suppress_short || k->hold_sent);
            if (old_code == 1u) ev |= DPC245_KEY_EVENT_DOWN_RELEASE;
        }

        k->active_code = new_code;
        k->pressed_ms = now_ms;
        k->repeat_deadline_ms = now_ms + 400u;
        k->hold_sent = false;
        k->repeat_fast = false;
        k->suppress_short = false;

        if (new_code == 1u) ev |= DPC245_KEY_EVENT_DOWN_PRESS;
        if (new_code == 2u) ev |= DPC245_KEY_EVENT_UP_PRESS;
        if (new_code == 3u) {
            k->chord_started_ms = now_ms;
            k->chord_sent = false;
        } else {
            k->chord_sent = false;
        }
    }
    k->sampled_raw = raw;

    if (k->active_code == 3u && !k->chord_sent &&
        elapsed(now_ms, k->chord_started_ms) >= DPC245_SETTINGS_CHORD_MS) {
        ev |= DPC245_KEY_EVENT_SETTINGS_CHORD;
        k->chord_sent = true;
        k->suppress_short = true;
    }

    if (k->active_code != 0u && !k->hold_sent &&
        elapsed(now_ms, k->pressed_ms) >= DPC245_KEY_HOLD_MS) {
        if (k->active_code == 2u) ev |= DPC245_KEY_EVENT_UP_HOLD;
        else if (k->active_code == 1u) ev |= DPC245_KEY_EVENT_DOWN_HOLD;
        else if (k->active_code == 8u) ev |= DPC245_KEY_EVENT_POWER_DOWN_HOLD;
        /* Standalone POWER has deliberately no application held action. */
        k->hold_sent = true;
        k->suppress_short = true;
    }

    /* Repeat is an input hint for value editors only; the application decides
     * whether the current screen is in edit mode. Stop while a release is
     * being debounced so no extra step follows the physical release. */
    if ((k->active_code == 1u || k->active_code == 2u) && !k->repeat_fast &&
        elapsed(now_ms, k->pressed_ms) >= 2000u &&
        (int32_t)(k->repeat_deadline_ms - (k->pressed_ms + 2000u)) > 0) {
        k->repeat_deadline_ms = k->pressed_ms + 2000u;
        k->repeat_fast = true;
    }

    if ((k->active_code == 1u || k->active_code == 2u) &&
        k->candidate_raw == k->sampled_raw &&
        (int32_t)(now_ms - k->repeat_deadline_ms) >= 0) {
        bool fast = k->repeat_fast;
        if (k->active_code == 2u)
            ev |= fast ? DPC245_KEY_EVENT_UP_REPEAT_FAST : DPC245_KEY_EVENT_UP_REPEAT;
        else
            ev |= fast ? DPC245_KEY_EVENT_DOWN_REPEAT_FAST : DPC245_KEY_EVENT_DOWN_REPEAT;
        k->repeat_deadline_ms = now_ms + (fast ? 40u : 120u);
    }

    return ev;
}
