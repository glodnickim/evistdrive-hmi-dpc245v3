/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_key_sampler.h"

#define REPEAT_EVENTS (DPC245_KEY_EVENT_UP_REPEAT|DPC245_KEY_EVENT_DOWN_REPEAT| \
                       DPC245_KEY_EVENT_UP_REPEAT_FAST|DPC245_KEY_EVENT_DOWN_REPEAT_FAST)

uint32_t dpc245_key_time_for_pass(uint32_t sample_ms, uint32_t pass_ms) {
    return (int32_t)(sample_ms-pass_ms)>0?pass_ms:sample_ms;
}

void dpc245_key_sampler_init(dpc245_key_sampler_t *s, uint8_t raw, uint32_t now_ms) {
    if (!s) return;
    dpc245_keys_init(&s->keys, raw, now_ms);
    s->read = s->write = 0u;
    s->overflow = false;
    s->navigation_suppressed = false;
    s->pending_down_release = false;
    s->pending_release_time_ms = now_ms;
}

void dpc245_key_sampler_tick(dpc245_key_sampler_t *s, uint8_t raw, uint32_t now_ms) {
    if (!s) return;
    uint32_t events = dpc245_keys_update(&s->keys, raw, now_ms);
    if (!events) return;
    /* Once full, keep the queued order and only retain the Walk safety release.
     * Resume new gestures after the queue drains and all keys are released. */
    if(s->navigation_suppressed){
        if(events&DPC245_KEY_EVENT_DOWN_RELEASE){
            s->pending_down_release=true;
            s->pending_release_time_ms=now_ms;
        }
        return;
    }
    uint8_t used=(uint8_t)((s->write+DPC245_KEY_SAMPLER_CAPACITY-s->read)%DPC245_KEY_SAMPLER_CAPACITY);
    if(used>=DPC245_KEY_SAMPLER_CAPACITY-8u){
        if(events&REPEAT_EVENTS)s->overflow=true;
        events &= ~REPEAT_EVENTS;
        if(!events)return;
    }
    uint8_t next = (uint8_t)((s->write + 1u) % DPC245_KEY_SAMPLER_CAPACITY);
    if (next == s->read) {
        s->overflow = true;
        s->navigation_suppressed=true;
        if(events&DPC245_KEY_EVENT_DOWN_RELEASE){
            s->pending_down_release=true;
            s->pending_release_time_ms=now_ms;
        }
        return;
    }
    s->queue[s->write] = (dpc245_key_sample_event_t){events, now_ms};
    s->write = next;
}

bool dpc245_key_sampler_pop(dpc245_key_sampler_t *s, dpc245_key_sample_event_t *event) {
    if (!s || !event) return false;
    if(s->read != s->write){
        *event = s->queue[s->read];
        s->read = (uint8_t)((s->read + 1u) % DPC245_KEY_SAMPLER_CAPACITY);
        return true;
    }
    if(s->pending_down_release){
        *event=(dpc245_key_sample_event_t){DPC245_KEY_EVENT_DOWN_RELEASE,s->pending_release_time_ms};
        s->pending_down_release=false;
        return true;
    }
    if(s->navigation_suppressed && s->keys.sampled_raw==0u &&
       s->keys.candidate_raw==0u)s->navigation_suppressed=false;
    return false;
}
