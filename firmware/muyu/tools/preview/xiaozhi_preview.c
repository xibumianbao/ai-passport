/* Real shell, app lifecycle and UI; cloud/audio snapshots are explicitly fake. */
#include "passport_apps.h"
#include "passport_ui.h"
#include "pp_voice.h"
#include "pp_pet.h"
#include "pp_pet_store.h"
#include "pp_avatar_view.h"
#include "lvgl.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern const pp_app_module_t pp_xiaozhi_module,pp_pet_module;
static pp_runtime_t runtime;
static pp_voice_snapshot_t voice;
static bool owned;
static unsigned opens,closes,presses;
static unsigned store_opens,store_writes,store_closes;
static uint8_t durable[PP_PET_SAVE_SIZE];
pp_runtime_t *pp_app_runtime(void) { return &runtime; }
void pp_voice_open(void) { assert(!owned); owned=true; ++opens; voice.state=PP_VOICE_READY; }
void pp_voice_close(void *ctx) { (void)ctx; if(owned) ++closes; owned=false; voice.state=PP_VOICE_OFF; }
void pp_voice_tick(void) {}
void pp_voice_press(void) { assert(owned); ++presses; }
void pp_voice_snapshot(pp_voice_snapshot_t *out) { *out=voice; }
bool pp_pet_store_open(pp_pet_save_t *save)
{ ++store_opens; if(!pp_pet_decode(save,durable,sizeof(durable))) pp_pet_defaults(save,987); return true; }
bool pp_pet_store_write(void *ctx,const uint8_t bytes[PP_PET_SAVE_SIZE])
{ (void)ctx; ++store_writes; memcpy(durable,bytes,sizeof(durable)); return true; }
void pp_pet_store_close(void) { ++store_closes; }
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
        assert(a.x1>=0 && a.x2<240 && a.y1>=30 && a.y2<320);
        ui_bounds(c);
    }
}
static lv_obj_t *content(void)
{
    /* Header structure may change independently; locate its geometry, never a
     * fragile child index. Hidden drawers are deliberately excluded. */
    lv_obj_t *screen=lv_screen_active(),*found=NULL;
    lv_obj_update_layout(screen);
    for(unsigned i=0;i<lv_obj_get_child_count(screen);++i) {
        lv_obj_t *child=lv_obj_get_child(screen,(int)i); lv_area_t a;
        lv_obj_get_coords(child,&a);
        if(a.x1==0 && a.y1==30 && lv_area_get_width(&a)==240 && lv_area_get_height(&a)==290) {
            assert(!found); found=child;
        }
    }
    assert(found); return found;
}
static unsigned avatars(lv_obj_t *parent)
{
    unsigned count=0;
    for(unsigned i=0;i<lv_obj_get_child_count(parent);++i) {
        lv_obj_t *child=lv_obj_get_child(parent,(int)i);
        if(lv_obj_check_type(child,&lv_image_class)) {
            const lv_image_dsc_t *src=lv_image_get_src(child);
            if(src && src->header.w==96 && src->header.h==88) ++count;
        }
        count+=avatars(child);
    }
    return count;
}
static void check_content(void)
{
    lv_obj_t *root=content(); ui_bounds(root);
    assert(avatars(root)==1); /* Detect a lost shared owner after app deletion. */
    assert(pp_avatar_view_create(root,0,0)==NULL); /* Only one foreground surface. */
}
static uint32_t region_hash(int x,int y,int w,int h)
{
    lv_refr_now(NULL);
    uint32_t hash=2166136261u;
    for(int row=y;row<y+h;++row) for(int col=x;col<x+w;++col)
        hash=(hash^pixels[row*240+col])*16777619u;
    return hash;
}
int main(void)
{
    /* Seed a real, non-default encoded save. Dialogue UI and controls must not
     * even open storage, let alone silently reset/rewrite these bytes. */
    pp_pet_save_t saved; pp_pet_defaults(&saved,987);
    saved.sequence=41; saved.cycles=17; saved.cycle_ms=12345;
    saved.coins=73; saved.xp=21; saved.fullness=64; saved.energy=78;
    pp_pet_encode(&saved,durable); assert(pp_pet_decode(&saved,durable,sizeof(durable)));
    uint8_t original[PP_PET_SAVE_SIZE]; memcpy(original,durable,sizeof(original));
    lv_init(); lv_display_t *d=lv_display_create(240,320);
    lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(d,draw_buffer,NULL,sizeof(draw_buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d,flush); pp_ui_create();
    pp_app_t apps[]={
        {.id="xiaozhi",.start=pp_xiaozhi_module.start,.stop=pp_xiaozhi_module.stop,.focus=pp_xiaozhi_module.focus,.key=pp_xiaozhi_module.key},
        {.id="pet",.start=pp_pet_module.start,.stop=pp_pet_module.stop,.focus=pp_pet_module.focus,.key=pp_pet_module.key}};
    pp_runtime_init(&runtime,apps,2); assert(pp_activate(&runtime,0));
    pp_view_t view={.battery=99,.buttons_ok=true,.module=&pp_xiaozhi_module,.wifi=PP_LINK_ON};
    strcpy(view.app,"Yaya Chat");
    const struct { pp_voice_state_t state; const char *detail,*file; } cases[]={
        {PP_VOICE_READY,"OK to start talking","passport-xiaozhi-ready.rgb"},
        {PP_VOICE_LISTENING,"Speak; pauses send automatically","passport-xiaozhi-listening.rgb"},
        {PP_VOICE_THINKING,"Waiting for the spoken reply","passport-xiaozhi-thinking.rgb"},
        {PP_VOICE_SPEAKING,"Listening resumes after reply","passport-xiaozhi-speaking.rgb"},
        {PP_VOICE_STOPPED,"Conversation stopped","passport-xiaozhi-stopped.rgb"},
        {PP_VOICE_ACTIVATION,"Add this code at xiaozhi.me","passport-xiaozhi-activation.rgb"},
        {PP_VOICE_WIFI,"Connect Wi-Fi in Settings","passport-xiaozhi-wifi.rgb"},
        {PP_VOICE_ERROR,"Connection ended. OK retry","passport-xiaozhi-error.rgb"},
        {PP_VOICE_ERROR,"Low stack after first capture encode: 2048 bytes","passport-xiaozhi-long-error.rgb"}};
    for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
        voice.state=cases[i].state; strcpy(voice.detail,cases[i].detail);
        view.wifi=voice.state==PP_VOICE_WIFI?PP_LINK_OFF:PP_LINK_ON;
        strcpy(voice.activation,"123456"); voice.level=1200;
        voice.playback_level=3000; voice.playback_age_ms=voice.state==PP_VOICE_SPEAKING?0:UINT16_MAX;
        for(unsigned step=0;step<8;++step) { pp_ui_render(&view); tick(200); }
        check_content(); snapshot(cases[i].file);
        assert(!memcmp(original,durable,sizeof(original)));
        assert(!store_opens && !store_writes && !store_closes);
    }
    /* Actual LVGL pixels must respond to playback, not a leftover microphone
     * level or a clock-driven mouth. Freeze time between loud/stale samples. */
    voice.state=PP_VOICE_SPEAKING; voice.level=32000;
    voice.playback_level=3200; voice.playback_age_ms=0;
    pp_ui_render(&view); uint32_t loud=region_hash(72,224,96,88);
    uint32_t loud_card=region_hash(24,58,192,78);
    voice.playback_age_ms=250; pp_ui_render(&view);
    uint32_t quiet=region_hash(72,224,96,88);
    assert(loud!=quiet && loud_card!=region_hash(24,58,192,78));
    snapshot("passport-xiaozhi-speaking-pause.rgb");
    voice.playback_age_ms=0; voice.playback_level=0; pp_ui_render(&view);
    assert(region_hash(72,224,96,88)==quiet);
    /* Foreground-only reset; dark-running continues voice but never draws or
     * catches up old motion. An under-two-second gap also tests explicit reset. */
    voice.state=PP_VOICE_LISTENING; voice.level=0;
    pp_xiaozhi_module.tick(NULL,20,false); pp_ui_render(&view);
    uint32_t fresh=region_hash(72,224,96,88);
    tick(1000); pp_ui_render(&view); uint32_t moved=region_hash(72,224,96,88);
    assert(moved!=fresh);
    pp_xiaozhi_module.tick(NULL,20,false); tick(1000);
    assert(region_hash(72,224,96,88)==moved);
    pp_xiaozhi_module.tick(NULL,20,true); pp_ui_render(&view);
    assert(region_hash(72,224,96,88)==fresh);
    /* Reviewable actual-render frames: variation, speech and silent pauses. */
    for(unsigned i=0;i<24;++i) {
        voice.state=i<6?PP_VOICE_LISTENING:i<12?PP_VOICE_THINKING:PP_VOICE_SPEAKING;
        voice.level=1200;
        voice.playback_level=i%5==0?0:i%3==0?3200:1200;
        voice.playback_age_ms=0;
        pp_ui_render(&view); tick(200);
        char filename[64]; snprintf(filename,sizeof(filename),"passport-xiaozhi-motion-%02u.rgb",i);
        snapshot(filename);
    }
    /* Dark-running allows an intentional talk key; menu/paused blocks it. */
    pp_xiaozhi_module.tick(NULL,20,false); pp_dispatch(&runtime,PP_OK); assert(presses==1);
    pp_menu_open(&runtime); pp_dispatch(&runtime,PP_OK); assert(presses==1 && !owned);
    pp_resume(&runtime); assert(owned); pp_dispatch(&runtime,PP_OK); assert(presses==2);
    pp_ui_render(&view); tick(200);
    assert(!memcmp(original,durable,sizeof(original)));
    assert(!store_opens && !store_writes && !store_closes);
    lv_mem_monitor_t before,after; lv_mem_monitor(&before);
    for(unsigned i=0;i<1000;++i) {
        assert(pp_activate(&runtime,1)); assert(!owned);
        view.module=&pp_pet_module; pp_pet_module.tick(NULL,20,true);
        pp_pet_module.tick(NULL,20,true); /* Make real pet state dirty before its own checkpoint. */
        pp_ui_render(&view); tick(20);
        check_content();
        assert(pp_activate(&runtime,0)); assert(owned);
        /* Pet focus/stop is allowed to checkpoint its own dirty state. Take
         * the baseline after that has finished, then test dialogue alone. */
        uint8_t before_chat[PP_PET_SAVE_SIZE]; memcpy(before_chat,durable,sizeof(before_chat));
        unsigned io_opens=store_opens,io_writes=store_writes,io_closes=store_closes;
        view.module=&pp_xiaozhi_module; pp_ui_render(&view); tick(20);
        check_content(); pp_xiaozhi_module.tick(NULL,200,true);
        pp_dispatch(&runtime,PP_OK);
        pp_menu_open(&runtime); assert(!owned); pp_resume(&runtime); assert(owned);
        assert(!memcmp(before_chat,durable,sizeof(before_chat)));
        assert(store_opens==io_opens && store_writes==io_writes && store_closes==io_closes);
    }
    pp_ui_render(&view); tick(200); lv_mem_monitor(&after);
    assert(store_writes>0 && memcmp(original,durable,sizeof(original)));
    assert(after.free_size+256>=before.free_size); assert(lv_mem_test()==LV_RESULT_OK);
    check_content();
    pp_menu_open(&runtime); pp_xiaozhi_module.stop(NULL); assert(opens==closes && !owned);
    /* Parent deletion must release the shared owner as well as its cache. */
    lv_obj_t *root=content(); lv_obj_clean(root);
    lv_obj_t *probe=pp_avatar_view_create(root,72,52); assert(probe); lv_obj_delete(probe);
    probe=pp_avatar_view_create(root,72,52); assert(probe); lv_obj_delete(probe);
    assert(lv_mem_test()==LV_RESULT_OK);
    printf("Yaya UI/lifecycle: PASS (2000 app switches; shared owner deletion; pet-save isolation; focus/dark input; playback/silence pixels; mock opens=%u closes=%u; LVGL %zu -> %zu free; peak=%zu)\n",
        opens,closes,before.free_size,after.free_size,after.max_used);
    puts("Cloud/audio snapshots are simulated; microphone and speaker require device acceptance.");
}
