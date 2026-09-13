#pragma once
#include "passport_apps.h"
typedef struct {
    bool menu, keyboard, buttons_ok;
    int battery, selected, row_count;
    unsigned keyboard_page, keyboard_selected, keyboard_length;
    bool keyboard_secret;
    const pp_app_module_t *module;
    char app[24], title[24], status[40], detail[320], input_name[33], input_text[64];
    char rows[6][48], footer[64];
} pp_view_t;
void pp_ui_create(void);
void pp_ui_render(const pp_view_t *view);
