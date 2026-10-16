/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "minifont.h"
#include "st7789_8080.h"
#include <stdint.h>
typedef struct { char c; uint8_t col[5]; } glyph_t;
static const glyph_t font[] = {
{' ',{0,0,0,0,0}},{'-',{8,8,8,8,8}},{'.',{0,0,0,0,0x60}},{':',{0,0x36,0x36,0,0}},
{'0',{0x3E,0x51,0x49,0x45,0x3E}},{'1',{0,0x42,0x7F,0x40,0}},{'2',{0x42,0x61,0x51,0x49,0x46}},
{'3',{0x21,0x41,0x45,0x4B,0x31}},{'4',{0x18,0x14,0x12,0x7F,0x10}},{'5',{0x27,0x45,0x45,0x45,0x39}},
{'6',{0x3C,0x4A,0x49,0x49,0x30}},{'7',{0x01,0x71,0x09,0x05,0x03}},{'8',{0x36,0x49,0x49,0x49,0x36}},
{'9',{0x06,0x49,0x49,0x29,0x1E}},
{'A',{0x7E,0x11,0x11,0x11,0x7E}},{'B',{0x7F,0x49,0x49,0x49,0x36}},{'C',{0x3E,0x41,0x41,0x41,0x22}},
{'D',{0x7F,0x41,0x41,0x22,0x1C}},{'E',{0x7F,0x49,0x49,0x49,0x41}},{'F',{0x7F,0x09,0x09,0x09,0x01}},
{'G',{0x3E,0x41,0x49,0x49,0x7A}},{'H',{0x7F,0x08,0x08,0x08,0x7F}},{'I',{0,0x41,0x7F,0x41,0}},
{'J',{0x20,0x40,0x41,0x3F,0x01}},{'K',{0x7F,0x08,0x14,0x22,0x41}},{'L',{0x7F,0x40,0x40,0x40,0x40}},
{'M',{0x7F,0x02,0x0C,0x02,0x7F}},{'N',{0x7F,0x04,0x08,0x10,0x7F}},{'O',{0x3E,0x41,0x41,0x41,0x3E}},
{'P',{0x7F,0x09,0x09,0x09,0x06}},{'Q',{0x3E,0x41,0x51,0x21,0x5E}},{'R',{0x7F,0x09,0x19,0x29,0x46}},
{'S',{0x46,0x49,0x49,0x49,0x31}},{'T',{0x01,0x01,0x7F,0x01,0x01}},{'U',{0x3F,0x40,0x40,0x40,0x3F}},
{'V',{0x1F,0x20,0x40,0x20,0x1F}},{'W',{0x3F,0x40,0x38,0x40,0x3F}},{'X',{0x63,0x14,0x08,0x14,0x63}},
{'Y',{0x07,0x08,0x70,0x08,0x07}},{'Z',{0x61,0x51,0x49,0x45,0x43}}
};
static const uint8_t *glyph(char c) {
    if (c>='a'&&c<='z') c=(char)(c-'a'+'A');
    for (unsigned i=0;i<sizeof(font)/sizeof(font[0]);i++) if(font[i].c==c) return font[i].col;
    return font[0].col;
}

void ui_text_box(uint16_t x,uint16_t y,const char *s,uint16_t fg,uint16_t bg,
                 uint8_t glyph_w,uint8_t glyph_h,uint8_t advance) {
    static uint16_t pix[15u*26u];
    if(!s||glyph_w==0u||glyph_h==0u||glyph_w>15u||glyph_h>26u)return;
    if(advance<glyph_w)advance=glyph_w;
    while(*s){
        if(x>=320u||y>=170u)break;
        const uint8_t *g=glyph(*s++);
        for(uint16_t py=0u;py<glyph_h;++py){
            uint8_t gy=(uint8_t)(((uint32_t)py*7u)/glyph_h);
            for(uint16_t px=0u;px<glyph_w;++px){
                uint8_t gx=(uint8_t)(((uint32_t)px*5u)/glyph_w);
                pix[(uint32_t)py*glyph_w+px]=(g[gx]&(1u<<gy))?fg:bg;
            }
        }
        dpc245_lcd_blit_rgb565(x,y,glyph_w,glyph_h,pix);
        if(advance>glyph_w)dpc245_lcd_fill_rect((uint16_t)(x+glyph_w),y,(uint16_t)(advance-glyph_w),glyph_h,bg);
        x=(uint16_t)(x+advance);
    }
}

/* Render one ST7789 window per glyph to minimize LCD command overhead. */
void ui_text(uint16_t x,uint16_t y,const char *s,uint16_t fg,uint16_t bg,uint8_t scale) {
    if(!s||!scale) return;
    if(scale>3u) scale=3u;
    static uint16_t pix[18u*21u];
    const uint16_t w=(uint16_t)(6u*scale), h=(uint16_t)(7u*scale);
    while(*s){
        if(x>=320u || y>=170u) break;
        const uint8_t *g=glyph(*s++);
        for(uint16_t py=0u;py<h;py++){
            const uint8_t gy=(uint8_t)(py/scale);
            for(uint16_t px=0u;px<w;px++){
                const uint8_t gx=(uint8_t)(px/scale);
                const uint8_t on=(uint8_t)(gx<5u && (g[gx]&(1u<<gy)));
                pix[(uint32_t)py*w+px]=on?fg:bg;
            }
        }
        dpc245_lcd_blit_rgb565(x,y,w,h,pix);
        x=(uint16_t)(x+w);
    }
}
void ui_u32(uint16_t x,uint16_t y,uint32_t v,uint16_t fg,uint16_t bg,uint8_t scale) {
    char b[11]; unsigned n=0; do { b[n++]=(char)('0'+v%10u); v/=10u; } while(v&&n<10); b[n]=0;
    for(unsigned i=0;i<n/2;i++){char c=b[i];b[i]=b[n-1-i];b[n-1-i]=c;} ui_text(x,y,b,fg,bg,scale);
}

void ui_i32(uint16_t x,uint16_t y,int32_t v,uint16_t fg,uint16_t bg,uint8_t scale) {
    if(v<0){ ui_text(x,y,"-",fg,bg,scale); x=(uint16_t)(x+6u*scale); v=-v; }
    ui_u32(x,y,(uint32_t)v,fg,bg,scale);
}
void ui_fixed_u32(uint16_t x,uint16_t y,uint32_t v,uint8_t decimals,uint16_t fg,uint16_t bg,uint8_t scale) {
    if(decimals==0u){ ui_u32(x,y,v,fg,bg,scale); return; }
    if(decimals>3u)decimals=3u;
    uint32_t div=1u; for(uint8_t i=0u;i<decimals;i++)div*=10u;
    uint32_t whole=v/div, frac=v%div;
    char b[16]; unsigned n=0u;
    do { b[n++]=(char)('0'+whole%10u); whole/=10u; } while(whole&&n<10u);
    for(unsigned i=0u;i<n/2u;i++){char c=b[i];b[i]=b[n-1u-i];b[n-1u-i]=c;}
    b[n++]='.';
    uint32_t place=div/10u;
    while(place){ b[n++]=(char)('0'+(frac/place)%10u); place/=10u; }
    b[n]=0; ui_text(x,y,b,fg,bg,scale);
}

static char hex_digit(uint8_t n){ return (char)(n<10u?('0'+n):('A'+(n-10u))); }
void ui_hex8(uint16_t x,uint16_t y,uint8_t v,uint16_t fg,uint16_t bg,uint8_t scale){
    char b[3]; b[0]=hex_digit((uint8_t)(v>>4)); b[1]=hex_digit((uint8_t)(v&0x0Fu)); b[2]=0; ui_text(x,y,b,fg,bg,scale);
}
void ui_hex32(uint16_t x,uint16_t y,uint32_t v,uint8_t digits,uint16_t fg,uint16_t bg,uint8_t scale){
    if(digits==0u||digits>8u)digits=8u; char b[9];
    for(uint8_t i=0;i<digits;i++){ uint8_t sh=(uint8_t)((digits-1u-i)*4u); b[i]=hex_digit((uint8_t)((v>>sh)&0x0Fu)); }
    b[digits]=0; ui_text(x,y,b,fg,bg,scale);
}

/* Large text without a pixel buffer: each lit font cell is one filled block, so
 * any scale works at no RAM cost. '.' takes only the width of its lit columns. */
static void big_glyph_span(char ch,const uint8_t *g,uint8_t *first,uint8_t *cols){
    *first=0u;*cols=5u;
    if(ch!='.')return;
    uint8_t lo=5u,hi=0u;
    for(uint8_t gx=0u;gx<5u;gx++)if(g[gx]){if(gx<lo)lo=gx;hi=gx;}
    if(lo<5u){*first=lo;*cols=(uint8_t)(hi-lo+1u);}
}

uint16_t ui_text_big_width(const char *s,uint8_t scale){
    uint16_t w=0u;
    if(!s)return 0u;
    while(*s){char ch=*s++;uint8_t first,cols;big_glyph_span(ch,glyph(ch),&first,&cols);w=(uint16_t)(w+(uint16_t)(cols+1u)*scale);}
    return w;
}

void ui_text_big(uint16_t x,uint16_t y,const char *s,uint16_t fg,uint16_t bg,uint8_t scale){
    if(!s||!scale)return;
    while(*s){
        if(x>=320u||y>=170u)break;
        char ch=*s++;
        const uint8_t *g=glyph(ch);
        uint8_t first,cols;big_glyph_span(ch,g,&first,&cols);
        uint16_t adv=(uint16_t)((cols+1u)*scale);
        dpc245_lcd_fill_rect(x,y,adv,(uint16_t)(7u*scale),bg);
        for(uint8_t gx=0u;gx<cols;gx++)
            for(uint8_t gy=0u;gy<7u;gy++)
                if(g[first+gx]&(1u<<gy))
                    dpc245_lcd_fill_rect((uint16_t)(x+gx*scale),(uint16_t)(y+gy*scale),scale,scale,fg);
        x=(uint16_t)(x+adv);
    }
}
