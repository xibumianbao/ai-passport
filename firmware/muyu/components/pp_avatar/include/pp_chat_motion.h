#pragma once
#include "pp_avatar.h"

/* Presentation only: no pet model, voice lifecycle, clocks, LVGL, heap or tasks.
 * Call on visible chat renders (normally every 200 ms). Reset on page creation
 * and before the first render after a menu / dark-screen interval. */
typedef struct {
    uint32_t last_ms, phase_ms, random;
    uint32_t blink_due_ms, blink_left_ms, gaze_due_ms;
    pp_avatar_state_t voice;
    int8_t gaze;
    bool initialized;
} pp_chat_motion_t;

typedef struct {
    pp_avatar_frame_t frame;
    uint8_t bars[3]; /* Pixel heights, 0..12. All zero when no active sound. */
    uint8_t dot_phase; /* 0..2; use only in the thinking state. */
} pp_chat_motion_output_t;

void pp_chat_motion_reset(pp_chat_motion_t *motion, uint32_t now_ms);
/* PCM levels are nonnegative mean absolute values (0..32768). Playback is
 * independent from mic_level and expires at age >=250 ms, including UINT16_MAX.
 * Voice state wins over levels: only SPEAKING can open the mouth; LISTENING
 * can show mic bars. No levels drive an audio or conversation transition.
 * Unsigned subtraction handles uint32 clock wrap; gaps >2 s reset animation.
 * NULL motion/output is ignored. Repeated calls at one timestamp are stable. */
void pp_chat_motion_step(pp_chat_motion_t *motion, uint32_t now_ms,
                         pp_avatar_state_t voice, uint16_t mic_level,
                         uint16_t playback_level, uint16_t playback_age_ms,
                         pp_chat_motion_output_t *out);
