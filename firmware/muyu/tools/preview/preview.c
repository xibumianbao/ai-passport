#include "lvgl.h"
#include "muyu_ui.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint16_t draw_buffer[240 * 320], pixels[240 * 320];

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *data)
{
    size_t width = (size_t)lv_area_get_width(area);
    for (int y = area->y1; y <= area->y2; ++y) {
        memcpy(&pixels[y * 240 + area->x1], data, width * 2);
        data += width * 2;
    }
    lv_display_flush_ready(display);
}

static void tick(unsigned ms)
{
    for (unsigned n = 0; n < ms; n += 5) {
        lv_tick_inc(5);
        lv_timer_handler();
    }
}

static void snapshot(const char *path)
{
    lv_refr_now(NULL);
    FILE *file = fopen(path, "wb");
    assert(file);
    for (size_t i = 0; i < 240 * 320; ++i) {
        unsigned value = pixels[i];
        unsigned char rgb[] = {
            (unsigned char)(((value >> 11) & 31) * 255 / 31),
            (unsigned char)(((value >> 5) & 63) * 255 / 63),
            (unsigned char)((value & 31) * 255 / 31)
        };
        assert(fwrite(rgb, 1, 3, file) == 3);
    }
    assert(fclose(file) == 0);
}

int main(void)
{
    lv_init();
    lv_display_t *display = lv_display_create(240, 320);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, draw_buffer, NULL, sizeof(draw_buffer), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, flush);
    muyu_ui_create();
    muyu_ui_refresh(0, 86, true, true, false);
    tick(100);
    snapshot("muyu-idle.rgb");
    muyu_ui_refresh(12, 86, true, true, true);
    tick(65);
    snapshot("muyu-hit.rgb");
    tick(1000);
    lv_mem_monitor_t before, after;
    lv_mem_monitor(&before);
    for (unsigned i = 13; i <= 1012; ++i) {
        muyu_ui_refresh(i, 86, true, true, true);
        tick(40);
    }
    tick(1000);
    lv_mem_monitor(&after);
    assert(after.free_size + 256 >= before.free_size);
    muyu_ui_refresh(UINT32_MAX, -1, false, true, false);
    tick(100);
    snapshot("muyu-max.rgb");
    printf("Muyu UI: PASS (1000 rapid strikes; free heap %zu -> %zu bytes)\n",
           (size_t)before.free_size, (size_t)after.free_size);
    return 0;
}
