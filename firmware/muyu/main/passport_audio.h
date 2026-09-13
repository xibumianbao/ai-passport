#pragma once
#include <stdbool.h>
#include <stdint.h>
bool pp_audio_init(unsigned volume);
void pp_audio_volume(unsigned volume);
void pp_audio_begin(uint32_t session);
void pp_audio_knock(uint32_t session);
void pp_audio_cancel(void *unused);
bool pp_audio_ok(void);
bool pp_audio_acquire(void);
void pp_audio_apply_volume(void);
void pp_audio_release(void);
