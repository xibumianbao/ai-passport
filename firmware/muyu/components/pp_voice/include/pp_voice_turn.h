#pragma once
#include <stdbool.h>
#include <stdint.h>

#define PP_TURN_LISTEN_LIMIT_MS 30000u
#define PP_TURN_RESPONSE_LIMIT_MS 60000u
typedef enum {
    PP_TURN_READY, PP_TURN_START_PENDING, PP_TURN_LISTENING,
    PP_TURN_WAIT_AUDIO, PP_TURN_SPEAKING, PP_TURN_RESUME_PENDING,
    PP_TURN_STOPPED, PP_TURN_FAILED
} pp_turn_phase_t;
typedef enum {
    PP_TURN_BUTTON, PP_TURN_LISTEN_SENT, PP_TURN_TX_FRAME, PP_TURN_STT,
    PP_TURN_TTS_START, PP_TURN_RX_AUDIO, PP_TURN_TTS_STOP, PP_TURN_DRAINED,
    PP_TURN_TICK, PP_TURN_REVOKE, PP_TURN_DISCONNECTED, PP_TURN_BAD_SESSION
} pp_turn_event_t;
typedef enum {
    PP_TURN_REASON_NONE, PP_TURN_REASON_USER, PP_TURN_REASON_QUIET,
    PP_TURN_REASON_TIMEOUT, PP_TURN_REASON_REVOKED, PP_TURN_REASON_NETWORK,
    PP_TURN_REASON_SESSION
} pp_turn_reason_t;
typedef struct {
    pp_turn_phase_t phase;
    pp_turn_reason_t reason;
    uint64_t listen_at, progress_at;
    uint32_t turns, sent_frames;
    bool saw_stt;
} pp_voice_turn_t;

/* Zero initialization means connected/Ready. Only the worker mutates this.
 * JSON events change state only; START/RESUME_PENDING are deliberately deferred
 * until the JSON object is deleted and playback/old capture are cleared. */
void pp_voice_turn_event(pp_voice_turn_t *turn,pp_turn_event_t event,uint64_t now_ms);
bool pp_voice_turn_captures(const pp_voice_turn_t *turn);
bool pp_voice_turn_plays(const pp_voice_turn_t *turn);
bool pp_voice_turn_active(const pp_voice_turn_t *turn);
