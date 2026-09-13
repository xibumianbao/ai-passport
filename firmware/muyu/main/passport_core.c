#include "passport_core.h"
#include <string.h>

pp_key_t pp_input_event(pp_input_t *s, unsigned key, pp_edge_t edge)
{
    if (key > PP_OK) return PP_NONE;
    if (edge == PP_PRESS) {
        if (s->down[key]) return PP_NONE;
        s->down[key] = true;
        s->consumed[key] = false;
        return key == PP_OK ? PP_NONE : (pp_key_t)key;
    }
    if (!s->down[key]) return PP_NONE;
    if (edge == PP_LONG && key == PP_OK && !s->consumed[key]) {
        s->consumed[key] = true;
        return PP_MENU;
    }
    if (edge == PP_RELEASE) {
        bool short_ok = key == PP_OK && !s->consumed[key];
        s->down[key] = false;
        return short_ok ? PP_OK : PP_NONE;
    }
    return PP_NONE;
}
void pp_input_cancel(pp_input_t *s)
{
    for (unsigned i = 0; i < 3; ++i) s->consumed[i] = true;
}
void pp_runtime_init(pp_runtime_t *r, const pp_app_t *apps, size_t count)
{
    memset(r, 0, sizeof(*r));
    r->apps = apps; r->count = count; r->active = -1; r->generation = 1;
}
int pp_find_app(const pp_runtime_t *r, const char *id)
{
    if (!id) return -1;
    for (size_t i = 0; i < r->count; ++i)
        if (!strcmp(id, r->apps[i].id)) return (int)i;
    return -1;
}
static void cancel_resources(pp_runtime_t *r)
{
    ++r->generation;
    if (!r->generation) ++r->generation;
    for (size_t i = 0; i < PP_MAX_RESOURCES; ++i) {
        pp_resource_t old = r->resources[i];
        memset(&r->resources[i], 0, sizeof(r->resources[i]));
        if (old.cancel) old.cancel(old.ctx);
    }
}
void pp_menu_open(pp_runtime_t *r)
{
    if (!r->focused) return;
    r->focused = false;
    cancel_resources(r);
    if (r->active >= 0 && r->apps[r->active].focus)
        r->apps[r->active].focus(r->apps[r->active].ctx, false);
}
void pp_resume(pp_runtime_t *r)
{
    if (r->active < 0 || r->focused) return;
    r->focused = true;
    if (r->apps[r->active].focus) r->apps[r->active].focus(r->apps[r->active].ctx, true);
}
bool pp_activate(pp_runtime_t *r, int index)
{
    if (index < 0 || (size_t)index >= r->count) return false;
    if (r->active == index) { pp_resume(r); return true; }
    pp_menu_open(r);
    if (r->active >= 0 && r->apps[r->active].stop) r->apps[r->active].stop(r->apps[r->active].ctx);
    r->active = -1;
    cancel_resources(r);
    const pp_app_t *app = &r->apps[index];
    if (app->start && !app->start(app->ctx)) {
        if (app->stop) app->stop(app->ctx);
        return false;
    }
    r->active = index;
    pp_resume(r);
    return true;
}
void pp_dispatch(pp_runtime_t *r, pp_key_t key)
{
    if (r->focused && r->active >= 0 && key <= PP_OK && r->apps[r->active].key)
        r->apps[r->active].key(r->apps[r->active].ctx, key);
}
bool pp_session_valid(const pp_runtime_t *r, uint32_t generation)
{
    return r->focused && r->active >= 0 && generation == r->generation;
}
int pp_resource_acquire(pp_runtime_t *r, pp_cancel_fn cancel, void *ctx)
{
    if (!r->focused || !cancel) return -1;
    for (int i = 0; i < PP_MAX_RESOURCES; ++i) if (!r->resources[i].cancel) {
        r->resources[i] = (pp_resource_t){ r->generation, cancel, ctx };
        return i;
    }
    return -1;
}
void pp_resource_release(pp_runtime_t *r, int slot, uint32_t generation)
{
    if (slot >= 0 && slot < PP_MAX_RESOURCES && r->resources[slot].generation == generation)
        memset(&r->resources[slot], 0, sizeof(r->resources[slot]));
}
void pp_settings_defaults(pp_settings_t *s) { *s = (pp_settings_t){65, 80, 1, 0, true}; }
void pp_settings_validate(pp_settings_t *s)
{
    if (s->volume > 100) s->volume = 65;
    if (s->brightness < 10 || s->brightness > 100) s->brightness = 80;
    if (s->timeout > 3) s->timeout = 1;
    if (s->screen_mode > 1) s->screen_mode = 0;
}
unsigned pp_timeout_seconds(uint8_t value)
{
    static const unsigned values[] = {0, 60, 120, 300};
    return value < 4 ? values[value] : 60;
}
bool pp_boot_safe(bool previous_pending, uint8_t previous_failures, uint8_t *failures)
{
    *failures = previous_pending ? (previous_failures < 2 ? previous_failures + 1 : 2) : 0;
    return *failures >= 2;
}
static int hex(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static bool decode(const char *src, size_t len, char *dst, size_t cap)
{
    size_t used = 0;
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)src[i];
        if (c == '+') c = ' ';
        else if (c == '%') {
            if (i + 2 >= len || hex(src[i+1]) < 0 || hex(src[i+2]) < 0) return false;
            c = (unsigned char)(hex(src[i+1]) * 16 + hex(src[i+2])); i += 2;
        }
        if (c < 32 || c == 127 || used + 1 >= cap) return false;
        dst[used++] = (char)c;
    }
    dst[used] = 0;
    return true;
}
bool pp_parse_wifi_form(const char *body, size_t len, char ssid[33], char pass[64])
{
    bool got_ssid = false, got_pass = false;
    memset(ssid, 0, 33); memset(pass, 0, 64);
    if (!body || !len || len > 384) return false;
    for (size_t pos = 0; pos < len;) {
        size_t end = pos, eq = pos;
        while (end < len && body[end] != '&') ++end;
        while (eq < end && body[eq] != '=') ++eq;
        if (eq == end) goto fail;
        if (eq-pos == 4 && !memcmp(body+pos, "ssid", 4) && !got_ssid) {
            got_ssid = decode(body+eq+1, end-eq-1, ssid, 33);
            if (!got_ssid) goto fail;
        } else if (eq-pos == 4 && !memcmp(body+pos, "pass", 4) && !got_pass) {
            got_pass = decode(body+eq+1, end-eq-1, pass, 64);
            if (!got_pass) goto fail;
        } else goto fail;
        pos = end + 1;
    }
    if (got_ssid && got_pass && ssid[0] && (!pass[0] || strlen(pass) >= 8)) return true;
fail:
    memset(ssid, 0, 33); memset(pass, 0, 64); return false;
}
