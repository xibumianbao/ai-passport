#include "passport_ui.h"
#include "passport_keyboard.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
static lv_obj_t *s_app,*s_battery,*s_status,*s_footer,*s_content,*s_drawer,*s_title,*s_rows[6],*s_labels[6],*s_note;
static lv_obj_t *s_keyboard,*s_kb_title,*s_kb_name,*s_kb_value,*s_kb_matrix,*s_kb_note;
static const pp_app_module_t *s_module;
static const char *s_keymap[PP_KB_MAX_KEYS+8];
static bool s_menu;
static unsigned s_keyboard_page=99;
static lv_obj_t *box(lv_obj_t *parent,int x,int y,int w,int h,uint32_t color,int radius)
{
    lv_obj_t *o=lv_obj_create(parent); lv_obj_remove_style_all(o); lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h); lv_obj_set_style_bg_color(o,lv_color_hex(color),0);
    lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0); lv_obj_set_style_radius(o,radius,0); return o;
}
static lv_obj_t *text(lv_obj_t *parent,int x,int y,int w,const lv_font_t *font,uint32_t color)
{
    lv_obj_t *o=lv_label_create(parent); lv_obj_set_pos(o,x,y); lv_obj_set_width(o,w);
    lv_obj_set_style_text_font(o,font,0); lv_obj_set_style_text_color(o,lv_color_hex(color),0); lv_label_set_text(o,""); return o;
}
static void drawer_x(void *obj,int32_t x) { lv_obj_set_x(obj,x); }
void pp_ui_create(void)
{
    lv_obj_t *screen=box(NULL,0,0,240,320,0xf4f3ef,0);
    s_app=text(screen,10,8,145,&lv_font_montserrat_14,0x151515);
    s_battery=text(screen,167,8,63,&lv_font_montserrat_14,0x151515);
    lv_obj_set_style_text_align(s_battery,LV_TEXT_ALIGN_RIGHT,0);
    box(screen,0,29,240,1,0x151515,0);
    s_status=text(screen,12,41,216,&lv_font_montserrat_14,0x555555);
    lv_label_set_long_mode(s_status,LV_LABEL_LONG_DOT);
    s_content=box(screen,0,60,240,235,0xf4f3ef,0);
    s_drawer=box(screen,28,35,212,255,0x151515,0);
    s_title=text(s_drawer,14,12,184,&lv_font_montserrat_20,0xffffff);
    for(int i=0;i<6;++i) {
        s_rows[i]=box(s_drawer,8,44+i*31,196,29,0x151515,3);
        s_labels[i]=text(s_rows[i],7,6,182,&lv_font_montserrat_14,0xffffff);
        lv_label_set_long_mode(s_labels[i],LV_LABEL_LONG_DOT);
    }
    s_note=text(s_drawer,14,205,184,&lv_font_montserrat_14,0xc8c8c8);
    s_keyboard=box(screen,0,35,240,255,0x151515,0);
    s_kb_title=text(s_keyboard,10,6,220,&lv_font_montserrat_20,0xffffff);
    s_kb_name=text(s_keyboard,10,32,220,&lv_font_montserrat_14,0xc8c8c8);
    lv_label_set_long_mode(s_kb_name,LV_LABEL_LONG_DOT);
    s_kb_value=text(s_keyboard,10,54,220,&lv_font_montserrat_14,0xffffff);
    lv_label_set_long_mode(s_kb_value,LV_LABEL_LONG_DOT);
    s_kb_matrix=lv_buttonmatrix_create(s_keyboard);
    lv_obj_set_pos(s_kb_matrix,6,80); lv_obj_set_size(s_kb_matrix,228,150);
    lv_obj_set_style_bg_color(s_kb_matrix,lv_color_hex(0x151515),0);
    lv_obj_set_style_border_width(s_kb_matrix,0,0); lv_obj_set_style_pad_all(s_kb_matrix,1,0);
    lv_obj_set_style_pad_row(s_kb_matrix,3,0); lv_obj_set_style_pad_column(s_kb_matrix,3,0);
    lv_obj_set_style_bg_color(s_kb_matrix,lv_color_hex(0x303030),LV_PART_ITEMS);
    lv_obj_set_style_text_color(s_kb_matrix,lv_color_hex(0xffffff),LV_PART_ITEMS);
    lv_obj_set_style_text_font(s_kb_matrix,&lv_font_montserrat_14,LV_PART_ITEMS);
    lv_obj_set_style_bg_color(s_kb_matrix,lv_color_hex(0xf4f3ef),LV_PART_ITEMS|LV_STATE_CHECKED);
    lv_obj_set_style_text_color(s_kb_matrix,lv_color_hex(0x151515),LV_PART_ITEMS|LV_STATE_CHECKED);
    s_kb_note=text(s_keyboard,10,235,220,&lv_font_montserrat_14,0xc8c8c8);
    box(screen,0,295,240,1,0x151515,0);
    s_footer=text(screen,10,302,225,&lv_font_montserrat_14,0x555555);
    lv_obj_add_flag(s_drawer,LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(s_keyboard,LV_OBJ_FLAG_HIDDEN);
    lv_screen_load(screen);
}
void pp_ui_render(const pp_view_t *v)
{
    lv_label_set_text(s_app,v->app);
    if(v->battery<0) lv_label_set_text(s_battery,"--%");
    else lv_label_set_text_fmt(s_battery,"%d%%",v->battery);
    lv_label_set_text(s_status,v->status);
    if(v->module!=s_module) {
        lv_obj_clean(s_content); s_module=v->module;
        if(s_module && s_module->create_ui) s_module->create_ui(s_content);
    }
    if(!v->menu && s_module && s_module->render_ui) s_module->render_ui();
    if(v->keyboard) {
        lv_obj_remove_flag(s_keyboard,LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(s_drawer,LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_kb_title,v->title); lv_label_set_text(s_kb_name,v->input_name);
        if(v->keyboard_secret) {
            char masked[44]; unsigned len=v->keyboard_length<20 ? v->keyboard_length : 20;
            memset(masked,'*',len); snprintf(masked+len,sizeof(masked)-len,"  (%u)",v->keyboard_length);
            lv_label_set_text(s_kb_value,masked);
        } else lv_label_set_text(s_kb_value,v->input_text);
        if(s_keyboard_page!=v->keyboard_page) {
            s_keyboard_page=v->keyboard_page; unsigned used=0,n=pp_keyboard_count(v->keyboard_page);
            for(unsigned i=0;i<n;++i) {
                if(i && i%6==0) s_keymap[used++]="\n";
                s_keymap[used++]=pp_keyboard_label(v->keyboard_page,i);
            }
            s_keymap[used]="";
            lv_buttonmatrix_set_map(s_kb_matrix,s_keymap);
        }
        lv_buttonmatrix_clear_button_ctrl_all(s_kb_matrix,LV_BUTTONMATRIX_CTRL_CHECKED);
        lv_buttonmatrix_set_button_ctrl(s_kb_matrix,v->keyboard_selected,LV_BUTTONMATRIX_CTRL_CHECKED);
        lv_label_set_text(s_kb_note,v->detail);
    } else {
        lv_obj_add_flag(s_keyboard,LV_OBJ_FLAG_HIDDEN);
        if(v->menu) {
            lv_obj_remove_flag(s_drawer,LV_OBJ_FLAG_HIDDEN); lv_label_set_text(s_title,v->title);
            for(int i=0;i<6;++i) {
                if(i>=v->row_count) { lv_obj_add_flag(s_rows[i],LV_OBJ_FLAG_HIDDEN); continue; }
                lv_obj_remove_flag(s_rows[i],LV_OBJ_FLAG_HIDDEN);
                bool selected=i==v->selected;
                lv_obj_set_style_bg_color(s_rows[i],lv_color_hex(selected?0xf4f3ef:0x151515),0);
                lv_obj_set_style_text_color(s_labels[i],lv_color_hex(selected?0x151515:0xffffff),0);
                lv_label_set_text(s_labels[i],v->rows[i]);
            }
            int top=44+v->row_count*31+3;
            lv_obj_set_y(s_note,top); lv_obj_set_height(s_note,250-top);
            lv_label_set_text(s_note,v->detail);
            if(!s_menu) {
                lv_anim_t a; lv_anim_init(&a); lv_anim_set_var(&a,s_drawer); lv_anim_set_exec_cb(&a,drawer_x);
                lv_anim_set_values(&a,240,28); lv_anim_set_duration(&a,140); lv_anim_set_path_cb(&a,lv_anim_path_ease_out); lv_anim_start(&a);
            }
        } else { lv_anim_delete(s_drawer,drawer_x); lv_obj_add_flag(s_drawer,LV_OBJ_FLAG_HIDDEN); }
    }
    lv_label_set_text(s_footer,!v->buttons_ok?"BUTTON ERROR":v->footer[0]?v->footer:v->menu?"OK: select   HOLD OK: back":"HOLD OK: system menu");
    s_menu=v->menu;
}
