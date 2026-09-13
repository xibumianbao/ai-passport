#pragma once
#include "passport_apps.h"
/* Presentation states, independent from ESP-IDF radio driver headers. */
typedef enum { PP_LINK_OFF, PP_LINK_ON, PP_LINK_BUSY, PP_LINK_UNCONFIGURED, PP_LINK_ERROR } pp_link_state_t;
typedef struct {
    bool menu, keyboard, buttons_ok;
    int battery, selected, row_count;
    unsigned keyboard_page, keyboard_selected, keyboard_length;
    bool keyboard_secret;
    const pp_app_module_t *module;
    pp_link_state_t wifi, ble;
    char app[24], title[24], detail[320], input_name[33], input_text[64];
    char rows[6][48], footer[64];
} pp_view_t;
void pp_ui_create(void);
void pp_ui_render(const pp_view_t *view);
