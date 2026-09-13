#include "passport_apps.h"
#include "passport_ui.h"
#include "passport_audio.h"
#include "pp_pet.h"
#include "pp_pet_store.h"
#include "pp_pet_ui.h"
#include "lvgl.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern const pp_app_module_t pp_pet_module,pp_muyu_module;
static pp_runtime_t runtime;
static uint8_t durable[PP_PET_SAVE_SIZE];
static unsigned writes;
static bool store_fail;
pp_runtime_t *pp_app_runtime(void) { return &runtime; }
bool pp_audio_ok(void) { return true; }
void pp_audio_begin(uint32_t gen) { (void)gen; }
void pp_audio_knock(uint32_t gen) { (void)gen; }
void pp_audio_cancel(void *ctx) { (void)ctx; }
bool pp_pet_store_open(pp_pet_save_t *save)
{
    if(pp_pet_decode(save,durable,sizeof(durable))) return true;
    pp_pet_defaults(save,987); return true;
}
bool pp_pet_store_write(void *ctx,const uint8_t bytes[PP_PET_SAVE_SIZE])
{
    (void)ctx; ++writes; if(store_fail) return false;
    memcpy(durable,bytes,sizeof(durable)); return true;
}
void pp_pet_store_close(void) { }
static uint16_t draw_buffer[240*20],pixels[240*320];
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *data)
{
    size_t width=(size_t)lv_area_get_width(a);
    for(int y=a->y1;y<=a->y2;++y) { memcpy(&pixels[y*240+a->x1],data,width*2); data+=width*2; }
    lv_display_flush_ready(d);
}
static void lv_tick(unsigned ms)
{
    for(unsigned i=0;i<ms;i+=5) { lv_tick_inc(5); lv_timer_handler(); }
}
static void snapshot(const char *name)
{
    lv_refr_now(NULL); FILE *f=fopen(name,"wb"); assert(f);
    for(unsigned i=0;i<240*320;++i) {
        unsigned v=pixels[i]; uint8_t rgb[]={(uint8_t)(((v>>11)&31)*255/31),
            (uint8_t)(((v>>5)&63)*255/63),(uint8_t)((v&31)*255/31)};
        assert(fwrite(rgb,1,3,f)==3);
    }
    assert(!fclose(f));
}
static pp_pet_save_t read_save(void) { pp_pet_save_t s; assert(pp_pet_decode(&s,durable,sizeof(durable))); return s; }
static void visible_time(unsigned ms)
{
    while(ms) { unsigned delta=ms>20?20:ms; pp_pet_module.tick(NULL,delta,true); ms-=delta; }
}
static void test_lifecycle(pp_view_t *v)
{
    pp_app_t apps[]={
        {.id="pet",.start=pp_pet_module.start,.stop=pp_pet_module.stop,.focus=pp_pet_module.focus,.key=pp_pet_module.key},
        {.id="muyu",.start=pp_muyu_module.start,.stop=pp_muyu_module.stop,.focus=pp_muyu_module.focus,.key=pp_muyu_module.key}};
    pp_runtime_init(&runtime,apps,2); assert(pp_activate(&runtime,0));
    pp_pet_module.tick(NULL,20,true); pp_dispatch(&runtime,PP_OK);
    pp_pet_save_t s=read_save(); assert(s.coins==5 && s.fullness==85);
    pp_dispatch(&runtime,PP_OK); /* Skip eat. */
    visible_time(27000); pp_pet_module.tick(NULL,20,false); /* Keep-running dark mode. */
    s=read_save(); assert(s.cycle_ms==27000); unsigned checkpoints=writes;
    for(unsigned i=0;i<2000;++i) { pp_dispatch(&runtime,PP_OK); pp_pet_module.tick(NULL,1000,false); }
    assert(writes==checkpoints && read_save().coins==5);
    pp_menu_open(&runtime); pp_resume(&runtime); pp_pet_module.tick(NULL,999,true);
    visible_time(33000); assert(read_save().coins==9 && read_save().cycles==1);
    assert(pp_activate(&runtime,1)); assert(pp_activate(&runtime,0)); pp_pet_module.tick(NULL,20,true);
    visible_time(1000); pp_menu_open(&runtime); s=read_save(); assert(s.cycles==1 && s.cycle_ms==1000);
    /* An input queue gap / menu must stop all active progression. */
    for(unsigned i=0;i<100;++i) pp_pet_module.tick(NULL,1000,true);
    assert(read_save().cycle_ms==1000);
    pp_resume(&runtime); pp_pet_module.tick(NULL,20,true);
    v->module=&pp_pet_module; pp_ui_render(v); lv_tick(200);
    lv_mem_monitor_t before,after; lv_mem_monitor(&before);
    for(unsigned i=0;i<2000;++i) {
        int index=(int)(i%2); assert(pp_activate(&runtime,index));
        v->module=index?&pp_muyu_module:&pp_pet_module;
        if(!index) pp_pet_module.tick(NULL,20,true);
        pp_ui_render(v); lv_tick(20);
    }
    assert(pp_activate(&runtime,0)); v->module=&pp_pet_module; pp_pet_module.tick(NULL,20,true);
    pp_ui_render(v); lv_tick(200); lv_mem_monitor(&after);
    assert(after.free_size+256>=before.free_size); assert(lv_mem_test()==LV_RESULT_OK);
    printf("Pet lifecycle: PASS (2000 app switches; dark input, menu, resume, durable recovery; LVGL free %zu -> %zu, peak %zu bytes)\n",
        before.free_size,after.free_size,after.max_used);
    pp_menu_open(&runtime); pp_pet_module.stop(NULL);
}
int main(void)
{
    lv_init(); lv_display_t *d=lv_display_create(240,320);
    lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d,draw_buffer,NULL,sizeof(draw_buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d,flush); pp_ui_create();
    pp_view_t v={.battery=99,.buttons_ok=true,.module=&pp_pet_module};
    strcpy(v.app,"Yaya Pet"); strcpy(v.status,"Wi-Fi OFF | BLE OFF"); pp_ui_render(&v);
    pp_pet_t p; pp_pet_save_t s; pp_pet_defaults(&s,987); pp_pet_init(&p,&s,pp_pet_store_write,NULL);
    pp_pet_ui_render(&p,AVATAR_IDLE); lv_tick(200); snapshot("passport-pet-home.rgb");
    p.clock_ms=11000; pp_pet_ui_render(&p,AVATAR_IDLE); lv_tick(200); snapshot("passport-pet-reading.rgb");
    pp_pet_press(&p); p.clock_ms+=200; pp_pet_ui_render(&p,AVATAR_IDLE); lv_tick(200); snapshot("passport-pet-feed.rgb");
    p.save.xp=16; p.save.coins=24; p.action=PET_GROW; p.notice=PET_EVOLVED;
    pp_pet_ui_render(&p,AVATAR_IDLE); lv_tick(200); snapshot("passport-pet-evolved.rgb");
    p.notice=PET_QUIET; p.action=PET_REST; pp_pet_ui_render(&p,AVATAR_IDLE); lv_tick(200); snapshot("passport-pet-rest.rgb");
    p.action=PET_LIVE;
    pp_pet_ui_render(&p,AVATAR_LISTENING); lv_tick(200); snapshot("passport-pet-listening.rgb");
    pp_pet_ui_render(&p,AVATAR_THINKING); lv_tick(200); snapshot("passport-pet-thinking.rgb");
    pp_pet_ui_render(&p,AVATAR_SPEAKING); lv_tick(200); snapshot("passport-pet-speaking.rgb");
    p.storage_error=true; pp_pet_ui_render(&p,AVATAR_IDLE); lv_tick(200); snapshot("passport-pet-storage-error.rgb");
    memset(durable,0,sizeof(durable)); test_lifecycle(&v);
    puts("Pet UI: PASS (actual LVGL 9.5, 32 KiB pool, 20-row display buffer, 9 rendered fixtures; voice fixtures are simulated)");
    return 0;
}
