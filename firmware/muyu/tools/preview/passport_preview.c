#include "lvgl.h"
#include "passport_ui.h"
#include "passport_keyboard.h"
#include "passport_audio.h"
#include "capacity_preview.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
/* Host framebuffer is outside the 32 KiB widget heap. A full frame avoids
 * coupling screenshots to the board transport's partial-buffer handling. */
static uint16_t draw_buffer[240*320], pixels[240*320];
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *data)
{
    size_t width = (size_t)lv_area_get_width(area);
    for (int y=area->y1; y<=area->y2; ++y) { memcpy(&pixels[y*240+area->x1], data, width*2); data+=width*2; }
    lv_display_flush_ready(display);
}
static void tick(unsigned ms)
{
    for (unsigned n=0; n<ms; n+=5) { lv_tick_inc(5); lv_timer_handler(); }
}
static void snapshot(const char *path)
{
    lv_refr_now(NULL); FILE *f=fopen(path,"wb"); assert(f);
    for (unsigned i=0; i<240*320; ++i) {
        unsigned v=pixels[i];
        unsigned char rgb[] = {(unsigned char)(((v>>11)&31)*255/31),(unsigned char)(((v>>5)&63)*255/63),(unsigned char)((v&31)*255/31)};
        assert(fwrite(rgb,1,3,f)==3);
    }
    assert(fclose(f)==0);
}
static pp_runtime_t runtime;
static bool audio_ok=true;
pp_runtime_t *pp_app_runtime(void) { return &runtime; }
bool pp_audio_ok(void) { return audio_ok; }
void pp_audio_begin(uint32_t session) { (void)session; }
void pp_audio_knock(uint32_t session) { (void)session; }
void pp_audio_cancel(void *ctx) { (void)ctx; }
extern const pp_app_module_t pp_muyu_module;
int main(void)
{
    lv_init(); lv_display_t *d=lv_display_create(240,320);
    lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d,draw_buffer,NULL,sizeof(draw_buffer),LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(d,flush);
    pp_keyboard_t kb; pp_keyboard_init(&kb,63); pp_ui_create();
    pp_view_t v={.buttons_ok=true,.battery=99,.module=&pp_muyu_module};
    for(int i=0;i<12;++i) pp_muyu_module.key(NULL,PP_OK);
    strcpy(v.app,"Muyu"); strcpy(v.status,"Wi-Fi ONLINE | BLE OFF");
    pp_ui_render(&v); tick(200); snapshot("passport-muyu.rgb");
    v.menu=true; v.row_count=5; v.selected=1; strcpy(v.title,"SYSTEM");
    const char *root[]={"Resume app","Applications","Screen off (run)","Settings","About"};
    for(int i=0;i<5;++i) strcpy(v.rows[i],root[i]);
    pp_ui_render(&v); tick(200); snapshot("passport-menu.rgb");
    v.row_count=6; v.selected=5; strcpy(v.title,"SETTINGS");
    const char *rows[]={"Sound","Brightness","Screen timeout","Screen-off mode","Device status","Storage"};
    for(int i=0;i<6;++i) strcpy(v.rows[i],rows[i]);
    pp_ui_render(&v); tick(200); snapshot("passport-settings.rgb");
    v.row_count=2; v.selected=0; strcpy(v.title,"APPLICATIONS"); strcpy(v.rows[0],"* Muyu"); strcpy(v.rows[1],"Yaya Pet");
    strcpy(v.detail,"Your custom apps.\nLast app opens on boot.");
    pp_ui_render(&v); tick(200); snapshot("passport-apps.rgb");
    v.keyboard=true; v.keyboard_secret=true; v.keyboard_length=12; v.keyboard_page=0; v.keyboard_selected=6;
    strcpy(v.title,"WI-FI PASSWORD"); strcpy(v.input_name,"Example 2.4 GHz");
    strcpy(v.detail,"8-63 chars; select GO"); strcpy(v.footer,"UP/DOWN: key   OK: type");
    pp_ui_render(&v); tick(200); snapshot("passport-keyboard.rgb");
    v.keyboard_page=2; v.keyboard_selected=32; v.keyboard_length=63;
    pp_ui_render(&v); tick(200); snapshot("passport-keyboard-symbols.rgb");
    v.keyboard=false; v.footer[0]=0; v.row_count=2; v.selected=1; strcpy(v.title,"SCREEN-OFF MODE");
    strcpy(v.rows[0],"Pause app"); strcpy(v.rows[1],"* Keep app running");
    strcpy(v.detail,"Keep running: voice,\nnetwork and keys stay.\nHold OK: light + menu.\nNot deep sleep.");
    pp_ui_render(&v); tick(200); snapshot("passport-screen-mode.rgb");
    v.row_count=0; strcpy(v.title,"FLASH / PROGRAM");
    strcpy(v.detail,PP_PREVIEW_CAPACITY);
    pp_ui_render(&v); tick(200); snapshot("passport-storage.rgb");
    strcpy(v.title,"DEVICE STATUS");
    strcpy(v.detail,"Wi-Fi: ONLINE\nIP: 192.0.2.10\nBLE: OFF\nVolume: 65%\nBrightness: 80%\nBattery: 99%\nPassport " PP_VERSION "\n\nOK: back");
    pp_ui_render(&v); tick(200); snapshot("passport-device.rgb");
    lv_mem_monitor_t before,after; lv_mem_monitor(&before);
    for(unsigned i=0;i<5000;++i) {
        v.menu=i%2; v.keyboard=i%3==0; v.keyboard_page=i%4;
        v.keyboard_selected=i%pp_keyboard_count(v.keyboard_page);
        v.module=i%7 ? &pp_muyu_module : NULL;
        pp_ui_render(&v); tick(20);
    }
    tick(1000); lv_mem_monitor(&after); assert(after.free_size+512>=before.free_size);
    audio_ok=false; v.menu=false; v.keyboard=false; v.module=&pp_muyu_module; v.battery=-1;
    strcpy(v.status,"Wi-Fi OFF | BLE OFF");
    pp_ui_render(&v); tick(200); snapshot("passport-error.rgb");
    printf("Passport UI: PASS (5000 transitions; 32 KiB pool; free %zu -> %zu bytes)\n",(size_t)before.free_size,(size_t)after.free_size);
    return 0;
}
