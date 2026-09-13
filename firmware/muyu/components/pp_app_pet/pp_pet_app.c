#include "passport_apps.h"
#include "pp_pet.h"
#include "pp_pet_store.h"
#include "pp_pet_ui.h"
static pp_pet_t pet;
static bool running,focused,visible;
static bool start(void *ctx)
{
    (void)ctx; pp_pet_save_t save;
    if(!pp_pet_store_open(&save)) return false;
    pp_pet_init(&pet,&save,pp_pet_store_write,NULL);
    running=true; visible=false; return true;
}
static void stop(void *ctx)
{
    (void)ctx;
    if(running) pp_pet_checkpoint(&pet);
    running=focused=visible=false; pp_pet_store_close();
}
static void focus(void *ctx,bool value)
{
    (void)ctx; focused=value;
    if(!value) { visible=false; pp_pet_checkpoint(&pet); }
}
static void key(void *ctx,pp_key_t key)
{
    (void)ctx;
    if(!running || !focused || !visible) return; /* No invisible purchase. */
    if(key==PP_OK) pp_pet_press(&pet);
    else if(key==PP_UP || key==PP_DOWN) pp_pet_select(&pet,key==PP_UP?-1:1);
}
static void tick(void *ctx,uint32_t elapsed_ms,bool showing)
{
    (void)ctx;
    if(!running) return;
    showing=showing && focused;
    if(visible && !showing) pp_pet_checkpoint(&pet);
    /* No catch-up on the first tick after resume, menu or a screen transition. */
    if(showing && visible) pp_pet_tick(&pet,elapsed_ms);
    visible=showing;
}
static void render(void) { pp_pet_ui_render(&pet,AVATAR_IDLE); }
const pp_app_module_t pp_pet_module={.start=start,.stop=stop,.focus=focus,.key=key,
    .create_ui=pp_pet_ui_create,.render_ui=render,.tick=tick};
