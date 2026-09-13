#pragma once
#include "pp_avatar.h"
#include <stddef.h>

#define PP_PIXEL_PALETTE_BYTES 64u
typedef struct { uint8_t *data; unsigned width, height; } pp_pixel_buffer_t;
/* LVGL I4: 16 BGRA palette entries, followed by high-nibble-first pixels.
 * Width must be even. Index 0 is transparent; all other entries are opaque. */
size_t pp_pixel_buffer_size(unsigned width, unsigned height);
void pp_pixel_buffer_init(pp_pixel_buffer_t *buffer, uint8_t *data, unsigned width, unsigned height);
void pp_pixel_buffer_rect(void *ctx,int x,int y,int w,int h,uint32_t rgb);
