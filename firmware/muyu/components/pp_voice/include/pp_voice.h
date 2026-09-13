#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PP_VOICE_OFF, PP_VOICE_STARTING, PP_VOICE_WIFI, PP_VOICE_CLOCK,
    PP_VOICE_DISCOVERY, PP_VOICE_ACTIVATION, PP_VOICE_CONNECTING,
    PP_VOICE_READY, PP_VOICE_LISTENING, PP_VOICE_THINKING,
    PP_VOICE_SPEAKING, PP_VOICE_ERROR, PP_VOICE_STOPPING
} pp_voice_state_t;
typedef struct {
    pp_voice_state_t state;
    char detail[80], activation[16];
    uint32_t tx_frames, rx_frames, free_heap, stack_free;
    uint16_t level;
} pp_voice_snapshot_t;
/* Control task only. Focus owns a cancellable session, independent of LVGL.
 * Close revokes permission immediately. The worker releases audio/transport
 * asynchronously; a new session cannot start until the old worker exits. */
void pp_voice_open(void);
void pp_voice_close(void *unused);
void pp_voice_tick(void);
void pp_voice_press(void);
void pp_voice_snapshot(pp_voice_snapshot_t *out);
