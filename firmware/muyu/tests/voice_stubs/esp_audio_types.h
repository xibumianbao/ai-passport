#pragma once
/* Test-only API subset. Firmware always uses the pinned SDK headers/library. */
typedef enum {
    ESP_AUDIO_ERR_OK=0, ESP_AUDIO_ERR_FAIL=-1, ESP_AUDIO_ERR_MEM_LACK=-2,
    ESP_AUDIO_ERR_INVALID_PARAMETER=-5
} esp_audio_err_t;
