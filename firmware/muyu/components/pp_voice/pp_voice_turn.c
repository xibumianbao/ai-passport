#include "pp_voice_turn.h"

bool pp_voice_turn_captures(const pp_voice_turn_t *t) { return t->phase==PP_TURN_LISTENING; }
bool pp_voice_turn_plays(const pp_voice_turn_t *t)
{ return t->phase==PP_TURN_WAIT_AUDIO || t->phase==PP_TURN_SPEAKING; }
bool pp_voice_turn_active(const pp_voice_turn_t *t)
{ return t->phase>=PP_TURN_START_PENDING && t->phase<=PP_TURN_RESUME_PENDING; }
static void stop(pp_voice_turn_t *t,pp_turn_phase_t phase,pp_turn_reason_t reason)
{ t->phase=phase; t->reason=reason; t->sent_frames=0; }
void pp_voice_turn_event(pp_voice_turn_t *t,pp_turn_event_t event,uint64_t now)
{
    /* A stop/error is terminal for this transport. Only a new explicit user
     * connection creates another turn state; late cloud events cannot reopen it. */
    if(t->phase==PP_TURN_STOPPED || t->phase==PP_TURN_FAILED) return;
    if(event==PP_TURN_REVOKE) { stop(t,PP_TURN_STOPPED,PP_TURN_REASON_REVOKED); return; }
    if(event==PP_TURN_DISCONNECTED) { stop(t,PP_TURN_FAILED,PP_TURN_REASON_NETWORK); return; }
    if(event==PP_TURN_BAD_SESSION) { stop(t,PP_TURN_FAILED,PP_TURN_REASON_SESSION); return; }
    if(event==PP_TURN_BUTTON) {
        if(t->phase==PP_TURN_READY) t->phase=PP_TURN_START_PENDING;
        else stop(t,PP_TURN_STOPPED,PP_TURN_REASON_USER);
        return;
    }
    if(event==PP_TURN_LISTEN_SENT && t->phase==PP_TURN_START_PENDING) {
        t->phase=PP_TURN_LISTENING; t->listen_at=now; t->progress_at=now;
        t->sent_frames=0; t->saw_stt=false; ++t->turns;
    } else if(event==PP_TURN_TX_FRAME && t->phase==PP_TURN_LISTENING) {
        if(t->sent_frames<UINT32_MAX) ++t->sent_frames;
    } else if(event==PP_TURN_STT && t->phase==PP_TURN_LISTENING) {
        /* The protocol has no is_final flag. STT must not revoke capture. */
        t->saw_stt=true;
    } else if(event==PP_TURN_TTS_START && t->phase==PP_TURN_LISTENING && t->sent_frames) {
        t->phase=PP_TURN_WAIT_AUDIO; t->progress_at=now;
    } else if(event==PP_TURN_RX_AUDIO && pp_voice_turn_plays(t)) {
        t->phase=PP_TURN_SPEAKING; t->progress_at=now;
    } else if(event==PP_TURN_TTS_STOP && pp_voice_turn_plays(t)) {
        /* Duplicate/late stop while listening or already pending is ignored. */
        t->phase=PP_TURN_RESUME_PENDING;
    } else if(event==PP_TURN_DRAINED && t->phase==PP_TURN_RESUME_PENDING) {
        t->phase=PP_TURN_START_PENDING;
    } else if(event==PP_TURN_TICK) {
        if(t->phase==PP_TURN_LISTENING && now-t->listen_at>=PP_TURN_LISTEN_LIMIT_MS)
            stop(t,t->saw_stt?PP_TURN_FAILED:PP_TURN_STOPPED,
                 t->saw_stt?PP_TURN_REASON_TIMEOUT:PP_TURN_REASON_QUIET);
        else if(pp_voice_turn_plays(t) && now-t->progress_at>=PP_TURN_RESPONSE_LIMIT_MS)
            stop(t,PP_TURN_FAILED,PP_TURN_REASON_TIMEOUT);
    }
}
