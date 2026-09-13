#pragma once
#include "pp_pet.h"
/* Storage is called only by the control worker, never by LVGL or button ISR. */
bool pp_pet_store_open(pp_pet_save_t *save);
bool pp_pet_store_write(void *unused, const uint8_t bytes[PP_PET_SAVE_SIZE]);
void pp_pet_store_close(void);
