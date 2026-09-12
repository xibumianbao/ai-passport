#pragma once
#include "passport_core.h"
typedef struct {
    bool menu, muyu, audio_ok, buttons_ok;
    int battery, selected, row_count;
    uint32_t count;
    char app[24], title[24], status[32], detail[192];
    char rows[6][40];
} pp_view_t;
/* One permanent object tree. All calls require the LVGL lock on the board. */
void pp_ui_create(void);
void pp_ui_render(const pp_view_t *view, bool strike);
