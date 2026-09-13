#include "passport_apps.h"
#include "pp_voice.h"
#include "pp_xiaozhi_ui.h"
static bool start(void *ctx) { (void)ctx; return true; }
static void stop(void *ctx) { pp_xiaozhi_ui_suspend(); pp_voice_close(ctx); }
static void focus(void *ctx,bool focused)
{
    (void)ctx;
    if(!focused) { pp_xiaozhi_ui_suspend(); pp_voice_close(NULL); return; }
    if(pp_resource_acquire(pp_app_runtime(),pp_voice_close,NULL)>=0) pp_voice_open();
}
static void key(void *ctx,pp_key_t key)
{ (void)ctx; if(key==PP_OK) pp_voice_press(); }
static void tick(void *ctx,uint32_t elapsed,bool showing)
{
    (void)ctx; (void)elapsed;
    if(!showing) pp_xiaozhi_ui_suspend();
    pp_voice_tick();
}
static void render(void)
{ pp_voice_snapshot_t s; pp_voice_snapshot(&s); pp_xiaozhi_ui_render(&s); }
const pp_app_module_t pp_xiaozhi_module={.start=start,.stop=stop,.focus=focus,.key=key,
    .create_ui=pp_xiaozhi_ui_create,.render_ui=render,.tick=tick};
