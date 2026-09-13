/* Pure host checks with pre-change pet pixel goldens, not a renderer clone. */
#include "pp_chat_motion.h"
#include "pp_pixel_buffer.h"
#ifdef NDEBUG
#error Chat motion tests require assertions
#endif
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

enum { PIXEL_BYTES=64+96*88/2 };
static uint8_t pixels[PIXEL_BYTES];
static uint64_t append_hash(uint64_t hash,const uint8_t *data,size_t size)
{
    for(size_t i=0;i<size;++i) { hash^=data[i]; hash*=UINT64_C(1099511628211); }
    return hash;
}
static uint64_t draw(const pp_avatar_frame_t *frame)
{
    pp_pixel_buffer_t b; pp_pixel_buffer_init(&b,pixels,96,88);
    pp_pixel_sink_t sink={pp_pixel_buffer_rect,&b}; pp_avatar_draw(&sink,10,8,frame);
    return append_hash(UINT64_C(14695981039346656037),pixels,sizeof(pixels));
}
static void pet_goldens(void)
{
    /* Captured from unmodified pp_pixel_art.c at c95dfa2553e0a5d8c033a60e939718a479f41c60.
     * 8 poses x 6 voices x 2 forms x 8 phases, including unsigned wrap. */
    static const uint64_t expected[]={
        UINT64_C(0x9027f4eaed41c085),UINT64_C(0x93f8356157827c05),
        UINT64_C(0x84a1864496ca3a55),UINT64_C(0x2f34f1e1b67170b5),
        UINT64_C(0xeaa22c417e86174d),UINT64_C(0xdd47f148e4c9cb8a),
        UINT64_C(0xecd240f03876b729),UINT64_C(0x37ae77955ff1cf7e)};
    const unsigned phases[]={0,1,2,3,19,23,71,UINT_MAX};
    for(unsigned p=AVATAR_LOOK;p<=AVATAR_GROW;++p) {
        uint64_t hash=UINT64_C(14695981039346656037);
        for(unsigned v=AVATAR_IDLE;v<=AVATAR_OFFLINE;++v)
        for(unsigned e=0;e<2;++e)
        for(unsigned i=0;i<sizeof(phases)/sizeof(phases[0]);++i) {
            pp_avatar_frame_t f={.pose=(pp_avatar_pose_t)p,.voice=(pp_avatar_state_t)v,
                .frame=phases[i],.evolved=e!=0};
            draw(&f); hash=append_hash(hash,pixels,sizeof(pixels));
        }
        assert(hash==expected[p]);
    }
}
static bool same_output(const pp_chat_motion_output_t *a,const pp_chat_motion_output_t *b)
{
    /* Outputs are explicitly zero-initialized, including padding. */
    return memcmp(a,b,sizeof(*a))==0;
}
static void levels_and_priority(void)
{
    pp_chat_motion_t m={0}; pp_chat_motion_output_t out;
    pp_chat_motion_step(&m,0,AVATAR_SPEAKING,32768,0,0,&out);
    assert(out.frame.chat.enabled && !out.frame.chat.mouth);
    assert(!out.bars[0] && !out.bars[1] && !out.bars[2]);
    const uint16_t levels[]={0,95,96,699,700,2199,2200,32768};
    const uint8_t mouths[]={0,0,1,1,2,2,3,3};
    uint64_t shapes[4]={0};
    for(unsigned i=0;i<sizeof(levels)/sizeof(levels[0]);++i) {
        pp_chat_motion_step(&m,0,AVATAR_SPEAKING,0,levels[i],0,&out);
        assert(out.frame.chat.mouth==mouths[i]);
        for(unsigned j=0;j<3;++j) assert(out.bars[j]==(j<mouths[i]?(j+1)*4:0));
        shapes[mouths[i]]=draw(&out.frame);
    }
    for(unsigned i=0;i<4;++i) for(unsigned j=i+1;j<4;++j) assert(shapes[i]!=shapes[j]);
    pp_chat_motion_step(&m,0,AVATAR_SPEAKING,0,32768,250,&out);
    assert(!out.frame.chat.mouth && !out.bars[0]);
    pp_chat_motion_step(&m,0,AVATAR_SPEAKING,0,32768,UINT16_MAX,&out);
    assert(!out.frame.chat.mouth && !out.bars[0]);
    for(unsigned v=AVATAR_IDLE;v<=AVATAR_OFFLINE;++v) if(v!=AVATAR_SPEAKING) {
        pp_chat_motion_step(&m,0,(pp_avatar_state_t)v,32768,32768,0,&out);
        assert(!out.frame.chat.mouth);
        if(v==AVATAR_LISTENING) assert(out.bars[2]==12 && out.frame.chat.ear>0);
        else assert(!out.bars[0] && !out.bars[1] && !out.bars[2]);
    }
    pp_chat_motion_step(&m,0,(pp_avatar_state_t)UINT_MAX,32768,32768,0,&out);
    assert(out.frame.voice==AVATAR_OFFLINE && !out.frame.chat.mouth);
    /* A previously loud reply cannot leak into the next silent reply. */
    pp_chat_motion_step(&m,0,AVATAR_SPEAKING,0,32768,0,&out); assert(out.frame.chat.mouth==3);
    pp_chat_motion_step(&m,200,AVATAR_IDLE,0,32768,0,&out); assert(!out.frame.chat.mouth);
    pp_chat_motion_step(&m,400,AVATAR_SPEAKING,0,0,0,&out); assert(!out.frame.chat.mouth);
    /* Network gaps age out without another PCM update. Release is monotonic. */
    uint8_t previous=3;
    for(unsigned age=0;age<=300;++age) {
        pp_chat_motion_step(&m,400,AVATAR_SPEAKING,0,2400,(uint16_t)age,&out);
        assert(out.frame.chat.mouth<=previous); previous=out.frame.chat.mouth;
        if(age>=250) assert(!out.frame.chat.mouth);
    }
}
static void independent_motion(void)
{
    pp_chat_motion_t m; pp_chat_motion_output_t out,last;
    pp_chat_motion_reset(&m,0);
    uint64_t forms[6];
    for(unsigned v=AVATAR_IDLE;v<=AVATAR_OFFLINE;++v) {
        pp_chat_motion_reset(&m,0);
        pp_chat_motion_step(&m,0,(pp_avatar_state_t)v,1200,2400,0,&out);
        forms[v]=draw(&out.frame);
    }
    assert(forms[AVATAR_IDLE]!=forms[AVATAR_LISTENING]);
    assert(forms[AVATAR_IDLE]!=forms[AVATAR_THINKING]);
    assert(forms[AVATAR_IDLE]!=forms[AVATAR_SPEAKING]);
    assert(forms[AVATAR_IDLE]!=forms[AVATAR_OFFLINE]);
    pp_chat_motion_reset(&m,0);
    uint32_t last_blink=0,interval=0; unsigned blinks=0; bool different_intervals=false;
    bool was_closed=false,saw_glance=false,saw_ear=false; unsigned breathe_mask=0;
    for(uint32_t t=0;t<=60000;t+=200) {
        pp_chat_motion_step(&m,t,AVATAR_IDLE,0,0,UINT16_MAX,&out);
        assert(out.frame.frame==0 && !out.frame.evolved && out.frame.pose==AVATAR_LOOK);
        assert(!out.frame.chat.mouth && !out.bars[0]);
        breathe_mask|=1u<<out.frame.chat.breathe;
        saw_glance|=out.frame.chat.gaze!=0; saw_ear|=out.frame.chat.ear!=0;
        if(out.frame.chat.blink && !was_closed) {
            if(blinks>1 && t-last_blink!=interval) different_intervals=true;
            interval=t-last_blink; last_blink=t; ++blinks;
        }
        was_closed=out.frame.chat.blink;
        last=out; pp_chat_motion_step(&m,t,AVATAR_IDLE,0,0,UINT16_MAX,&out);
        assert(same_output(&last,&out));
    }
    assert(blinks>=8 && different_intervals && saw_glance && saw_ear && breathe_mask==7);
    pp_chat_motion_reset(&m,0);
    unsigned dot_mask=0;
    for(uint32_t t=0;t<2400;t+=200) {
        pp_chat_motion_step(&m,t,AVATAR_THINKING,0,0,0,&out);
        dot_mask|=1u<<out.dot_phase;
    }
    assert(dot_mask==7);
    /* Breathing changes the body but leaves the last feet rows and shadow
     * byte-identical, and never relies on a translated image/canvas. */
    pp_avatar_frame_t f={.voice=AVATAR_IDLE,.chat={.enabled=true}};
    uint64_t body[3]; uint8_t anchored[(88-78)*48];
    for(unsigned breath=0;breath<3;++breath) {
        f.chat.breathe=(uint8_t)breath; body[breath]=draw(&f);
        const uint8_t *floor=pixels+64+78*48;
        if(!breath) memcpy(anchored,floor,sizeof(anchored));
        else assert(!memcmp(anchored,floor,sizeof(anchored)));
    }
    assert(body[0]!=body[1] && body[1]!=body[2] && body[0]!=body[2]);
}
static void wrap_reset_and_stress(void)
{
    pp_chat_motion_t a,b; pp_chat_motion_output_t x,y;
    pp_chat_motion_reset(NULL,0); pp_chat_motion_step(NULL,0,AVATAR_IDLE,0,0,0,&x);
    pp_chat_motion_reset(&a,0); pp_chat_motion_step(&a,0,AVATAR_IDLE,0,0,0,NULL);
    pp_chat_motion_reset(&a,0); pp_chat_motion_reset(&b,UINT32_MAX-1000);
    for(uint32_t t=0;t<12000;t+=200) {
        pp_chat_motion_step(&a,t,AVATAR_IDLE,0,0,0,&x);
        pp_chat_motion_step(&b,(UINT32_MAX-1000)+t,AVATAR_IDLE,0,0,0,&y);
        assert(same_output(&x,&y));
    }
    pp_chat_motion_reset(&a,900000); pp_chat_motion_reset(&b,0);
    pp_chat_motion_step(&a,900000,AVATAR_IDLE,0,0,0,&x);
    pp_chat_motion_step(&b,0,AVATAR_IDLE,0,0,0,&y); assert(same_output(&x,&y));
    pp_chat_motion_step(&b,900000,AVATAR_IDLE,0,0,0,&y); assert(same_output(&x,&y));
    /* Long visible run through uint32 wrap with random state/audio events.
     * Every event is bounded; interval gaps must not trigger catch-up loops. */
    uint32_t random=31,t=UINT32_MAX-60000;
    uint8_t guarded[PIXEL_BYTES+32]; memset(guarded,0xa5,sizeof(guarded));
    for(unsigned i=0;i<100000;++i) {
        random=random*1664525u+1013904223u;
        t+=200; if(i%997==0) t+=60000;
        pp_avatar_state_t state=(pp_avatar_state_t)(random%6);
        uint16_t age=(uint16_t)((random>>16)%500);
        pp_chat_motion_step(&a,t,state,(uint16_t)random,(uint16_t)(random>>1),age,&x);
        assert(x.frame.chat.enabled && x.frame.chat.breathe<=2 && x.frame.chat.mouth<=3);
        assert(x.frame.chat.ear<=2 && x.frame.chat.gaze>=-2 && x.frame.chat.gaze<=2);
        assert(x.frame.chat.tilt>=-1 && x.frame.chat.tilt<=1 && x.dot_phase<3);
        for(unsigned j=0;j<3;++j) assert(x.bars[j]<=12);
        if(state!=AVATAR_SPEAKING || age>=250) assert(!x.frame.chat.mouth);
        if(i%97==0) {
            pp_pixel_buffer_t buffer; pp_pixel_buffer_init(&buffer,guarded+16,96,88);
            pp_pixel_sink_t sink={pp_pixel_buffer_rect,&buffer};
            pp_avatar_draw(&sink,(int)(random%80)-40,(int)((random>>8)%80)-40,&x.frame);
            for(unsigned j=0;j<16;++j) assert(guarded[j]==0xa5 && guarded[PIXEL_BYTES+16+j]==0xa5);
        }
    }
    /* Pixel rendering clamps invalid chat offsets and suppresses an injected
     * mouth value outside SPEAKING without ever enabling a pet prop/form. */
    x.frame=(pp_avatar_frame_t){.pose=AVATAR_GROW,.voice=AVATAR_IDLE,.evolved=true,
        .frame=UINT_MAX,.chat={.enabled=true,.breathe=255,.ear=255,.mouth=255,.dots=255,.gaze=127,.tilt=-128}};
    uint64_t closed=draw(&x.frame); x.frame.chat.mouth=0; assert(draw(&x.frame)==closed);
}
int main(void)
{
    pet_goldens(); levels_and_priority(); independent_motion(); wrap_reset_and_stress();
    printf("Chat motion: PASS (768 pet goldens, sound/silence, fixed feet, irregular blink, wrap/reset, 100000 events; state=%zu bytes)\n",
        sizeof(pp_chat_motion_t));
    return 0;
}
