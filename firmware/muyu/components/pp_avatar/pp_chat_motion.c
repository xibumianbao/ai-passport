#include "pp_chat_motion.h"
#include <string.h>

_Static_assert(sizeof(pp_chat_motion_t)<=128, "Chat animation must remain small");

static uint32_t random_next(pp_chat_motion_t *m)
{
    uint32_t x=m->random;
    x^=x<<13; x^=x>>17; x^=x<<5;
    return m->random=x;
}
void pp_chat_motion_reset(pp_chat_motion_t *m,uint32_t now_ms)
{
    if(!m) return;
    *m=(pp_chat_motion_t){.last_ms=now_ms,.random=UINT32_C(0x61b74f35),
        .blink_due_ms=2800,.gaze_due_ms=1600,.voice=AVATAR_IDLE,.initialized=true};
}
static uint8_t sound_band(uint16_t level)
{
    /* A compact activity indicator, not a calibrated sound-pressure meter. */
    return level<96?0:level<700?1:level<2200?2:3;
}
static void advance(pp_chat_motion_t *m,uint32_t elapsed)
{
    m->phase_ms=(m->phase_ms+elapsed)%60000;
    if(m->blink_left_ms) {
        if(elapsed>=m->blink_left_ms) {
            m->blink_left_ms=0;
            m->blink_due_ms=2600+random_next(m)%3400;
        } else m->blink_left_ms-=elapsed;
    } else if(elapsed>=m->blink_due_ms) {
        m->blink_left_ms=200;
        m->blink_due_ms=0;
    } else m->blink_due_ms-=elapsed;
    if(elapsed>=m->gaze_due_ms) {
        static const int8_t directions[]={0,-2,0,1,0,2};
        m->gaze=directions[random_next(m)%6];
        m->gaze_due_ms=1800+random_next(m)%2600;
    } else m->gaze_due_ms-=elapsed;
}
void pp_chat_motion_step(pp_chat_motion_t *m,uint32_t now_ms,
                         pp_avatar_state_t voice,uint16_t mic_level,
                         uint16_t playback_level,uint16_t playback_age_ms,
                         pp_chat_motion_output_t *out)
{
    if(!m || !out) return;
    if(!m->initialized || (uint32_t)(now_ms-m->last_ms)>2000) pp_chat_motion_reset(m,now_ms);
    uint32_t elapsed=now_ms-m->last_ms;
    m->last_ms=now_ms;
    advance(m,elapsed);
    if((unsigned)voice>AVATAR_OFFLINE) voice=AVATAR_OFFLINE;
    if(voice!=m->voice) {
        /* A new intent immediately cancels an old glance / lean. Blinking is
         * independent so frequent turns cannot keep the eyes open forever. */
        m->voice=voice; m->gaze=0; m->gaze_due_ms=1600;
    }
    memset(out,0,sizeof(*out));
    out->frame.pose=AVATAR_LOOK;
    out->frame.voice=voice;
    /* The absolute clock is deliberately absent from the frame/cache key. */
    out->frame.chat.enabled=true;
    pp_avatar_chat_frame_t *f=&out->frame.chat;
    static const uint8_t breath[]={0,1,2,1};
    f->breathe=breath[(m->phase_ms/1000)%4];
    f->blink=m->blink_left_ms!=0;
    f->gaze=m->gaze;
    f->tilt=m->gaze<0?-1:m->gaze>0?1:0;
    uint8_t band=0;
    switch(voice) {
    case AVATAR_LISTENING:
        band=sound_band(mic_level);
        f->gaze=0;
        f->ear=band?1:0;
        f->tilt=(m->phase_ms%5000>=3600 && m->phase_ms%5000<4200)?1:0;
        break;
    case AVATAR_THINKING:
        f->gaze=-2;
        f->tilt=(m->phase_ms/1600)%2?-1:0;
        f->dots=(uint8_t)((m->phase_ms/400)%3);
        break;
    case AVATAR_SPEAKING:
        /* Never animate fake speech through silence or missing PCM. A short
         * age-based release bridges DMA scheduling but ends at the cutoff. */
        if(playback_age_ms<250) {
            if(playback_age_ms>100)
                playback_level=(uint16_t)((uint32_t)playback_level*(250-playback_age_ms)/150);
            band=sound_band(playback_level);
        }
        f->mouth=band;
        f->gaze=0;
        f->ear=band==3?1:0;
        f->tilt=band && m->phase_ms%2400<400?1:0;
        break;
    case AVATAR_OFFLINE: case AVATAR_INTERRUPTED:
        f->breathe=0; f->gaze=0; f->tilt=0;
        break;
    default:
        f->ear=m->phase_ms%9000>=6400 && m->phase_ms%9000<6800?1:0;
        break;
    }
    for(unsigned i=0;i<3;++i) if(i<band) out->bars[i]=(uint8_t)((i+1)*4);
    out->dot_phase=f->dots;
}
