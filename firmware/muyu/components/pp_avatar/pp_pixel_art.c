#include "pp_avatar.h"

/* Original pixel art, built from small palette-colored spans. No imported
 * artwork, raster assets, textures, frame tables or floating-point drawing. */
#define INK 0x283f45u
#define DEEP 0x306868u
#define TEAL 0x50877au
#define GREEN 0x88b887u
#define LIGHT 0xbddb9eu
#define CREAM 0xf5dfadu
#define GOLD 0xe7ac67u
#define RUST 0xad6857u
static void r(const pp_pixel_sink_t *s, int x,int y,int w,int h,uint32_t c)
{ if (w>0 && h>0) s->rect(s->ctx,x,y,w,h,c); }
static void star(const pp_pixel_sink_t *s,int x,int y,uint32_t c)
{ r(s,x+2,y,2,8,c); r(s,x,y+3,6,2,c); }
static void plant(const pp_pixel_sink_t *s,int x,int y)
{
    r(s,x+10,y,2,28,DEEP); r(s,x+2,y+5,10,6,DEEP); r(s,x+4,y+5,6,3,GREEN);
    r(s,x+12,y+1,8,5,DEEP); r(s,x+14,y+1,4,2,LIGHT);
    r(s,x,y+15,12,5,DEEP); r(s,x+3,y+15,7,2,GREEN);
    r(s,x+12,y+11,11,6,DEEP); r(s,x+14,y+11,6,3,GREEN);
    r(s,x+1,y+26,23,5,INK); r(s,x+3,y+28,19,14,RUST);
    r(s,x+5,y+30,5,10,GOLD); r(s,x+5,y+42,15,2,INK);
}
void pp_pet_room_draw(const pp_pixel_sink_t *s)
{
    /* Native 240 x 148 room: dusky teal wall, sunlit window, warm oak floor. */
    r(s,0,0,240,148,0x577a77); r(s,0,0,240,4,DEEP);
    for(int x=0;x<240;x+=24) { r(s,x,4,1,97,0x648782); r(s,x+11,4,1,97,0x547773); }
    r(s,0,93,240,8,INK); r(s,0,95,240,3,0xc59973);
    r(s,0,101,240,47,0xb78a68);
    for(int y=102;y<148;y+=12) {
        r(s,0,y,240,1,0x906b57); r(s,0,y+1,240,1,0xcd9e75);
        for(int x=(y%24?24:0);x<240;x+=60) { r(s,x,y+2,1,10,0x957057); r(s,x+8,y+6,21,1,0xc1946c); }
    }
    /* Framed window with tiny sea/hill silhouettes, original scenery. */
    r(s,71,10,91,74,INK); r(s,73,12,87,70,0xe0b581); r(s,77,16,79,60,DEEP);
    r(s,79,18,75,56,0x99c7b3); r(s,79,19,75,18,0xbcd2b5);
    r(s,139,24,8,8,CREAM); r(s,137,26,12,4,CREAM);
    r(s,79,49,75,25,0x80afa0); r(s,79,54,24,20,0x689c8e);
    r(s,99,58,55,16,0x527f7a); r(s,128,53,26,21,0x527f7a);
    r(s,79,64,75,10,0x86b3a0); r(s,91,66,15,2,0xbcd2b5); r(s,123,69,23,1,0xbcd2b5);
    r(s,114,17,4,60,0xf1d29c); r(s,78,44,77,3,0xf1d29c);
    r(s,67,79,101,6,INK); r(s,67,79,98,3,0xf1d29c);
    /* Curtains in two-pixel folds. */
    r(s,65,7,6,67,0xc59b83); r(s,67,9,4,41,0xe2baa0); r(s,69,9,2,36,0xf1d2b1);
    r(s,162,7,7,67,0xc59b83); r(s,162,9,3,41,0xe2baa0);
    r(s,64,48,8,4,GOLD); r(s,162,48,8,4,GOLD);
    /* Left bookcase; book spines and leaf on the wall. */
    r(s,9,36,47,57,INK); r(s,11,38,43,53,0xb98b68); r(s,14,41,36,18,0x72594d);
    r(s,14,64,36,22,0x72594d); r(s,9,59,47,4,0xe1b787); r(s,11,88,43,4,0xe1b787);
    const uint32_t books[]={0x819b82,0xca9873,0x8fa9ac,0xc2ad80,0xa97a75};
    for(int i=0;i<5;++i) { int y=43+(i%2)*3; r(s,16+i*6,y,5,58-y,books[i]); r(s,17+i*6,y+3,3,1,CREAM); }
    r(s,17,78,27,6,0x83a09a); r(s,19,75,26,3,CREAM); r(s,17,72,22,3,RUST);
    r(s,14,92,5,12,INK); r(s,46,92,5,12,INK);
    r(s,25,10,24,18,INK); r(s,27,12,20,14,CREAM); r(s,34,17,3,7,TEAL);
    r(s,29,15,7,5,GREEN); r(s,37,14,6,5,GREEN);
    /* Bed, stitched quilt and plant. */
    r(s,179,92,53,31,INK); r(s,181,94,49,20,0xc49572);
    r(s,182,91,44,8,CREAM); r(s,195,98,35,20,0x749b9a);
    r(s,197,98,31,3,0xb3cbb6); r(s,195,111,35,3,TEAL);
    for(int x=199;x<227;x+=8) { r(s,x,102,3,2,0xb3cbb6); r(s,x+1,107,3,2,0xb3cbb6); }
    r(s,179,121,5,8,INK); r(s,226,121,5,8,INK); r(s,185,98,8,7,0xf1d6a5);
    plant(s,197,39);
    /* Layered woven mat, centered beneath the pet. */
    r(s,61,125,113,13,0x957663); r(s,65,122,105,15,0xd3b18a);
    r(s,68,125,99,9,0xe4c497); r(s,75,128,85,3,0xd1ad84);
    for(int x=69;x<168;x+=5) r(s,x,136,2,3,0xebcba1);
    r(s,10,135,31,6,INK); r(s,12,133,27,6,0x769796); r(s,15,132,21,3,0xb3cbb6);
    r(s,216,137,9,3,0x99735b); r(s,217,133,7,6,GOLD);
}

/* A stepped, filled oval on a 2 px grid. Shape layers supply volume without
 * anti-aliasing, alpha buffers or storing a picture for every animation frame. */
static void oval(const pp_pixel_sink_t *s,int x,int y,int w,int h,uint32_t c)
{
    for(int j=0;j<h;j+=2) {
        int edge=(j<4 || j>=h-4)?8:(j<8 || j>=h-8)?4:0;
        r(s,x+edge,y+j,w-2*edge,2,c);
    }
}
void pp_avatar_draw(const pp_pixel_sink_t *s,int x,int y,const pp_avatar_frame_t *f)
{
    unsigned t=f->frame;
    bool sleep=f->pose==AVATAR_SLEEP && f->voice==AVATAR_IDLE;
    bool blink=(t%23==19) || sleep;
    bool speak=f->voice==AVATAR_SPEAKING,listen=f->voice==AVATAR_LISTENING;
    int bob=sleep?0:(int)((t/3)%2)*2;
    int sway=(f->pose==AVATAR_WALK)?((int)(t%12)-6):0;
    x+=sway; y-=bob;
    if (f->pose==AVATAR_GROW || f->pose==AVATAR_PLAY) y-=((t%8<4)?4:0);
    /* Feet and rear shadow. */
    r(s,x+12,y+72+bob,51,5,0x8f8064);
    r(s,x+15,y+64,15,8,INK); r(s,x+45,y+64,15,8,INK);
    r(s,x+17,y+64,12,5,TEAL); r(s,x+46,y+64,12,5,GREEN);
    /* Small leaf ears, recognizable in both forms. */
    int ear=listen?4:0;
    r(s,x+17,y+7-ear,6,22,INK); r(s,x+13,y+9-ear,12,10,INK);
    r(s,x+15,y+9-ear,6,8,GREEN); r(s,x+19,y+15-ear,4,10,TEAL);
    r(s,x+49,y+3-ear,8,26,INK); r(s,x+53,y+5-ear,10,12,INK);
    r(s,x+51,y+7-ear,10,9,GREEN); r(s,x+53,y+7-ear,6,3,LIGHT);
    r(s,x+51,y+17-ear,4,10,TEAL);
    if (f->evolved) {
        r(s,x+59,y+1-ear,6,10,INK); r(s,x+57,y+3-ear,6,6,GREEN);
        r(s,x+9,y+5-ear,6,10,INK); r(s,x+11,y+7-ear,8,5,LIGHT);
        star(s,x+33,y+6,CREAM);
    }
    /* Body: outline, shaded sides, highlight and cream belly. */
    oval(s,x+9,y+23,60,46,INK); oval(s,x+11,y+23,56,42,DEEP);
    oval(s,x+11,y+23,52,38,GREEN); oval(s,x+17,y+23,42,16,LIGHT);
    r(s,x+19,y+25,18,2,0xd7e7b3); r(s,x+17,y+29,6,2,0xd7e7b3);
    oval(s,x+23,y+47,34,18,0xcbd7a3); r(s,x+31,y+51,14,10,CREAM);
    r(s,x+15,y+49,4,8,TEAL); r(s,x+59,y+43,4,14,TEAL);
    /* Eyes shift slightly while looking / thinking, but retain face anchors. */
    int look=f->voice==AVATAR_THINKING?-2:((t/18)%3==1?2:0);
    if (blink) { r(s,x+23,y+39,8,2,INK); r(s,x+47,y+39,8,2,INK); }
    else {
        r(s,x+24+look,y+33,6,10,INK); r(s,x+48+look,y+33,6,10,INK);
        r(s,x+24+look,y+33,2,3,0xf9f0d2); r(s,x+48+look,y+33,2,3,0xf9f0d2);
        r(s,x+27+look,y+40,2,2,DEEP); r(s,x+51+look,y+40,2,2,DEEP);
    }
    r(s,x+19,y+43,8,3,0xd9a28c); r(s,x+51,y+43,8,3,0xd9a28c);
    if (speak || (f->pose==AVATAR_EAT && t%2)) {
        r(s,x+35,y+44,8,((t%3)+1)*2,INK); r(s,x+37,y+48,4,2,RUST);
    } else { r(s,x+35,y+45,3,2,INK); r(s,x+41,y+45,3,2,INK); r(s,x+38,y+47,3,2,INK); }
    /* Arms. */
    r(s,x+5,y+44,10,10,INK); r(s,x+7,y+44,8,7,GREEN);
    r(s,x+63,y+44,10,10,INK); r(s,x+63,y+44,8,7,TEAL);
    if (f->evolved) {
        r(s,x+17,y+51,45,6,RUST); r(s,x+19,y+51,41,2,GOLD);
        r(s,x+53,y+54,8,12,RUST); r(s,x+53,y+64,10,2,GOLD);
    }
    if (sleep) {
        r(s,x+8,y+53,63,15,DEEP); r(s,x+10,y+53,59,11,0x87aaa0);
        r(s,x+12,y+53,55,3,0xb5cab0); star(s,x+63,y+13-(int)(t%4),CREAM);
    } else if (f->pose==AVATAR_READ) {
        r(s,x+17,y+54,46,17,INK); r(s,x+19,y+54,19,13,CREAM);
        r(s,x+40,y+54,21,13,0xe5cda0); r(s,x+38,y+54,2,15,RUST);
        for(int j=0;j<3;++j) { r(s,x+23,y+57+j*3,11,1,0xb7a37b); r(s,x+44,y+57+j*3,11,1,0xb7a37b); }
        if (t%8<3) r(s,x+40,y+54,3,13,0xf7e8be);
    } else if (f->pose==AVATAR_HELP) {
        r(s,x+28,y+61,26,13,RUST); r(s,x+26,y+59,30,4,INK); r(s,x+29,y+59,24,2,GOLD);
        r(s,x+38,y+49,3,10,DEEP); r(s,x+32,y+46,9,6,GREEN); r(s,x+41,y+44,9,6,LIGHT);
    } else if (f->pose==AVATAR_EAT) {
        r(s,x+23,y+59,34,6,INK); r(s,x+25,y+59,30,3,CREAM);
        oval(s,x+27,y+49,26,12,GOLD); r(s,x+35,y+49,8,2,CREAM);
        r(s,x+29,y+53,4,2,RUST); r(s,x+45,y+54,3,2,RUST);
    } else if (f->pose==AVATAR_PLAY) {
        int by=y+58-((int)(t%6)-3)*2;
        oval(s,x-10,by,22,18,INK); oval(s,x-8,by+2,18,14,GOLD);
        r(s,x-2,by+2,4,14,CREAM); r(s,x-8,by+7,18,3,0xc18e67);
    }
    if (f->pose==AVATAR_GROW) {
        star(s,x-1,y+25-(int)(t%4)*2,GOLD); star(s,x+73,y+38-(int)(t%4)*2,CREAM);
        star(s,x+34,y-4,GOLD);
    }
    if (f->voice==AVATAR_THINKING) {
        for(unsigned i=0;i<3;++i) r(s,x+28+(int)i*8,y-1,4,4,i==t%3?CREAM:TEAL);
    } else if (listen) {
        r(s,x-3,y+22,2,12,CREAM); r(s,x-7,y+25,2,6,GOLD);
        r(s,x+78,y+22,2,12,CREAM); r(s,x+82,y+25,2,6,GOLD);
    } else if (f->voice==AVATAR_OFFLINE || f->voice==AVATAR_INTERRUPTED) {
        r(s,x+34,y+1,8,2,CREAM);
    }
}
