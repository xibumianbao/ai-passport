#pragma once
#include "esp_audio_types.h"

#define PP_VOICE_PCM_BYTES 1920 /* 16 kHz mono, 16-bit, 60 ms */

typedef enum { PP_CODEC_OFF, PP_CODEC_ENCODE, PP_CODEC_DECODE } pp_codec_mode_t;
/* One worker, one handle: a half-duplex session cannot retain both codecs. */
typedef struct { void *handle; pp_codec_mode_t mode; } pp_voice_codec_t;

/* Release the old direction BEFORE allocating the new one. Failure leaves OFF.
 * Selecting the current direction preserves its stream state. */
esp_audio_err_t pp_voice_codec_select(pp_voice_codec_t *codec, pp_codec_mode_t mode);
void pp_voice_codec_close(pp_voice_codec_t *codec);
