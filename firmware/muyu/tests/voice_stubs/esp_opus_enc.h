#pragma once
#include "esp_audio_types.h"
#include <stdbool.h>
#include <stdint.h>
enum { ESP_OPUS_ENC_FRAME_DURATION_60_MS=5, ESP_OPUS_ENC_APPLICATION_VOIP=0 };
typedef struct {
    int sample_rate,channel,bits_per_sample,bitrate,frame_duration,application_mode,complexity;
    bool enable_fec,enable_dtx,enable_vbr;
} esp_opus_enc_config_t;
esp_audio_err_t esp_opus_enc_open(void *config,uint32_t size,void **handle);
esp_audio_err_t esp_opus_enc_get_frame_size(void *handle,int *input,int *output);
void esp_opus_enc_close(void *handle);
