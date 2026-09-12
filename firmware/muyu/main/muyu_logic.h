#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MUYU_SAMPLE_RATE 16000
#define MUYU_TONE_SAMPLES 3072
#define MUYU_MAX_VOICES 4

/* Fixed storage: no allocation and no unbounded queue of old knocks. */
typedef struct {
    size_t cursor[MUYU_MAX_VOICES];
    unsigned next_voice;
} muyu_voices_t;

uint32_t muyu_count_add(uint32_t count, uint32_t knocks);
void muyu_tone_generate(int16_t samples[MUYU_TONE_SAMPLES]);
void muyu_voices_init(muyu_voices_t *voices);
void muyu_voices_trigger(muyu_voices_t *voices, uint32_t knocks);
bool muyu_voices_active(const muyu_voices_t *voices);
void muyu_voices_render(muyu_voices_t *voices, const int16_t *tone,
                        int16_t *output, size_t samples);
