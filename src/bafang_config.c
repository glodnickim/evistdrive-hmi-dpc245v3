/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "bafang_config.h"
void evist_bafang_config_from_telemetry(evist_bafang_config_t *c,const bafang_telemetry_t *t){
    if(!c||!t||!t->have_speed_limit||!t->have_wheel_size||!t->have_circumference)return;
    c->speed_limit_centi_kph=t->speed_limit_centi_kph;
    c->wheel_size_tenth_in=t->wheel_size_tenth_in;
    c->circumference_mm=t->circumference_mm;
    c->have_readback=true;c->dirty=false;
}
bool evist_bafang_config_validate(const evist_bafang_config_t *c){
    if(!c)return false;
    /* Wide engineering bounds, not region/legal limits. */
    if(c->speed_limit_centi_kph<500u||c->speed_limit_centi_kph>10000u)return false;
    if(c->wheel_size_tenth_in<100u||c->wheel_size_tenth_in>400u)return false;
    if(c->circumference_mm<500u||c->circumference_mm>4000u)return false;
    return true;
}
bool evist_bafang_config_build_write(const evist_bafang_config_t *c,
                                      const evist_can_router_t *router,
                                      bool explicit_confirm,
                                      bafang_can_frame_t *out){
    if(!c||!router||!out||!explicit_confirm||router->tx_mode!=EVIST_TX_EXPERT||!evist_bafang_config_validate(c))return false;
    if(!bafang_build_besst_controller_config_write(out,c->speed_limit_centi_kph,c->wheel_size_tenth_in,c->circumference_mm))return false;
    return evist_can_router_tx_allowed(router,out,true);
}
