/* Real shell, app lifecycle and UI; cloud/audio snapshots are explicitly fake. */
#include "passport_apps.h"
#include "passport_ui.h"
#include "pp_voice.h"
#include "pp_pet.h"
#include "pp_pet_store.h"
#include "lvgl.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern const pp_app_module_t pp_xiaozhi_module,pp_pet_module;
static pp_runtime_t runtime;
static pp_voice_snapshot_t voice;
static bool owned;
static unsigned opens,closes,presses;
static uint8_t durable[PP_PET_SAVE_SIZE];
pp_runtime_t *pp_app_runtime(void) { return &runtime; }
void pp_voice_open(void) { assert(!owned); owned=true; ++opens; voice.state=PP_VOICE_READY; }
void pp_voice_close(void *ctx) { (void)ctx; if(owned) ++closes; owned=false; voice.state=PP_VOICE_OFF; }
void pp_voice_tick(void) {}
void pp_voice_press(void) { assert(owned); ++presses; }
void pp_voice_snapshot(pp_voice_snapshot_t *out) { *out=voice; }
bool pp_pet_store_open(pp_pet_save_t *save)
{ if(!pp_pet_decode(save,durable,sizeof(durable))) pp_pet_defaults(save,987); return true; }
bool pp_pet_store_write(void *ctx,const uint8_t bytes[PP_PET_SAVE_SIZE])
{ (void)ctx; memcpy(durable,bytes,sizeof(durable)); return true; }
void pp_pet_store_close(void) {}
static uint16_t draw_buffer[240*20],pixels[240*320];
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *data)
{
    size_t width=(size_t)lv_area_get_width(a);
    for(int y=a->y1;y<=a->y2;++y) { memcpy(&pixels[y*240+a->x1],data,width*2); data+=width*2; }
    lv_display_flush_ready(d);
}
static void tick(unsigned ms)
{ for(unsigned i=0;i<ms;i+=5) { lv_tick_inc(5); lv_timer_handler(); } }
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
static void ui_bounds(lv_obj_t *p)
{
    for(unsigned i=0;i<lv_obj_get_child_count(p);++i) {
        lv_obj_t *c=lv_obj_get_child(p,(int)i); lv_area_t a; lv_obj_get_coords(c,&a);
        assert(a.x1>=0 && a.x2<240 && a.y1>=0 && a.y2<320);
        ui_bounds(c);
    }
}
int main(void)
{
    lv_init(); lv_display_t *d=lv_display_create(240,320);
    lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d,draw_buffer,NULL,sizeof(draw_buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d,flush); pp_ui_create();
    pp_app_t apps[]={
        {.id="xiaozhi",.start=pp_xiaozhi_module.start,.stop=pp_xiaozhi_module.stop,.focus=pp_xiaozhi_module.focus,.key=pp_xiaozhi_module.key},
        {.id="pet",.start=pp_pet_module.start,.stop=pp_pet_module.stop,.focus=pp_pet_module.focus,.key=pp_pet_module.key}};
    pp_runtime_init(&runtime,apps,2); assert(pp_activate(&runtime,0));
    pp_view_t view={.battery=99,.buttons_ok=true,.module=&pp_xiaozhi_module,.wifi=PP_LINK_ON};
    strcpy(view.app,"Xiaozhi");
    const struct { pp_voice_state_t state; const char *detail,*file; } cases[]={
        {PP_VOICE_READY,"OK to start talking","passport-xiaozhi-ready.rgb"},
        {PP_VOICE_LISTENING,"Speak now. OK to send","passport-xiaozhi-listening.rgb"},
        {PP_VOICE_SPEAKING,"OK to stop the answer","passport-xiaozhi-speaking.rgb"},
        {PP_VOICE_ACTIVATION,"Add this code at xiaozhi.me","passport-xiaozhi-activation.rgb"},
        {PP_VOICE_ERROR,"Connection ended. OK retry","passport-xiaozhi-error.rgb"}};
    for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
        voice.state=cases[i].state; strcpy(voice.detail,cases[i].detail);
        strcpy(voice.activation,"123456"); voice.level=1200;
        pp_ui_render(&view); tick(200); snapshot(cases[i].file);
    }
    /* Dark-running allows an intentional talk key; menu/paused blocks it. */
    pp_xiaozhi_module.tick(NULL,20,false); pp_dispatch(&runtime,PP_OK); assert(presses==1);
    pp_menu_open(&runtime); pp_dispatch(&runtime,PP_OK); assert(presses==1 && !owned);
    pp_resume(&runtime); assert(owned); pp_dispatch(&runtime,PP_OK); assert(presses==2);
    pp_ui_render(&view); tick(200);
    lv_mem_monitor_t before,after; lv_mem_monitor(&before);
    for(unsigned i=0;i<1000;++i) {
        assert(pp_activate(&runtime,1)); assert(!owned);
        view.module=&pp_pet_module; pp_pet_module.tick(NULL,20,true); pp_ui_render(&view); tick(20);
        assert(pp_activate(&runtime,0)); assert(owned);
        view.module=&pp_xiaozhi_module; pp_ui_render(&view); tick(20);
        pp_menu_open(&runtime); assert(!owned); pp_resume(&runtime); assert(owned);
    }
    pp_ui_render(&view); tick(200); lv_mem_monitor(&after);
    assert(after.free_size+256>=before.free_size); assert(lv_mem_test()==LV_RESULT_OK);
    /* Inspect app content only; hidden animated system drawers may be offscreen. */
    ui_bounds(lv_obj_get_child(lv_screen_active(),5));
    pp_menu_open(&runtime); pp_xiaozhi_module.stop(NULL); assert(opens==closes && !owned);
    printf("Xiaozhi UI/lifecycle: PASS (2000 app switches; focus/dark input; mock opens=%u closes=%u; LVGL %zu -> %zu free)\n",
        opens,closes,before.free_size,after.free_size);
    puts("Cloud/audio snapshots are simulated; microphone and speaker require device acceptance.");
}
