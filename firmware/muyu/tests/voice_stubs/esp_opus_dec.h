#pragma once
#include "esp_audio_types.h"
#include <stdbool.h>
#include <stdint.h>
enum { ESP_OPUS_DEC_FRAME_DURATION_60_MS=5 };
enum { ESP_AUDIO_DEC_RECOVERY_NONE=0 };
typedef struct { uint32_t sample_rate; uint8_t channel; int frame_duration; bool self_delimited; } esp_opus_dec_cfg_t;
typedef struct { uint8_t *buffer; uint32_t len,consumed; int frame_recover; } esp_audio_dec_in_raw_t;
typedef struct { uint8_t *buffer; uint32_t len,needed_size,decoded_size; } esp_audio_dec_out_frame_t;
typedef struct { uint32_t sample_rate; uint8_t bits_per_sample,channel; uint32_t bitrate,frame_size; } esp_audio_dec_info_t;
esp_audio_err_t esp_opus_dec_open(void *config,uint32_t size,void **handle);
esp_audio_err_t esp_opus_dec_decode(void *handle,esp_audio_dec_in_raw_t *input,
    esp_audio_dec_out_frame_t *output,esp_audio_dec_info_t *info);
void esp_opus_dec_close(void *handle);
