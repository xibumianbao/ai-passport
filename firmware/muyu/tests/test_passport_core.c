#include "passport_core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct { int starts, stops, focused, keys, cancels; bool fail; } fake_t;
static bool start(void *v) { fake_t *f=v; ++f->starts; return !f->fail; }
static void stop(void *v) { ++((fake_t *)v)->stops; }
static void focus(void *v, bool on) { ((fake_t *)v)->focused = on; }
static void key(void *v, pp_key_t k) { (void)k; ++((fake_t *)v)->keys; }
static void cancel(void *v) { ++((fake_t *)v)->cancels; }
static void inputs(void)
{
    pp_input_t s = {0};
    assert(pp_input_event(&s, PP_OK, PP_RELEASE) == PP_NONE);
    assert(pp_input_event(&s, PP_OK, PP_PRESS) == PP_NONE);
    assert(pp_input_event(&s, PP_OK, PP_PRESS) == PP_NONE);
    assert(pp_input_event(&s, PP_OK, PP_LONG) == PP_MENU);
    assert(pp_input_event(&s, PP_OK, PP_LONG) == PP_NONE);
    assert(pp_input_event(&s, PP_OK, PP_RELEASE) == PP_NONE);
    for (int i=0; i<10000; ++i) {
        assert(pp_input_event(&s, PP_OK, PP_PRESS) == PP_NONE);
        assert(pp_input_event(&s, PP_OK, PP_RELEASE) == PP_OK);
        assert(pp_input_event(&s, PP_OK, PP_RELEASE) == PP_NONE);
    }
    assert(pp_input_event(&s, PP_UP, PP_PRESS) == PP_UP);
    assert(pp_input_event(&s, PP_UP, PP_LONG) == PP_NONE);
    assert(pp_input_event(&s, PP_UP, PP_RELEASE) == PP_NONE);
    pp_input_event(&s, PP_OK, PP_PRESS); pp_input_cancel(&s);
    assert(pp_input_event(&s, PP_OK, PP_LONG) == PP_NONE);
    assert(pp_input_event(&s, PP_OK, PP_RELEASE) == PP_NONE);
    assert(pp_input_event(&s, 9, PP_PRESS) == PP_NONE);
}
static void lifecycle(void)
{
    fake_t a={0}, b={0};
    pp_app_t apps[] = {
        {"a", "A", "1", "Test", start, stop, focus, key, &a},
        {"b", "B", "1", "Test", start, stop, focus, key, &b}
    };
    pp_runtime_t r; pp_runtime_init(&r, apps, 2);
    assert(r.active == -1 && !r.focused);
    assert(pp_find_app(&r, "a") == 0 && pp_find_app(&r, "missing") == -1);
    assert(!pp_activate(&r, 3)); assert(pp_activate(&r, 0));
    uint32_t generation = r.generation;
    for (int i=0; i<PP_MAX_RESOURCES; ++i) assert(pp_resource_acquire(&r, cancel, &a) == i);
    assert(pp_resource_acquire(&r, cancel, &a) == -1);
    pp_dispatch(&r, PP_OK); assert(a.keys == 1);
    pp_menu_open(&r); assert(a.cancels == PP_MAX_RESOURCES && !a.focused);
    pp_menu_open(&r); assert(a.cancels == PP_MAX_RESOURCES);
    assert(!pp_session_valid(&r, generation)); pp_dispatch(&r, PP_OK); assert(a.keys == 1);
    pp_resume(&r); assert(a.focused && a.starts == 1);
    int slot = pp_resource_acquire(&r, cancel, &a); assert(slot == 0);
    pp_resource_release(&r, slot, generation); assert(r.resources[0].cancel);
    pp_resource_release(&r, slot, r.generation); assert(!r.resources[0].cancel);
    assert(pp_activate(&r, 1)); assert(a.stops == 1 && b.focused);
    assert(!pp_session_valid(&r, generation));
    for (int i=0; i<10000; ++i) {
        assert(pp_activate(&r, i%2));
        fake_t *f = i%2 ? &b : &a;
        assert(pp_resource_acquire(&r, cancel, f) == 0);
        uint32_t old = r.generation;
        pp_menu_open(&r); assert(!pp_session_valid(&r, old)); pp_resume(&r);
        assert(pp_session_valid(&r, r.generation));
    }
    b.fail = true; pp_activate(&r, 0);
    assert(!pp_activate(&r, 1)); assert(r.active == -1 && !r.focused);
    assert(!pp_session_valid(&r, r.generation));
    pp_menu_open(&r); pp_resume(&r); pp_dispatch(&r, PP_OK);
    assert(pp_activate(&r, 0));
}
static void settings(void)
{
    pp_settings_t s; pp_settings_defaults(&s);
    assert(s.volume == 65 && s.wifi && !s.ble);
    s.volume = 255; s.brightness = 0; s.timeout = 50;
    pp_settings_validate(&s); assert(s.volume == 65 && s.brightness == 80 && s.timeout == 1);
    assert(pp_timeout_seconds(0) == 0 && pp_timeout_seconds(3) == 300);
    uint8_t failures;
    assert(!pp_boot_safe(false, 2, &failures) && failures == 0);
    assert(!pp_boot_safe(true, 0, &failures) && failures == 1);
    assert(pp_boot_safe(true, 1, &failures) && failures == 2);
    assert(pp_boot_safe(true, 255, &failures) && failures == 2);
}
static void forms(void)
{
    char ssid[33], pass[64];
    const char *good = "ssid=Cafe+%26+Tea&pass=hello%2B123";
    assert(pp_parse_wifi_form(good, strlen(good), ssid, pass));
    assert(!strcmp(ssid, "Cafe & Tea") && !strcmp(pass, "hello+123"));
    good = "pass=&ssid=Open"; assert(pp_parse_wifi_form(good, strlen(good), ssid, pass));
    const char *bad[] = {"", "ssid=&pass=", "ssid=a&pass=short", "ssid=x", "ssid=x&pass=12345678&ssid=y",
        "ssid=%00&pass=", "ssid=%GG&pass=", "ssid=%&pass=", "ssid=x%0A&pass=", "ssid=x&pass=&unknown=x",
        "ssid=123456789012345678901234567890123&pass=", "ssid=x&pass=1234567890123456789012345678901234567890123456789012345678901234"};
    for (unsigned i=0; i<sizeof(bad)/sizeof(bad[0]); ++i) {
        assert(!pp_parse_wifi_form(bad[i], strlen(bad[i]), ssid, pass));
        assert(ssid[0] == 0 && pass[0] == 0);
    }
    /* Bounded random bytes cover truncated escapes and malformed field shapes. */
    char fuzz[384]; unsigned seed=42;
    for (int n=0; n<20000; ++n) {
        size_t len = (size_t)n % sizeof(fuzz);
        for (size_t i=0; i<len; ++i) { seed = seed*1664525U+1013904223U; fuzz[i]=(char)(seed>>24); }
        pp_parse_wifi_form(fuzz, len, ssid, pass);
    }
}
int main(void)
{
    inputs(); lifecycle(); settings(); forms();
    puts("Passport core: PASS (10000 clicks, 10000 switches, cleanup/stale callbacks, boot/settings, 20000 malformed forms)");
    return 0;
}
