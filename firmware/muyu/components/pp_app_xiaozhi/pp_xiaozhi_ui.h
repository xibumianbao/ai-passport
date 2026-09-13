#pragma once
#include "pp_voice.h"
struct _lv_obj_t;
void pp_xiaozhi_ui_create(struct _lv_obj_t *parent);
void pp_xiaozhi_ui_render(const pp_voice_snapshot_t *s);
/* May be called by the control worker without the LVGL lock. */
void pp_xiaozhi_ui_suspend(void);
