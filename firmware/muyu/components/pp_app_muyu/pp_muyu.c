#include "passport_apps.h"
#include "passport_audio.h"
#include "muyu_logic.h"
#include "lvgl.h"
#include <inttypes.h>
LV_FONT_DECLARE(font_muyu_22);
static uint32_t s_count;
static bool s_strike;
static int s_wood_y;
static lv_obj_t *s_value, *s_wood, *s_note;
static lv_obj_t *box(lv_obj_t *parent,int x,int y,int w,int h,uint32_t color,int radius)
{
    lv_obj_t *o=lv_obj_create(parent); lv_obj_remove_style_all(o); lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h); lv_obj_set_style_bg_color(o,lv_color_hex(color),0);
    lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0); lv_obj_set_style_radius(o,radius,0); return o;
}
static lv_obj_t *label(lv_obj_t *parent,int x,int y,int w,const lv_font_t *font)
{
    lv_obj_t *o=lv_label_create(parent); lv_obj_set_pos(o,x,y); lv_obj_set_width(o,w);
    lv_obj_set_style_text_font(o,font,0); lv_obj_set_style_text_color(o,lv_color_hex(0x151515),0); return o;
}
static bool start(void *ctx) { (void)ctx; return true; }
static void stop(void *ctx) { (void)ctx; s_strike=false; }
static void focus(void *ctx,bool active)
{
    (void)ctx;
    if(active && pp_audio_ok()) {
        pp_runtime_t *r=pp_app_runtime();
        if(pp_resource_acquire(r,pp_audio_cancel,NULL)>=0) pp_audio_begin(r->generation);
    }
}
static void key(void *ctx,pp_key_t value)
{
    (void)ctx; (void)value; s_count=muyu_count_add(s_count,1); s_strike=true;
    pp_audio_knock(pp_app_runtime()->generation);
}
static void bounce(void *obj,int32_t y) { lv_obj_set_y(obj,y); }
static void create_ui(lv_obj_t *parent)
{
    lv_label_set_text(label(parent,16,16,52,&font_muyu_22),"功德");
    s_value=label(parent,70,17,154,&lv_font_montserrat_20);
    lv_obj_set_style_text_align(s_value,LV_TEXT_ALIGN_RIGHT,0);
    int extra=lv_obj_get_height(parent)-235; s_wood_y=82+extra/2;
    s_wood=box(parent,47,s_wood_y,146,77,0xc58c50,36);
    lv_obj_set_style_border_width(s_wood,3,0); lv_obj_set_style_border_color(s_wood,lv_color_hex(0x4b3527),0);
    box(s_wood,13,8,110,12,0xe9ba7c,6); box(s_wood,24,36,85,5,0x4b3527,2); box(s_wood,105,33,10,10,0x4b3527,5);
    s_note=label(parent,16,178+extra,208,&lv_font_montserrat_14);
}
static void render_ui(void)
{
    lv_label_set_text_fmt(s_value,"%" PRIu32,s_count);
    lv_label_set_text(s_note,pp_audio_ok() ? "Tap a key to knock.\nCount resets on reboot." : "Audio unavailable.\nCounter still works.");
    if(s_strike) {
        s_strike=false; lv_anim_delete(s_wood,bounce);
        lv_anim_t a; lv_anim_init(&a); lv_anim_set_var(&a,s_wood); lv_anim_set_exec_cb(&a,bounce);
        lv_anim_set_values(&a,s_wood_y,s_wood_y+6); lv_anim_set_duration(&a,50); lv_anim_set_playback_duration(&a,100); lv_anim_start(&a);
    }
}
const pp_app_module_t pp_muyu_module={.start=start,.stop=stop,.focus=focus,.key=key,.create_ui=create_ui,.render_ui=render_ui};
