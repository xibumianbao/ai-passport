#include "muyu_ui.h"
#include "ui_pixel.h"
#include "lvgl.h"
#include <inttypes.h>

LV_FONT_DECLARE(font_muyu_22);

static lv_obj_t *s_count, *s_battery, *s_status, *s_feedback;
static lv_obj_t *s_mallet, *s_handle, *s_impact;
static lv_point_precise_t s_handle_points[2];
static const lv_point_precise_t SLIT[] = {{4, 12}, {35, 2}, {72, 7}};
static const lv_point_precise_t RAY_A[] = {{0, 0}, {7, 9}};
static const lv_point_precise_t RAY_B[] = {{0, 0}, {0, 11}};
static const lv_point_precise_t RAY_C[] = {{0, 9}, {8, 0}};

static lv_obj_t *shape(lv_obj_t *parent, int x, int y, int w, int h,
                       uint32_t color, int radius)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    return obj;
}

static lv_obj_t *label(lv_obj_t *parent, const char *text,
                       const lv_font_t *font, int x, int y)
{
    lv_obj_t *obj = ui_pixel_label(parent, text, font, UI_INK);
    lv_obj_set_pos(obj, x, y);
    return obj;
}

static lv_obj_t *line(lv_obj_t *parent, int x, int y,
                      const lv_point_precise_t *points, uint32_t count,
                      int width, uint32_t color)
{
    lv_obj_t *obj = lv_line_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_line_set_points(obj, points, count);
    lv_obj_set_style_line_width(obj, width, 0);
    lv_obj_set_style_line_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_line_rounded(obj, true, 0);
    return obj;
}

static void mallet_y(void *obj, int32_t y)
{
    lv_obj_set_pos(obj, 155, y);
    s_handle_points[0] = (lv_point_precise_t){169, y + 13};
    s_handle_points[1] = (lv_point_precise_t){204, y + 5};
    lv_line_set_points_mutable(s_handle, s_handle_points, 2);
}

static void feedback_progress(void *obj, int32_t progress)
{
    lv_obj_set_y(obj, 209 - progress * 17 / 255);
    lv_obj_set_style_opa(obj, (lv_opa_t)(255 - progress), 0);
    lv_obj_set_style_opa(s_impact, progress < 100 ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
}

static void strike_animation(void)
{
    /* Reuse the same objects and animations on every hit. No growing labels
       and no relative-position drift when a hit interrupts an animation. */
    lv_anim_delete(s_mallet, mallet_y);
    mallet_y(s_mallet, 105);
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, s_mallet);
    lv_anim_set_exec_cb(&anim, mallet_y);
    lv_anim_set_values(&anim, 105, 132);
    lv_anim_set_duration(&anim, 55);
    lv_anim_set_playback_duration(&anim, 115);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_in_out);
    lv_anim_start(&anim);

    lv_anim_delete(s_feedback, feedback_progress);
    feedback_progress(s_feedback, 0);
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, s_feedback);
    lv_anim_set_exec_cb(&anim, feedback_progress);
    lv_anim_set_values(&anim, 0, 255);
    lv_anim_set_duration(&anim, 550);
    lv_anim_start(&anim);
}

void muyu_ui_create(void)
{
    lv_obj_t *screen = ui_pixel_screen_create("MUYU");
    /* Keep the upstream sky, title, grass and mascot visual identity. */
    s_battery = label(screen, "--%", &lv_font_montserrat_14, 173, 31);
    lv_obj_set_width(s_battery, 59);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);
    ui_pixel_panel_create(screen, 14, 60, 207, 178, UI_PAPER);
    label(screen, "功德", &font_muyu_22, 28, 76);
    s_count = label(screen, "0", &lv_font_montserrat_20, 77, 78);
    lv_obj_set_width(s_count, 128);
    lv_obj_set_style_text_align(s_count, LV_TEXT_ALIGN_RIGHT, 0);

    shape(screen, 56, 190, 132, 13, 0xD4C5AD, LV_RADIUS_CIRCLE);
    lv_obj_t *wood = shape(screen, 57, 133, 126, 65, 0xCC8242, 32);
    lv_obj_set_style_bg_grad_color(wood, lv_color_hex(0x8A4525), 0);
    lv_obj_set_style_bg_grad_dir(wood, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(wood, 3, 0);
    lv_obj_set_style_border_color(wood, lv_color_hex(0x593522), 0);
    shape(screen, 69, 139, 93, 11, 0xE6A863, 6);
    line(screen, 78, 156, SLIT, 3, 5, 0x4A2C20);
    shape(screen, 145, 161, 8, 8, 0x3D291F, LV_RADIUS_CIRCLE);
    shape(screen, 147, 162, 2, 2, 0xF6DCA0, 0);
    shape(screen, 68, 179, 34, 3, 0xA65F30, 1);
    shape(screen, 112, 185, 42, 3, 0x703C23, 1);

    s_handle = lv_line_create(screen);
    lv_obj_set_style_line_width(s_handle, 9, 0);
    lv_obj_set_style_line_color(s_handle, lv_color_hex(0x87532D), 0);
    lv_obj_set_style_line_rounded(s_handle, true, 0);
    s_mallet = shape(screen, 155, 105, 27, 27, 0xECC88A, LV_RADIUS_CIRCLE);
    lv_obj_set_style_border_width(s_mallet, 2, 0);
    lv_obj_set_style_border_color(s_mallet, lv_color_hex(0x754629), 0);
    mallet_y(s_mallet, 105);

    s_impact = shape(screen, 128, 117, 29, 22, UI_PAPER, 0);
    lv_obj_set_style_bg_opa(s_impact, LV_OPA_TRANSP, 0);
    line(s_impact, 1, 7, RAY_A, 2, 2, UI_ORANGE);
    line(s_impact, 13, 1, RAY_B, 2, 2, UI_ORANGE);
    line(s_impact, 20, 6, RAY_C, 2, 2, UI_ORANGE);
    lv_obj_set_style_opa(s_impact, LV_OPA_TRANSP, 0);
    s_feedback = label(screen, "功德 +1", &font_muyu_22, 72, 209);
    lv_obj_set_style_text_color(s_feedback, lv_color_hex(0x996220), 0);
    lv_obj_set_style_opa(s_feedback, LV_OPA_TRANSP, 0);
    s_status = label(screen, "ANY KEY: KNOCK", &lv_font_montserrat_14, 20, 251);
    label(screen, "Count resets\non reboot", &lv_font_montserrat_14, 20, 272);
    ui_pixel_mascot_create(screen, 184, 256);
    lv_screen_load(screen);
}

void muyu_ui_refresh(uint32_t count, int battery, bool audio_ok,
                     bool buttons_ok, bool struck)
{
    lv_label_set_text_fmt(s_count, "%" PRIu32, count);
    if (battery < 0) lv_label_set_text(s_battery, "--%");
    else lv_label_set_text_fmt(s_battery, "%d%%", battery);
    lv_label_set_text(s_status, !buttons_ok ? "BUTTON ERROR" :
                              audio_ok ? "ANY KEY: KNOCK" : "SOUND UNAVAILABLE");
    if (struck) strike_animation();
}
