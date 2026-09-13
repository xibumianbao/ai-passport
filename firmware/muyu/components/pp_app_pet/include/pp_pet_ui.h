#pragma once
#include "pp_pet.h"
#include "pp_avatar.h"
struct _lv_obj_t;
void pp_pet_ui_create(struct _lv_obj_t *parent);
void pp_pet_ui_render(const pp_pet_t *pet, pp_avatar_state_t voice);
