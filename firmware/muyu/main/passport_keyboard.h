#pragma once
#include "passport_core.h"
#define PP_KB_MAX_KEYS 36
typedef struct { char value[64]; uint8_t page, selected, limit; } pp_keyboard_t;
typedef enum { PP_KB_EDIT, PP_KB_SUBMIT, PP_KB_CANCEL, PP_KB_FULL } pp_kb_result_t;
void pp_keyboard_init(pp_keyboard_t *kb, unsigned limit);
unsigned pp_keyboard_count(unsigned page);
const char *pp_keyboard_label(unsigned page, unsigned index);
pp_kb_result_t pp_keyboard_input(pp_keyboard_t *kb, pp_key_t key);
bool pp_wifi_credentials_valid(const char *ssid, const char *password, bool open);
