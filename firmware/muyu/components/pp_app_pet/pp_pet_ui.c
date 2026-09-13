#include "pp_pet_ui.h"
#include "pp_avatar_view.h"
#include "lvgl.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#include <stdio.h>
#include <string.h>
LV_FONT_DECLARE(font_pet_14);
extern const uint8_t pp_pet_room_i4[64+240*148/2];
static const lv_image_dsc_t room={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_I4,
    .w=240,.h=148,.stride=120},.data_size=64+240*148/2,.data=pp_pet_room_i4};
static lv_obj_t *s_pet,*s_title,*s_coins,*s_message,*s_food,*s_energy,*s_xp,*s_buttons[3],*s_labels[3];
static lv_obj_t *box(lv_obj_t *parent,int x,int y,int w,int h,uint32_t color)
{
    lv_obj_t *o=lv_obj_create(parent); lv_obj_remove_style_all(o); lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h); lv_obj_set_style_bg_color(o,lv_color_hex(color),0);
    lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0); return o;
}
static lv_obj_t *label(lv_obj_t *parent,int x,int y,int w,uint32_t color)
{
    lv_obj_t *o=lv_label_create(parent); lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,20);
    lv_obj_set_style_text_font(o,&font_pet_14,0); lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP); return o;
}
static void text(lv_obj_t *o,const char *value)
{
    if(strcmp(lv_label_get_text(o),value)) lv_label_set_text(o,value);
}
static void deleted(lv_event_t *e)
{
    (void)e; lv_image_cache_drop(&room);
}
void pp_pet_ui_create(lv_obj_t *parent)
{
    int height=lv_obj_get_height(parent),extra=height-235;
    lv_obj_t *root=box(parent,0,0,240,height,0xf5ebd6);
    lv_obj_add_event_cb(root,deleted,LV_EVENT_DELETE,NULL);
    s_title=label(root,9,1,98,0x283f45); s_coins=label(root,112,1,63,0x936344);
    /* Bowl and energy marks remain legible without reading the numbers. */
    box(root,175,6,7,4,0xad6857); box(root,176,10,5,2,0xad6857);
    box(root,178,15,3,7,0xe7ac67); box(root,176,17,6,3,0xe7ac67);
    box(root,185,7,46,4,0xdacdb5); s_food=box(root,185,7,23,4,0x50877a);
    box(root,185,17,46,4,0xdacdb5); s_energy=box(root,185,17,32,4,0xb78a68);
    /* Center the original pixel scene in the extra room; never stretch a bitmap. */
    lv_obj_t *scene=lv_image_create(root); lv_image_set_src(scene,&room); lv_obj_set_pos(scene,0,25+extra/2);
    s_pet=pp_avatar_view_create(root,72,77+extra/2);
    s_message=label(root,8,height-58,224,0x506f66); lv_obj_set_style_text_align(s_message,LV_TEXT_ALIGN_CENTER,0);
    box(root,12,height-37,216,3,0xdacdb5); s_xp=box(root,12,height-37,1,3,0x50877a);
    for(unsigned i=0;i<3;++i) {
        s_buttons[i]=box(root,6+(int)i*78,height-28,72,25,0xe7dcc4);
        s_labels[i]=label(s_buttons[i],1,3,70,0x283f45);
        lv_obj_set_style_text_align(s_labels[i],LV_TEXT_ALIGN_CENTER,0);
    }
}
static pp_avatar_pose_t pose(const pp_pet_t *p)
{
    switch(p->action) {
    case PET_EAT:return AVATAR_EAT;
    case PET_PLAY:return AVATAR_PLAY;
    case PET_REST:return AVATAR_SLEEP;
    case PET_GROW:return AVATAR_GROW;
    default: break;
    }
    if(p->save.fullness<=15) return AVATAR_LOOK;
    const pp_avatar_pose_t schedule[]={AVATAR_LOOK,AVATAR_READ,AVATAR_HELP,AVATAR_WALK,AVATAR_READ,AVATAR_LOOK};
    return schedule[(p->clock_ms/10000)%6];
}
static const char *message(const pp_pet_t *p,pp_avatar_pose_t activity,pp_avatar_state_t voice)
{
    if(p->storage_error) return "保存失败 按 OK 重试";
    switch(voice) {
    case AVATAR_LISTENING:return "倾听";
    case AVATAR_THINKING:return "思考";
    case AVATAR_SPEAKING:return "说话";
    case AVATAR_INTERRUPTED:return "已打断";
    case AVATAR_OFFLINE:return "离线";
    default:break;
    }
    switch(p->notice) {
    case PET_REWARD:return "帮忙得金币 经验 +3";
    case PET_FED:return "吃得好香";
    case PET_FULL:return "不饿啦 陪我玩";
    case PET_FREE_FOOD:return "免费口粮 吃得好香";
    case PET_LEVEL:return "长大啦 等级 +1";
    case PET_EVOLVED:return "进化了 新伙伴";
    case PET_RESTED:return "精神满满";
    default:break;
    }
    if(p->save.fullness<=15 && p->action==PET_LIVE) return "肚子有点饿";
    switch(activity) {
    case AVATAR_READ:return "看书";
    case AVATAR_HELP:return "帮忙";
    case AVATAR_WALK:return "散步";
    case AVATAR_EAT:return "吃得好香";
    case AVATAR_PLAY:return "接住！";
    case AVATAR_SLEEP:return "睡一会儿";
    case AVATAR_GROW:return "长大啦";
    default:return "想心事";
    }
}
void pp_pet_ui_render(const pp_pet_t *p,pp_avatar_state_t voice)
{
    char str[48]; unsigned level=pp_pet_level(&p->save);
    snprintf(str,sizeof(str),"芽芽 Lv%u",level); text(s_title,str);
    snprintf(str,sizeof(str),"★ %u",p->save.coins); text(s_coins,str);
    lv_obj_set_width(s_food,46*p->save.fullness/100); lv_obj_set_width(s_energy,46*p->save.energy/100);
    lv_obj_set_width(s_xp,(int)(216*pp_pet_progress(&p->save)/100));
    /* Visual clock remains independent when future voice pauses the economy. */
    pp_avatar_pose_t activity=pose(p); unsigned frame=lv_tick_get()/200;
    if(voice!=AVATAR_IDLE) activity=AVATAR_LOOK;
    pp_avatar_frame_t f={activity,voice,frame,level>=3};
    pp_avatar_view_render(s_pet,&f);
    text(s_message,message(p,activity,voice));
    const char *labels[]={p->save.fullness>60?"不饿啦":p->save.coins>=5?"喂食 5★":"免费口粮","陪玩","休息"};
    for(unsigned i=0;i<3;++i) {
        bool selected=i==(unsigned)p->choice;
        text(s_labels[i],labels[i]);
        lv_obj_set_style_bg_color(s_buttons[i],lv_color_hex(selected?0x306868:0xe7dcc4),0);
        lv_obj_set_style_text_color(s_labels[i],lv_color_hex(selected?0xf5dfad:0x506f66),0);
    }
}
