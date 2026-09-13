#pragma once
#include "esp_audio_types.h"
#include <stdbool.h>
#include <stdint.h>
enum { ESP_OPUS_DEC_FRAME_DURATION_60_MS=5 };
typedef struct { uint32_t sample_rate; uint8_t channel; int frame_duration; bool self_delimited; } esp_opus_dec_cfg_t;
esp_audio_err_t esp_opus_dec_open(void *config,uint32_t size,void **handle);
void esp_opus_dec_close(void *handle);
