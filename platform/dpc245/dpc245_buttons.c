/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_buttons.h"
#include "dpc245_board.h"
#include <stdint.h>
#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define GPIOA 0x40010800u
#define RCC_APB2ENR REG32(0x40021018u)
#define GPIO_CRL REG32(GPIOA+0x00u)
#define GPIO_IDR REG32(GPIOA+0x08u)
#define GPIO_ODR REG32(GPIOA+0x0Cu)
extern volatile uint32_t evist_ms_ticks;

/* 10 ms is long enough to reject normal switch bounce but short enough that a
 * deliberate quick tap is accepted.  More importantly, sampling is done from
 * SysTick rather than the rendering loop, so LCD traffic cannot hide clicks. */
#define DPC245_BTN_DEBOUNCE_MS 10u

static volatile uint8_t debounced_state;
static volatile uint8_t pressed_latch;
static volatile uint8_t released_latch;
static uint8_t cnt_down, cnt_up, cnt_power;
static dpc245_key_sampler_t *key_sampler;

#ifdef DPC245_BUTTONS_HOST_TEST
static uint8_t host_raw;
void dpc245_buttons_host_raw_set(uint8_t raw) { host_raw=raw; }
static uint32_t buttons_irq_save(void) { return 0u; }
static void buttons_irq_restore(uint32_t primask) { (void)primask; }
#else
static uint32_t buttons_irq_save(void) {
    uint32_t primask;
    __asm volatile ("mrs %0, primask" : "=r"(primask));
    __asm volatile ("cpsid i" ::: "memory");
    return primask;
}
static void buttons_irq_restore(uint32_t primask) {
    if((primask & 1u)==0u) __asm volatile ("cpsie i" ::: "memory");
}
static void cfg_input_pullup(unsigned pin) {
    unsigned sh=pin*4u; uint32_t v=GPIO_CRL;
    v &= ~(0xFu<<sh); v |= (0x8u<<sh); GPIO_CRL=v; /* MODE=00 CNF=10 */
    GPIO_ODR |= 1u<<pin;
}
#endif

void dpc245_buttons_init(void) {
    key_sampler=0;
#ifndef DPC245_BUTTONS_HOST_TEST
    RCC_APB2ENR |= 1u<<2;
    cfg_input_pullup(DPC245_BTN_DOWN_PIN);
    cfg_input_pullup(DPC245_BTN_UP_PIN);
    cfg_input_pullup(DPC245_BTN_POWER_PIN);
#else
    host_raw=0u;
#endif
    debounced_state=0u;
    pressed_latch=0u;
    released_latch=0u;
    cnt_down=cnt_up=cnt_power=0u;
}

uint8_t dpc245_buttons_raw(void) {
#ifdef DPC245_BUTTONS_HOST_TEST
    return host_raw;
#else
    uint32_t v=GPIO_IDR; uint8_t r=0;
    if (!(v&(1u<<DPC245_BTN_DOWN_PIN)))  r|=DPC245_KEY_DOWN;
    if (!(v&(1u<<DPC245_BTN_UP_PIN)))    r|=DPC245_KEY_UP;
    if (!(v&(1u<<DPC245_BTN_POWER_PIN))) r|=DPC245_KEY_POWER;
    return r;
#endif
}

static void sample_one(uint8_t raw, uint8_t bit, uint8_t *cnt) {
    if(raw & bit) {
        if(*cnt < DPC245_BTN_DEBOUNCE_MS) (*cnt)++;
        if(*cnt >= DPC245_BTN_DEBOUNCE_MS && !(debounced_state & bit)) {
            debounced_state |= bit;
            pressed_latch |= bit;
        }
    } else {
        if(*cnt) (*cnt)--;
        if(*cnt == 0u && (debounced_state & bit)) {
            debounced_state &= (uint8_t)~bit;
            released_latch |= bit;
        }
    }
}

void dpc245_buttons_tick_1ms(void) {
    const uint8_t raw=dpc245_buttons_raw();
    sample_one(raw,DPC245_KEY_DOWN,&cnt_down);
    sample_one(raw,DPC245_KEY_UP,&cnt_up);
    sample_one(raw,DPC245_KEY_POWER,&cnt_power);
    if(key_sampler)dpc245_key_sampler_tick(key_sampler,raw,evist_ms_ticks);
}

void dpc245_buttons_key_sampler_start(dpc245_key_sampler_t *sampler, uint32_t now_ms) {
    uint32_t primask=buttons_irq_save();
    dpc245_key_sampler_init(sampler,dpc245_buttons_raw(),now_ms);
    key_sampler=sampler;
    buttons_irq_restore(primask);
}

bool dpc245_buttons_key_event(dpc245_key_sample_event_t *event) {
    uint32_t primask=buttons_irq_save();
    bool found=dpc245_key_sampler_pop(key_sampler,event);
    buttons_irq_restore(primask);
    return found;
}

uint8_t dpc245_buttons_state(void) { return debounced_state; }

uint8_t dpc245_buttons_take_pressed(void) {
    uint32_t primask=buttons_irq_save();
    const uint8_t v=pressed_latch;
    pressed_latch=0u;
    buttons_irq_restore(primask);
    return v;
}

uint8_t dpc245_buttons_take_released(void) {
    uint32_t primask=buttons_irq_save();
    const uint8_t v=released_latch;
    released_latch=0u;
    buttons_irq_restore(primask);
    return v;
}
