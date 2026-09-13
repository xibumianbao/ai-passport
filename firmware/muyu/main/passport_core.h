#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PP_VERSION "0.4.0"
#define PP_LONG_PRESS_MS 800
#define PP_MAX_RESOURCES 8
typedef enum { PP_UP, PP_DOWN, PP_OK, PP_MENU, PP_NONE } pp_key_t;
typedef enum { PP_PRESS, PP_RELEASE, PP_LONG } pp_edge_t;
typedef struct { bool down[3], consumed[3]; } pp_input_t;
pp_key_t pp_input_event(pp_input_t *s, unsigned key, pp_edge_t edge);
void pp_input_cancel(pp_input_t *s);

typedef struct {
    const char *id, *name, *version, *author;
    bool (*start)(void *ctx);
    void (*stop)(void *ctx);
    void (*focus)(void *ctx, bool focused);
    void (*key)(void *ctx, pp_key_t key);
    void *ctx;
} pp_app_t;
typedef void (*pp_cancel_fn)(void *ctx);
typedef struct { uint32_t generation; pp_cancel_fn cancel; void *ctx; } pp_resource_t;
typedef struct {
    const pp_app_t *apps;
    size_t count;
    int active;
    bool focused;
    uint32_t generation;
    pp_resource_t resources[PP_MAX_RESOURCES];
} pp_runtime_t;
void pp_runtime_init(pp_runtime_t *r, const pp_app_t *apps, size_t count);
int pp_find_app(const pp_runtime_t *r, const char *id);
bool pp_activate(pp_runtime_t *r, int index);
void pp_menu_open(pp_runtime_t *r);
void pp_resume(pp_runtime_t *r);
void pp_dispatch(pp_runtime_t *r, pp_key_t key);
bool pp_session_valid(const pp_runtime_t *r, uint32_t generation);
int pp_resource_acquire(pp_runtime_t *r, pp_cancel_fn cancel, void *ctx);
void pp_resource_release(pp_runtime_t *r, int slot, uint32_t generation);

typedef struct { uint8_t volume, brightness, timeout, screen_mode; bool wifi, ble; } pp_settings_t;
void pp_settings_defaults(pp_settings_t *s);
void pp_settings_validate(pp_settings_t *s);
unsigned pp_timeout_seconds(uint8_t value);
bool pp_boot_safe(bool previous_pending, uint8_t previous_failures, uint8_t *failures);
/* Decode a bounded application/x-www-form-urlencoded Wi-Fi form. No logging. */
bool pp_parse_wifi_form(const char *body, size_t len, char ssid[33], char pass[64]);
