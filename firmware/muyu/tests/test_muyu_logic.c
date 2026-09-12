#include "muyu_logic.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static int16_t tone[MUYU_TONE_SAMPLES];

int main(int argc, char **argv)
{
    uint32_t count = 0;
    for (unsigned n = 0; n < 1000; ++n) count = muyu_count_add(count, 1);
    assert(count == 1000);
    assert(muyu_count_add(UINT32_MAX - 2, 2) == UINT32_MAX);
    assert(muyu_count_add(UINT32_MAX - 2, 3) == UINT32_MAX);
    assert(muyu_count_add(42, UINT32_MAX) == UINT32_MAX);
    assert(muyu_count_add(UINT32_MAX, 0) == UINT32_MAX);

    muyu_tone_generate(tone);
    assert(tone[0] == 0 && tone[MUYU_TONE_SAMPLES - 1] == 0);
    int64_t early = 0, late = 0;
    bool positive = false, negative = false;
    for (size_t i = 0; i < MUYU_TONE_SAMPLES; ++i) {
        assert(tone[i] >= -9000 && tone[i] <= 9000);
        positive |= tone[i] > 1000;
        negative |= tone[i] < -1000;
        int64_t energy = (int64_t)tone[i] * tone[i];
        if (i < MUYU_TONE_SAMPLES / 2) early += energy;
        else late += energy;
    }
    assert(positive && negative && early > late * 10);

    muyu_voices_t voices;
    muyu_voices_init(&voices);
    int16_t output[MUYU_TONE_SAMPLES + 160];
    memset(output, 1, sizeof(output));
    muyu_voices_render(&voices, tone, output, MUYU_TONE_SAMPLES + 160);
    for (size_t i = 0; i < MUYU_TONE_SAMPLES + 160; ++i) assert(output[i] == 0);
    muyu_voices_trigger(&voices, 1);
    muyu_voices_render(&voices, tone, output, MUYU_TONE_SAMPLES + 160);
    assert(memcmp(output, tone, sizeof(tone)) == 0);
    for (size_t i = MUYU_TONE_SAMPLES; i < MUYU_TONE_SAMPLES + 160; ++i) assert(output[i] == 0);
    assert(!muyu_voices_active(&voices));

    /* Thousands of rapid hits must drain in one tone length after the last
       hit, regardless of the notification count, rather than queueing sounds. */
    for (unsigned n = 0; n < 1000; ++n) {
        muyu_voices_trigger(&voices, n % 2 ? UINT32_MAX : 1);
        muyu_voices_render(&voices, tone, output, 32);
    }
    muyu_voices_render(&voices, tone, output, MUYU_TONE_SAMPLES + 160);
    assert(!muyu_voices_active(&voices));
    for (size_t i = MUYU_TONE_SAMPLES; i < MUYU_TONE_SAMPLES + 160; ++i) assert(output[i] == 0);

    /* Artificial worst-case samples exercise the signed 16-bit limiter. */
    for (unsigned sign = 0; sign < 2; ++sign) {
        int16_t full[MUYU_TONE_SAMPLES];
        for (size_t i = 0; i < MUYU_TONE_SAMPLES; ++i) full[i] = sign ? INT16_MIN : INT16_MAX;
        muyu_voices_init(&voices);
        muyu_voices_trigger(&voices, MUYU_MAX_VOICES);
        muyu_voices_render(&voices, full, output, 160);
        for (size_t i = 0; i < 160; ++i) assert(output[i] == (sign ? INT16_MIN : INT16_MAX));
    }

    if (argc == 2) {
        FILE *file = fopen(argv[1], "wb");
        assert(file);
        assert(fwrite(tone, sizeof(tone[0]), MUYU_TONE_SAMPLES, file) == MUYU_TONE_SAMPLES);
        assert(fclose(file) == 0);
    }
    puts("Muyu host tests: PASS (counter, saturation, tone, mixing, burst drain, limiter)");
    return 0;
}
