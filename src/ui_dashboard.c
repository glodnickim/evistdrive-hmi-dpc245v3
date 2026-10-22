/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui_dashboard.h"
#include "app_config.h"
#include "minifont.h"
#include "st7789_8080.h"
#include "boot_splash.h"
#include "ui_font1.h"
#include "ui_arc_geometry.h"
#include "dpc245_settings.h"
#include "bafang_can.h"
#include "assist_profile.h"
#include "dpc245_ble_app.h"
#include <stddef.h>
#include <stdint.h>

#define BLACK 0x0000u
static uint16_t g_white=0xFFFFu;
static uint16_t g_dim=0x7BEFu;
static uint16_t g_hi=0xBDF7u;
static bool g_menu_screen;
/* Dashboard palette. */
#define DASH_BG    0x0000u
#define DASH_GREEN 0x3E47u
#define TEMP_WARN_RED 0xF800u
#define RIDE_OFFROAD_COL ARC_FRAME_OFFROAD_COL  /* orange ring around the mode frame */
static const uint16_t mode_frame_colours[6] = {
    0x8410u, /* 0: neutral grey */
    0x26CDu, /* E: green */
    0x25DDu, /* T: cyan */
    0x42BAu, /* S: indigo */
    0xE9A9u, /* S+: red */
    0xA27Cu  /* B: violet */
};
#define WHITE g_white
/* Labels are white like values. */
#define DIM   g_white
#define HI    g_hi
#define MENU_SEL 0x07E0u  /* selected row (green), shared by all menus */

/* Menu and info text at its real size: same arguments as the 5x7
 * helpers, y is the top of the text. Scale 1 = menu font (cap 11 px), scale 2 =
 * title font (cap 17 px), numbers at scale 3 = large digits (cap 22 px). */
static const ui_font1_t *m_font(uint8_t scale,int *cap){
    if(scale>=2u){*cap=17;return &ui_font1_title;}
    *cap=g_menu_screen?14:11;
    return g_menu_screen?&ui_font1_menu:&ui_font1_dashboard;
}
static void m_text(uint16_t x,uint16_t y,const char *s,uint16_t fg,uint16_t bg,uint8_t scale){
    int cap;const ui_font1_t *f=m_font(scale,&cap);
    (void)ui_font1_text(f,x,(int)y+cap,s,fg,bg);
}
void ui_dashboard_temp_limit_text(char text[6],uint8_t limit){
    if(limit==0u){text[0]='O';text[1]='F';text[2]='F';text[3]=0;return;}
    uint8_t pos=0u;
    if(limit>=100u)text[pos++]=(char)('0'+limit/100u);
    text[pos++]=(char)('0'+(limit/10u)%10u);
    text[pos++]=(char)('0'+limit%10u);
    text[pos++]=' ';text[pos++]='C';text[pos]=0;
}
static void m_num(uint16_t x,uint16_t y,const char *s,uint16_t fg,uint16_t bg,uint8_t scale){
    if(scale>=3u){(void)ui_font1_text(&ui_font1_value,x,(int)y+22,s,fg,bg);return;}
    m_text(x,y,s,fg,bg,scale);
}
static void m_u32(uint16_t x,uint16_t y,uint32_t v,uint16_t fg,uint16_t bg,uint8_t scale){
    char b[11],t[11];unsigned n=0u,k=0u;do{t[k++]=(char)('0'+v%10u);v/=10u;}while(v&&k<10u);
    while(k)b[n++]=t[--k];b[n]=0;m_num(x,y,b,fg,bg,scale);
}
static void m_i32(uint16_t x,uint16_t y,int32_t v,uint16_t fg,uint16_t bg,uint8_t scale){
    char b[12],t[11];unsigned n=0u,k=0u;uint32_t a=v<0?(uint32_t)(-(v+1))+1u:(uint32_t)v;
    if(v<0)b[n++]='-';do{t[k++]=(char)('0'+a%10u);a/=10u;}while(a&&k<10u);
    while(k)b[n++]=t[--k];b[n]=0;m_num(x,y,b,fg,bg,scale);
}
static void m_fixed_u32(uint16_t x,uint16_t y,uint32_t v,uint8_t decimals,uint16_t fg,uint16_t bg,uint8_t scale){
    if(decimals>3u)decimals=3u;
    uint32_t div=1u;for(uint8_t i=0u;i<decimals;i++)div*=10u;
    char b[16],t[11];unsigned n=0u,k=0u;uint32_t whole=v/div,frac=v%div;
    do{t[k++]=(char)('0'+whole%10u);whole/=10u;}while(whole&&k<10u);
    while(k)b[n++]=t[--k];
    if(decimals){b[n++]='.';for(uint32_t p=div/10u;p;p/=10u)b[n++]=(char)('0'+(frac/p)%10u);}
    b[n]=0;m_num(x,y,b,fg,bg,scale);
}
static void m_hex32(uint16_t x,uint16_t y,uint32_t v,uint8_t digits,uint16_t fg,uint16_t bg,uint8_t scale){
    char b[9];if(digits>8u)digits=8u;if(digits==0u)digits=1u;
    for(uint8_t i=0u;i<digits;i++){uint8_t d=(uint8_t)((v>>(4u*(digits-1u-i)))&0xFu);b[i]=(char)(d<10u?'0'+d:'A'+d-10u);}
    b[digits]=0;m_text(x,y,b,fg,bg,scale);
}

static void clear(void){ dpc245_lcd_fill(BLACK); }
static void title(const char *s){ m_text(6,4,s,WHITE,BLACK,2); }
static void apply_theme(const dpc245_app_t *c){
    uint8_t t=c?dpc245_theme(&c->runtime.settings):0u;
    if(t==DPC245_THEME_YELLOW){g_white=0xFFE0u;g_dim=0xFD20u;g_hi=0x8410u;}
    else{g_white=0xFFFFu;g_dim=0x7BEFu;g_hi=0xBDF7u;}
}

static void splash_emit(void *ctx,uint16_t pixel,uint8_t count){
    (void)ctx;
    for(uint8_t i=0u;i<count;i++)dpc245_lcd_data16(pixel);
}

/* Streams the EVistDrive splash straight to the panel. Falls back to the
 * text splash only if the built-in asset is ever malformed. */
void ui_boot_splash_draw(void){
    g_menu_screen=false;
    g_white=0xFFFFu;g_dim=0x7BEFu;g_hi=0xBDF7u;
    dpc245_lcd_set_window(0u,0u,(uint16_t)(DPC245_LCD_WIDTH-1u),(uint16_t)(DPC245_LCD_HEIGHT-1u));
    if(boot_splash_emit_all(splash_emit,0))return;
    clear();
    m_text(54u,54u,"EVISTDRIVE",WHITE,BLACK,3u);
    m_text(94u,94u,"HMI",DIM,BLACK,2u);
    m_text(84u,128u,"DPC245",DIM,BLACK,1u);
}

static void value_u32(uint16_t y,const char *name,uint32_t v,const char *unit){
    m_text(7,y,name,DIM,BLACK,1); m_u32(150,y,v,WHITE,BLACK,1); if(unit)m_text(220,y,unit,DIM,BLACK,1);
}
static void value_i32(uint16_t y,const char *name,int32_t v,const char *unit){
    m_text(7,y,name,DIM,BLACK,1); m_i32(150,y,v,WHITE,BLACK,1); if(unit)m_text(220,y,unit,DIM,BLACK,1);
}
/* Value with two decimals from 0.01 units (e.g. 3655 -> 36.55), signed or not. */
static void value_x100(uint16_t y,const char *name,uint32_t v,const char *unit){
    m_text(7,y,name,DIM,BLACK,1); m_fixed_u32(150,y,v,2u,WHITE,BLACK,1); if(unit)m_text(220,y,unit,DIM,BLACK,1);
}
static void value_x100_i(uint16_t y,const char *name,int32_t v,const char *unit){
    m_text(7,y,name,DIM,BLACK,1);
    uint16_t x=150u; if(v<0){m_text(x,y,"-",WHITE,BLACK,1);x=(uint16_t)(x+ui_font1_width(&ui_font1_menu,"-"));}
    m_fixed_u32(x,y,v<0?(uint32_t)(-(v+1))+1u:(uint32_t)v,2u,WHITE,BLACK,1); if(unit)m_text(220,y,unit,DIM,BLACK,1);
}
/* Key hints, raised so descenders stay on screen. */
static void footer(const char *s){ m_text(6,151,s,WHITE,BLACK,1); }
static const char *menu_status(const char *s){
    const char *message="SAVING DISABLED IN THIS BUILD";
    const char *p=s;
    if(!p)return s;
    while(*p && *message && *p==*message){p++;message++;}
    return *p==*message?"SAVE DISABLED":s;
}

/* Integer metric-to-imperial conversion without 64-bit runtime helpers.
 * 621371 ppm = miles/km. Internal units remain metric. */
static uint32_t metric_to_imperial(uint32_t v){
    uint32_t q=v/1000000u, r=v%1000000u;
    uint32_t a=r/1000u, b=r%1000u;
    return q*621371u + (a*621371u + (b*621371u)/1000u)/1000u;
}
static uint32_t display_metric(const dpc245_app_t *c,uint32_t v){
    return dpc245_unit(&c->runtime.settings)==DPC245_UNIT_IMPERIAL?metric_to_imperial(v):v;
}
static const char *distance_unit(const dpc245_app_t *c){
    return dpc245_unit(&c->runtime.settings)==DPC245_UNIT_IMPERIAL?"MI":"KM";
}
static const char *speed_unit(const dpc245_app_t *c){
    return dpc245_unit(&c->runtime.settings)==DPC245_UNIT_IMPERIAL?"MPH":"KMH";
}
/* Dashboard spelling of the speed unit (font1 has '/' and lower case k, m, h). */
static const char *dash_speed_unit(const dpc245_app_t *c,bool lower){
    if(dpc245_unit(&c->runtime.settings)==DPC245_UNIT_IMPERIAL)return "MPH";
    return lower?"km/h":"KM/H";
}
static void fmt_u32_5(char b[6],uint32_t v);

static void ride_speed_text(char out[16],const dpc245_app_t *c,uint16_t centi,bool unit){
    uint32_t shown=display_metric(c,centi);
    uint32_t tenth=(shown+5u)/10u;
    char digits[6];fmt_u32_5(digits,tenth/10u);
    unsigned n=0u;
    for(unsigned i=0u;digits[i]&&n<5u;i++)out[n++]=digits[i];
    out[n++]='.';out[n++]=(char)('0'+tenth%10u);
    if(unit){
        const char *suffix=dpc245_unit(&c->runtime.settings)==DPC245_UNIT_IMPERIAL?" MPH":" KMH";
        for(unsigned i=0u;suffix[i]&&n<15u;i++)out[n++]=suffix[i];
    }
    out[n]=0;
}

static void flip_buffer(char *b,unsigned n){for(unsigned i=0u;i<n/2u;++i){char c=b[i];b[i]=b[n-1u-i];b[n-1u-i]=c;}}
static void fmt_u32_5(char b[6],uint32_t v){
    if(v>99999u)v=99999u;unsigned n=0u;do{b[n++]=(char)('0'+v%10u);v/=10u;}while(v&&n<5u);flip_buffer(b,n);b[n]=0;
}
static void fmt_i32_5(char b[6],int32_t v){
    if(v>=0){fmt_u32_5(b,(uint32_t)v);return;}uint32_t a=(uint32_t)(-v);if(a>9999u)a=9999u;char t[6];fmt_u32_5(t,a);b[0]='-';unsigned n=0u;while(t[n]&&n<4u){b[n+1u]=t[n];n++;}b[n+1u]=0;
}
static void fmt_x10_5(char b[6],uint32_t v){
    if(v>9999u){fmt_u32_5(b,v/10u);return;}
    uint32_t whole=v/10u,frac=v%10u;char t[6];fmt_u32_5(t,whole);unsigned n=0u;while(t[n]&&n<3u){b[n]=t[n];n++;}b[n++]='.';b[n++]=(char)('0'+frac);b[n]=0;
}
static void fmt_x100_5(char b[6],uint32_t v){fmt_x10_5(b,v/10u);}

static bool temperature_warning_visible(const dpc245_app_t *c,uint32_t now_ms){
    return c->temp_warning_sources && (int32_t)(now_ms-c->temp_warning_text_until_ms)<0 &&
           !c->ride_confirm_pending && !c->ride_toast_active &&
           c->ride_apply_result!=EVIST_RIDE_APPLY_FAILED;
}
static void draw_temperature_warning(const dpc245_app_t *c,uint16_t x,uint16_t y){
    const char *prefix=c->temp_warning_sources==3u?"MOTOR+CTRL HOT ":
                       c->temp_warning_sources==1u?"MOTOR HOT ":"CTRL HOT ";
    bool motor_valid=(c->temp_warning_sources&1u)!=0u&&c->temp_motor_valid;
    bool ctrl_valid=(c->temp_warning_sources&2u)!=0u&&c->temp_ctrl_valid;
    char digits[6]="--",message[26];
    if(motor_valid||ctrl_valid){
        int32_t temperature=motor_valid?c->temp_motor_c:c->temp_ctrl_c;
        if(ctrl_valid&&c->temp_ctrl_c>temperature)temperature=c->temp_ctrl_c;
        fmt_i32_5(digits,temperature);
    }
    unsigned n=0u;while(prefix[n]&&n<18u){message[n]=prefix[n];++n;}
    for(unsigned i=0u;digits[i]&&n<23u;++i)message[n++]=digits[i];
    message[n++]='C';message[n]=0;
    m_text(x,y,message,TEMP_WARN_RED,BLACK,1u);
}

/* One half of the lower pair: value right-aligned to x+78, label above and unit
 * after it from x+80. Fonts at their real size (font1 value/small, 5x7 label). */
static void lower_cell(uint16_t x,const char *label,const char *value,const char *unit){
    dpc245_lcd_fill_rect(x,132u,136u,38u,DASH_BG);
    uint16_t w=ui_font1_width(&ui_font1_value,value);
    uint16_t value_right=(uint16_t)(x+((x==10u)?73u:78u));
    uint16_t value_x=(uint16_t)(value_right-(w<76u?w:76u));
    ui_font1_text(&ui_font1_value,value_x,166,value,WHITE,DASH_BG);
    m_text((uint16_t)(x+2u),132u,label,DIM,DASH_BG,1);
    if(unit&&unit[0]){
        uint16_t uw=ui_font1_width(&ui_font1_small,unit);
        uint16_t ux=(uint16_t)(value_right+3u);
        if(x!=10u)ux=(uint16_t)(x+81u);
        else if((uint16_t)(ux+uw)>138u)ux=(uint16_t)(138u-uw);
        ui_font1_text(&ui_font1_small,ux,166,unit,WHITE,DASH_BG);
    }
}

static void temperature_value(const bafang_telemetry_t *t,bool motor,char value[6],const char **label){
    if(motor){
        *label="MOTOR";
        if(t->have_motor_temp&&bafang_temperature_raw_valid(t->controller_aux_temp_raw)){fmt_i32_5(value,t->motor_temp_c);return;}
    }else{
        *label="CTRL";
        if(t->have_controller_temp&&bafang_temperature_raw_valid(t->controller_temp_raw)){fmt_i32_5(value,t->controller_temp_c);return;}
    }
    value[0]='-';value[1]='-';value[2]=0;
}

static bool selected_motor(const dpc245_app_t *c,uint32_t now_ms){
    return c->preferences.temp_view==EVIST_TEMP_2 ||
           (c->preferences.temp_view==EVIST_TEMP_ALTERNATE && ((now_ms/3000u)&1u)!=0u);
}

void ui_dashboard_lower_pair_text(const dpc245_app_t *c,const bafang_telemetry_t *t,uint8_t panel,uint32_t now_ms,
                                  char l[6],char r[6],const char **ll,const char **rl,const char **lu,const char **ru){
    uint32_t trip=dpc245_get_u32_le(&c->runtime.settings,DPC245_OFS_TRIP_DISTANCE_RAW);
    uint32_t odo=dpc245_get_u32_le(&c->runtime.settings,DPC245_OFS_TOTAL_DISTANCE_RAW);
    uint16_t maxs=dpc245_get_u16_le(&c->runtime.settings,DPC245_OFS_MAX_SPEED_RAW);
    uint16_t avgs=dpc245_get_u16_le(&c->runtime.settings,DPC245_OFS_AVG_SPEED_RAW);
    uint32_t mins=dpc245_get_u32_le(&c->runtime.settings,DPC245_OFS_TRIP_TIME_HALFSEC)/120u;
    int32_t motor=bafang_power_w(t);if(motor<0)motor=0;
    switch(panel){
    case 0: *ll="TRIP";*rl="ODO";*lu=distance_unit(c);*ru=distance_unit(c);fmt_x100_5(l,display_metric(c,trip));fmt_x100_5(r,display_metric(c,odo));break;
    case 1: *ll="MAX";*rl="AVG";*lu=dash_speed_unit(c,false);*ru=dash_speed_unit(c,false);fmt_x10_5(l,display_metric(c,maxs));fmt_x10_5(r,display_metric(c,avgs));break;
    case 2: *ll="CADENCE";*rl="CAL";*lu="RPM";*ru="KCAL";fmt_u32_5(l,t->cadence_rpm);fmt_u32_5(r,dpc245_runtime_calories_kcal(&c->runtime));break;
    case 3: *ll="CADENCE";*rl="TORQUE";*lu="RPM";*ru="";fmt_u32_5(l,t->cadence_rpm);fmt_u32_5(r,t->torque_signal_mv);break;
    case 4: *ll="TIME";*rl="TRIP";*lu="MIN";*ru=distance_unit(c);fmt_u32_5(l,mins);fmt_x100_5(r,display_metric(c,trip));break;
    case 5: *ll="WATT";*lu="W";*ru="C";fmt_u32_5(l,(uint32_t)motor);temperature_value(t,selected_motor(c,now_ms),r,rl);break;
    case 6: *ll="PEDAL";*rl="MOTOR";*lu="W";*ru="W";
        { uint16_t pedal;
          if(dpc245_runtime_pedal_avg(&c->runtime,&pedal)){ uint32_t w=((uint32_t)pedal+2u)/5u*5u; if(w>9995u)w=9995u; fmt_u32_5(l,w); }
          else{l[0]='-';l[1]='-';l[2]=0;} }
        fmt_u32_5(r,(uint32_t)motor);break;
    case 7: *ll="RANGE";*rl="VOLT";*lu=distance_unit(c);*ru="V";if(!t->have_range||t->range_centi_km==0xFFFFu){l[0]='-';l[1]='-';l[2]='.';l[3]='-';l[4]=0;}else fmt_x100_5(l,display_metric(c,t->range_centi_km));if(t->have_voltage)fmt_x100_5(r,t->voltage_centi_volt);else{r[0]='-';r[1]='-';r[2]='.';r[3]='-';r[4]=0;}break;
    case 8: *ll="VOLT";*lu="V";*ru="C";if(t->have_voltage)fmt_x100_5(l,t->voltage_centi_volt);else{l[0]='-';l[1]='-';l[2]='.';l[3]='-';l[4]=0;}temperature_value(t,selected_motor(c,now_ms),r,rl);break;
    default:
        *lu="C";
        temperature_value(t,selected_motor(c,now_ms),l,ll);
        if(c->preferences.temp_view==EVIST_TEMP_BOTH){temperature_value(t,true,r,rl);*ru="C";}
        else {*rl="WATT";*ru="W";fmt_u32_5(r,(uint32_t)motor);}
        break;
    }
}

typedef struct { uint16_t colour; bool blink; } soc_style_t;
static soc_style_t soc_style(uint8_t soc){
    soc_style_t s; s.blink=soc<=5u;
    if(soc<=9u)s.colour=0xF800u; else if(soc<=15u)s.colour=0xFD20u; else s.colour=WHITE;
    return s;
}
static bool soc_blink_on(soc_style_t style,uint32_t now_ms){return !style.blink||(now_ms%900u)<600u;}
static uint8_t round_inset(uint8_t radius,uint8_t row){
    uint8_t d=row;
    if(row>(uint8_t)(radius-1u))d=(uint8_t)(radius-1u-row);
    uint16_t rr=(uint16_t)radius*radius;
    uint16_t dy=(uint16_t)(radius-1u-d);
    uint16_t rem=(uint16_t)(rr-dy*dy);
    uint8_t x=0u;
    while((uint16_t)(x+1u)*(x+1u)<=rem)x++;
    return (uint8_t)(radius-x);
}
static void round_rect(uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint8_t radius,uint16_t colour){
    for(uint16_t row=0u;row<h;row++){
        uint8_t inset=0u;
        if(row<radius)inset=round_inset(radius,(uint8_t)row);
        else if(row>=(uint16_t)(h-radius))inset=round_inset(radius,(uint8_t)(h-1u-row));
        if((uint16_t)(2u*inset)<w)dpc245_lcd_fill_rect((uint16_t)(x+inset),(uint16_t)(y+row),(uint16_t)(w-2u*inset),1u,colour);
    }
}
static const ui_font1_glyph_t *badge_glyph(const ui_font1_t *f,char c){
    for(uint8_t i=0u;f&&i<f->count;i++)if(f->codes[i]==c)return &f->glyphs[i];
    return 0;
}
/* Return the pen position that centres the actual glyph ink in the inner field. */
static uint16_t badge_ink_left(const ui_font1_t *f,const char *s){
    int pen=0,min=32767,max=-32768;
    for(;s&&*s;s++){
        const ui_font1_glyph_t *g=badge_glyph(f,*s);
        if(!g)continue;
        int left=pen+g->ox,right=left+g->w-1;
        if(g->w&&left<min)min=left;
        if(g->w&&right>max)max=right;
        pen+=g->adv;
    }
    if(max<min)return 41u;
    return (uint16_t)((85-(min+max))/2);
}
static int badge_ink_top(const ui_font1_t *f,const char *s){
    int pen=0,min=32767,max=-32768;
    for(;s&&*s;s++){
        const ui_font1_glyph_t *g=badge_glyph(f,*s);
        if(!g)continue;
        int top=g->oy,bottom=top+g->h-1;
        if(g->h&&top<min)min=top;
        if(g->h&&bottom>max)max=bottom;
        pen+=g->adv;
    }
    (void)pen;
    return max<min?0:(149-(min+max))/2;
}
#define BAT_BAR_X 2u
#define BAT_BAR_Y 2u
#define BAT_BAR_H 166u
static void draw_battery_bar(const bafang_telemetry_t *t,bool enabled,uint32_t now_ms){
    dpc245_lcd_fill_rect(BAT_BAR_X,BAT_BAR_Y,4u,BAT_BAR_H,BLACK);
    if(!enabled||!t->have_battery_pct)return;
    uint8_t pct=t->battery_pct>100u?100u:t->battery_pct;
    soc_style_t style=soc_style(pct);
    dpc245_lcd_fill_rect(BAT_BAR_X,BAT_BAR_Y,4u,BAT_BAR_H,WHITE);
    dpc245_lcd_fill_rect((uint16_t)(BAT_BAR_X+1u),(uint16_t)(BAT_BAR_Y+1u),2u,(uint16_t)(BAT_BAR_H-2u),BLACK);
    uint16_t h=(uint16_t)((uint32_t)pct*(BAT_BAR_H-2u)/100u);
    if(h && soc_blink_on(style,now_ms))
        dpc245_lcd_fill_rect((uint16_t)(BAT_BAR_X+1u),(uint16_t)(BAT_BAR_Y+BAT_BAR_H-1u-h),2u,h,style.colour);
    const uint8_t thresholds[3]={6u,10u,16u};
    for(uint8_t i=0u;i<3u;i++){
        uint16_t y=(uint16_t)(BAT_BAR_Y+BAT_BAR_H-1u-(uint16_t)((BAT_BAR_H-2u)*thresholds[i]/100u));
        dpc245_lcd_fill_rect((uint16_t)(BAT_BAR_X+4u),y,2u,1u,soc_style((uint8_t)(thresholds[i]+1u)).colour);
    }
    uint16_t mid_y=(uint16_t)(BAT_BAR_Y+BAT_BAR_H-1u-(uint16_t)((BAT_BAR_H-2u)*50u/100u));
    dpc245_lcd_fill_rect((uint16_t)(BAT_BAR_X+4u),mid_y,2u,1u,WHITE);
}
/* Battery group layout, right to left: icon at the right edge,
 * then voltage, then percent. Each part is optional (preferences mask). */
static void draw_battery_widget(const dpc245_app_t *c,const bafang_telemetry_t *t,uint32_t now_ms){
    uint8_t mask=(uint8_t)(c->preferences.battery_mask&EVIST_BATTERY_ALL);
    uint8_t soc=t->have_battery_pct?(t->battery_pct>100u?100u:t->battery_pct):0u;
    soc_style_t style=soc_style(soc);
    uint16_t col=style.colour;
    bool visible=soc_blink_on(style,now_ms);
    draw_battery_bar(t,(mask&EVIST_BATTERY_BAR)!=0u,now_ms);
    uint16_t right=276u;
    if(mask&EVIST_BATTERY_ICON){
        uint16_t x=(uint16_t)(right-40u);
        dpc245_lcd_fill_rect(x,7u,36u,20u,WHITE);
        dpc245_lcd_fill_rect((uint16_t)(x+2u),9u,32u,16u,DASH_BG);
        dpc245_lcd_fill_rect((uint16_t)(x+36u),12u,4u,10u,WHITE);
        uint16_t fill=(uint16_t)((uint32_t)soc*28u/100u);
        if(soc&&fill==0u)fill=1u;
        if(t->have_battery_pct&&fill&&visible)dpc245_lcd_fill_rect((uint16_t)(x+4u),11u,fill,12u,col);
        right=(uint16_t)(x-8u);
    }
    if(mask&EVIST_BATTERY_VOLT){
        char b[8];
        if(t->have_voltage){uint32_t v=t->voltage_centi_volt/10u;char n5[6];fmt_u32_5(n5,v/10u);unsigned n=0u;while(n5[n]){b[n]=n5[n];n++;}b[n++]='.';b[n++]=(char)('0'+v%10u);b[n++]='V';b[n]=0;}
        else{b[0]='-';b[1]='-';b[2]='.';b[3]='-';b[4]='V';b[5]=0;}
        uint16_t w=ui_font1_width(&ui_font1_small,b);
        right=(uint16_t)(right>w?right-w:0u);
        ui_font1_text(&ui_font1_small,right,24,b,WHITE,DASH_BG);
        right=(uint16_t)(right>8u?right-8u:0u);
    }
    if(mask&EVIST_BATTERY_SOC){
        char b[8];
        if(t->have_battery_pct){fmt_u32_5(b,soc);unsigned n=0u;while(b[n])n++;b[n++]='%';b[n]=0;}
        else{b[0]='-';b[1]='-';b[2]='%';b[3]=0;col=WHITE;}
        uint16_t w=ui_font1_width(&ui_font1_small,b);
        right=(uint16_t)(right>w?right-w:0u);
        ui_font1_text(&ui_font1_small,right,24,b,t->have_battery_pct&&style.blink&&!visible?BLACK:col,DASH_BG);
    }
}

/* Power bar as a rev-counter, full screen height: the filled part takes the
 * colour of the scale zone it reaches (green to 50 %, yellow to 70 %, orange to
 * 85 %, red to the end). */
#define PWR_X       289u
#define PWR_W       24u
#define PWR_BOTTOM  166u
#define PWR_H       164u
static void draw_power_graph(const dpc245_app_t *c){
    uint32_t pw=c->power_graph_w;uint32_t scale=c->preferences.graph_scale_w?c->preferences.graph_scale_w:EVIST_PREF_GRAPH_DEFAULT_W;if(pw>scale)pw=scale;
    dpc245_lcd_fill_rect(289u,0u,24u,170u,DASH_BG);
    uint16_t h=(uint16_t)((uint32_t)pw*PWR_H/scale);
    static const uint8_t zone_end_pct[4]={50u,70u,85u,100u};
    static const uint16_t zone_col[4]={0x07E0u,0xFFE0u,0xFD20u,0xF800u};
    uint16_t from=0u;
    for(uint8_t z=0u;z<4u&&from<h;z++){
        uint16_t to=(uint16_t)((uint32_t)PWR_H*zone_end_pct[z]/100u);if(to>h)to=h;
        if(to>from)dpc245_lcd_fill_rect(PWR_X,(uint16_t)(PWR_BOTTOM-to),PWR_W,(uint16_t)(to-from),zone_col[z]);
        from=to;
    }
}

/* Fixed 20x20 top-left slots. Each contiguous ink run uses one LCD fill. */
static const uint32_t status_light[20]={
    0x00000,0x00000,0x007F0,0x007F0,0x00618,0xFE60C,0xFE60C,0x00606,0x00606,0xFE606,
    0xFE606,0x00606,0x00606,0xFE60C,0xFE60C,0x00618,0x007F0,0x007F0,0x00000,0x00000};
static const uint32_t status_ble[20]={
    0x00600,0x00630,0x00638,0x0063C,0x1866C,0x1C6CC,0x0E78C,0x0770C,0x03E00,0x01E00,
    0x01E00,0x03E00,0x0770C,0x0E78C,0x1C6CC,0x1866C,0x0063C,0x00638,0x00630,0x00600};
static const uint32_t status_err[20]={
    0x00F00,0x01F80,0x01980,0x030C0,0x030C0,0x06060,0x06060,0x0CC30,0x0CC30,0x18C18,
    0x18C18,0x30C0C,0x3000C,0x60606,0x60606,0xC0603,0xC0003,0xFFFFF,0xFFFFF,0x00000};
static void draw_status_glyph(uint16_t x,uint16_t y0,const uint32_t rows[20]){
    for(uint16_t y=0u;y<20u;y++){
        uint32_t row=rows[y];
        for(uint16_t b=0u;b<20u;){
            if(!(row&(0x80000u>>b))){b++;continue;}
            uint16_t start=b;
            do{b++;}while(b<20u&&(row&(0x80000u>>b)));
            dpc245_lcd_fill_rect((uint16_t)(x+start),(uint16_t)(y0+y),(uint16_t)(b-start),1u,WHITE);
        }
    }
}
static void draw_status_icons(const dpc245_ui_status_t *s){
    /* Lamp body and three beams; Bluetooth rune; warning with ink exclamation. */
    if(!s)return;
    if(s->light_on)draw_status_glyph(8u,8u,status_light);
    if(s->ble_online)draw_status_glyph(36u,8u,status_ble);
    if(s->controller_error)draw_status_glyph(64u,8u,status_err);
}

static void draw_lower_panel(const dpc245_app_t *c,const bafang_telemetry_t *t,bool show_left,uint32_t now_ms){
    char l[6]={0},r[6]={0};const char *ll="",*rl="",*lu="",*ru="";uint8_t panel=c->preferences.lower_panel<EVIST_PANEL_COUNT?c->preferences.lower_panel:0u;
    ui_dashboard_lower_pair_text(c,t,panel,now_ms,l,r,&ll,&rl,&lu,&ru);dpc245_lcd_fill_rect(139u,132u,2u,38u,DIM);
    if(show_left)lower_cell((c->preferences.battery_mask&EVIST_BATTERY_BAR)?10u:2u,ll,l,lu);
    lower_cell(143u,rl,r,ru);
}

/* Mode letters: level 0 = 0, 1..5 = E, T, S, S+, B
 * (motor slot map 0,2,4,6,8,9). Levels above 5 show the number. */
static void draw_mode_badge(uint8_t level,bool offroad,bool walking,uint16_t walk_colour){
    const ui_font1_t *f=&ui_font1_badge;
    char label[4]={0};
    if(level==0u){label[0]='0';}
    else if(level<=5u){static const char *const names[6]={"","E","T","S","S+","B"};unsigned n=0u;for(const char *p=names[level];*p&&n<3u;p++)label[n++]=*p;}
    else{label[0]=(char)('0'+level/10u);label[1]=(char)('0'+level%10u);f=&ui_font1_value;}
    /* OFFROAD: orange 2 px ring around the frame; LEGAL shows nothing extra. */
    if(offroad)round_rect(9u,41u,68u,68u,24u,RIDE_OFFROAD_COL);
    uint16_t frame_colour=(level==0u||walking)?WHITE:(level<=5u?mode_frame_colours[level]:mode_frame_colours[1]);
    round_rect(11u,43u,64u,64u,22u,frame_colour);
    round_rect(19u,51u,48u,48u,14u,DASH_BG);
    if(walking){label[0]='W';label[1]=0;f=&ui_font1_badge;}
    if(level==4u&&!walking){
        const ui_font1_glyph_t *sg=badge_glyph(&ui_font1_badge,'S');
        const ui_font1_glyph_t *pg=badge_glyph(&ui_font1_badge_plus,'+');
        int total=(int)sg->w+(int)pg->w;
        int left=(85-total)/2;
        int sbase=badge_ink_top(&ui_font1_badge,"S");
        int pbase=sbase+(sg->oy+(int)sg->h/2)-(pg->oy+(int)pg->h/2);
        ui_font1_text(&ui_font1_badge,(uint16_t)(left-sg->ox),sbase,"S",WHITE,DASH_BG);
        ui_font1_text(&ui_font1_badge_plus,(uint16_t)(left+sg->w-pg->ox),pbase,"+",WHITE,DASH_BG);
        return;
    }
    int baseline=badge_ink_top(f,label);
    ui_font1_text(f,badge_ink_left(f,label),baseline,label,walking?walk_colour:WHITE,DASH_BG);
}

/* Fixed speed cells retain the 88.8 reference geometry. */
#define SPEED_TENS_X0  81u
#define SPEED_CELL_W   76u
#define SPEED_TENS_X1  (SPEED_TENS_X0 + SPEED_CELL_W - 1u)
#define SPEED_UNITS_X0 (SPEED_TENS_X1 + 1u)
#define SPEED_UNITS_X1 (SPEED_UNITS_X0 + SPEED_CELL_W - 1u)
#define SPEED_DOT_X    231u
#define SPEED_TENTHS_X0 243u
#define SPEED_TENTHS_X1 281u

#ifdef UI_FONT1_TRACE
void (*ui_dashboard_speed_cells_trace)(int units_x0, int dot_x, int tenths_x0,
                                       int tens_x0, int tens_x1, int units_x1,
                                       int tenths_x1, int tens_drawn) = 0;
#endif

static const ui_font1_glyph_t *speed_glyph(const ui_font1_t *font,char ch){
    for(uint8_t i=0u;i<font->count;i++)if(font->codes[i]==ch)return &font->glyphs[i];
    return 0;
}
static void draw_speed_digit(const ui_font1_t *font,char ch,uint16_t cell_x0,uint16_t cell_x1,uint16_t colour){
    const ui_font1_glyph_t *g=speed_glyph(font,ch);if(!g)return;
    int cell_w=(int)cell_x1-(int)cell_x0+1;
    int x=(int)cell_x0+(cell_w-(int)g->w)/2-(int)g->ox;
    char glyph[2]={ch,0};
    ui_font1_text_clipped(font,(uint16_t)x,128,glyph,colour,DASH_BG,cell_x0,cell_x1);
}

/* Speed as one decimal (0.1 km/h units); each place keeps a fixed ink-centred cell. */
static void draw_speed(uint32_t tenths,uint16_t colour){
    uint32_t whole=tenths/10u;
    char tens=(char)('0'+(whole/10u)%10u),units=(char)('0'+whole%10u);
    char tenth=(char)('0'+tenths%10u),dot[2]={'.',0};
    ui_font1_text_clipped(&ui_font1_speed_small,SPEED_DOT_X,128,dot,colour,DASH_BG,
                          SPEED_DOT_X,SPEED_DOT_X+14u);
    draw_speed_digit(&ui_font1_speed,units,SPEED_UNITS_X0,SPEED_UNITS_X1,colour);
    if(whole>=10u)draw_speed_digit(&ui_font1_speed,tens,SPEED_TENS_X0,SPEED_TENS_X1,colour);
    draw_speed_digit(&ui_font1_speed_small,tenth,SPEED_TENTHS_X0,SPEED_TENTHS_X1,colour);
#ifdef UI_FONT1_TRACE
    if(ui_dashboard_speed_cells_trace)
        ui_dashboard_speed_cells_trace(SPEED_UNITS_X0,SPEED_DOT_X,SPEED_TENTHS_X0,
                                       SPEED_TENS_X0,SPEED_TENS_X1,SPEED_UNITS_X1,SPEED_TENTHS_X1,
                                       whole>=10u);
#endif
}

/* The main screen uses speed as the dominant element (speed dominant, mode
 * badge left, battery top right, power bar right, lower pairs at the bottom). */
static void draw_dashboard(const dpc245_app_t *c,const evist_can_router_t *r,const dpc245_ui_status_t *status,uint32_t now_ms){
    const bafang_telemetry_t *t=&r->telemetry;
    draw_battery_widget(c,t,now_ms);
    draw_status_icons(status);
    /* speed_centi_kph is 0.01 km/h; the dashboard shows one decimal (0.1 km/h). */
    draw_speed(t->have_speed?display_metric(c,t->speed_centi_kph)/10u:0u,
               c->temp_warning_sources?TEMP_WARN_RED:WHITE);
    const char *unit=dash_speed_unit(c,true);
    uint16_t unit_w=ui_font1_width(&ui_font1_small,unit);
    ui_font1_text(&ui_font1_small,(uint16_t)(284u-unit_w),58,unit,WHITE,DASH_BG);
    bool walk_pushing=c->walk&&(bafang_walk_bit_fresh(t,now_ms)||(c->down_held&&t->have_speed&&t->speed_centi_kph>=50u));
    draw_mode_badge(c->assist_level,c->ride_indicator==DPC245_RIDE_IND_OFFROAD,c->walk,
                    walk_pushing?0xFDE9u:WHITE);
    if(dpc245_runtime_service_tip_active(&c->runtime))m_text(150u,112u,"SERVICE",WHITE,DASH_BG,1);
    if(c->ride_confirm_pending || c->ride_toast_active || c->ride_apply_result==EVIST_RIDE_APPLY_FAILED ||
       temperature_warning_visible(c,now_ms))
        dpc245_lcd_fill_rect(0u,132u,280u,38u,DASH_BG);
    else draw_lower_panel(c,t,true,now_ms);
    if(temperature_warning_visible(c,now_ms))draw_temperature_warning(c,8u,145u);
    if(c->ride_confirm_pending){
        m_text(8u,134u,c->ride_indicator==DPC245_RIDE_IND_OFFROAD?"BACK TO LEGAL?":"OFFROAD UNLOCK?",0xFFE0u,DASH_BG,1u);
        m_text(8u,153u,"POWER=YES  UP/DOWN=NO",WHITE,DASH_BG,1u);
    }else if(c->ride_toast_active){
        char legal_toast[24]="LEGAL ";
        if(c->ride_indicator!=DPC245_RIDE_IND_OFFROAD)
            ride_speed_text(legal_toast+6,c,c->ride_cfg.legal_x100,false);
        m_text(8u,134u,c->ride_indicator==DPC245_RIDE_IND_OFFROAD?"OFFROAD":legal_toast,0xFFE0u,DASH_BG,1u);
        m_text(8u,153u,"MODE APPLIED",WHITE,DASH_BG,1u);
    }else if(c->ride_apply_result==EVIST_RIDE_APPLY_FAILED){
        m_text(8u,140u,"RIDE APPLY",WHITE,DASH_BG,1u);
        m_text(8u,154u,"FAILED",WHITE,DASH_BG,1u);
    }
    draw_power_graph(c);
}

typedef struct { uint16_t x,y,w,h; } theme_region_t;
/* Independent regions keep live values in fixed areas for tile comparison. */
static const theme_region_t arc_regions[]={
    {0u,0u,28u,170u}, {28u,0u,211u,26u}, {28u,26u,211u,108u},
    {28u,134u,211u,36u}, {239u,0u,81u,85u}, {239u,85u,81u,85u}
};
static const theme_region_t wedge_regions[]={
    {0u,0u,216u,35u}, {0u,35u,216u,109u}, {216u,0u,104u,144u},
    {0u,144u,320u,26u}
};
static void theme_clear_regions(const theme_region_t *regions,size_t count){
    for(size_t i=0u;i<count;i++)
        dpc245_lcd_fill_rect(regions[i].x,regions[i].y,regions[i].w,regions[i].h,BLACK);
}
static uint16_t power_zone_pct(uint8_t pct){
    if(pct<50u)return 0x07E0u;
    if(pct<70u)return 0xFFE0u;
    if(pct<85u)return 0xFD20u;
    return 0xF800u;
}
static uint8_t power_pct(const dpc245_app_t *c){
    uint32_t scale=c->preferences.graph_scale_w?c->preferences.graph_scale_w:EVIST_PREF_GRAPH_DEFAULT_W;
    uint32_t value=c->power_graph_w>scale?scale:c->power_graph_w;
    return (uint8_t)(value*100u/scale);
}
static uint16_t theme_mode_colour(const dpc245_app_t *c,const bafang_telemetry_t *t,uint32_t now_ms){
    if(c->walk)return bafang_walk_bit_fresh(t,now_ms)||(c->down_held&&t->have_speed&&t->speed_centi_kph>=50u)?0xFDE9u:WHITE;
    return c->assist_level<=5u?mode_frame_colours[c->assist_level]:DASH_GREEN;
}
static const char *theme_mode_name(const dpc245_app_t *c){
    static const char *const labels[]={"0","ECO","TOUR","SPORT","SPORT+","BOOST"};
    if(c->walk)return "WALK";
    return c->assist_level<=5u?labels[c->assist_level]:"MODE";
}
static char theme_mode_letter(const dpc245_app_t *c){
    static const char letters[]={'0','E','T','S','S','B'};
    if(c->walk)return 'W';
    return c->assist_level<=5u?letters[c->assist_level]:'9';
}
static void theme_speed_cell(const ui_font1_t *font,char digit,uint16_t x,uint16_t width,int baseline,uint16_t colour){
    const ui_font1_glyph_t *glyph=speed_glyph(font,digit);
    if(!glyph)return;
    int pen=(int)x+((int)width-(int)glyph->w)/2-(int)glyph->ox;
    char text[2]={digit,0};
    ui_font1_text_clipped(font,(uint16_t)pen,baseline,text,colour,BLACK,x,(uint16_t)(x+width-1u));
}
static void theme_speed(const dpc245_app_t *c,const bafang_telemetry_t *t,
                        uint16_t tens_x,int baseline,int unit_baseline){
    uint32_t tenth=t->have_speed?display_metric(c,t->speed_centi_kph)/10u:0u;
    uint32_t whole=tenth/10u;
    uint16_t colour=c->temp_warning_sources?TEMP_WARN_RED:WHITE;
    if(whole>99u)whole=99u;
    if(whole>=10u)theme_speed_cell(&ui_font1_speed,(char)('0'+whole/10u),tens_x,76u,baseline,colour);
    theme_speed_cell(&ui_font1_speed,(char)('0'+whole%10u),(uint16_t)(tens_x+76u),76u,baseline,colour);
    ui_font1_text_clipped(&ui_font1_speed_small,(uint16_t)(tens_x+152u),baseline,".",colour,BLACK,
                          (uint16_t)(tens_x+152u),(uint16_t)(tens_x+166u));
    theme_speed_cell(&ui_font1_speed_small,(char)('0'+tenth%10u),(uint16_t)(tens_x+167u),39u,baseline,colour);
    ui_font1_text(&ui_font1_small,(uint16_t)(tens_x+157u),unit_baseline,dash_speed_unit(c,true),WHITE,BLACK);
}
static uint16_t theme_isqrt(uint32_t v){
    uint32_t root=0u,bit=1u<<30;
    while(bit>v)bit>>=2;
    while(bit){if(v>=root+bit){v-=root+bit;root=(root>>1)+bit;}else root>>=1;bit>>=2;}
    return (uint16_t)root;
}
static void theme_pair(const dpc245_app_t *c,const bafang_telemetry_t *t,uint32_t now_ms,
                       char left[6],char right[6],const char **ll,const char **rl,const char **lu,const char **ru){
    ui_dashboard_lower_pair_text(c,t,c->preferences.lower_panel,now_ms,left,right,ll,rl,lu,ru);
}
static void theme_right_text(const char *s,const ui_font1_t *font,uint16_t right,int baseline,uint16_t bg){
    uint16_t width=ui_font1_width(font,s);
    ui_font1_text(font,(uint16_t)(right-width),baseline,s,WHITE,bg);
}
static void arc_tile(bool bottom,const char *label,const char *value,const char *unit){
    uint16_t y0=bottom?88u:3u,y1=bottom?167u:81u;
    for(uint16_t y=y0;y<=y1;y++){
        uint16_t left=bottom?(uint16_t)(240u+(y-88u)/4u):
            (uint16_t)(y<23u?257u-(y-3u)*22u/20u:235u+(y-23u)*5u/57u);
        dpc245_lcd_fill_rect(left,y,(uint16_t)(319u-left),1u,0x3186u);
    }
    theme_right_text(label,&ui_font1_dashboard,316u,bottom?112:34,0x3186u);
    theme_right_text(value,&ui_font1_small,316u,bottom?141:59,0x3186u);
    theme_right_text(unit,&ui_font1_dashboard,316u,bottom?160:76,0x3186u);
}
/* Each ARC digit cell clears only its own area inside the fixed frame. */
static void arc_speed_cell(const ui_font1_t *font,char digit,uint16_t x,uint16_t width,uint16_t colour){
    dpc245_lcd_fill_rect(x,ARC_CELL_TOP,width,ARC_CELL_BOTTOM-ARC_CELL_TOP+1u,BLACK);
    if(digit)theme_speed_cell(font,digit,x,width,ARC_SPEED_BASE,colour);
}
static void theme_speed_arc(const dpc245_app_t *c,const bafang_telemetry_t *t){
    uint32_t tenth=t->have_speed?display_metric(c,t->speed_centi_kph)/10u:0u;
    uint32_t whole=tenth/10u;if(whole>99u)whole=99u;
    uint16_t colour=c->temp_warning_sources?TEMP_WARN_RED:WHITE;
    arc_speed_cell(&ui_font1_arc_integer,whole>=10u?(char)('0'+whole/10u):0,ARC_DIGIT_X,ARC_DIGIT_W,colour);
    arc_speed_cell(&ui_font1_arc_integer,(char)('0'+whole%10u),ARC_DIGIT_X+ARC_DIGIT_W,ARC_DIGIT_W,colour);
    dpc245_lcd_fill_rect(ARC_DIGIT_X+2u*ARC_DIGIT_W,ARC_CELL_TOP,ARC_DOT_W,ARC_CELL_BOTTOM-ARC_CELL_TOP+1u,BLACK);
    ui_font1_text_clipped(&ui_font1_arc_tenth,ARC_DIGIT_X+2u*ARC_DIGIT_W,ARC_SPEED_BASE,".",colour,BLACK,
                          ARC_DIGIT_X+2u*ARC_DIGIT_W,ARC_DIGIT_X+2u*ARC_DIGIT_W+ARC_DOT_W-1u);
    arc_speed_cell(&ui_font1_arc_tenth,(char)('0'+tenth%10u),ARC_DIGIT_X+2u*ARC_DIGIT_W+ARC_DOT_W,ARC_TENTH_W,colour);
    const char *unit=dash_speed_unit(c,true);
    ui_font1_text(&ui_font1_small,(uint16_t)(ARC_UNIT_RIGHT-ui_font1_width(&ui_font1_small,unit)),ARC_UNIT_BASE,unit,WHITE,BLACK);
}
/* The power arc, tiles and battery keep clear of the speed frame. */
static void theme_arc_speed_frame(bool offroad){
    uint16_t colour=offroad?ARC_FRAME_OFFROAD_COL:ARC_FRAME_LEGAL_COL;
    dpc245_lcd_fill_rect(ARC_FRAME_LEFT+ARC_FRAME_W-1u,ARC_FRAME_TOP,ARC_FRAME_RIGHT-ARC_FRAME_LEFT-1u,ARC_FRAME_W,colour);
    dpc245_lcd_fill_rect(ARC_FRAME_LEFT+ARC_FRAME_W-1u,ARC_FRAME_BOTTOM-ARC_FRAME_W+1u,
                         ARC_FRAME_RIGHT-ARC_FRAME_LEFT-1u,ARC_FRAME_W,colour);
    dpc245_lcd_fill_rect(ARC_FRAME_LEFT,ARC_FRAME_TOP+ARC_FRAME_W-1u,ARC_FRAME_W,
                         ARC_FRAME_BOTTOM-ARC_FRAME_TOP-ARC_FRAME_W,colour);
    for(uint16_t y=ARC_FRAME_TOP+1u;y<=ARC_FRAME_MID;y++){
        uint16_t x=(uint16_t)(ARC_FRAME_RIGHT+(y-ARC_FRAME_TOP)* (ARC_FRAME_TIP-ARC_FRAME_RIGHT)/(ARC_FRAME_MID-ARC_FRAME_TOP));
        dpc245_lcd_fill_rect(x-ARC_FRAME_W+1u,y,ARC_FRAME_W,1u,colour);
        dpc245_lcd_fill_rect(x-ARC_FRAME_W+1u,(uint16_t)(ARC_FRAME_TOP+ARC_FRAME_BOTTOM-y),ARC_FRAME_W,1u,colour);
    }
}
static void theme_status(const dpc245_ui_status_t *s,bool wedge){
    if(!s)return;
    if(s->light_on)draw_status_glyph(wedge?139u:3u,wedge?8u:2u,status_light);
    if(s->ble_online)draw_status_glyph(wedge?166u:3u,wedge?8u:27u,status_ble);
    if(s->controller_error)draw_status_glyph(wedge?193u:3u,wedge?8u:143u,status_err);
}
static void theme_soc_text(const bafang_telemetry_t *t,char value[6]){
    if(t->have_battery_pct)fmt_u32_5(value,t->battery_pct>100u?100u:t->battery_pct);
    else{value[0]='-';value[1]='-';value[2]=0;}
    unsigned n=0u;while(value[n]&&n<4u)n++;value[n++]='%';value[n]=0;
}
static void theme_banner(const dpc245_app_t *c,uint16_t x,uint16_t y,uint16_t width){
    if(!(c->ride_confirm_pending||c->ride_toast_active||c->ride_apply_result==EVIST_RIDE_APPLY_FAILED))return;
    dpc245_lcd_fill_rect(x,y,width,32u,BLACK);
    const char *line=c->ride_confirm_pending?"OFFROAD?":(c->ride_toast_active?"MODE APPLIED":"RIDE FAILED");
    m_text((uint16_t)(x+5u),(uint16_t)(y+7u),line,0xFFE0u,BLACK,1u);
}
static void draw_arc(const dpc245_app_t *c,const evist_can_router_t *r,const dpc245_ui_status_t *status,uint32_t now_ms){
    theme_clear_regions(arc_regions,sizeof(arc_regions)/sizeof(arc_regions[0]));
    const bafang_telemetry_t *t=&r->telemetry;
    uint8_t pct=power_pct(c);
    for(uint16_t y=24u;y<=143u;y++){
        int32_t dy=(int32_t)y-84;
        uint16_t outer=(uint16_t)(105u-theme_isqrt((uint32_t)(98*98-dy*dy)));
        uint16_t inner=(uint16_t)(105u-theme_isqrt((uint32_t)(78*78-dy*dy)));
        uint8_t position=(uint8_t)((uint32_t)(143u-y)*100u/119u);
        uint16_t colour=position<pct?power_zone_pct(position):0x3186u;
        dpc245_lcd_fill_rect(outer,y,(uint16_t)(inner-outer),1u,colour);
    }
    theme_status(status,false);
    ui_font1_text(&ui_font1_title,31u,17,theme_mode_name(c),theme_mode_colour(c,t,now_ms),BLACK);
    theme_arc_speed_frame(c->ride_indicator==DPC245_RIDE_IND_OFFROAD);
    theme_speed_arc(c,t);
    char left[6],right[6];const char *ll,*rl,*lu,*ru;
    theme_pair(c,t,now_ms,left,right,&ll,&rl,&lu,&ru);
    arc_tile(false,ll,left,lu);
    arc_tile(true,rl,right,ru);
    if(c->ride_confirm_pending||c->ride_toast_active||c->ride_apply_result==EVIST_RIDE_APPLY_FAILED)
        theme_banner(c,30u,135u,208u);
    else if(temperature_warning_visible(c,now_ms)){
        dpc245_lcd_fill_rect(30u,135u,208u,34u,BLACK);
        draw_temperature_warning(c,34u,148u);
    }
    else{
        uint8_t soc=t->have_battery_pct?(t->battery_pct>100u?100u:t->battery_pct):0u;
        char value[6];theme_soc_text(t,value);
        for(uint16_t y=145u;y<169u;y++){
            uint16_t base_left=(uint16_t)(79u-(y-145u)/2u);
            uint16_t base_right=(uint16_t)(224u+(y-145u)/2u);
            dpc245_lcd_fill_rect(base_left,y,(uint16_t)(base_right-base_left),1u,0x3186u);
        }
        dpc245_lcd_fill_rect(34u,138u,29u,18u,WHITE);
        dpc245_lcd_fill_rect(36u,140u,25u,14u,BLACK);
        dpc245_lcd_fill_rect(63u,143u,3u,8u,WHITE);
        if(t->have_battery_pct)dpc245_lcd_fill_rect(38u,142u,(uint16_t)((uint32_t)soc*21u/100u),10u,soc_style(soc).colour);
        ui_font1_text(&ui_font1_title,72u,155,value,WHITE,BLACK);
        dpc245_lcd_fill_rect(72u,162u,160u,6u,0x3186u);
        dpc245_lcd_fill_rect(72u,162u,(uint16_t)((uint32_t)soc*160u/100u),6u,soc_style(soc).colour);
    }
    (void)now_ms;
}
static void draw_wedge(const dpc245_app_t *c,const evist_can_router_t *r,const dpc245_ui_status_t *status,uint32_t now_ms){
    theme_clear_regions(wedge_regions,sizeof(wedge_regions)/sizeof(wedge_regions[0]));
    const bafang_telemetry_t *t=&r->telemetry;
    uint8_t soc=t->have_battery_pct?(t->battery_pct>100u?100u:t->battery_pct):0u;
    dpc245_lcd_fill_rect(10u,8u,30u,17u,WHITE);
    dpc245_lcd_fill_rect(12u,10u,26u,13u,BLACK);
    dpc245_lcd_fill_rect(40u,13u,4u,7u,WHITE);
    if(t->have_battery_pct)dpc245_lcd_fill_rect(14u,12u,(uint16_t)((uint32_t)soc*22u/100u),9u,soc_style(soc).colour);
    char value[6];theme_soc_text(t,value);
    ui_font1_text(&ui_font1_title,50u,27,value,WHITE,BLACK);
    theme_status(status,true);
    bool banner=c->ride_confirm_pending||c->ride_toast_active||c->ride_apply_result==EVIST_RIDE_APPLY_FAILED;
    if(!banner)theme_speed(c,t,8u,133,73);
    uint16_t mode=theme_mode_colour(c,t,now_ms);
    for(uint16_t y=9u;y<143u;y++){
        uint16_t inset=y<27u?(uint16_t)(27u-y):((y>125u)?(uint16_t)(y-125u):0u);
        dpc245_lcd_fill_rect((uint16_t)(216u+inset),y,(uint16_t)(102u-inset),1u,0x2945u);
    }
    for(uint16_t y=49u;y<92u;y++){
        uint16_t tip=y<=70u?(uint16_t)(y-49u):(uint16_t)(91u-y);
        uint16_t cut=y<55u?(uint16_t)(55u-y):y>85u?(uint16_t)(y-85u):0u;
        dpc245_lcd_fill_rect((uint16_t)(216u+cut),y,(uint16_t)(36u+tip/2u-cut),1u,mode);
    }
    char letter[2]={theme_mode_letter(c),0};
    const ui_font1_glyph_t *glyph=badge_glyph(&ui_font1_badge,letter[0]);
    if(glyph){
        uint16_t x=(uint16_t)(238-(int)glyph->w/2-(int)glyph->ox);
        if(c->assist_level==4u&&!c->walk){
            x=217u;
            ui_font1_text(&ui_font1_badge_plus,239u,83,"+",BLACK,mode);
        }
        ui_font1_text(&ui_font1_badge,x,91,letter,BLACK,mode);
    }
    char left[6],right[6];const char *ll,*rl,*lu,*ru;
    theme_pair(c,t,now_ms,left,right,&ll,&rl,&lu,&ru);
    theme_right_text(left,&ui_font1_small,315u,31,0x2945u);
    theme_right_text(ll,&ui_font1_dashboard,315u,45,0x2945u);
    theme_right_text(lu,&ui_font1_dashboard,315u,72,0x2945u);
    dpc245_lcd_fill_rect(261u,78u,55u,2u,0x630Cu);
    theme_right_text(right,&ui_font1_small,315u,110,0x2945u);
    theme_right_text(rl,&ui_font1_dashboard,315u,124,0x2945u);
    theme_right_text(ru,&ui_font1_dashboard,315u,138,0x2945u);
    uint8_t pct=power_pct(c);
    if(!temperature_warning_visible(c,now_ms))for(uint16_t y=148u;y<166u;y++){
        uint16_t width=(uint16_t)(169u+theme_isqrt((uint32_t)(y-148u)*129u*129u/17u));
        dpc245_lcd_fill_rect(11u,y,width,1u,0x2104u);
        uint16_t filled=(uint16_t)((uint32_t)width*pct/100u);
        for(uint8_t zone=0u;zone<4u;zone++){
            uint16_t start=(uint16_t)((uint32_t)width*(zone==0u?0u:(zone==1u?50u:(zone==2u?70u:85u)))/100u);
            uint16_t end=(uint16_t)((uint32_t)width*(zone==0u?50u:(zone==1u?70u:(zone==2u?85u:100u)))/100u);
            if(end>filled)end=filled;
            if(end>start)dpc245_lcd_fill_rect((uint16_t)(11u+start),y,(uint16_t)(end-start),1u,
                                              power_zone_pct((uint8_t)(zone==0u?0u:(zone==1u?50u:(zone==2u?70u:85u)))));
        }
    }
    if(temperature_warning_visible(c,now_ms))draw_temperature_warning(c,10u,149u);
    if(banner)theme_banner(c,10u,103u,200u);
    (void)now_ms;
}

static void draw_menu(const dpc245_app_t *c,const char *heading){
    title(heading);
    size_t count=dpc245_app_menu_count(c->screen);
    size_t first=c->selection>4u?(size_t)c->selection-4u:0u;
    if(count>6u && first+6u>count) first=count-6u;
    for(size_t row=0;row<6u && first+row<count;row++){
        size_t idx=first+row; uint16_t y=(uint16_t)(31u+row*20u);
        bool selected=idx==c->selection;
        /* One selection style for every menu: the green bar of ADV SET. */
        if(selected)dpc245_lcd_fill_rect(3,(uint16_t)(y-3u),314,18,MENU_SEL);
        m_text(8,y,dpc245_app_menu_label(c->screen,idx),WHITE,selected?MENU_SEL:BLACK,1);
        if(c->screen==DPC245_APP_SCREEN_THEMES_MENU && idx<4u){
            const char *state=dpc245_app_theme_row_value(c,(uint8_t)idx);
            if(state)m_text(150u,y,state,WHITE,selected?MENU_SEL:BLACK,1u);
        }
    }
    if(c->screen==DPC245_APP_SCREEN_THEMES_MENU && c->theme_status)
        m_text(8u,151u,menu_status(c->theme_status),0xFFE0u,BLACK,1u);
    else footer("UP/DN  POWER SELECT");
}

static void draw_edit(const char *heading,const char *value){
    title(heading); m_text(25,70,value,WHITE,BLACK,2); footer("UP/DN CHANGE  POWER BACK");
}
static void draw_edit_num(const char *heading,uint32_t v,const char *unit){
    /* Large value with its unit right after it, on the same baseline. */
    char b[11],t[11];unsigned n=0u,k=0u;do{t[k++]=(char)('0'+v%10u);v/=10u;}while(v&&k<10u);while(k)b[n++]=t[--k];b[n]=0;
    title(heading); uint16_t x=ui_font1_text(&ui_font1_value,35u,88,b,WHITE,BLACK);
    if(unit)(void)ui_font1_text(&ui_font1_menu,(uint16_t)(x+8u),88,unit,DIM,BLACK); footer("UP/DN CHANGE  POWER BACK");
}

static void draw_hmi_info(const dpc245_app_t *c,const dpc245_hwinfo_t *hw,uint8_t slot,bool loaded,bool wr){
    title("HMI INFO");
    /* One value column (x 150) for every row, as value_u32 uses. */
    m_text(7,32,"MODEL",DIM,BLACK,1);m_text(150,32,"DP C245.C 3.0",WHITE,BLACK,1);
    m_text(7,52,"APP",DIM,BLACK,1);m_text(150,52,"EVISTDRIVE " EVISTDRIVE_HMI_VERSION,WHITE,BLACK,1);
    value_u32(72,"FLASH",hw?hw->flash_kib:0u,"KIB");
    {   /* "0X" and the four hex digits as one value string. */
        uint32_t m=hw?hw->app_marker:0u;char b[8]={'0','X',0,0,0,0,0,0};
        for(uint8_t i=0u;i<4u;i++){uint8_t d=(uint8_t)((m>>(12u-4u*i))&0xFu);b[2u+i]=(char)(d<10u?'0'+d:'A'+d-10u);}
        m_text(7,92,"MARKER",DIM,BLACK,1);m_text(150,92,b,WHITE,BLACK,1);
    }
    m_text(7,112,"SETTINGS",DIM,BLACK,1);m_text(150,112,loaded?"SPI LOADED":"DEFAULTS",WHITE,BLACK,1);
    value_u32(132,"SLOT",slot,0);
    m_text(190,132,wr?"WRITE ON":"WRITE LOCK",DIM,BLACK,1);
    (void)c; footer("POWER BACK");
}
static void draw_wheel(const evist_can_router_t *r){
    title("WHEEL SIZE");
    m_text(7,48,"WHEEL",DIM,BLACK,1); m_fixed_u32(150,48,r->telemetry.wheel_size_tenth_in,1u,WHITE,BLACK,1); m_text(220,48,"IN",DIM,BLACK,1);
    value_u32(73,"CIRCUMFERENCE",r->telemetry.circumference_mm,"MM");
    footer("POWER BACK");
}
static void draw_speed_limit(const dpc245_app_t *c,const evist_can_router_t *r){
    title("SPEED LIMIT"); m_text(7,60,"LIMIT",DIM,BLACK,1); m_fixed_u32(150,60,display_metric(c,r->telemetry.speed_limit_centi_kph),2u,WHITE,BLACK,1); m_text(220,60,speed_unit(c),DIM,BLACK,1); footer("POWER BACK");
}
static void value_text(uint16_t y,const char *name,const char *v){
    m_text(7,y,name,DIM,BLACK,1);
    uint16_t right=(uint16_t)(7u+ui_font1_width(&ui_font1_menu,name)+8u);
    m_text(right>116u?right:116u,y,(v&&v[0])?v:"--",WHITE,BLACK,1);
}
static uint32_t raw4_le(const uint8_t *p,uint8_t n){
    uint32_t v=0u;uint8_t m=n>4u?4u:n;for(uint8_t i=0u;i<m;i++)v|=(uint32_t)p[i]<<(8u*i);return v;
}
/* "P1/5" as one string, right-aligned at the top right. */
static void draw_page_no(uint8_t page,size_t pages){
    char b[12];unsigned n=0u;uint32_t p=(uint32_t)page+1u;
    b[n++]='P';if(p>=10u)b[n++]=(char)('0'+p/10u%10u);b[n++]=(char)('0'+p%10u);
    b[n++]='/';if(pages>=10u)b[n++]=(char)('0'+pages/10u%10u);b[n++]=(char)('0'+pages%10u);b[n]=0;
    m_text((uint16_t)(314u-ui_font1_width(&ui_font1_menu,b)),6,b,DIM,BLACK,1);
}
static void draw_battery(const dpc245_app_t *c,const evist_can_router_t *r){
    const bafang_telemetry_t *t=&r->telemetry;
    uint8_t page=c->detail_page;
    size_t pages=dpc245_app_detail_page_count(c->screen);
    title("BATTERY INFO"); draw_page_no(page,pages);
    if(page==0u){
        value_i32(28,"TEMP",t->have_battery_state?t->battery_temp_c:0,"C");
        value_x100(50,"VOLTAGE",t->have_battery_state?t->battery_voltage_centi_volt:t->voltage_centi_volt,"V");
        value_x100_i(72,"CURRENT",t->have_battery_state?t->battery_current_centi_amp:t->current_centi_amp,"A");
        value_u32(94,"RES CAP",t->have_battery_capacity?t->battery_remaining_capacity_raw:0u,0);
        m_text(7,126,"NEXT PAGE...",DIM,BLACK,1);
    } else if(page==1u){
        value_u32(34,"FULL CAP",t->have_battery_capacity?t->battery_full_capacity_raw:0u,0);
        value_u32(58,"REL CHARGE",t->have_battery_capacity?t->battery_rsoc_percent:t->battery_pct,"PCT");
        value_u32(82,"ABS CHARGE",t->have_battery_capacity?t->battery_asoc_percent:0u,"PCT");
        m_text(7,106,"NEXT PAGE...",DIM,BLACK,1);
    } else if(page==2u){
        m_text(7,31,"CHARGE 0X",DIM,BLACK,1);
        if(r->battery_service.charging_len)m_hex32(104,31,raw4_le(r->battery_service.charging_raw,r->battery_service.charging_len),8,WHITE,BLACK,1);else m_text(104,31,"--",WHITE,BLACK,1);
        value_u32(55,"CHARGE LEN",r->battery_service.charging_len,0);
        m_text(7,79,"CHAR 0X",DIM,BLACK,1);
        if(r->battery_service.characteristics_len)m_hex32(104,79,raw4_le(r->battery_service.characteristics_raw,r->battery_service.characteristics_len),8,WHITE,BLACK,1);else m_text(104,79,"--",WHITE,BLACK,1);
        value_u32(103,"CHAR LEN",r->battery_service.characteristics_len,0);
        m_text(7,131,"UNLABELLED FIELDS KEPT RAW",DIM,BLACK,1);
    } else if(page==3u){
        for(uint8_t i=0u;i<8u;i++){
            uint16_t x=i<4u?7u:165u;
            uint16_t y=(uint16_t)(30u+(i&3u)*27u);
            m_text(x,y,"CELL",DIM,BLACK,1); m_u32((uint16_t)(x+36u),y,(uint32_t)i+1u,DIM,BLACK,1);
            if((r->battery_service.cell_valid_mask&((uint32_t)1u<<i))!=0u)
                m_u32((uint16_t)(x+60u),y,r->battery_service.cell_mv[i],WHITE,BLACK,1);
            else m_text((uint16_t)(x+60u),y,"--",WHITE,BLACK,1);
            m_text((uint16_t)(x+104u),y,"MV",DIM,BLACK,1);
        }
    } else {
        value_text(30,"HARDWARE",r->battery_identity.hardware);
        value_text(53,"SOFTWARE",r->battery_identity.software);
        value_text(76,"MODEL",r->battery_identity.model);
        value_text(99,"SERIAL",r->battery_identity.serial);
        value_text(122,"MFG",r->battery_identity.manufacturer);
    }
    footer("UP/DN PAGE  POWER BACK");
}
static void draw_controller(const dpc245_app_t *c,const evist_can_router_t *r){
    const bafang_telemetry_t *t=&r->telemetry;
    uint8_t page=c->detail_page;
    title("CONTROLLER INFO"); draw_page_no(page,dpc245_app_detail_page_count(c->screen));
    if(page==0u){
        value_x100(34,"SPEED",t->speed_centi_kph,"KM/H");
        value_x100_i(55,"CURRENT",t->current_centi_amp,"A");
        value_x100(76,"VOLTAGE",t->voltage_centi_volt,"V");
        if(t->have_controller_temp&&bafang_temperature_raw_valid(t->controller_temp_raw))
            value_i32(97,"CTRL TEMP",t->controller_temp_c,"C");
        else value_text(97,"CTRL TEMP","--");
        value_u32(118,"STAT BIT0",t->have_controller_status_bit0?(t->controller_status_bit0?1u:0u):0u,0);
        if(t->have_controller_fast5_raw){m_text(7,139,"3205 RAW",DIM,BLACK,1);m_hex32(150,139,t->controller_fast5_raw,4,WHITE,BLACK,1);}
    }else{
        value_text(30,"HARDWARE",r->controller_identity.hardware);
        value_text(53,"SOFTWARE",r->controller_identity.software);
        value_text(76,"MODEL",r->controller_identity.model);
        value_text(99,"SERIAL",r->controller_identity.serial);
        value_text(122,"MFG",r->controller_identity.manufacturer);
    }
    footer("UP/DN PAGE  POWER BACK");
}
static void draw_sensor(const dpc245_app_t *c,const evist_can_router_t *r,const dpc245_adc_snapshot_t *a){
    const bafang_telemetry_t *t=&r->telemetry;
    uint8_t page=c->detail_page;
    title("SENSOR INFO"); draw_page_no(page,dpc245_app_detail_page_count(c->screen));
    if(page==0u){
        value_u32(31,"TORQUE RAW",t->torque_sensor_raw,0);
        value_u32(52,"TORQUE MV",t->torque_signal_mv,"MV");
        value_u32(73,"CADENCE",t->torque_sensor_cadence_rpm?t->torque_sensor_cadence_rpm:t->cadence_rpm,"RPM");
        value_u32(94,"ALS",a&&a->valid?a->als_raw:0u,"ADC");
        value_u32(115,"BUTTON SIG",a&&a->valid?a->button_signal_raw:0u,"ADC");
    }else{
        value_text(30,"HARDWARE",r->sensor_identity.hardware);
        value_text(53,"SOFTWARE",r->sensor_identity.software);
        value_text(76,"MODEL",r->sensor_identity.model);
        value_text(99,"SERIAL",r->sensor_identity.serial);
        value_text(122,"CUSTOMER",r->sensor_identity.customer);
    }
    footer("UP/DN PAGE  POWER BACK");
}
static void draw_errors(const dpc245_app_t *c,const evist_can_router_t *r){
    title("ERROR CODE");
    size_t n=dpc245_error_count(&c->runtime.settings);
    size_t first=n>6u?n-6u:0u;
    for(size_t row=0u;row<6u;row++){
        /* 17 px rows so six entries and CAN RX stay clear of the key hints. */
        uint16_t y=(uint16_t)(28u+row*17u);
        size_t idx=first+row;
        m_text(8,y,"E-CODE",DIM,BLACK,1); m_u32(90,y,(uint32_t)(idx+1u),DIM,BLACK,1);
        if(idx<n) m_u32(110,y,dpc245_error_at(&c->runtime.settings,idx),WHITE,BLACK,1);
        else m_text(110,y,"--",DIM,BLACK,1);
    }
    m_text(182,28,"CTRL",DIM,BLACK,1); if(c->runtime.have_controller_error)m_u32(232,28,c->runtime.controller_error,WHITE,BLACK,1);
    m_text(182,45,"SENS",DIM,BLACK,1); if(c->runtime.have_sensor_error)m_u32(232,45,c->runtime.sensor_error,WHITE,BLACK,1);
    m_text(182,62,"BATT",DIM,BLACK,1); if(c->runtime.have_battery_error)m_u32(232,62,c->runtime.battery_error,WHITE,BLACK,1);
    value_u32(134,"CAN RX",r->rx_sequence,0);
    footer("POWER BACK");
}
static void draw_trip_reset(void){
    title("TRIP RESET"); m_text(30,64,"RESET TRIP DATA",WHITE,BLACK,2); m_text(45,98,"POWER CONFIRM",DIM,BLACK,1); m_text(45,118,"DOWN CANCEL",DIM,BLACK,1);
}


static void adv_row(uint16_t y,const char *name,const char *value,bool selected,bool edit){
    uint16_t bg=selected?(edit?0xFFE0u:MENU_SEL):BLACK;uint16_t fg=selected?BLACK:WHITE;
    if(selected)dpc245_lcd_fill_rect(3u,(uint16_t)(y-2u),274u,18u,bg);
    m_text(8u,y,name,fg,bg,1u);if(value)m_text(150u,y,value,fg,bg,1u);
}
static void adv_u16(char b[8],uint16_t v){char t[6];fmt_u32_5(t,v);unsigned i=0u;while(t[i]&&i<5u){b[i]=t[i];i++;}b[i]=0;}
/* "25.00" from centi-units (0..655.35) */
static void adv_centi(char b[8],uint16_t v){
    char t[6];fmt_u32_5(t,(uint16_t)(v/100u));unsigned i=0u;while(t[i]&&i<3u){b[i]=t[i];i++;}
    b[i++]='.';b[i++]=(char)('0'+(v/10u)%10u);b[i++]=(char)('0'+v%10u);b[i]=0;
}
static void adv_status_or_footer(const evist_advanced_t *a,const char *hint){
    if(a->status&&a->status[0])m_text(8u,151u,menu_status(a->status),0xFFE0u,BLACK,1u);else footer(hint);
}
static void draw_adv_wheel(const evist_advanced_t *a){
    static const char *const names[EVIST_WHEEL_ROWS]={"MAX SPEED","CIRC MM","DIAMETER","SAVE","BACK"};
    for(uint8_t i=0u;i<EVIST_WHEEL_ROWS;i++){
        char v[8]={0};const char *val="";
        if(i<EVIST_WHEEL_ROW_SAVE&&!a->wheel_valid)val="--";
        else if(i==EVIST_WHEEL_ROW_SPEED){adv_centi(v,a->wheel_speed_centi_kph);val=v;}
        else if(i==EVIST_WHEEL_ROW_CIRC){adv_u16(v,a->wheel_circumference_mm);val=v;}
        else if(i==EVIST_WHEEL_ROW_DIAMETER)val=evist_advanced_wheel_diameter_label(a);
        adv_row((uint16_t)(42u+i*22u),names[i],val,i==a->cursor,a->edit&&i==a->cursor);
    }
    if(a->wheel_dirty)m_text(270u,10u,"EDIT",0xFFE0u,BLACK,1u);
    adv_status_or_footer(a,a->edit?"UP/DN VALUE  POWER DONE":"POWER SELECT");
}
/* Enabled modes have two value rows each, then SAVE and BACK. */
static void draw_adv_assist(const evist_advanced_t *a){
    uint8_t cells=evist_advanced_assist_cells(a),rows=(uint8_t)(cells+2u);
    uint8_t top=a->cursor>4u?(uint8_t)(a->cursor-4u):0u;if(rows>5u&&top+5u>rows)top=(uint8_t)(rows-5u);
    for(uint8_t row=0u;row<5u&&top+row<rows;++row){
        uint8_t idx=(uint8_t)(top+row);char name[12]={0};char v[8]={0};const char *val=v;
        if(idx==cells){name[0]='S';name[1]='A';name[2]='V';name[3]='E';val="";}
        else if(idx==cells+1u){name[0]='B';name[1]='A';name[2]='C';name[3]='K';val="";}
        else{
            const char *col=(idx&1u)?(a->committed.firmware_profile==EVIST_PROFILE_FAKE_TAXI?" ASSIST%":" SPEED%"):" POWER%";
            static const char *const modes[EVIST_ASSIST_MODE_COUNT]={"E","T","S","S+","B"};
            uint8_t level=evist_advanced_assist_cell_level(a,idx);
            unsigned n=0u;
            if(level>=1u&&level<=EVIST_ASSIST_MODE_COUNT)
                for(const char *p=modes[level-1u];*p&&n<sizeof(name)-1u;++p)name[n++]=*p;
            for(unsigned k=0u;col[k]&&n<sizeof(name)-1u;k++)name[n++]=col[k];
            if(a->assist_valid)adv_u16(v,evist_advanced_assist_cell_value(a,idx));else val="--";
        }
        adv_row((uint16_t)(42u+row*22u),name,val,idx==a->cursor,a->edit&&idx==a->cursor);
    }
    if(cells==0u)m_text(8u,104u,"NO MODES ON",DIM,BLACK,1u);
    if(a->assist_dirty)m_text(270u,10u,"EDIT",0xFFE0u,BLACK,1u);
    adv_status_or_footer(a,a->edit?"UP/DN VALUE  POWER DONE":"POWER SELECT");
}
/* DIAG (read-only, page-based layout). Fields without a data source show
 * "--" or NOT TRACKED instead of a value. */
static void diag_u32(uint16_t y,const char *name,bool have,uint32_t v,const char *unit){
    if(have)value_u32(y,name,v,unit);else value_text(y,name,"--");
}
static void diag_i32(uint16_t y,const char *name,bool have,int32_t v,const char *unit){
    if(have)value_i32(y,name,v,unit);else value_text(y,name,"--");
}
static void diag_append(char *out,unsigned *n,const char *s){
    while(*s&&*n<31u)out[(*n)++]=*s++;
    out[*n]=0;
}
static void diag_number(char *out,unsigned *n,uint32_t v){
    char d[11];unsigned k=0u;
    do{d[k++]=(char)('0'+v%10u);v/=10u;}while(v&&k<10u);
    while(k&&*n<31u)out[(*n)++]=d[--k];
    out[*n]=0;
}
static void diag_elapsed_ms(char *out,unsigned *n,uint32_t v){
    if(v>9999u)diag_append(out,n,">9999MS");
    else{diag_number(out,n,v);diag_append(out,n,"MS");}
}
static void diag_signed(char *out,unsigned *n,int16_t v){
    if(v<0){diag_append(out,n,"-");diag_number(out,n,(uint32_t)(-(int32_t)v));}
    else diag_number(out,n,(uint32_t)v);
}
static void diag_hex_byte(char *out,unsigned *n,uint8_t v){
    static const char hex[]="0123456789ABCDEF";
    if(*n<29u){out[(*n)++]=hex[v>>4];out[(*n)++]=hex[v&15u];out[*n]=0;}
}
static const char *diag_sources(uint8_t bits){
    switch(bits&3u){
    case 1u:return "M";
    case 2u:return "C";
    case 3u:return "MC";
    default:return "-";
    }
}
static void diag_stage(uint16_t x,uint16_t y,const char *name,uint32_t us){
    m_text(x,y,name,WHITE,BLACK,1u);
    if(us>999999u)m_text((uint16_t)(x+54u),y,">999999",WHITE,BLACK,1u);
    else m_u32((uint16_t)(x+54u),y,us,WHITE,BLACK,1u);
}
static const char *ble_reject_text(uint8_t reason){
    switch(reason){
    case DPC245_BLE_REJECT_CLASS: return "CLASS";
    case DPC245_BLE_REJECT_READ_REQUIRED: return "READ REQUIRED";
    case DPC245_BLE_REJECT_BUSY: return "TX BUSY";
    case DPC245_BLE_REJECT_GLOBAL_MODE: return "GLOBAL MODE";
    case DPC245_BLE_REJECT_PAYLOAD: return "BAD PAYLOAD";
    case DPC245_BLE_REJECT_POLICY: return "WRITE POLICY";
    default: return "UNSUPPORTED";
    }
}
static const char *diag_result_text(evist_txn_result_t result){
    switch(result){
    case EVIST_TXN_NO_ACK:return "NO CTRL ACK";
    case EVIST_TXN_TX_TIMEOUT:return "TX TIMEOUT";
    case EVIST_TXN_MISMATCH:return "VERIFY DIFF";
    case EVIST_TXN_REJECTED:return "CTRL REJECTED";
    default:return evist_txn_result_text(result);
    }
}
static void diag_live_source(uint16_t y,const dpc245_temp_live_source_t *live,
                             bool router_valid,int16_t router_c,const char *values_label,
                             const char *age_label,const char *transition_label){
    char b[32]={0};unsigned n=0u;
    if(live->sample_received&&live->sensor_valid)diag_signed(b,&n,live->sampled_c);else diag_append(b,&n,"--");
    diag_append(b,&n,"/");
    if(router_valid)diag_signed(b,&n,router_c);else diag_append(b,&n,"--");
    diag_append(b,&n,"/");diag_number(b,&n,live->limit_c);
    value_text(y,values_label,b);
    n=0u;b[0]=0;
    diag_append(b,&n,live->decision_valid?"OK":"NO");diag_append(b,&n,"/");
    diag_append(b,&n,live->fresh?"OK ":"NO ");
    if(live->sample_received)diag_elapsed_ms(b,&n,live->sample_age_ms);
    else diag_append(b,&n,"--");
    value_text((uint16_t)(y+17u),age_label,b);
    n=0u;b[0]=0;
    diag_append(b,&n,live->transition?"ON":"OFF");diag_append(b,&n,"/");
    diag_append(b,&n,live->pending?"ON ":"OFF ");
    if(live->pending)diag_elapsed_ms(b,&n,live->pending_ms);
    else diag_append(b,&n,"--");
    value_text((uint16_t)(y+34u),transition_label,b);
}
static void draw_adv_diag(const dpc245_app_t *c,const evist_can_router_t *r,uint8_t page,uint32_t now_ms){
    const bafang_telemetry_t *t=&r->telemetry;
    draw_page_no(page,EVIST_DIAG_PAGES);
    if(page==0u){
        diag_u32(30,"SOC RAW",t->have_battery_pct,t->battery_pct,"%");
        if(t->have_voltage)value_x100(50,"VOLTAGE",t->voltage_centi_volt,"V");else value_text(50,"VOLTAGE","--");
        diag_i32(70,"BAT TEMP",t->have_battery_state,t->battery_temp_c,"C");
        diag_u32(90,"CAP RAW",t->have_battery_capacity,t->battery_remaining_capacity_raw,0);
        value_text(110,"ACCEL MODE",c->advanced.accel_mode==EVIST_ACCEL_PER_LEVEL?"PER LEVEL":
                   (c->advanced.accel_mode==EVIST_ACCEL_GLOBAL?"GLOBAL":"UNKNOWN"));
        if(c->start_angle_status==3u)value_u32(130,"START ANGLE",c->start_angle_raw,0);
        else if(c->start_angle_status==1u)value_text(130,"START ANGLE","READING");
        else if(c->start_angle_status==4u)value_text(130,"START ANGLE","ERROR");
        else value_text(130,"START ANGLE","--");
    }else if(page==1u){
        value_u32(28,"TORQUE RAW",t->torque_sensor_raw,0);
        value_u32(45,"TORQUE MV",t->torque_signal_mv,"MV");
        value_u32(62,"CADENCE",t->torque_sensor_cadence_rpm?t->torque_sensor_cadence_rpm:t->cadence_rpm,"RPM");
        value_i32(79,"POWER",bafang_power_w(t),"W");
        { uint16_t pedal;
          if(dpc245_runtime_pedal_avg(&c->runtime,&pedal))value_u32(96,"PEDAL W",pedal,"W");
          else value_text(96,"PEDAL W","--"); }
        diag_i32(113,"CTRL TEMP",t->have_controller_temp&&bafang_temperature_raw_valid(t->controller_temp_raw),t->controller_temp_c,"C");
        diag_i32(130,"MOTOR TEMP",t->have_motor_temp&&bafang_temperature_raw_valid(t->controller_aux_temp_raw),t->motor_temp_c,"C");
    }else if(page==2u){
        /* Seven rows fit between the title and footer. */
        char hold[12]={0};uint16_t x=(uint16_t)c->preferences.quick_walk_hold_steps*5u;
        const char *state=c->preferences.quick_walk_enabled?"ON ":"OFF ";
        unsigned n=0u;while(state[n]){hold[n]=state[n];n++;}
        hold[n++]=(char)('0'+x/10u);hold[n++]='.';
        hold[n++]=(char)('0'+x%10u);hold[n++]='S';hold[n]=0;
        value_text(28,"QWALK",hold);
        value_x100(45,"LEGAL SPD",display_metric(c,c->ride_cfg.legal_x100),speed_unit(c));
        value_u32(62,"ASSIST LVL",c->assist_level,0);
        value_text(79,"RIDE MODE",c->ride_cfg.mode==EVIST_RIDE_OFFROAD?"OFFROAD":"LEGAL");
        value_text(96,"SESSION",c->ride_session==DPC245_RIDE_SESSION_OFFROAD?"OFFROAD":
            c->ride_session==DPC245_RIDE_SESSION_LEGAL?"LEGAL":"DEFAULT");
        if(c->ride_last_applied_x100)value_x100(113,"3203 LAST",c->ride_last_applied_x100,"KM/H");
        else value_text(113,"3203 LAST","--");
        const char *apply="PENDING";
        if(c->ride_apply_result==EVIST_RIDE_APPLY_OK)apply="OK";
        else if(c->ride_apply_result==EVIST_RIDE_APPLY_FAILED){
            if(c->ride_last_error==EVIST_TXN_NACK)apply="NACK";
            else if(c->ride_last_error==EVIST_TXN_NO_ACK)apply="NO ACK";
            else if(c->ride_last_error==EVIST_TXN_NO_READBACK)apply="TIMEOUT";
            else if(c->ride_last_error==EVIST_TXN_MISMATCH)apply="MISMATCH";
            else apply="FAILED";
        }
        value_text(130,"APPLY",apply);
    }else if(page==3u){
        value_u32(28,"KEY EVENTS",c->key_events,0);
        /* Number and unit as one string: up to 10 digits fit. */
        char t[16];unsigned n=0u;char d[11];unsigned k;uint32_t v;
        v=c->loop_max_us;k=0u;do{d[k++]=(char)('0'+v%10u);v/=10u;}while(v&&k<10u);while(k)t[n++]=d[--k];
        t[n++]=' ';t[n++]='U';t[n++]='S';t[n]=0;value_text(46,"LOOP MAX",t);
        n=0u;v=c->loop_1s_max_us;k=0u;do{d[k++]=(char)('0'+v%10u);v/=10u;}while(v&&k<10u);while(k)t[n++]=d[--k];
        t[n++]=' ';t[n++]='U';t[n++]='S';t[n]=0;value_text(64,"LOOP 1S",t);
        static const char *const names[8]={"CANR","BLER","ADC","APP","SPI","CANT","BLET","LCD"};
        for(uint8_t i=0u;i<8u;i++)
            diag_stage((i&1u)?164u:7u,(uint16_t)(82u+(i/2u)*15u),names[i],c->stage_max_us[i]);
    }else if(page==4u){
        m_text(7u,28u,"WRITE CMD",WHITE,BLACK,1u);
        if(c->last_write_seen)m_hex32(150u,28u,c->last_write_command,4u,WHITE,BLACK,1u);
        else m_text(150u,28u,"--",WHITE,BLACK,1u);
        value_u32(46u,"ACK COUNT",c->last_write_acks,0);
        value_text(64u,"FINAL ACK",c->last_write_final_ack?"YES":"NO");
        m_text(7u,82u,"ERROR ACK",WHITE,BLACK,1u);
        if(c->last_write_error){
            for(uint8_t i=0u;i<4u;i++)
                m_hex32((uint16_t)(150u+i*24u),82u,c->last_write_error_payload[i],2u,WHITE,BLACK,1u);
        }else m_text(150u,82u,"--",WHITE,BLACK,1u);
        value_text(100u,"RESULT",c->last_write_seen?diag_result_text(c->last_write_result):"--");
        m_text(7u,116u,"BLE REJECT",WHITE,BLACK,1u);
        m_u32(140u,116u,c->ble_rejected_writes,WHITE,BLACK,1u);
        if(c->ble_rejected_writes){
            m_hex32(194u,116u,c->ble_rejected_class,2u,WHITE,BLACK,1u);
            m_hex32(218u,116u,c->ble_rejected_command,2u,WHITE,BLACK,1u);
            m_hex32(242u,116u,c->ble_rejected_operation,2u,WHITE,BLACK,1u);
            m_hex32(266u,116u,c->ble_rejected_selector,2u,WHITE,BLACK,1u);
        }
        value_text(132u,"BLE REASON",c->ble_rejected_writes?
                   ble_reject_text(c->ble_rejected_reason):"--");
    }else if(page==5u){
        const dpc245_temp_warning_record_t *w=&c->temp_warning_last;
        bool have=c->temp_warning_activations!=0u;
        char b[32]={0};unsigned n=0u;
        diag_number(b,&n,c->temp_warning_activations);diag_append(b,&n," / ");
        diag_append(b,&n,diag_sources(c->temp_warning_sources));value_text(28u,"COUNT / NOW",b);
        n=0u;b[0]=0;
        if(have){diag_number(b,&n,w->uptime_s);
                 diag_append(b,&n,diag_sources(w->sources));
                 diag_number(b,&n,w->motor_limit_c);diag_append(b,&n,"/");
                 diag_number(b,&n,w->ctrl_limit_c);}
        else diag_append(b,&n,"--");
        value_text(45u,"ON S SRC L",b);
        n=0u;b[0]=0;
        if(have){if(w->motor_valid)diag_signed(b,&n,w->motor_c);else diag_append(b,&n,"--");
                 diag_append(b,&n,"/");
                 if(w->ctrl_valid)diag_signed(b,&n,w->ctrl_c);else diag_append(b,&n,"--");
                 diag_append(b,&n," ");diag_append(b,&n,w->motor_valid?"OK":"NO");
                 diag_append(b,&n,"/");diag_append(b,&n,w->ctrl_valid?"OK":"NO");}
        else diag_append(b,&n,"--");
        value_text(62u,"TEMP/VALID M/C",b);
        n=0u;b[0]=0;
        if(have){diag_number(b,&n,w->sample_age_ms);diag_append(b,&n," / ");
                 diag_hex_byte(b,&n,w->raw6);diag_append(b,&n," ");diag_hex_byte(b,&n,w->raw7);}
        else diag_append(b,&n,"--");
        value_text(79u,"AGE MS / RAW",b);
        n=0u;b[0]=0;
        if(c->temp_motor_max_valid)diag_signed(b,&n,c->temp_motor_max_c);else diag_append(b,&n,"--");
        diag_append(b,&n," / ");
        if(c->temp_ctrl_max_valid)diag_signed(b,&n,c->temp_ctrl_max_c);else diag_append(b,&n,"--");
        value_text(96u,"MAX M/C",b);
        n=0u;b[0]=0;
        diag_number(b,&n,c->preferences.motor_temp_limit_c);diag_append(b,&n,"/");
        diag_number(b,&n,c->preferences.ctrl_temp_limit_c);
        value_text(113u,"LIMIT USED M/C",b);
        n=0u;b[0]=0;
        diag_number(b,&n,c->advanced.working.motor_temp_limit_c);diag_append(b,&n,"/");
        diag_number(b,&n,c->advanced.working.ctrl_temp_limit_c);
        value_text(130u,"LIMIT MENU M/C",b);
    }else{
        char b[32]={0};unsigned n=0u;
        diag_append(b,&n,diag_sources(c->temp_warning_sources));diag_append(b,&n,"/");
        if(c->temp_screen_seen){
            diag_append(b,&n,diag_sources(c->temp_screen_sources));diag_append(b,&n," ");
            uint32_t age_s=(uint32_t)(now_ms-c->temp_screen_render_ms)/1000u;
            if(age_s>99999u)diag_append(b,&n,">99999");
            else diag_number(b,&n,age_s);
            diag_append(b,&n,"S");
        }else diag_append(b,&n,"-- --");
        value_text(28u,"APP/SCR AGE",b);
        diag_live_source(45u,&c->temp_live[0],t->have_motor_temp&&bafang_temperature_raw_valid(t->controller_aux_temp_raw),t->motor_temp_c,
                         "M APP/RT/LIM","M VAL/FRESH","M TRANS/PEND");
        diag_live_source(96u,&c->temp_live[1],t->have_controller_temp&&bafang_temperature_raw_valid(t->controller_temp_raw),t->controller_temp_c,
                         "C APP/RT/LIM","C VAL/FRESH","C TRANS/PEND");
    }
    footer(page>=5u?"M=MOTOR C=CTRL U/D PWR BK":"UP/DN PAGE  POWER BACK");
}

static void draw_advanced(const dpc245_app_t *c,const evist_can_router_t *r,uint32_t now_ms){
    const evist_advanced_t *a=&c->advanced;
    if(a->screen==EVIST_ADV_DIAG&&a->diag_page>=5u)
        title(a->diag_page==5u?"TEMP WARN":"TEMP NOW");
    else if(a->screen==EVIST_ADV_ACCEL)
        title("ACCELERATION");
    else title(evist_advanced_screen_title(a->screen));
    if(a->screen==EVIST_ADV_DIAG){draw_adv_diag(c,r,a->diag_page,now_ms);return;}
    if(a->screen==EVIST_ADV_LIGHT){
        /* Live preview of the sensor and backlight the settings act on ("ALS / BL"). */
        char als[8],bl[8];fmt_u32_5(als,c->light_als_filtered);fmt_u32_5(bl,c->light_backlight_position);
        uint16_t x=(uint16_t)ui_font1_text(&ui_font1_menu,7u,37,"ALS ",DIM,BLACK);
        x=ui_font1_text(&ui_font1_menu,x,37,als,WHITE,BLACK);
        x=ui_font1_text(&ui_font1_menu,(uint16_t)(x+16u),37,"BL ",DIM,BLACK);
        x=ui_font1_text(&ui_font1_menu,x,37,bl,WHITE,BLACK);
        (void)ui_font1_text(&ui_font1_menu,x,37," / 16",DIM,BLACK);
    }
    if(a->screen==EVIST_ADV_MENU){
        uint8_t count=(uint8_t)evist_advanced_row_count(a);
        uint8_t top=a->cursor>4u?(uint8_t)(a->cursor-4u):0u;if(top+5u>count)top=(uint8_t)(count-5u);
        for(uint8_t row=0u;row<5u;++row){uint8_t idx=(uint8_t)(top+row);adv_row((uint16_t)(42u+row*22u),evist_advanced_menu_label(idx),0,idx==a->cursor,false);}
        footer("UP/DN  POWER SELECT");return;
    }
    if(a->screen==EVIST_ADV_WHEEL){draw_adv_wheel(a);return;}
    if(a->screen==EVIST_ADV_ASSIST){draw_adv_assist(a);return;}
    if(a->screen==EVIST_ADV_ACCEL){
        uint8_t rows=(uint8_t)evist_advanced_row_count(a),values=(uint8_t)(rows-2u);
        uint8_t top=a->cursor>4u?(uint8_t)(a->cursor-4u):0u;
        if(rows>5u&&top+5u>rows)top=(uint8_t)(rows-5u);
        for(uint8_t row=0u;row<5u&&top+row<rows;++row){
            uint8_t idx=(uint8_t)(top+row);char name[8]={0},val[8]={0};const char *label=name,*value=val;
            if(idx==values){label="SAVE";value="";}
            else if(idx==values+1u){label="BACK";value="";}
            else if(a->accel_mode==EVIST_ACCEL_PER_LEVEL){
                name[0]='L';name[1]=(char)('1'+idx);
                if(a->accel_valid)adv_u16(val,a->accel_values[evist_assist_level_slot(a->assist_level_count,(uint8_t)(idx+1u))]);
                else value="--";
            }else{
                label=a->accel_mode==EVIST_ACCEL_GLOBAL?"GLOBAL":"MODE";
                if(a->accel_valid)adv_u16(val,a->accel_global);else value="--";
            }
            adv_row((uint16_t)(42u+row*22u),label,value,idx==a->cursor,a->edit&&idx==a->cursor);
        }
        if(a->accel_dirty)m_text(270u,10u,"EDIT",0xFFE0u,BLACK,1u);
        adv_status_or_footer(a,a->edit?"UP/DN VALUE  POWER DONE":"POWER SELECT");return;
    }
    size_t rows=evist_advanced_row_count(a);uint8_t top=a->cursor>4u?(uint8_t)(a->cursor-4u):0u;if(rows>5u&&top+5u>rows)top=(uint8_t)(rows-5u);
    for(uint8_t row=0u;row<5u&&top+row<rows;++row){
        uint8_t idx=(uint8_t)(top+row);char v[16]={0};const char *name="";const char *val=v;
        uint8_t save=(uint8_t)(rows-2u),back=(uint8_t)(rows-1u);
        if(idx==save){name="SAVE";val="";}else if(idx==back){name="BACK";val="";}
        else switch(a->screen){
        case EVIST_ADV_GRAPH:name="SCALE";adv_u16(v,a->working.graph_scale_w);break;
        case EVIST_ADV_FIRMWARE:name="PROFILE";val=a->working.firmware_profile==EVIST_PROFILE_BAFANG?"BAFANG":"FAKE TAXI";break;
        case EVIST_ADV_BATTERY:
            if(idx==0u){name="ICON";val=(a->working.battery_mask&EVIST_BATTERY_ICON)?"ON":"OFF";}
            else if(idx==1u){name="SOC %";val=(a->working.battery_mask&EVIST_BATTERY_SOC)?"ON":"OFF";}
            else if(idx==2u){name="VOLT";val=(a->working.battery_mask&EVIST_BATTERY_VOLT)?"ON":"OFF";}
            else{name="BAR";val=(a->working.battery_mask&EVIST_BATTERY_BAR)?"ON":"OFF";}break;
        case EVIST_ADV_LIGHT:
            if(idx==0u){name="MODE";val=a->working.light_mode==EVIST_PREF_LIGHT_AUTO?"AUTO":"MANUAL";}
            else if(idx==1u){name="GAIN";adv_u16(v,a->working.light_gain);}
            else if(idx==2u){name="DWELL";adv_u16(v,a->working.light_dwell);}
            else if(idx==3u){name="THR";adv_u16(v,a->working.light_threshold);}
            else {name="LEVEL";adv_u16(v,a->working.light_level);}break;
        case EVIST_ADV_PANELS:{static const char *names[EVIST_PANEL_COUNT]={"TRIP+ODO","MAX+AVG","CAD+CAL","CAD+TRQ","TIME+TRP","WATT+TMP","PDL+MOT","RNG+VOLT","VOLT+TMP","TEMP"};name=names[idx];val=(a->working.panel_mask&(uint16_t)(1u<<idx))?"ON":"OFF";break;}
        case EVIST_ADV_TEMP_VIEW:
            if(idx==0u){name="VIEW";val=a->working.temp_view==EVIST_TEMP_1?"TEMP 1":a->working.temp_view==EVIST_TEMP_2?"TEMP 2":a->working.temp_view==EVIST_TEMP_ALTERNATE?"ALTERNATE":"BOTH";}
            else{
                uint8_t limit=idx==1u?a->working.motor_temp_limit_c:a->working.ctrl_temp_limit_c;
                name=idx==1u?"MOTOR LIMIT":"CTRL LIMIT";
                ui_dashboard_temp_limit_text(v,limit);
            }
            break;
        case EVIST_ADV_QUICK_WALK:
            if(idx==0u){name="ENABLE";val=a->working.quick_walk_enabled?"ON":"OFF";}else{name="HOLD";uint16_t x=(uint16_t)a->working.quick_walk_hold_steps*5u;v[0]=(char)('0'+x/10u);v[1]='.';v[2]=(char)('0'+x%10u);v[3]='S';v[4]=0;}break;
        case EVIST_ADV_MODES:{
            static const char *const modes[EVIST_ASSIST_MODE_COUNT]={"E","T","S","S+","B"};
            name=modes[idx];val=(a->working.assist_mode_mask&(uint8_t)(1u<<idx))?"ON":"OFF";
            break;
        }
        case EVIST_ADV_MODE:
            if(idx==0u){name="DEFAULT";val=a->ride_working.mode==EVIST_RIDE_OFFROAD?"OFFROAD":"LEGAL";}
            else if(idx==1u){name="AFTER START";val=a->ride_working.start==EVIST_RIDE_START_LAST?"LAST":
                a->ride_working.start==EVIST_RIDE_START_OFFROAD?"OFFROAD":"LEGAL";}
            else if(idx==2u){name="LEGAL SPEED";ride_speed_text(v,c,a->ride_working.legal_x100,true);}
            else {name="OFFRD SPD";ride_speed_text(v,c,a->ride_working.offroad_x100,true);}
            break;
        default:break;
        }
        adv_row((uint16_t)(42u+row*22u),name,val,idx==a->cursor,a->edit&&idx==a->cursor);
    }
    adv_status_or_footer(a,a->edit?"UP/DN VALUE  POWER DONE":"UP/DN  POWER SELECT");
}

void ui_dashboard_draw(const dpc245_app_t *c,const evist_can_router_t *r,const dpc245_adc_snapshot_t *a,const dpc245_hwinfo_t *hw,uint8_t slot,bool loaded,bool persist_write_enabled,bool can_tx_enabled,uint32_t now_ms,const dpc245_ui_status_t *status){
    if(!c||!r)return; apply_theme(c);
    g_menu_screen=c->screen!=DPC245_APP_SCREEN_DASHBOARD;
    uint8_t active_theme=dpc245_theme(&c->runtime.settings);
    if(!(c->screen==DPC245_APP_SCREEN_DASHBOARD &&
         (active_theme==DPC245_THEME_ARC||active_theme==DPC245_THEME_WEDGE)))clear();
    if(dpc245_app_menu_count(c->screen)>0u)g_white=0xFFFFu;
    switch(c->screen){
    case DPC245_APP_SCREEN_DASHBOARD:
        if(dpc245_theme(&c->runtime.settings)==DPC245_THEME_ARC)draw_arc(c,r,status,now_ms);
        else if(dpc245_theme(&c->runtime.settings)==DPC245_THEME_WEDGE)draw_wedge(c,r,status,now_ms);
        else draw_dashboard(c,r,status,now_ms);
        break;
    case DPC245_APP_SCREEN_ROOT_MENU: draw_menu(c,"SETTING");break;
    case DPC245_APP_SCREEN_DISPLAY_MENU: draw_menu(c,"DISPLAY");break;
    case DPC245_APP_SCREEN_BIKE_MENU: draw_menu(c,"BIKE");break;
    case DPC245_APP_SCREEN_INFO_MENU: draw_menu(c,"INFORMATION");break;
    case DPC245_APP_SCREEN_THEMES_MENU: draw_menu(c,"THEMES");break;
    case DPC245_APP_SCREEN_EDIT_UNIT: draw_edit("UNIT",dpc245_unit(&c->runtime.settings)?"IMPERIAL":"METRIC");break;
    case DPC245_APP_SCREEN_EDIT_SERVICE_TIPS: draw_edit("SERVICE TIPS",dpc245_service_tips(&c->runtime.settings)?"ON":"OFF");break;
    case DPC245_APP_SCREEN_EDIT_AUTO_OFF:{uint8_t v=dpc245_auto_off_min(&c->runtime.settings);if(v==0u)draw_edit("AUTO OFF","OFF");else draw_edit_num("AUTO OFF",v,"MIN");break;}
    case DPC245_APP_SCREEN_TRIP_RESET: draw_trip_reset();break;
    case DPC245_APP_SCREEN_EDIT_BRIGHTNESS:{uint8_t v=dpc245_brightness_level(&c->runtime.settings);draw_edit_num("BRIGHTNESS",v*25u,"PCT");break;}
    case DPC245_APP_SCREEN_EDIT_AL_SENS: {uint8_t v=dpc245_al_sensitivity(&c->runtime.settings);if(v==0u)draw_edit("AL SENSITIVITY","OFF");else draw_edit_num("AL SENSITIVITY",v,0);break;}
    case DPC245_APP_SCREEN_INFO_HMI: draw_hmi_info(c,hw,slot,loaded,persist_write_enabled);break;
    case DPC245_APP_SCREEN_INFO_WHEEL: draw_wheel(r);break;
    case DPC245_APP_SCREEN_INFO_SPEED_LIMIT: draw_speed_limit(c,r);break;
    case DPC245_APP_SCREEN_INFO_BATTERY: draw_battery(c,r);break;
    case DPC245_APP_SCREEN_INFO_CONTROLLER: draw_controller(c,r);break;
    case DPC245_APP_SCREEN_INFO_SENSOR: draw_sensor(c,r,a);break;
    case DPC245_APP_SCREEN_INFO_ERRORS: draw_errors(c,r);break;
    case DPC245_APP_SCREEN_ADVANCED: draw_advanced(c,r,now_ms);break;
    default: title("UNKNOWN");break;
    }
    if(!can_tx_enabled)m_text(244,4,"RX",DIM,BLACK,1);
}
