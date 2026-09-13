#include "pp_pet.h"
#include "pp_avatar.h"
#include "pp_pixel_buffer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t saved[PP_PET_SAVE_SIZE];
static bool fail;
static unsigned writes;
static bool write_save(void *ctx,const uint8_t *bytes)
{
    (void)ctx; ++writes;
    if(fail) return false;
    memcpy(saved,bytes,sizeof(saved)); return true;
}
static void fresh(pp_pet_t *p)
{
    pp_pet_save_t s; pp_pet_defaults(&s,123); pp_pet_init(p,&s,write_save,NULL); fail=false; writes=0;
}
static void advance(pp_pet_t *p,unsigned ms) { while(ms) { unsigned step=ms>100?100:ms; pp_pet_tick(p,step); ms-=step; } }
static void test_economy(void)
{
    pp_pet_t p; fresh(&p); assert(p.choice==PET_FEED);
    pp_pet_press(&p); assert(p.save.coins==5 && p.save.xp==2 && p.save.fullness==85 && writes==1);
    pp_pet_press(&p); assert(p.action==PET_LIVE && p.save.coins==5); /* skip != spend */
    p.choice=PET_FEED; pp_pet_press(&p); assert(p.notice==PET_FULL && writes==1);
    advance(&p,60000); assert(p.save.coins==9 && p.save.xp==5 && p.save.cycles==1 && writes==2);
    p.save.coins=0; p.save.fullness=15; p.choice=PET_FEED;
    pp_pet_press(&p); assert(p.notice==PET_FREE_FOOD && p.save.coins==0 && p.save.xp==5);
    pp_pet_press(&p); advance(&p,60000); assert(p.save.xp==8 && pp_pet_level(&p.save)==2 && p.action==PET_GROW);
    p.save.xp=15; p.save.fullness=30; p.choice=PET_FEED; pp_pet_press(&p); /* skip growth */
    p.choice=PET_FEED; p.save.coins=5; pp_pet_press(&p);
    assert(pp_pet_level(&p.save)==3 && p.notice==PET_EVOLVED);
    pp_pet_press(&p); advance(&p,60000); assert(p.save.coins==5);
    p.save.xp=55; p.save.coins=998; p.save.fullness=90; p.save.energy=90;
    advance(&p,60000); assert(p.save.coins==999 && p.save.xp==56 && pp_pet_progress(&p.save)==100);
}
static void test_time_and_input(void)
{
    pp_pet_t p; fresh(&p); advance(&p,29000); uint32_t progress=p.save.cycle_ms;
    p.choice=PET_TOGETHER; pp_pet_press(&p); advance(&p,10000);
    assert(p.action==PET_LIVE && p.save.cycle_ms==progress && writes==0);
    pp_pet_select(&p,1); pp_pet_choice_t chosen=p.choice;
    advance(&p,31000); assert(p.choice==chosen); /* reward cannot steal selection */
    advance(&p,5000); p.choice=PET_SLEEP; pp_pet_press(&p); unsigned energy=p.save.energy;
    advance(&p,5000); assert(p.save.energy==energy+10 && p.save.resting);
    pp_pet_press(&p); assert(!p.save.resting && p.action==PET_LIVE);
    p.save.energy=24; advance(&p,100); assert(p.action==PET_REST);
    advance(&p,30000); assert(p.action==PET_LIVE && p.save.energy==84);
    p.save.fullness=15; uint16_t coins=p.save.coins; progress=p.save.cycle_ms;
    advance(&p,300000); assert(p.save.coins==coins && p.save.cycle_ms==progress);
    fresh(&p); pp_pet_tick(&p,UINT32_MAX); assert(p.save.cycle_ms==1000 && writes==0);
    pp_pet_save_t unchanged=p.save; pp_pet_tick(&p,0); assert(!memcmp(&unchanged,&p.save,sizeof(unchanged)));
}
static void test_storage(void)
{
    pp_pet_t p; fresh(&p); fail=true; pp_pet_press(&p);
    assert(p.storage_error && p.save.coins==10 && p.save.fullness==50);
    unsigned tries=writes; advance(&p,90000); assert(writes==tries); /* no failed-write storm */
    fail=false; pp_pet_press(&p); assert(!p.storage_error && p.save.coins==10);
    pp_pet_press(&p); assert(p.save.coins==5);
    pp_pet_save_t s; assert(pp_pet_decode(&s,saved,sizeof(saved)) && s.coins==5 && s.fullness==85);
    pp_pet_init(&p,&s,write_save,NULL); advance(&p,0); assert(p.save.coins==5);
    uint8_t a[PP_PET_SAVE_SIZE],b[PP_PET_SAVE_SIZE]; pp_pet_encode(&s,a); ++s.sequence; s.coins=7; pp_pet_encode(&s,b);
    assert(pp_pet_choose_save(&s,a,sizeof(a),b,sizeof(b))==1 && s.coins==7);
    for(unsigned i=0;i<sizeof(b);++i) for(unsigned bit=0;bit<8;++bit) {
        b[i]^=(uint8_t)(1u<<bit); assert(!pp_pet_decode(&s,b,sizeof(b)));
        assert(pp_pet_choose_save(&s,a,sizeof(a),b,sizeof(b))==0); b[i]^=(uint8_t)(1u<<bit);
    }
    for(size_t n=0;n<sizeof(a);++n) assert(!pp_pet_decode(&s,a,n));
    assert(pp_pet_choose_save(&s,NULL,0,NULL,0)==-1);
    s.sequence=UINT32_MAX; pp_pet_encode(&s,a); s.sequence=0; pp_pet_encode(&s,b);
    assert(pp_pet_choose_save(&s,a,sizeof(a),b,sizeof(b))==1);
    fresh(&p); advance(&p,59900); fail=true; advance(&p,100);
    assert(p.save.coins==10 && p.save.cycles==0 && p.save.cycle_ms==59900);
    fail=false; pp_pet_press(&p); advance(&p,100);
    assert(p.save.coins==14 && p.save.cycles==1);
    assert(pp_pet_decode(&s,saved,sizeof(saved))); pp_pet_init(&p,&s,write_save,NULL);
    advance(&p,100); assert(p.save.cycles==1); /* no replay after reboot */
    p.choice=PET_SLEEP; pp_pet_press(&p); advance(&p,2000); pp_pet_checkpoint(&p);
    assert(pp_pet_decode(&s,saved,sizeof(saved))); unsigned energy=s.energy;
    pp_pet_init(&p,&s,write_save,NULL); assert(p.action==PET_REST && p.save.energy==energy);
    advance(&p,1000); assert(p.save.energy==energy+2); /* switching gives no free rest */
}
static void test_avatar(void)
{
    pp_avatar_t a={0}; assert(!pp_avatar_event(&a,1,1,AVATAR_SPEAKING));
    pp_avatar_begin(&a,7,3); assert(pp_avatar_event(&a,7,3,AVATAR_LISTENING));
    assert(!pp_avatar_event(&a,6,3,AVATAR_SPEAKING)); assert(!pp_avatar_event(&a,7,2,AVATAR_SPEAKING));
    assert(pp_avatar_event(&a,7,3,AVATAR_THINKING)); assert(pp_avatar_event(&a,7,3,AVATAR_SPEAKING));
    assert(pp_avatar_event(&a,7,3,AVATAR_INTERRUPTED)); assert(!pp_avatar_event(&a,7,3,AVATAR_SPEAKING));
    pp_avatar_begin(&a,7,4); assert(pp_avatar_event(&a,7,4,AVATAR_LISTENING)); pp_avatar_end(&a);
    assert(!pp_avatar_event(&a,7,4,AVATAR_SPEAKING));
    /* Deliberately clipped poses must never write outside the I4 surface. */
    uint8_t data[64+96*88/2+32]; memset(data,0xaa,sizeof(data));
    pp_pixel_buffer_t b; pp_pixel_buffer_init(&b,data+16,96,88); pp_pixel_sink_t sink={pp_pixel_buffer_rect,&b};
    for(unsigned p=0;p<=AVATAR_GROW;++p) for(unsigned f=0;f<100;++f) {
        pp_avatar_frame_t frame={.pose=(pp_avatar_pose_t)p,.voice=(pp_avatar_state_t)(f%6),.frame=f,.evolved=f%2};
        pp_avatar_draw(&sink,(int)(f%50)-25,(int)(f%50)-25,&frame);
    }
    for(unsigned i=0;i<16;++i) assert(data[i]==0xaa && data[sizeof(data)-1-i]==0xaa);
}
static void test_random_sessions(void)
{
    pp_pet_t p; fresh(&p); uint32_t random=19;
    for(unsigned i=0;i<100000;++i) {
        random=random*1664525u+1013904223u;
        switch(random%9) {
        case 0:pp_pet_press(&p);break;
        case 1:pp_pet_select(&p,-1);break;
        case 2:pp_pet_select(&p,1);break;
        case 3: {
            assert(pp_pet_checkpoint(&p));
            uint8_t bytes[PP_PET_SAVE_SIZE]; pp_pet_encode(&p.save,bytes); pp_pet_save_t s;
            assert(pp_pet_decode(&s,bytes,sizeof(bytes))); pp_pet_init(&p,&s,write_save,NULL); break;
        }
        default:pp_pet_tick(&p,(random>>8)%1500);break;
        }
        assert(p.save.coins<=999 && p.save.xp<=56 && p.save.fullness<=100 && p.save.energy<=100);
        assert(p.save.cycle_ms<60000 && p.choice<=PET_SLEEP);
    }
}
int main(void)
{
    test_economy(); test_time_and_input(); test_storage(); test_avatar(); test_random_sessions();
    puts("Pet logic: PASS (economy, 384 corruptions, recovery, failure injection, 100000 mixed events, avatar isolation)");
}
