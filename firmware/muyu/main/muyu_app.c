#include "muyu_logic.h"
#include "muyu_ui.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <inttypes.h>
#include <stdatomic.h>

static const char *TAG = "muyu";
static TaskHandle_t s_control_task, s_audio_task;
static atomic_bool s_audio_ok, s_buttons_ok;
static bool s_battery_ok;
static int16_t s_tone[MUYU_TONE_SAMPLES];

static void on_button(bsp_btn_t button, bsp_btn_ev_t event, void *user)
{
    (void)button;
    (void)user;
    /* The timer callback never waits for LVGL, audio, Flash or I2C.
       Ignore CLICK/DOUBLE/LONG so each physical down edge counts once. */
    if (event != BSP_BTN_PRESS) return;
    xTaskNotifyGive(s_control_task);
    if (atomic_load(&s_audio_ok)) xTaskNotifyGive(s_audio_task);
}

static void audio_worker(void *arg)
{
    (void)arg;
    muyu_voices_t voices;
    muyu_voices_init(&voices);
    int16_t chunk[160]; /* 10 ms / 320 bytes; no full recording buffer. */
    for (;;) {
        uint32_t knocks = ulTaskNotifyTake(pdTRUE,
            muyu_voices_active(&voices) ? 0 : portMAX_DELAY);
        muyu_voices_trigger(&voices, knocks);
        muyu_voices_render(&voices, s_tone, chunk, 160);
        if (bsp_audio_write(chunk, sizeof(chunk)) != ESP_OK) {
            ESP_LOGE(TAG, "audio output failed; counter remains usable");
            atomic_store(&s_audio_ok, false);
            /* Keep the task handle valid for a callback already in flight. */
            for (;;) ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        }
    }
}

static void control_worker(void *arg)
{
    (void)arg;
    uint32_t count = 0;
    int battery = -1;
    int64_t next_battery = 0;
    bool old_audio = false, old_buttons = false, dirty = true, struck = false;
    for (;;) {
        uint32_t knocks = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20));
        if (knocks) {
            count = muyu_count_add(count, knocks);
            dirty = struck = true;
            ESP_LOGI(TAG, "count=%" PRIu32 " presses=%" PRIu32, count, knocks);
        }
        int64_t now = esp_timer_get_time();
        if (now >= next_battery) {
            battery = s_battery_ok ? bsp_battery_soc() : -1;
            next_battery = now + 30000000;
            dirty = true;
        }
        bool audio = atomic_load(&s_audio_ok);
        bool buttons = atomic_load(&s_buttons_ok);
        if (audio != old_audio || buttons != old_buttons) dirty = true;
        if (dirty && bsp_lvgl_lock(20)) {
            muyu_ui_refresh(count, battery, audio, buttons, struck);
            bsp_lvgl_unlock();
            old_audio = audio;
            old_buttons = buttons;
            dirty = struck = false;
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Muyu MVP: any key strikes; session counter starts at zero");
    if (bsp_i2c_init() != ESP_OK || bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "display initialization failed");
        return;
    }
    bsp_display_backlight(80);
    if (!bsp_lvgl_lock(1000)) return;
    muyu_ui_create();
    bsp_lvgl_unlock();

    if (bsp_audio_init() == ESP_OK &&
        bsp_audio_set_format(MUYU_SAMPLE_RATE, 16, 1) == ESP_OK) {
        bsp_audio_set_volume(65);
        muyu_tone_generate(s_tone);
        atomic_store(&s_audio_ok, xTaskCreate(audio_worker, "muyu_audio", 3072,
                     NULL, 5, &s_audio_task) == pdPASS);
    }
    s_battery_ok = bsp_battery_init() == ESP_OK;
    if (xTaskCreate(control_worker, "muyu_control", 4096, NULL, 3,
                    &s_control_task) != pdPASS) {
        if (bsp_lvgl_lock(1000)) {
            muyu_ui_refresh(0, -1, atomic_load(&s_audio_ok), false, false);
            bsp_lvgl_unlock();
        }
        ESP_LOGE(TAG, "control task creation failed; buttons remain disabled");
        return;
    }
    atomic_store(&s_buttons_ok, bsp_button_init(on_button, NULL) == ESP_OK);
    ESP_LOGI(TAG, "ready: buttons=%d audio=%d battery=%d", atomic_load(&s_buttons_ok),
             atomic_load(&s_audio_ok), s_battery_ok);
}
