/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "light_controller.h"

static uint16_t median3(uint16_t a,uint16_t b,uint16_t c){
    uint16_t lo=a<b?a:b,hi=a<b?b:a;
    if(c<lo)return lo;
    if(c>hi)return hi;
    return c;
}
static uint8_t auto_position(uint16_t shaped,uint8_t gain){
    uint32_t g=(gain>=1u&&gain<=9u)?gain:5u;
    uint32_t span=3300u*5u/g;
    int32_t num=(int32_t)shaped-300;
    int32_t pos=1+(num*15)/(int32_t)span;
    if(pos<1)pos=1;
    if(pos>16)pos=16;
    return (uint8_t)pos;
}
void evist_light_init(evist_light_controller_t *c,uint8_t sensitivity){
    if(!c)return;
    c->mode=EVIST_LIGHT_AUTO;c->sensitivity=sensitivity;c->gain=5u;c->dwell_limit=3u;
    c->threshold=3200u;c->manual_level=2u;c->light_on=false;c->last_als_raw=0u;
    c->history0=c->history1=c->shaped_als=0u;c->filter_valid=false;c->als_dark_known=false;
    c->als_dark=false;c->dwell_count=0u;c->backlight_position=1u;
}
void evist_light_configure(evist_light_controller_t *c,bool automatic,uint8_t gain,uint8_t dwell,uint16_t threshold,uint8_t manual_level){
    if(!c)return;
    c->mode=automatic?EVIST_LIGHT_AUTO:EVIST_LIGHT_MANUAL_OFF;
    c->gain=(gain>=1u&&gain<=9u)?gain:5u;
    c->dwell_limit=(dwell>=1u&&dwell<=30u)?dwell:3u;
    c->threshold=(threshold>=1800u&&threshold<=5000u)?threshold:3200u;
    c->manual_level=manual_level<=15u?manual_level:2u;
    c->dwell_count=0u;c->als_dark_known=false;
    if(!automatic)c->backlight_position=(uint8_t)(16u-c->manual_level);
}
void evist_light_set_mode(evist_light_controller_t *c,evist_light_mode_t mode){
    if(!c)return;c->mode=mode;c->als_dark_known=false;c->dwell_count=0u;
    if(mode==EVIST_LIGHT_MANUAL_ON)c->light_on=true;
    if(mode==EVIST_LIGHT_MANUAL_OFF)c->light_on=false;
}
bool evist_light_update_als(evist_light_controller_t *c,uint16_t raw){
    if(!c)return false;bool old=c->light_on;c->last_als_raw=raw;
    if(!c->filter_valid){c->history0=c->history1=raw;c->shaped_als=raw;c->filter_valid=true;}
    else{uint16_t med=median3(c->history0,c->history1,raw);c->shaped_als=(uint16_t)((int32_t)c->shaped_als+((int32_t)med-(int32_t)c->shaped_als)/4);c->history0=c->history1;c->history1=raw;}
    if(c->mode==EVIST_LIGHT_AUTO){
        c->backlight_position=auto_position(c->shaped_als,c->gain);
        if(c->sensitivity==0u)return old!=c->light_on;
        bool dark=((uint32_t)c->shaped_als+(uint32_t)c->sensitivity*5u)>=c->threshold;
        if(!c->als_dark_known){c->als_dark=dark;c->als_dark_known=true;c->dwell_count=0u;c->light_on=dark;}
        else if(dark!=c->als_dark){if(++c->dwell_count>=c->dwell_limit){c->als_dark=dark;c->light_on=dark;c->dwell_count=0u;}}
        else c->dwell_count=0u;
    }else c->backlight_position=(uint8_t)(16u-c->manual_level);
    return old!=c->light_on;
}
bool evist_light_user_toggle(evist_light_controller_t *c){if(!c)return false;c->light_on=!c->light_on;return c->light_on;}
uint8_t evist_light_backlight_position(const evist_light_controller_t *c){return c?c->backlight_position:1u;}
