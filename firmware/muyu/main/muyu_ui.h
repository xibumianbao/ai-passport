#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Called only in LVGL context, or while holding the BSP LVGL lock. */
void muyu_ui_create(void);
void muyu_ui_refresh(uint32_t count, int battery, bool audio_ok,
                     bool buttons_ok, bool struck);
