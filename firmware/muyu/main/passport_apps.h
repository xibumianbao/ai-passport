#pragma once
#include "passport_core.h"
struct _lv_obj_t;
typedef struct {
    bool (*start)(void *ctx);
    void (*stop)(void *ctx);
    void (*focus)(void *ctx, bool focused);
    void (*key)(void *ctx, pp_key_t key);
    void *ctx;
    /* Control task holds LVGL lock. create gets a 240x235 content container.
     * stop must join UI producers before the platform removes that container. */
    void (*create_ui)(struct _lv_obj_t *parent);
    void (*render_ui)(void);
} pp_app_module_t;
/* Control-task access only, never from network/audio worker callbacks. */
pp_runtime_t *pp_app_runtime(void);
