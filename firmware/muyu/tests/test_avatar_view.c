/* Real LVGL ownership and pixel-parity tests; no pet model, saves or audio. */
#include "pp_avatar_view.h"
#include "pp_pixel_buffer.h"
#ifdef NDEBUG
#error Avatar view tests require assertions
#endif
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static uint16_t draw_buffer[240*20];
static uint8_t reference_pixels[64+96*88/2];
static unsigned draw_events;
static void count_draw(lv_event_t *event) { (void)event; ++draw_events; }
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *data)
{ (void)area; (void)data; lv_display_flush_ready(display); }
static void check_pixels(lv_obj_t *view,const pp_avatar_frame_t *frame)
{
    pp_pixel_buffer_t buffer;
    pp_pixel_buffer_init(&buffer,reference_pixels,96,88);
    pp_pixel_sink_t sink={pp_pixel_buffer_rect,&buffer};
    pp_avatar_draw(&sink,10,8,frame);
    pp_avatar_view_render(view,frame);
    const lv_image_dsc_t *image=lv_image_get_src(view);
    assert(image->header.w==96 && image->header.h==88 && image->header.stride==48);
    assert(image->header.cf==LV_COLOR_FORMAT_I4 && image->data_size==sizeof(reference_pixels));
    assert(!memcmp(image->data,reference_pixels,sizeof(reference_pixels)));
    /* A repeated identical frame must retain the same pixel result. */
    pp_avatar_view_render(view,frame);
    assert(!memcmp(image->data,reference_pixels,sizeof(reference_pixels)));
}
int main(void)
{
    lv_init();
    lv_display_t *display=lv_display_create(240,320); assert(display);
    lv_display_set_buffers(display,draw_buffer,NULL,sizeof(draw_buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush);
    lv_obj_t *screen=lv_obj_create(NULL); lv_screen_load(screen);
    lv_obj_t *parent=lv_obj_create(screen),*other=lv_obj_create(screen);
    lv_obj_set_size(parent,240,265);
    assert(pp_avatar_view_create(NULL,0,0)==NULL);
    lv_obj_t *view=pp_avatar_view_create(parent,72,77); assert(view);
    lv_obj_update_layout(parent);
    assert(lv_obj_get_x(view)==72 && lv_obj_get_y(view)==77);
    const lv_image_dsc_t *image=lv_image_get_src(view);
    const void *only_surface=image->data;
    assert(pp_avatar_view_create(other,0,0)==NULL);
    assert(lv_obj_get_child_count(other)==0);
    const unsigned phases[]={0,1,2,3,4,UINT_MAX};
    unsigned comparisons=0;
    for(unsigned pose=AVATAR_LOOK;pose<=AVATAR_GROW;++pose)
        for(unsigned voice=AVATAR_IDLE;voice<=AVATAR_OFFLINE;++voice)
            for(unsigned evolved=0;evolved<2;++evolved)
                for(unsigned i=0;i<sizeof(phases)/sizeof(phases[0]);++i) {
                    pp_avatar_frame_t frame={.pose=(pp_avatar_pose_t)pose,.voice=(pp_avatar_state_t)voice,.frame=phases[i],.evolved=evolved!=0};
                    check_pixels(view,&frame); ++comparisons;
                }
    /* Only chat parameters change here: stale cache keys must not retain an
     * old mouth, blink, leaf, breath, glance or thought marker. */
    lv_obj_add_event_cb(view,count_draw,LV_EVENT_DRAW_MAIN,NULL);
    for(unsigned state=AVATAR_IDLE;state<=AVATAR_OFFLINE;++state)
        for(unsigned field=0;field<8;++field) {
            pp_avatar_frame_t chat={.pose=AVATAR_LOOK,.voice=(pp_avatar_state_t)state,
                .chat={.enabled=true}};
            check_pixels(view,&chat);
            switch(field) {
            case 0: chat.chat.blink=true; break;
            case 1: chat.chat.breathe=2; break;
            case 2: chat.chat.ear=1; break;
            case 3: chat.chat.mouth=3; break;
            case 4: chat.chat.dots=2; break;
            case 5: chat.chat.gaze=2; break;
            case 6: chat.chat.tilt=1; break;
            default: chat.chat.enabled=false; break;
            }
            check_pixels(view,&chat); ++comparisons;
        }
    pp_avatar_frame_t clockless={.pose=AVATAR_LOOK,.voice=AVATAR_SPEAKING,
        .chat={.enabled=true,.mouth=2}};
    check_pixels(view,&clockless); lv_refr_now(display);
    unsigned stable_draws=draw_events;
    clockless.frame=12345; pp_avatar_view_render(view,&clockless); lv_refr_now(display);
    assert(draw_events==stable_draws);
    pp_avatar_frame_t frame={.pose=AVATAR_READ,.voice=AVATAR_IDLE,.frame=1,.evolved=false};
    check_pixels(view,&frame);
    pp_avatar_view_render(NULL,&frame); pp_avatar_view_render(other,&frame);
    pp_avatar_view_render(view,NULL);
    assert(!memcmp(image->data,reference_pixels,sizeof(reference_pixels)));
    lv_refr_now(display); lv_obj_clean(parent);
    /* Parent cleanup must deliver DELETE and release the unique surface. */
    view=pp_avatar_view_create(other,2,3); assert(view);
    assert(((const lv_image_dsc_t *)lv_image_get_src(view))->data==only_surface);
    check_pixels(view,&frame); lv_refr_now(display); lv_obj_delete(view);
    lv_mem_monitor_t before,after;
    lv_mem_monitor(&before);
    for(unsigned i=0;i<2000;++i) {
        lv_obj_t *current=i%2?parent:other;
        view=pp_avatar_view_create(current,72,77); assert(view);
        assert(((const lv_image_dsc_t *)lv_image_get_src(view))->data==only_surface);
        frame.frame=i; check_pixels(view,&frame);
        lv_refr_now(display);
        if(i%2) lv_obj_delete(view); else lv_obj_clean(current);
    }
    lv_mem_monitor(&after);
    assert(after.free_size==before.free_size);
    printf("{\"pixel_comparisons\":%u,\"ownership_cycles\":2000,\"sprite_bytes\":%zu,\"lvgl_free_before\":%zu,\"lvgl_free_after\":%zu}\n",
           comparisons,sizeof(reference_pixels),(size_t)before.free_size,(size_t)after.free_size);
    lv_obj_delete(parent); lv_obj_delete(other); lv_display_delete(display); lv_deinit();
    return 0;
}
