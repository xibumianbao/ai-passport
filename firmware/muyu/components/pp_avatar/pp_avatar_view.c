#include "pp_avatar_view.h"
#include "pp_pixel_buffer.h"
#include "src/misc/cache/instance/lv_image_cache.h"

enum { SPRITE_WIDTH = 96, SPRITE_HEIGHT = 88 };

/* One shared 4,288-byte surface, including its 16-entry BGRA palette.
 * LVGL row-decodes I4; never allocate one canvas per application. */
static uint8_t sprite_pixels[PP_PIXEL_PALETTE_BYTES + SPRITE_WIDTH * SPRITE_HEIGHT / 2];
static const lv_image_dsc_t sprite = {
    .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_I4,
               .w = SPRITE_WIDTH, .h = SPRITE_HEIGHT, .stride = SPRITE_WIDTH / 2},
    .data_size = sizeof(sprite_pixels), .data = sprite_pixels,
};
static lv_obj_t *owner;
static pp_avatar_frame_t rendered_frame;
static bool rendered;

static void deleted(lv_event_t *event)
{
    if(lv_event_get_target_obj(event) != owner) return;
    /* An image decoder may retain the descriptor after its object is deleted.
     * Drop that entry before another foreground page reuses the pixel bytes. */
    lv_image_cache_drop(&sprite);
    owner = NULL;
    rendered = false;
}

lv_obj_t *pp_avatar_view_create(lv_obj_t *parent, int x, int y)
{
    if(!parent || owner) return NULL;
    lv_obj_t *view = lv_image_create(parent);
    if(!view) return NULL;
    if(!lv_obj_add_event_cb(view, deleted, LV_EVENT_DELETE, NULL)) {
        lv_obj_delete(view);
        return NULL;
    }

    lv_image_cache_drop(&sprite);
    pp_pixel_buffer_t pixels;
    pp_pixel_buffer_init(&pixels, sprite_pixels, SPRITE_WIDTH, SPRITE_HEIGHT);
    owner = view;
    rendered = false;
    lv_image_set_src(view, &sprite);
    lv_obj_set_pos(view, x, y);
    return view;
}

void pp_avatar_view_render(lv_obj_t *view, const pp_avatar_frame_t *frame)
{
    if(!view || view != owner || !frame) return;
    if(rendered && frame->frame == rendered_frame.frame && frame->pose == rendered_frame.pose &&
       frame->voice == rendered_frame.voice && frame->evolved == rendered_frame.evolved) return;

    lv_image_cache_drop(&sprite);
    pp_pixel_buffer_t pixels;
    pp_pixel_buffer_init(&pixels, sprite_pixels, SPRITE_WIDTH, SPRITE_HEIGHT);
    pp_pixel_sink_t sink = {pp_pixel_buffer_rect, &pixels};
    pp_avatar_draw(&sink, 10, 8, frame);
    rendered_frame = *frame;
    rendered = true;
    lv_obj_invalidate(view);
}
