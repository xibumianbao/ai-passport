#include "passport_ui.h"
#include "lvgl.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
LV_FONT_DECLARE(font_muyu_22);
static lv_obj_t *s_app, *s_battery, *s_status, *s_footer, *s_count, *s_detail;
static lv_obj_t *s_wood, *s_drawer, *s_title, *s_rows[6], *s_labels[6], *s_note;
static bool s_menu;
static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color, int radius)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, radius, 0);
    return o;
}
static lv_obj_t *text(lv_obj_t *parent, int x, int y, int w, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *o = lv_label_create(parent);
    lv_obj_set_pos(o, x, y); lv_obj_set_width(o, w);
    lv_obj_set_style_text_font(o, font, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
    lv_label_set_text(o, ""); return o;
}
static void drawer_x(void *obj, int32_t value) { lv_obj_set_x(obj, value); }
static void wood_scale(void *obj, int32_t value) { lv_obj_set_y(obj, value); }
void pp_ui_create(void)
{
    lv_obj_t *screen = box(NULL, 0, 0, 240, 320, 0xf4f3ef, 0);
    s_app = text(screen, 10, 8, 145, &lv_font_montserrat_14, 0x151515);
    s_battery = text(screen, 167, 8, 63, &lv_font_montserrat_14, 0x151515);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);
    box(screen, 0, 29, 240, 1, 0x151515, 0);
    s_status = text(screen, 12, 41, 216, &lv_font_montserrat_14, 0x555555);
    s_count = text(screen, 16, 76, 208, &font_muyu_22, 0x151515);
    s_wood = box(screen, 47, 142, 146, 77, 0xc58c50, 36);
    lv_obj_set_style_border_width(s_wood, 3, 0);
    lv_obj_set_style_border_color(s_wood, lv_color_hex(0x4b3527), 0);
    box(s_wood, 13, 8, 110, 12, 0xe9ba7c, 6);
    box(s_wood, 24, 36, 85, 5, 0x4b3527, 2);
    box(s_wood, 105, 33, 10, 10, 0x4b3527, 5);
    s_detail = text(screen, 16, 238, 208, &lv_font_montserrat_14, 0x333333);
    s_drawer = box(screen, 28, 35, 212, 255, 0x151515, 0);
    s_title = text(s_drawer, 14, 12, 184, &lv_font_montserrat_20, 0xffffff);
    for (int i = 0; i < 6; ++i) {
        s_rows[i] = box(s_drawer, 8, 44+i*31, 196, 29, 0x151515, 3);
        s_labels[i] = text(s_rows[i], 7, 6, 182, &lv_font_montserrat_14, 0xffffff);
        lv_label_set_long_mode(s_labels[i], LV_LABEL_LONG_DOT);
    }
    s_note = text(s_drawer, 14, 205, 184, &lv_font_montserrat_14, 0xc8c8c8);
    box(screen, 0, 295, 240, 1, 0x151515, 0);
    s_footer = text(screen, 10, 302, 225, &lv_font_montserrat_14, 0x555555);
    lv_obj_add_flag(s_drawer, LV_OBJ_FLAG_HIDDEN);
    lv_screen_load(screen);
}
void pp_ui_render(const pp_view_t *v, bool strike)
{
    lv_label_set_text(s_app, v->app);
    if (v->battery < 0) lv_label_set_text(s_battery, "--%");
    else lv_label_set_text_fmt(s_battery, "%d%%", v->battery);
    lv_label_set_text(s_status, v->status);
    if (v->muyu) {
        lv_obj_remove_flag(s_wood, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_fmt(s_count, "功德  %" PRIu32, v->count);
        lv_obj_set_style_text_font(s_count, &font_muyu_22, 0);
        if (!v->menu) {
            lv_obj_set_y(s_detail, 238);
            lv_label_set_text(s_detail, v->audio_ok ? "Tap a key to knock.\nCount resets on reboot." : "Audio unavailable.\nCounter still works.");
        }
    } else {
        lv_obj_add_flag(s_wood, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_count, "DEVICE STATUS");
        lv_obj_set_style_text_font(s_count, &lv_font_montserrat_20, 0);
        lv_obj_set_y(s_detail, 117);
        lv_label_set_text(s_detail, v->detail);
    }
    if (v->menu) {
        lv_obj_remove_flag(s_drawer, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_title, v->title);
        for (int i = 0; i < 6; ++i) {
            if (i >= v->row_count) { lv_obj_add_flag(s_rows[i], LV_OBJ_FLAG_HIDDEN); continue; }
            lv_obj_remove_flag(s_rows[i], LV_OBJ_FLAG_HIDDEN);
            bool selected = i == v->selected;
            lv_obj_set_style_bg_color(s_rows[i], lv_color_hex(selected ? 0xf4f3ef : 0x151515), 0);
            lv_obj_set_style_text_color(s_labels[i], lv_color_hex(selected ? 0x151515 : 0xffffff), 0);
            lv_label_set_text(s_labels[i], v->rows[i]);
        }
        lv_obj_set_y(s_note, 44 + v->row_count * 31 + 3);
        lv_obj_set_height(s_note, 250 - (44 + v->row_count * 31 + 3));
        lv_label_set_text(s_note, v->detail);
        if (!s_menu) {
            lv_anim_t a; lv_anim_init(&a); lv_anim_set_var(&a, s_drawer);
            lv_anim_set_exec_cb(&a, drawer_x); lv_anim_set_values(&a, 240, 28);
            lv_anim_set_duration(&a, 140); lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
            lv_anim_start(&a);
        }
    } else {
        lv_anim_delete(s_drawer, drawer_x);
        lv_obj_add_flag(s_drawer, LV_OBJ_FLAG_HIDDEN);
    }
    if (v->menu || !v->muyu) { lv_anim_delete(s_wood, wood_scale); lv_obj_set_y(s_wood, 142); }
    else if (strike) {
        lv_anim_delete(s_wood, wood_scale);
        lv_anim_t a; lv_anim_init(&a); lv_anim_set_var(&a, s_wood);
        lv_anim_set_exec_cb(&a, wood_scale); lv_anim_set_values(&a, 142, 148);
        lv_anim_set_duration(&a, 50); lv_anim_set_playback_duration(&a, 100); lv_anim_start(&a);
    }
    lv_label_set_text(s_footer, !v->buttons_ok ? "BUTTON ERROR" : v->menu ? "OK: select   HOLD OK: back" : "HOLD OK: system menu");
    s_menu = v->menu;
}
