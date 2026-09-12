#include "lvgl.h"
#include "passport_ui.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint16_t draw_buffer[240*20], pixels[240*320];
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
int main(void)
{
    lv_init(); lv_display_t *d=lv_display_create(240,320);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d, draw_buffer, NULL, sizeof(draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, flush); pp_ui_create();
    pp_view_t v={.muyu=true,.audio_ok=true,.buttons_ok=true,.battery=99,.count=12};
    strcpy(v.app,"Muyu"); strcpy(v.status,"Wi-Fi ONLINE | BLE OFF");
    pp_ui_render(&v,false); tick(200); snapshot("passport-muyu.rgb");
    v.menu=true; v.row_count=4; v.selected=1; strcpy(v.title,"SYSTEM");
    strcpy(v.rows[0],"Resume app"); strcpy(v.rows[1],"Applications"); strcpy(v.rows[2],"Settings"); strcpy(v.rows[3],"About");
    strcpy(v.detail,"One device. Your apps."); pp_ui_render(&v,false); tick(200); snapshot("passport-menu.rgb");
    v.row_count=6; v.selected=0; strcpy(v.title,"SETTINGS");
    const char *rows[]={"Wi-Fi","Bluetooth LE","Sound","Display","Screen timeout","Diagnostics"};
    for (int i=0;i<6;++i) strcpy(v.rows[i],rows[i]);
    v.detail[0]=0;
    pp_ui_render(&v,false); tick(200); snapshot("passport-settings.rgb");
    v.row_count=2; strcpy(v.title,"WI-FI SETUP"); strcpy(v.rows[0],"Stop setup"); strcpy(v.rows[1],"Back (keep setup)");
    strcpy(v.detail,"Join:\nPassport-DEMO\nPassword:\nexample-only\nOpen 192.168.4.1\nCloses after 5 min.");
    pp_ui_render(&v,false); tick(200); snapshot("passport-setup.rgb");
    v.menu=false; v.muyu=false; strcpy(v.app,"Device status");
    strcpy(v.detail,"Wi-Fi: ONLINE\nIP: 192.0.2.10\nBLE beacon: OFF\nVolume: 65%\nBrightness: 80%\nHeap: host preview\nPassport 0.2.0");
    pp_ui_render(&v,false); tick(200); snapshot("passport-device.rgb");
    lv_mem_monitor_t before, after; lv_mem_monitor(&before);
    for (unsigned i=0; i<5000; ++i) {
        v.menu=i%2; v.muyu=i%3 != 0; v.count=i; v.selected=i%2;
        pp_ui_render(&v,true); tick(20);
    }
    tick(1000); lv_mem_monitor(&after);
    assert(after.free_size + 256 >= before.free_size);
    v.menu=false; v.muyu=true; v.count=UINT32_MAX; v.audio_ok=false; v.battery=-1;
    pp_ui_render(&v,false); tick(200); snapshot("passport-error.rgb");
    printf("Passport UI: PASS (5000 transitions; 32 KiB pool; free %zu -> %zu bytes)\n", (size_t)before.free_size,(size_t)after.free_size);
    return 0;
}
