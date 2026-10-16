/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "st7789_8080.h"
#include "dpc245_board.h"
#include "app_config.h"
#include "ui_strip.h"
/* Minimal-redraw strip drawing exists only in the product image; PROBE and
 * RECOVERY_FAULT keep writing straight to the panel, unchanged. */
#if EVIST_BUILD_PROFILE == EVIST_PROFILE_PRODUCT
#define LCD_TO_STRIP() ui_strip_capturing()
#else
#define LCD_TO_STRIP() false
#endif
#include <stdint.h>

#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define GPIO_CRL(base)  REG32((base)+0x00u)
#define GPIO_CRH(base)  REG32((base)+0x04u)
#define GPIO_IDR(base)  REG32((base)+0x08u)
#define GPIO_ODR(base)  REG32((base)+0x0Cu)
#define GPIO_BSRR(base) REG32((base)+0x10u)
#define GPIO_BRR(base)  REG32((base)+0x14u)
#define RCC_APB2ENR     REG32(0x40021018u)

#define GPIOA DPC245_LCD_CTRL_PORT_BASE
#define GPIOB DPC245_LCD_DATA_PORT_BASE

static inline void pin_set(uint32_t base, unsigned pin) { GPIO_BSRR(base) = 1u << pin; }
static inline void pin_clr(uint32_t base, unsigned pin) { GPIO_BRR(base) = 1u << pin; }

static void gpio_cfg_output_pp_50mhz(uint32_t base, unsigned pin) {
    volatile uint32_t *cr = (volatile uint32_t *)(uintptr_t)(base + (pin < 8u ? 0u : 4u));
    unsigned shift = (pin & 7u) * 4u;
    uint32_t v = *cr;
    v &= ~(0xFu << shift);
    v |=  (0x3u << shift); /* MODE=11 50MHz, CNF=00 push-pull */
    *cr = v;
}

void dpc245_lcd_bus_init(void) {
    /* Release JTAG-DP but retain SW-DP before using PB3/PB4 as
     * LCD D3/D4. Without this remap a clean reset can leave two data bits
     * owned by the debug port and the panel bus will be corrupt. */
    dpc245_debug_keep_swd_release_jtag();
    RCC_APB2ENR |= (1u<<2) | (1u<<3); /* GPIOA, GPIOB */
    for (unsigned p=0; p<8; ++p) gpio_cfg_output_pp_50mhz(GPIOB,p);
    gpio_cfg_output_pp_50mhz(GPIOB,DPC245_LCD_CS_PIN);
    gpio_cfg_output_pp_50mhz(GPIOB,DPC245_LCD_RST_PIN);
    gpio_cfg_output_pp_50mhz(GPIOA,DPC245_LCD_WR_PIN);
    gpio_cfg_output_pp_50mhz(GPIOA,DPC245_LCD_RD_PIN);
    gpio_cfg_output_pp_50mhz(GPIOA,DPC245_LCD_DC_PIN);
    pin_set(GPIOB,DPC245_LCD_CS_PIN);
    pin_set(GPIOB,DPC245_LCD_RST_PIN);
    pin_set(GPIOA,DPC245_LCD_WR_PIN);
    pin_set(GPIOA,DPC245_LCD_RD_PIN); /* 8080 RD inactive, held HIGH */
    pin_set(GPIOA,DPC245_LCD_DC_PIN);
}

static inline void write_byte(uint8_t v) {
    pin_clr(GPIOB,DPC245_LCD_CS_PIN);
    pin_clr(GPIOA,DPC245_LCD_WR_PIN);
    GPIO_ODR(GPIOB) = (GPIO_ODR(GPIOB) & 0xFF00u) | v;
    pin_set(GPIOA,DPC245_LCD_WR_PIN);
    pin_set(GPIOB,DPC245_LCD_CS_PIN);
}

void dpc245_lcd_command(uint8_t cmd) {
    pin_clr(GPIOA,DPC245_LCD_DC_PIN);
    write_byte(cmd);
    pin_set(GPIOA,DPC245_LCD_DC_PIN);
}
void dpc245_lcd_data8(uint8_t v) { write_byte(v); }
void dpc245_lcd_data16(uint16_t v) {
    if (LCD_TO_STRIP()) { ui_strip_data16(v); return; }
    write_byte((uint8_t)(v>>8)); write_byte((uint8_t)v);
}

static void cmd_data(uint8_t c, const uint8_t *p, size_t n) {
    dpc245_lcd_command(c);
    while (n--) dpc245_lcd_data8(*p++);
}

void dpc245_lcd_init(void (*delay_ms)(uint32_t)) {
    static const uint8_t madctl[]={0xA0}, colmod[]={0x05};
    static const uint8_t b2[]={0x0C,0x0C,0x00,0x33,0x33};
    static const uint8_t b7[]={0x35}, bb[]={0x1A}, c0[]={0x2C}, c2[]={0x01};
    static const uint8_t c3[]={0x0B}, c4[]={0x20}, c6[]={0x0F}, d0[]={0xA4,0xA1};
    static const uint8_t e0[]={0xF0,0x00,0x04,0x04,0x04,0x05,0x29,0x33,0x3E,0x38,0x12,0x12,0x28,0x30};
    static const uint8_t e1[]={0xF0,0x07,0x0A,0x0D,0x0B,0x07,0x28,0x33,0x3E,0x36,0x14,0x14,0x29,0x32};
    if (!delay_ms) return;
    dpc245_lcd_bus_init();
    pin_clr(GPIOB,DPC245_LCD_RST_PIN); delay_ms(100);
    pin_set(GPIOB,DPC245_LCD_RST_PIN); delay_ms(100);
    dpc245_lcd_command(0x11); delay_ms(120);
    cmd_data(0x36,madctl,sizeof madctl);
    cmd_data(0x3A,colmod,sizeof colmod);
    cmd_data(0xB2,b2,sizeof b2);
    cmd_data(0xB7,b7,sizeof b7);
    cmd_data(0xBB,bb,sizeof bb);
    cmd_data(0xC0,c0,sizeof c0);
    cmd_data(0xC2,c2,sizeof c2);
    cmd_data(0xC3,c3,sizeof c3);
    cmd_data(0xC4,c4,sizeof c4);
    cmd_data(0xC6,c6,sizeof c6);
    cmd_data(0xD0,d0,sizeof d0);
    dpc245_lcd_command(0x21);
    cmd_data(0xE0,e0,sizeof e0);
    cmd_data(0xE1,e1,sizeof e1);
    dpc245_lcd_command(0x11);
    dpc245_lcd_fill(0x0000);
    delay_ms(120);
    dpc245_lcd_command(0x29); delay_ms(100);
}

void dpc245_lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    if (LCD_TO_STRIP()) { ui_strip_set_window(x0,y0,x1,y1); return; }
    uint16_t py0=(uint16_t)(y0+DPC245_LCD_Y_OFFSET), py1=(uint16_t)(y1+DPC245_LCD_Y_OFFSET);
    dpc245_lcd_command(0x2A);
    dpc245_lcd_data8((uint8_t)(x0>>8)); dpc245_lcd_data8((uint8_t)x0);
    dpc245_lcd_data8((uint8_t)(x1>>8)); dpc245_lcd_data8((uint8_t)x1);
    dpc245_lcd_command(0x2B);
    dpc245_lcd_data8((uint8_t)(py0>>8)); dpc245_lcd_data8((uint8_t)py0);
    dpc245_lcd_data8((uint8_t)(py1>>8)); dpc245_lcd_data8((uint8_t)py1);
    dpc245_lcd_command(0x2C);
}

void dpc245_lcd_fill(uint16_t rgb565) {
    if (LCD_TO_STRIP()) { ui_strip_fill_rect(0,0,DPC245_LCD_WIDTH,DPC245_LCD_HEIGHT,rgb565); return; }
    dpc245_lcd_set_window(0,0,DPC245_LCD_WIDTH-1,DPC245_LCD_HEIGHT-1);
    for (uint32_t i=0;i<(uint32_t)DPC245_LCD_WIDTH*DPC245_LCD_HEIGHT;++i) dpc245_lcd_data16(rgb565);
}

void dpc245_lcd_fill_rect(uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint16_t rgb565) {
    if (LCD_TO_STRIP()) { ui_strip_fill_rect(x,y,w,h,rgb565); return; }
    if (!w || !h || x>=DPC245_LCD_WIDTH || y>=DPC245_LCD_HEIGHT) return;
    if ((uint32_t)x+w>DPC245_LCD_WIDTH) w=(uint16_t)(DPC245_LCD_WIDTH-x);
    if ((uint32_t)y+h>DPC245_LCD_HEIGHT) h=(uint16_t)(DPC245_LCD_HEIGHT-y);
    dpc245_lcd_set_window(x,y,(uint16_t)(x+w-1),(uint16_t)(y+h-1));
    for (uint32_t i=0;i<(uint32_t)w*h;++i) dpc245_lcd_data16(rgb565);
}

void dpc245_lcd_blit_rgb565(uint16_t x,uint16_t y,uint16_t w,uint16_t h,const uint16_t *pixels) {
    if (LCD_TO_STRIP()) { ui_strip_blit(x,y,w,h,pixels); return; }
    if (!pixels || !w || !h || x>=DPC245_LCD_WIDTH || y>=DPC245_LCD_HEIGHT) return;
    /* Clip the window but keep the source row stride w: a
     * clipped blit shows the same pixels as the strip path. */
    uint16_t cw=w, ch=h;
    if ((uint32_t)x+cw>DPC245_LCD_WIDTH) cw=(uint16_t)(DPC245_LCD_WIDTH-x);
    if ((uint32_t)y+ch>DPC245_LCD_HEIGHT) ch=(uint16_t)(DPC245_LCD_HEIGHT-y);
    dpc245_lcd_set_window(x,y,(uint16_t)(x+cw-1),(uint16_t)(y+ch-1));
    for (uint32_t r=0;r<ch;++r)
        for (uint32_t c=0;c<cw;++c) dpc245_lcd_data16(pixels[r*w+c]);
}
