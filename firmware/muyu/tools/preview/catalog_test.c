#include "passport_catalog.h"
#include "passport_keyboard.h"
#include "passport_ui.h"
#include "passport_audio.h"
#include "lvgl.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static pp_runtime_t runtime;
static unsigned cancels;
pp_runtime_t *pp_app_runtime(void) { return &runtime; }
bool pp_audio_ok(void) { return true; }
void pp_audio_begin(uint32_t session) { assert(pp_session_valid(&runtime,session)); }
void pp_audio_knock(uint32_t session) { assert(pp_session_valid(&runtime,session)); }
void pp_audio_cancel(void *ctx) { (void)ctx; ++cancels; }
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p) { (void)a; (void)p; lv_display_flush_ready(d); }
int main(void)
{
    pp_catalog_init(); pp_runtime_init(&runtime,pp_apps,PP_APP_COUNT);
    int original=pp_find_app(&runtime,"muyu"), probe=pp_find_app(&runtime,"probe");
    assert(original>=0 && probe>=0 && original!=probe);
    lv_init(); lv_display_t *d=lv_display_create(240,320);
    static uint16_t pixels[240*20];
    lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d,flush);
    pp_keyboard_t kb; pp_keyboard_init(&kb,63); pp_ui_create();
    pp_view_t v={.battery=99,.buttons_ok=true};
    for(unsigned i=0;i<2000;++i) {
        int index=i%2 ? probe : original;
        assert(pp_activate(&runtime,index)); pp_dispatch(&runtime,PP_OK);
        v.module=pp_modules[index]; snprintf(v.app,sizeof(v.app),"%s",pp_apps[index].name);
        pp_ui_render(&v); lv_tick_inc(20); lv_timer_handler();
        assert(pp_session_valid(&runtime,runtime.generation));
    }
    assert(cancels==1000);
    puts("Application scaffold: PASS (generated module + generated registry, 2000 real LVGL app switches)");
}
