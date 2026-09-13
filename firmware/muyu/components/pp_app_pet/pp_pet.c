#include "pp_pet.h"
#include <string.h>

static const uint16_t levels[] = {0, 6, 16, 32, 56};
static unsigned cap(unsigned n, unsigned limit) { return n < limit ? n : limit; }
static void put32(uint8_t *p, uint32_t v) { for (unsigned i=0;i<4;++i) p[i]=(uint8_t)(v>>(8*i)); }
static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static uint32_t crc32(const uint8_t *p, size_t n)
{
    uint32_t crc=UINT32_MAX;
    while (n--) { crc^=*p++; for (unsigned i=0;i<8;++i) crc=(crc>>1)^((crc&1)?0xedb88320u:0); }
    return ~crc;
}
void pp_pet_defaults(pp_pet_save_t *s, uint32_t id)
{
    *s=(pp_pet_save_t){.id=id?id:1,.coins=10,.fullness=50,.energy=70};
}
unsigned pp_pet_level(const pp_pet_save_t *s)
{
    unsigned l=1; while (l<5 && s->xp>=levels[l]) ++l; return l;
}
unsigned pp_pet_progress(const pp_pet_save_t *s)
{
    unsigned l=pp_pet_level(s);
    return l==5 ? 100 : (s->xp-levels[l-1])*100/(levels[l]-levels[l-1]);
}
void pp_pet_encode(const pp_pet_save_t *s, uint8_t b[PP_PET_SAVE_SIZE])
{
    memset(b,0,PP_PET_SAVE_SIZE); memcpy(b,"PET1",4); b[4]=1; b[5]=PP_PET_SAVE_SIZE;
    put32(b+8,s->id); put32(b+12,s->sequence); put32(b+16,s->cycles); put32(b+20,s->cycle_ms);
    b[24]=(uint8_t)s->coins; b[25]=(uint8_t)(s->coins>>8); b[26]=(uint8_t)s->xp;
    b[27]=s->fullness; b[28]=s->energy; b[29]=s->resting;
    put32(b+44,crc32(b,44));
}
bool pp_pet_decode(pp_pet_save_t *s, const uint8_t *b, size_t n)
{
    if (!b || n!=PP_PET_SAVE_SIZE || memcmp(b,"PET1",4) || b[4]!=1 || b[5]!=n ||
        b[6] || b[7] || crc32(b,44)!=get32(b+44)) return false;
    for (unsigned i=30;i<44;++i) if (b[i]) return false;
    pp_pet_save_t v={.id=get32(b+8),.sequence=get32(b+12),.cycles=get32(b+16),
        .cycle_ms=get32(b+20),.coins=(uint16_t)(b[24]|(uint16_t)b[25]<<8),
        .xp=b[26],.fullness=b[27],.energy=b[28],.resting=b[29]!=0};
    if (!v.id || v.coins>999 || v.xp>PP_PET_MAX_XP || v.fullness>100 || v.energy>100 ||
        v.cycle_ms>=PP_PET_CYCLE_MS || b[29]>1) return false;
    *s=v; return true;
}
int pp_pet_choose_save(pp_pet_save_t *s, const uint8_t *a, size_t na, const uint8_t *b, size_t nb)
{
    pp_pet_save_t va,vb; bool oka=pp_pet_decode(&va,a,na),okb=pp_pet_decode(&vb,b,nb);
    if (!oka && !okb) return -1;
    bool useb=okb && (!oka || (vb.sequence!=va.sequence && (uint32_t)(vb.sequence-va.sequence)<0x80000000u));
    *s=useb?vb:va; return useb?1:0;
}
static void recommend(pp_pet_t *p)
{
    if (!p->selection_locked)
        p->choice=p->save.fullness<=60?PET_FEED:p->save.energy<=35?PET_SLEEP:PET_TOGETHER;
}
static void notice(pp_pet_t *p, pp_pet_notice_t value)
{
    p->notice=value; p->notice_ms=3000;
}
static bool commit(pp_pet_t *p, pp_pet_save_t *next)
{
    next->sequence=p->save.sequence+1;
    uint8_t bytes[PP_PET_SAVE_SIZE]; pp_pet_encode(next,bytes);
    if (!p->write || !p->write(p->write_ctx,bytes)) {
        p->storage_error=true; notice(p,PET_STORAGE_ERROR); return false;
    }
    p->save=*next; p->storage_error=false; p->dirty=false; return true;
}
void pp_pet_init(pp_pet_t *p, const pp_pet_save_t *s, pp_pet_write_fn write, void *ctx)
{
    memset(p,0,sizeof(*p)); p->save=*s; p->write=write; p->write_ctx=ctx;
    /* A saved nap resumes; switching applications never gives free energy. */
    if (s->resting) p->action=PET_REST;
    recommend(p);
}
bool pp_pet_checkpoint(pp_pet_t *p)
{
    if (!p->dirty && !p->storage_error) return true;
    pp_pet_save_t next=p->save;
    return commit(p,&next);
}
static void finish(pp_pet_t *p)
{
    p->action=PET_LIVE; p->action_ms=0; p->rest_ms=0;
    p->selection_locked=false; recommend(p);
}
static void growth(pp_pet_t *p, unsigned before, pp_pet_notice_t usual)
{
    unsigned after=pp_pet_level(&p->save);
    if (after>before) { p->action=PET_GROW; p->action_ms=0; notice(p,before<3 && after>=3?PET_EVOLVED:PET_LEVEL); }
    else notice(p,usual);
}
void pp_pet_select(pp_pet_t *p, int direction)
{
    if (p->action!=PET_LIVE || p->storage_error) return;
    p->choice=(pp_pet_choice_t)(((int)p->choice+(direction<0?2:1))%3); p->selection_locked=true;
}
void pp_pet_press(pp_pet_t *p)
{
    if (p->storage_error) {
        if (pp_pet_checkpoint(p)) notice(p,PET_QUIET);
        return; /* Retry does not also spend coins. */
    }
    if (p->action!=PET_LIVE) {
        if (p->action==PET_REST) {
            pp_pet_save_t next=p->save; next.resting=false;
            if (!commit(p,&next)) return;
        }
        finish(p); return; /* Skip/wake never executes a second action. */
    }
    if (p->choice==PET_TOGETHER) { p->action=PET_PLAY; p->action_ms=0; return; }
    pp_pet_save_t next=p->save;
    if (p->choice==PET_SLEEP) {
        next.resting=true;
        if (commit(p,&next)) { p->action=PET_REST; p->action_ms=0; p->rest_ms=0; }
        return;
    }
    if (next.fullness>60) { notice(p,PET_FULL); return; }
    unsigned before=pp_pet_level(&next); bool paid=next.coins>=5;
    if (paid) { next.coins-=5; next.xp=(uint16_t)cap(next.xp+2,PP_PET_MAX_XP); }
    next.fullness=(uint8_t)cap(next.fullness+35,100);
    if (!commit(p,&next)) return;
    p->action=PET_EAT; p->action_ms=0;
    growth(p,before,paid?PET_FED:PET_FREE_FOOD);
}
void pp_pet_tick(pp_pet_t *p, uint32_t ms)
{
    if (!ms || p->storage_error) return;
    /* Bound catch-up after a stalled control task; no offline reward burst. */
    ms=cap(ms,1000); p->clock_ms+=ms;
    p->notice_ms=p->notice_ms>ms?p->notice_ms-ms:0;
    if (!p->notice_ms) p->notice=PET_QUIET;
    if (p->action==PET_REST) {
        p->action_ms+=ms; p->rest_ms+=ms;
        while (p->rest_ms>=500) { p->rest_ms-=500; p->save.energy=(uint8_t)cap(p->save.energy+1,100); p->dirty=true; }
        if (p->action_ms>=30000 || p->save.energy==100) {
            pp_pet_save_t next=p->save; next.resting=false;
            if (commit(p,&next)) { finish(p); notice(p,PET_RESTED); }
        }
        return;
    }
    if (p->action!=PET_LIVE) {
        p->action_ms+=ms;
        unsigned duration=p->action==PET_EAT?4000:p->action==PET_PLAY?10000:5000;
        if (p->action_ms>=duration) finish(p);
        return;
    }
    if (p->save.energy<=25) {
        pp_pet_save_t next=p->save; next.resting=true;
        if (commit(p,&next)) { p->action=PET_REST; p->action_ms=0; p->rest_ms=0; }
        return;
    }
    if (p->save.fullness<=15) { recommend(p); return; }
    uint32_t progress=p->save.cycle_ms+ms;
    if (progress<PP_PET_CYCLE_MS) { p->save.cycle_ms=progress; p->dirty=true; return; }
    pp_pet_save_t next=p->save; unsigned before=pp_pet_level(&next);
    next.cycle_ms=progress-PP_PET_CYCLE_MS; ++next.cycles;
    next.coins=(uint16_t)cap(next.coins+(before>=3?5:4),999);
    next.xp=(uint16_t)cap(next.xp+3,PP_PET_MAX_XP);
    next.fullness=(uint8_t)(next.fullness-8); next.energy=(uint8_t)(next.energy-6);
    if (commit(p,&next)) { growth(p,before,PET_REWARD); recommend(p); }
}
