#include "muyu_logic.h"
#include <limits.h>
#include <math.h>

uint32_t muyu_count_add(uint32_t count, uint32_t knocks)
{
    return knocks > UINT32_MAX - count ? UINT32_MAX : count + knocks;
}

void muyu_tone_generate(int16_t samples[MUYU_TONE_SAMPLES])
{
    /* Decaying wood resonances, adapted from upstream demo/coloros-muyu.
       A short onset and a zero-ended tail avoid discontinuities at silence. */
    uint32_t noise_state = 0x4D595955U;
    for (size_t i = 0; i < MUYU_TONE_SAMPLES; ++i) {
        float t = (float)i / MUYU_SAMPLE_RATE;
        float body = 0.68f * sinf(6.28318530718f * 720.0f * t)
                   + 0.32f * sinf(6.28318530718f * 1170.0f * t)
                   + 0.16f * sinf(6.28318530718f * 1890.0f * t);
        noise_state = noise_state * 1664525U + 1013904223U;
        float noise = ((float)(noise_state >> 16) - 32768.0f) / 32768.0f;
        float attack = i < 160 ? noise * 0.18f * (1.0f - (float)i / 160) : 0;
        float onset = i < 16 ? (float)i / 16 : 1;
        size_t remaining = MUYU_TONE_SAMPLES - 1 - i;
        float tail = remaining < 240 ? (float)remaining / 240 : 1;
        float value = onset * tail * (expf(-27.0f * t) * body + attack);
        if (value > 1) value = 1;
        if (value < -1) value = -1;
        samples[i] = (int16_t)(value * 9000.0f);
    }
}

void muyu_voices_init(muyu_voices_t *voices)
{
    voices->next_voice = 0;
    for (unsigned v = 0; v < MUYU_MAX_VOICES; ++v) {
        voices->cursor[v] = MUYU_TONE_SAMPLES;
    }
}

void muyu_voices_trigger(muyu_voices_t *voices, uint32_t knocks)
{
    /* All presses count; a burst replaces old sound tails instead of building
       seconds of delayed audio. At most four voices are mixed simultaneously. */
    if (knocks > MUYU_MAX_VOICES) knocks = MUYU_MAX_VOICES;
    for (uint32_t n = 0; n < knocks; ++n) {
        voices->cursor[voices->next_voice] = 0;
        voices->next_voice = (voices->next_voice + 1) % MUYU_MAX_VOICES;
    }
}

bool muyu_voices_active(const muyu_voices_t *voices)
{
    for (unsigned v = 0; v < MUYU_MAX_VOICES; ++v) {
        if (voices->cursor[v] < MUYU_TONE_SAMPLES) return true;
    }
    return false;
}

void muyu_voices_render(muyu_voices_t *voices, const int16_t *tone,
                        int16_t *output, size_t samples)
{
    for (size_t i = 0; i < samples; ++i) {
        int32_t value = 0;
        for (unsigned v = 0; v < MUYU_MAX_VOICES; ++v) {
            if (voices->cursor[v] < MUYU_TONE_SAMPLES) {
                value += tone[voices->cursor[v]++];
            }
        }
        if (value > INT16_MAX) value = INT16_MAX;
        if (value < INT16_MIN) value = INT16_MIN;
        output[i] = (int16_t)value;
    }
}
