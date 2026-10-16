/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "bxcan_raw.h"
#include <stdint.h>

#define CAN1_BASE 0x40006400u
#define CANF_BASE 0x40006600u

typedef struct {
    volatile uint32_t TIR, TDTR, TDLR, TDHR;
} can_tx_mailbox_t;
typedef struct {
    volatile uint32_t RIR, RDTR, RDLR, RDHR;
} can_fifo_mailbox_t;

/* Minimal offsets used by bxCAN. Uses the GD32F303 register map. */
#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define CAN_TSR   REG32(CAN1_BASE + 0x08u)
#define CAN_RF0R  REG32(CAN1_BASE + 0x0Cu)
#define CAN_TX(i) ((can_tx_mailbox_t *)(uintptr_t)(CAN1_BASE + 0x180u + 0x10u*(i)))
#define CAN_RX0   ((can_fifo_mailbox_t *)(uintptr_t)(CAN1_BASE + 0x1B0u))

void dpc245_can_filter_accept_all(void) {
    /* The acceptance filter uses mask=0 accepts all.
       Full FINIT/bank programming belongs in board bring-up after clock setup. */
    (void)CANF_BASE;
}

bool dpc245_can_receive_poll(bafang_can_frame_t *out) {
    if (!out || (CAN_RF0R & 0x3u) == 0u) return false;
    can_fifo_mailbox_t *m = CAN_RX0;
    uint32_t rir = m->RIR;
    uint32_t rdtr = m->RDTR;
    uint32_t lo = m->RDLR, hi = m->RDHR;
    if ((rir & 0x4u) == 0u) { /* extended ID only */
        CAN_RF0R |= (1u << 5);
        return false;
    }
    out->id = (rir >> 3) & 0x1FFFFFFFu;
    out->dlc = (uint8_t)(rdtr & 0xFu);
    if (out->dlc > 8u) out->dlc = 8u;
    out->data[0]=(uint8_t)lo; out->data[1]=(uint8_t)(lo>>8);
    out->data[2]=(uint8_t)(lo>>16); out->data[3]=(uint8_t)(lo>>24);
    out->data[4]=(uint8_t)hi; out->data[5]=(uint8_t)(hi>>8);
    out->data[6]=(uint8_t)(hi>>16); out->data[7]=(uint8_t)(hi>>24);
    CAN_RF0R |= (1u << 5); /* RFOM0 release */
    return true;
}

bool dpc245_can_transmit(const bafang_can_frame_t *f) {
    if (!f || f->dlc > 8u) return false;
    uint32_t tsr = CAN_TSR;
    unsigned mb;
    if (tsr & (1u << 26)) mb = 0;
    else if (tsr & (1u << 27)) mb = 1;
    else if (tsr & (1u << 28)) mb = 2;
    else return false;
    can_tx_mailbox_t *m = CAN_TX(mb);
    uint32_t lo = (uint32_t)f->data[0] | ((uint32_t)f->data[1]<<8) |
                  ((uint32_t)f->data[2]<<16) | ((uint32_t)f->data[3]<<24);
    uint32_t hi = (uint32_t)f->data[4] | ((uint32_t)f->data[5]<<8) |
                  ((uint32_t)f->data[6]<<16) | ((uint32_t)f->data[7]<<24);
    m->TDTR = f->dlc;
    m->TDLR = lo;
    m->TDHR = hi;
    m->TIR = ((f->id & 0x1FFFFFFFu) << 3) | 0x4u | 0x1u; /* EXID + IDE + TXRQ */
    return true;
}


bool dpc245_can_tx_idle(void) {
    return (CAN_TSR & ((1u << 26) | (1u << 27) | (1u << 28))) ==
           ((1u << 26) | (1u << 27) | (1u << 28));
}
