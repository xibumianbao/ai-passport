#include "passport_audio.h"
#include "muyu_logic.h"
#include "bsp_audio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdatomic.h>
typedef struct { uint32_t session; } knock_t;
static QueueHandle_t s_queue;
static SemaphoreHandle_t s_stopped;
static TaskHandle_t s_task;
static atomic_bool s_ok, s_stop;
static atomic_uint s_session, s_volume;
static int16_t s_tone[MUYU_TONE_SAMPLES];
static void audio_worker(void *arg)
{
    (void)arg; muyu_voices_t voices; muyu_voices_init(&voices);
    int16_t pcm[160]; unsigned volume = atomic_load(&s_volume);
    for (;;) {
        if (atomic_exchange(&s_stop, false)) {
            muyu_voices_init(&voices);
            for (unsigned i = 0; i < 160; ++i) pcm[i] = 0;
            /* Drain the I2S pipeline before acknowledging focus loss. */
            for (int i = 0; i < 10; ++i)
                if (bsp_audio_write(pcm, sizeof(pcm)) != ESP_OK) atomic_store(&s_ok, false);
            xSemaphoreGive(s_stopped);
        }
        unsigned requested_volume = atomic_load(&s_volume);
        if (requested_volume != volume) { volume = requested_volume; bsp_audio_set_volume(volume); }
        knock_t knock;
        if (xQueueReceive(s_queue, &knock, muyu_voices_active(&voices) ? 0 : pdMS_TO_TICKS(10)) == pdTRUE &&
            knock.session && knock.session == atomic_load(&s_session) && !atomic_load(&s_stop))
            muyu_voices_trigger(&voices, 1);
        if (muyu_voices_active(&voices) && atomic_load(&s_ok) && !atomic_load(&s_stop)) {
            muyu_voices_render(&voices, s_tone, pcm, 160);
            if (bsp_audio_write(pcm, sizeof(pcm)) != ESP_OK) {
                atomic_store(&s_ok, false); muyu_voices_init(&voices);
            }
        }
    }
}
bool pp_audio_init(unsigned volume)
{
    if (bsp_audio_init() != ESP_OK || bsp_audio_set_format(MUYU_SAMPLE_RATE, 16, 1) != ESP_OK) return false;
    s_queue = xQueueCreate(8, sizeof(knock_t)); s_stopped = xSemaphoreCreateBinary();
    if (!s_queue || !s_stopped) return false;
    atomic_store(&s_volume, volume); bsp_audio_set_volume(volume); muyu_tone_generate(s_tone);
    atomic_store(&s_ok, true);
    if (xTaskCreate(audio_worker, "pp_audio", 3072, NULL, 5, &s_task) != pdPASS) atomic_store(&s_ok, false);
    return atomic_load(&s_ok);
}
void pp_audio_volume(unsigned volume) { atomic_store(&s_volume, volume > 100 ? 100 : volume); }
void pp_audio_begin(uint32_t session) { atomic_store(&s_session, session); }
void pp_audio_knock(uint32_t session)
{
    if (!atomic_load(&s_ok) || session != atomic_load(&s_session)) return;
    const knock_t k = {session}; xQueueSend(s_queue, &k, 0);
}
void pp_audio_cancel(void *unused)
{
    (void)unused; atomic_store(&s_session, 0);
    if (!s_task) return;
    xQueueReset(s_queue);
    while (xSemaphoreTake(s_stopped, 0) == pdTRUE) {}
    atomic_store(&s_stop, true);
    if (xSemaphoreTake(s_stopped, pdMS_TO_TICKS(1000)) != pdTRUE) atomic_store(&s_ok, false);
}
bool pp_audio_ok(void) { return atomic_load(&s_ok); }
