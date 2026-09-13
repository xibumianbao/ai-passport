#pragma once
#include "esp_audio_types.h"
#include <stdbool.h>
#include <stdint.h>
enum { ESP_OPUS_ENC_FRAME_DURATION_60_MS=5, ESP_OPUS_ENC_APPLICATION_VOIP=0 };
typedef struct {
    int sample_rate,channel,bits_per_sample,bitrate,frame_duration,application_mode,complexity;
    bool enable_fec,enable_dtx,enable_vbr;
} esp_opus_enc_config_t;
typedef struct { uint8_t *buffer; uint32_t len; } esp_audio_enc_in_frame_t;
typedef struct { uint8_t *buffer; uint32_t len,encoded_bytes; uint64_t pts; } esp_audio_enc_out_frame_t;
esp_audio_err_t esp_opus_enc_open(void *config,uint32_t size,void **handle);
esp_audio_err_t esp_opus_enc_get_frame_size(void *handle,int *input,int *output);
esp_audio_err_t esp_opus_enc_process(void *handle,esp_audio_enc_in_frame_t *input,esp_audio_enc_out_frame_t *output);
void esp_opus_enc_close(void *handle);
