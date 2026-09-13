#include "pp_voice_turn.h"
#include <assert.h>
#include <stdio.h>

static void event(pp_voice_turn_t *t,pp_turn_event_t e,uint64_t ms)
{ pp_voice_turn_event(t,e,ms); }
static pp_voice_turn_t listening(uint64_t ms)
{
    pp_voice_turn_t t={0};
    event(&t,PP_TURN_BUTTON,ms);
    assert(t.phase==PP_TURN_START_PENDING && !pp_voice_turn_captures(&t));
    event(&t,PP_TURN_LISTEN_SENT,ms);
    assert(pp_voice_turn_captures(&t) && t.turns==1 && !t.sent_frames);
    event(&t,PP_TURN_TX_FRAME,ms);
    return t;
}
static void normal_rounds(void)
{
    pp_voice_turn_t t=listening(1000);
    for(unsigned turn=1;turn<=1000;++turn) {
        uint64_t ms=(uint64_t)turn*1000;
        event(&t,PP_TURN_STT,ms+10);
        event(&t,PP_TURN_STT,ms+20); /* No final flag: both may be partial. */
        assert(pp_voice_turn_captures(&t));
        event(&t,PP_TURN_TTS_START,ms+30);
        assert(t.phase==PP_TURN_WAIT_AUDIO && !pp_voice_turn_captures(&t));
        event(&t,PP_TURN_TTS_START,ms+31); /* Duplicate does not reset timeout. */
        assert(t.progress_at==ms+30);
        event(&t,PP_TURN_TX_FRAME,ms+32);
        assert(t.sent_frames==1); /* No upload permitted once TTS starts. */
        event(&t,PP_TURN_RX_AUDIO,ms+40);
        assert(t.phase==PP_TURN_SPEAKING && pp_voice_turn_plays(&t));
        event(&t,PP_TURN_TTS_STOP,ms+50);
        assert(t.phase==PP_TURN_RESUME_PENDING && !pp_voice_turn_plays(&t));
        assert(!pp_voice_turn_captures(&t));
        event(&t,PP_TURN_TTS_STOP,ms+51);
        event(&t,PP_TURN_LISTEN_SENT,ms+52); /* Cannot skip playback drain. */
        event(&t,PP_TURN_RX_AUDIO,ms+53); /* Late audio cannot revive playback. */
        assert(t.phase==PP_TURN_RESUME_PENDING);
        event(&t,PP_TURN_DRAINED,ms+60);
        assert(t.phase==PP_TURN_START_PENDING && !pp_voice_turn_captures(&t));
        event(&t,PP_TURN_TTS_STOP,ms+61);
        event(&t,PP_TURN_LISTEN_SENT,ms+70);
        assert(pp_voice_turn_captures(&t) && !t.saw_stt && !t.sent_frames);
        assert(t.turns==turn+1 && t.listen_at==ms+70);
        event(&t,PP_TURN_TTS_STOP,ms+71); /* Late duplicate stop while listening. */
        assert(pp_voice_turn_captures(&t));
        event(&t,PP_TURN_TX_FRAME,ms+72);
    }
}
static void order_and_timeouts(void)
{
    pp_voice_turn_t t={0};
    event(&t,PP_TURN_TTS_START,0);
    event(&t,PP_TURN_RX_AUDIO,0);
    event(&t,PP_TURN_TTS_STOP,0);
    assert(t.phase==PP_TURN_READY);
    event(&t,PP_TURN_BUTTON,0);
    event(&t,PP_TURN_TTS_START,0);
    assert(t.phase==PP_TURN_START_PENDING);
    event(&t,PP_TURN_LISTEN_SENT,100);
    event(&t,PP_TURN_TTS_START,101); /* No upload for this turn yet. */
    assert(pp_voice_turn_captures(&t));
    event(&t,PP_TURN_TX_FRAME,102);
    event(&t,PP_TURN_TTS_START,103); /* TTS is allowed before STT. */
    assert(t.phase==PP_TURN_WAIT_AUDIO);
    event(&t,PP_TURN_STT,104);
    assert(!pp_voice_turn_captures(&t));
    event(&t,PP_TURN_TICK,103+PP_TURN_RESPONSE_LIMIT_MS-1);
    assert(t.phase==PP_TURN_WAIT_AUDIO);
    event(&t,PP_TURN_TICK,103+PP_TURN_RESPONSE_LIMIT_MS);
    assert(t.phase==PP_TURN_FAILED && t.reason==PP_TURN_REASON_TIMEOUT);

    t=listening(200);
    event(&t,PP_TURN_TICK,200+PP_TURN_LISTEN_LIMIT_MS-1);
    assert(pp_voice_turn_captures(&t));
    event(&t,PP_TURN_TICK,200+PP_TURN_LISTEN_LIMIT_MS);
    assert(t.phase==PP_TURN_STOPPED && t.reason==PP_TURN_REASON_QUIET);
    event(&t,PP_TURN_LISTEN_SENT,40000);
    assert(!pp_voice_turn_captures(&t));

    t=listening(200);
    event(&t,PP_TURN_STT,300);
    event(&t,PP_TURN_TICK,200+PP_TURN_LISTEN_LIMIT_MS);
    assert(t.phase==PP_TURN_FAILED && t.reason==PP_TURN_REASON_TIMEOUT);

    t=listening(200);
    event(&t,PP_TURN_TTS_START,300);
    event(&t,PP_TURN_RX_AUDIO,50000);
    event(&t,PP_TURN_TICK,300+PP_TURN_RESPONSE_LIMIT_MS);
    assert(t.phase==PP_TURN_SPEAKING); /* Timeout tracks actual playback progress. */
    event(&t,PP_TURN_TICK,50000+PP_TURN_RESPONSE_LIMIT_MS);
    assert(t.phase==PP_TURN_FAILED);
}
static void terminal_events(void)
{
    const pp_turn_event_t events[]={PP_TURN_BUTTON,PP_TURN_REVOKE,PP_TURN_DISCONNECTED,PP_TURN_BAD_SESSION};
    for(unsigned phase=PP_TURN_START_PENDING;phase<=PP_TURN_RESUME_PENDING;++phase) {
        for(unsigned i=0;i<sizeof(events)/sizeof(events[0]);++i) {
            pp_voice_turn_t t=listening(100);
            t.phase=(pp_turn_phase_t)phase;
            assert(pp_voice_turn_active(&t));
            event(&t,events[i],200);
            assert(t.phase==(i<2?PP_TURN_STOPPED:PP_TURN_FAILED));
            assert(t.reason==(i==0?PP_TURN_REASON_USER:i==1?PP_TURN_REASON_REVOKED:
                i==2?PP_TURN_REASON_NETWORK:PP_TURN_REASON_SESSION));
            pp_turn_reason_t reason=t.reason;
            for(unsigned e=PP_TURN_BUTTON;e<=PP_TURN_BAD_SESSION;++e) event(&t,(pp_turn_event_t)e,300);
            assert(!pp_voice_turn_captures(&t) && !pp_voice_turn_plays(&t) && !pp_voice_turn_active(&t));
            assert(t.reason==reason); /* Late packets/cancel must retain first fault. */
        }
    }
    pp_voice_turn_t t=listening(0);
    t.sent_frames=UINT32_MAX;
    event(&t,PP_TURN_TX_FRAME,1);
    assert(t.sent_frames==UINT32_MAX);
}
int main(void)
{
    normal_rounds(); order_and_timeouts(); terminal_events();
    puts("Voice turn state: PASS (1000 rounds, partial STT, drain gate, cancel, timeouts, late events)");
    return 0;
}
