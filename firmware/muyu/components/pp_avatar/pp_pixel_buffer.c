#include "pp_pixel_buffer.h"
#include <limits.h>
#include <string.h>
static const uint32_t colors[16]={0,0x283f45,0x306868,0x50877a,0x88b887,0xbddb9e,
    0xf5dfad,0xe7ac67,0xad6857,0x72594d,0xb78a68,0x749b9a,0xb3cbb6,0xd9a28c,0xe4c497,0x906b57};
size_t pp_pixel_buffer_size(unsigned w,unsigned h) { return PP_PIXEL_PALETTE_BYTES+(size_t)w*h/2; }
void pp_pixel_buffer_init(pp_pixel_buffer_t *b,uint8_t *data,unsigned w,unsigned h)
{
    *b=(pp_pixel_buffer_t){data,w,h}; memset(data,0,pp_pixel_buffer_size(w,h));
    for(unsigned i=1;i<16;++i) {
        data[i*4]=(uint8_t)colors[i]; data[i*4+1]=(uint8_t)(colors[i]>>8);
        data[i*4+2]=(uint8_t)(colors[i]>>16); data[i*4+3]=255;
    }
}
void pp_pixel_buffer_rect(void *ctx,int x,int y,int w,int h,uint32_t rgb)
{
    pp_pixel_buffer_t *b=ctx;
    if(w<=0 || h<=0 || x>=(int)b->width || y>=(int)b->height || x+w<=0 || y+h<=0) return;
    unsigned best=1; int distance=INT_MAX;
    for(unsigned i=1;i<16;++i) {
        int red=(int)((rgb>>16)&255)-(int)((colors[i]>>16)&255);
        int green=(int)((rgb>>8)&255)-(int)((colors[i]>>8)&255);
        int blue=(int)(rgb&255)-(int)(colors[i]&255);
        int d=red*red+green*green+blue*blue;
        if(d<distance) { best=i; distance=d; if(!d) break; }
    }
    int x1=x<0?0:x,y1=y<0?0:y;
    int x2=x+w>(int)b->width?(int)b->width:x+w, y2=y+h>(int)b->height?(int)b->height:y+h;
    for(int yy=y1;yy<y2;++yy) for(int xx=x1;xx<x2;++xx) {
        size_t p=(size_t)yy*b->width+(unsigned)xx; uint8_t *dst=b->data+64+p/2;
        *dst=(p&1)?(uint8_t)((*dst&0xf0)|best):(uint8_t)((*dst&15)|(best<<4));
    }
}
