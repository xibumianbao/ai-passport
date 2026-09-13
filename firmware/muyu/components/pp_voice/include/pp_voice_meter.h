#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PP_VOICE_METER_STALE_MS 250u
/* The existing decoder's largest PCM buffer is 1920 signed 16-bit samples. */
#define PP_VOICE_METER_MAX_BYTES 3840u

typedef struct {
    uint64_t at_ms;
    uint16_t level;
    bool valid;
} pp_voice_meter_t;

/* Native signed 16-bit PCM, with an even byte count. Failed writes and invalid
 * buffers leave the meter unchanged. No PCM copy, allocation or SDK calls. */
bool pp_voice_meter_submit(pp_voice_meter_t *meter,const int16_t *pcm,
    size_t bytes,bool written,uint64_t now_ms);
void pp_voice_meter_clear(pp_voice_meter_t *meter);
/* Use the same monotonic millisecond clock as submit. No sample or a backwards
 * clock gives level=0, age=UINT16_MAX. Age saturates; stale amplitude is zero. */
void pp_voice_meter_read(const pp_voice_meter_t *meter,uint64_t now_ms,
    uint16_t *level,uint16_t *age_ms);
