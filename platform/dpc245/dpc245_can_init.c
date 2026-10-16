/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dpc245_can_init.h"
#include <stdint.h>
#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define RCC_APB2ENR REG32(0x40021018u)
#define RCC_APB1ENR REG32(0x4002101Cu)
#define RCC_APB1RSTR REG32(0x40021010u)
#define AFIO_MAPR REG32(0x40010004u)
#define GPIOA_CRH REG32(0x40010804u)
#define CAN_MCR REG32(0x40006400u)
#define CAN_MSR REG32(0x40006404u)
#define CAN_BTR REG32(0x4000641Cu)
#define CAN_FMR REG32(0x40006600u)
#define CAN_FM1R REG32(0x40006604u)
#define CAN_FS1R REG32(0x4000660Cu)
#define CAN_FFA1R REG32(0x40006614u)
#define CAN_FA1R REG32(0x4000661Cu)
#define CAN_F0R1 REG32(0x40006640u)
#define CAN_F0R2 REG32(0x40006644u)
static bool wait_bit(volatile uint32_t *r,uint32_t mask,bool set) {
    for (uint32_t n=0;n<1000000u;n++) if (((*r & mask)!=0u)==set) return true;
    return false;
}
/* 250 kbit/s bit timings.
 * 60 MHz APB1 (normal): prescaler 24, 1+5+4 tq, SJW 1 (for the 250 kbit/s profile).
 * 8 MHz APB1 (HSI safe mode after an oscillator failure): prescaler 2,
 * 1+9+6 tq, SJW 4 so the less accurate internal oscillator can resynchronise. */
#define CAN_BTR_250K_APB1_8MHZ 0x03580001u

static bool can_init_btr(uint32_t btr) {
    RCC_APB2ENR |= (1u<<0)|(1u<<2); /* AFIO, GPIOA */
    RCC_APB1ENR |= 1u<<25;          /* CAN1 */
    RCC_APB1RSTR |= 1u<<25; RCC_APB1RSTR &= ~(1u<<25);
    AFIO_MAPR &= ~(3u<<13);         /* CAN_REMAP=00: PA11 RX / PA12 TX */
    uint32_t crh=GPIOA_CRH;
    crh &= ~((0xFu<<12)|(0xFu<<16));
    crh |=  (0x4u<<12) | (0xBu<<16); /* PA11 floating input, PA12 AF PP 50MHz */
    GPIOA_CRH=crh;
    CAN_MCR = 1u; /* INRQ */
    if (!wait_bit((volatile uint32_t *)(uintptr_t)0x40006404u,1u,true)) return false;
    CAN_BTR = btr;
    CAN_MCR |= 1u<<6; /* ABOM */
    CAN_FMR |= 1u;
    CAN_FA1R &= ~1u;
    CAN_FM1R &= ~1u; /* mask mode */
    CAN_FS1R |= 1u;  /* 32 bit */
    CAN_FFA1R &= ~1u; /* FIFO0 */
    CAN_F0R1=0u; CAN_F0R2=0u; /* ID=0, MASK=0 -> accept all */
    CAN_FA1R |= 1u;
    CAN_FMR &= ~1u;
    CAN_MCR &= ~1u;
    return wait_bit((volatile uint32_t *)(uintptr_t)0x40006404u,1u,false);
}

bool dpc245_can_init_250k(void) {
    return can_init_btr(0x00340017u);
}

bool dpc245_can_init_250k_hsi(void) {
    return can_init_btr(CAN_BTR_250K_APB1_8MHZ);
}
