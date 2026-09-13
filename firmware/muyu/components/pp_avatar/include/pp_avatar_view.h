#pragma once

#include "pp_avatar.h"
#include "lvgl.h"

/* Shared 96 x 88 native-pixel view. Call only from the LVGL task or while
 * holding the system LVGL lock. No audio, timers, model or saved pet state.
 * There is one static I4 surface for the foreground view: delete the previous
 * view (or its parent) before creating another. Returns NULL for a NULL parent,
 * an existing owner, or an allocation failure reported by LVGL. */
lv_obj_t *pp_avatar_view_create(lv_obj_t *parent, int x, int y);

/* Redraw only if the complete presentation frame changes. NULL or non-owner
 * views and NULL frames are ignored. The caller supplies its own visual clock. */
void pp_avatar_view_render(lv_obj_t *view, const pp_avatar_frame_t *frame);
