#include "pp_xiaozhi_ui.h"
#include "lvgl.h"
#include <stdio.h>
static lv_obj_t *title,*detail,*code,*hint,*orb,*bars[9];
static lv_obj_t *label(lv_obj_t *p,int x,int y,int w,const lv_font_t *font)
{
    lv_obj_t *o=lv_label_create(p); lv_obj_set_pos(o,x,y); lv_obj_set_width(o,w);
    lv_obj_set_style_text_font(o,font,0); lv_obj_set_style_text_color(o,lv_color_hex(0xe9f2eb),0);
    lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0); lv_label_set_text(o,""); return o;
}
void pp_xiaozhi_ui_create(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent,lv_color_hex(0x172825),0);
    title=label(parent,10,14,220,&lv_font_montserrat_20);
    orb=lv_obj_create(parent); lv_obj_remove_style_all(orb); lv_obj_remove_flag(orb,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(orb,76,55); lv_obj_set_size(orb,88,88);
    lv_obj_set_style_bg_opa(orb,LV_OPA_COVER,0); lv_obj_set_style_radius(orb,44,0);
    for(unsigned i=0;i<9;++i) {
        bars[i]=lv_obj_create(orb); lv_obj_remove_style_all(bars[i]);
        lv_obj_set_style_bg_color(bars[i],lv_color_hex(0x172825),0);
        lv_obj_set_style_bg_opa(bars[i],LV_OPA_COVER,0);
    }
    code=label(parent,10,83,220,&lv_font_montserrat_20);
    detail=label(parent,14,158,212,&lv_font_montserrat_14);
    lv_obj_set_height(detail,52);
    hint=label(parent,12,224,216,&lv_font_montserrat_14);
}
void pp_xiaozhi_ui_render(const pp_voice_snapshot_t *s)
{
    static const char *names[]={"Xiaozhi","Starting","Wi-Fi needed","Secure clock",
        "Xiaozhi","Link device","Connecting","Ready","Listening","Thinking",
        "Speaking","Please retry","Closing"};
    unsigned state=(unsigned)s->state;
    if(state>=sizeof(names)/sizeof(names[0])) state=PP_VOICE_ERROR;
    lv_label_set_text(title,names[state]); lv_label_set_text(detail,s->detail);
    bool activation=state==PP_VOICE_ACTIVATION;
    if(activation) lv_obj_add_flag(orb,LV_OBJ_FLAG_HIDDEN); else lv_obj_remove_flag(orb,LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(code,activation?s->activation:"");
    uint32_t color=state==PP_VOICE_ERROR?0xe6aa83:state==PP_VOICE_LISTENING?0x9bd9b5:0xc6e2d8;
    lv_obj_set_style_bg_color(orb,lv_color_hex(color),0);
    bool animated=state==PP_VOICE_LISTENING || state==PP_VOICE_SPEAKING || state==PP_VOICE_THINKING;
    unsigned phase=lv_tick_get()/160;
    for(unsigned i=0;i<9;++i) {
        unsigned h=animated?10+((i*7+phase*3)%24):6+(i<5?i:8-i)*4;
        if(state==PP_VOICE_LISTENING) h=6+(h*(s->level>1800?1800:s->level))/1800;
        lv_obj_set_pos(bars[i],14+(int)i*7,44-(int)h/2); lv_obj_set_size(bars[i],4,(int)h);
    }
    lv_label_set_text(hint,state==PP_VOICE_LISTENING?"OK: send (30s max)":
        state==PP_VOICE_READY?"OK: talk":state==PP_VOICE_ERROR?"OK: retry":
        state==PP_VOICE_THINKING || state==PP_VOICE_SPEAKING?"OK: stop":"Hold OK: menu");
}
