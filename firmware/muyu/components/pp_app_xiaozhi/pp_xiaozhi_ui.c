#include "pp_xiaozhi_ui.h"
#include "pp_avatar_view.h"
#include "pp_chat_motion.h"
#include "lvgl.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#include <stdatomic.h>
#include <string.h>

LV_FONT_DECLARE(font_pet_14);
extern const uint8_t pp_pet_room_i4[64+240*148/2];
/* Reuse the existing immutable room and the shared 96 x 88 avatar surface.
 * No second sprite buffer, new bitmap, image transform, pet model or save. */
static const lv_image_dsc_t room={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_I4,
    .w=240,.h=148,.stride=120},.data_size=64+240*148/2,.data=pp_pet_room_i4};
static lv_obj_t *s_avatar,*s_bubble,*s_title,*s_detail,*s_code,*s_action,*s_tail[2];
static int s_room_y;
static pp_chat_motion_t s_motion;
static atomic_bool s_reset_motion=true;
/* Draw feedback into the existing card; no extra objects, canvas or font. */
static struct { uint8_t mode,bars[3],dot; } s_feedback;

void pp_xiaozhi_ui_suspend(void) { atomic_store(&s_reset_motion,true); }

static void draw_feedback(lv_event_t *event)
{
    if(!s_feedback.mode) return;
    lv_area_t card; lv_obj_get_coords(lv_event_get_target_obj(event),&card);
    lv_layer_t *layer=lv_event_get_layer(event);
    const int center=(card.x1+card.x2)/2,baseline=card.y1+57;
    lv_draw_rect_dsc_t rect; lv_draw_rect_dsc_init(&rect);
    for(unsigned i=0;i<3;++i) {
        bool dots=s_feedback.mode==2;
        unsigned h=dots?4:s_feedback.bars[i];
        bool bright=dots?s_feedback.dot==i:h>0;
        if(!h) h=2;
        int x=center-14+(int)i*12;
        lv_area_t area={.x1=x,.x2=x+5,.y1=baseline-(int)h+1,.y2=baseline};
        rect.bg_color=lv_color_hex(bright?0x47786a:0xd2bd97);
        lv_draw_rect(layer,&rect,&area);
    }
}

static lv_obj_t *box(lv_obj_t *parent,int x,int y,int w,int h,uint32_t color)
{
    lv_obj_t *o=lv_obj_create(parent); lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_set_style_bg_color(o,lv_color_hex(color),0); lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
    return o;
}
static lv_obj_t *label(lv_obj_t *parent,int x,int y,int w,int h,const lv_font_t *font,uint32_t color)
{
    lv_obj_t *o=lv_label_create(parent); lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_set_style_text_font(o,font,0); lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0); lv_label_set_text(o,"");
    return o;
}
static void text(lv_obj_t *o,const char *value)
{
    if(strcmp(lv_label_get_text(o),value)) lv_label_set_text(o,value);
}
static void visible(lv_obj_t *o,bool show)
{
    if(show) lv_obj_remove_flag(o,LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);
}
static void deleted(lv_event_t *event)
{
    (void)event; lv_image_cache_drop(&room);
    /* The shared avatar's own DELETE handler drops its sprite cache/owner. */
    s_avatar=s_bubble=s_title=s_detail=s_code=s_action=NULL;
    pp_xiaozhi_ui_suspend();
}
void pp_xiaozhi_ui_create(lv_obj_t *parent)
{
    int height=lv_obj_get_height(parent);
    lv_obj_t *root=box(parent,0,0,240,height,0xf5ebd6);
    lv_obj_add_event_cb(root,deleted,LV_EVENT_DELETE,NULL);
    s_room_y=height-148;
    lv_obj_t *scene=lv_image_create(root); lv_image_set_src(scene,&room); lv_obj_set_pos(scene,0,s_room_y);
    s_avatar=pp_avatar_view_create(root,72,s_room_y+52);

    /* A quiet speech card above the room; ordinary states show the companion's
     * intent, without a permanent toolbar or button-instruction footer. */
    s_bubble=box(root,24,28,192,78,0xfff7e6);
    lv_obj_set_style_border_width(s_bubble,2,0);
    lv_obj_set_style_border_color(s_bubble,lv_color_hex(0xd2bd97),0);
    lv_obj_set_style_radius(s_bubble,4,0);
    memset(&s_feedback,0,sizeof(s_feedback));
    lv_obj_add_event_cb(s_bubble,draw_feedback,LV_EVENT_DRAW_MAIN_END,NULL);
    pp_xiaozhi_ui_suspend();
    s_tail[0]=box(root,113,106,14,3,0xd2bd97); s_tail[1]=box(root,117,109,6,4,0xd2bd97);
    s_title=label(root,24,32,192,22,&font_pet_14,0x283f45);
    s_detail=label(root,24,62,192,40,&font_pet_14,0x506f66);
    s_code=label(root,24,53,192,26,&lv_font_montserrat_20,0x283f45);
    s_action=label(root,14,s_room_y-25,212,20,&font_pet_14,0x936344);
    visible(s_code,false); visible(s_action,false);
}
void pp_xiaozhi_ui_render(const pp_voice_snapshot_t *s)
{
    if(!s_title) return;
    pp_voice_state_t state=s->state;
    if((unsigned)state>PP_VOICE_STOPPED) state=PP_VOICE_ERROR;
    const char *title="芽芽来啦",*detail="稍等一下",*action="";
    pp_avatar_state_t voice=AVATAR_IDLE;
    switch(state) {
    case PP_VOICE_READY: title="我在呀"; detail="陪你说说话"; break;
    case PP_VOICE_LISTENING: title="我在听"; detail=""; voice=AVATAR_LISTENING; break;
    case PP_VOICE_THINKING: title="让我想一想"; detail=""; voice=AVATAR_THINKING; break;
    case PP_VOICE_SPEAKING: title="说给你听"; detail=""; voice=AVATAR_SPEAKING; break;
    case PP_VOICE_STOPPED: title="我在这里"; detail="陪着你"; break;
    case PP_VOICE_WIFI: title="等你连上 Wi-Fi"; detail="设置 > Wi-Fi"; voice=AVATAR_OFFLINE; break;
    case PP_VOICE_ACTIVATION:
        title="和芽芽见面吧"; detail="xiaozhi.me"; action="添加设备 输入上方代码"; break;
    case PP_VOICE_ERROR:
        title="没连上"; detail=s->detail; action="按 OK 重试"; voice=AVATAR_OFFLINE; break;
    case PP_VOICE_OFF: case PP_VOICE_STOPPING:
        title="一会儿见"; detail=""; break;
    default: voice=AVATAR_THINKING; break;
    }
    bool error=state==PP_VOICE_ERROR,activation=state==PP_VOICE_ACTIVATION;
    const uint32_t now=lv_tick_get();
    if(atomic_exchange(&s_reset_motion,false)) pp_chat_motion_reset(&s_motion,now);
    pp_chat_motion_output_t motion;
    pp_chat_motion_step(&s_motion,now,voice,s->level,s->playback_level,s->playback_age_ms,&motion);
    uint8_t mode=voice==AVATAR_THINKING?2:
        voice==AVATAR_LISTENING||voice==AVATAR_SPEAKING?1:0;
    if(activation||error) mode=0;
    if(mode) detail="";
    if(s_feedback.mode!=mode || s_feedback.dot!=motion.dot_phase ||
       memcmp(s_feedback.bars,motion.bars,sizeof(s_feedback.bars))) {
        s_feedback.mode=mode; s_feedback.dot=motion.dot_phase;
        memcpy(s_feedback.bars,motion.bars,sizeof(s_feedback.bars));
        lv_obj_invalidate(s_bubble);
    }
    text(s_title,title); text(s_detail,detail); text(s_action,action);
    visible(s_action,*action!=0); visible(s_code,activation);
    for(unsigned i=0;i<2;++i) visible(s_tail[i],!error&&!activation);
    if(activation) text(s_code,s->activation);
    /* Preserve exact first-failure diagnostics and readable activation digits.
     * Existing Latin font keeps arbitrary SDK error text legible. */
    lv_obj_set_style_text_font(s_detail,error?&lv_font_montserrat_14:&font_pet_14,0);
    lv_obj_set_y(s_title,error||activation?22:41);
    lv_obj_set_y(s_detail,activation?85:error?48:70);
    lv_obj_set_height(s_detail,error?60:activation?20:40);
    lv_obj_set_pos(s_bubble,error||activation?14:24,error||activation?16:28);
    lv_obj_set_width(s_bubble,error||activation?212:192);
    lv_obj_set_height(s_bubble,error||activation?s_room_y-45:78);
    lv_obj_set_style_border_color(s_bubble,lv_color_hex(error?0xad6857:0xd2bd97),0);
    pp_avatar_view_render(s_avatar,&motion.frame);
}
