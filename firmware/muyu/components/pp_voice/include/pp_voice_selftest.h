#pragma once
#include "pp_voice_codec.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PP_VOICE_TEST_FRAMES 4
#define PP_VOICE_TEST_PATTERNS 4
#define PP_VOICE_TEST_GUARD 8

typedef struct {
    const char *stage;
    unsigned pattern, frames;
    esp_audio_err_t error;
    size_t bytes, consumed;
    uint32_t elapsed_us;
    bool bounds_ok, log;
} pp_voice_test_event_t;
typedef struct {
    void *context;
    bool (*live)(void *context);
    /* Returning false stops the test, for example when stack margin is low. */
    bool (*observe)(void *context,const pp_voice_test_event_t *event);
    uint64_t (*now_us)(void *context);
    void (*yield)(void *context);
} pp_voice_test_hooks_t;

/* Diagnostic only: four continuous frames per pattern, encoder released before
 * decoder allocation. Reuses caller-owned heap buffers; no I2S/network/files.
 * PCM must be int16_t-aligned and hold PCM_BYTES+16; packet holds OPUS_MAX+16;
 * cache holds four maximum Opus packets. Always leaves codec OFF, even on
 * cancellation, bad SDK output, guard damage or observer refusal. */
bool pp_voice_selftest(pp_voice_codec_t *codec,void *pcm,size_t pcm_size,
    uint8_t *packet,size_t packet_size,uint8_t *cache,size_t cache_size,
    const pp_voice_test_hooks_t *hooks);
