#include "passport_keyboard.h"
#include "passport_screen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void keyboard(void)
{
    pp_keyboard_t k; pp_keyboard_init(&k,63);
    bool reachable[128]={0}; reachable[' ']=true;
    for(unsigned page=0;page<4;++page) {
        unsigned n=pp_keyboard_count(page); assert(n>8 && n<=PP_KB_MAX_KEYS);
        for(unsigned i=0;i<n-8;++i) {
            const char *key=pp_keyboard_label(page,i); assert(strlen(key)==1);
            reachable[(unsigned char)*key]=true;
            k.page=page; k.selected=i;
            size_t len=strlen(k.value); pp_kb_result_t r=pp_keyboard_input(&k,PP_OK);
            assert(r==(len<63?PP_KB_EDIT:PP_KB_FULL));
        }
    }
    for(int c=32;c<127;++c) assert(reachable[c]);
    assert(strlen(k.value)==63);
    k.page=0; k.selected=pp_keyboard_count(0)-3;
    assert(pp_keyboard_input(&k,PP_OK)==PP_KB_EDIT && strlen(k.value)==62);
    k.selected=pp_keyboard_count(0)-2; assert(pp_keyboard_input(&k,PP_OK)==PP_KB_SUBMIT);
    assert(pp_keyboard_input(&k,PP_MENU)==PP_KB_CANCEL);
    pp_keyboard_init(&k,32);
    for(int i=0;i<10000;++i) { pp_keyboard_input(&k,PP_DOWN); assert(k.selected<pp_keyboard_count(k.page)); }
    k.selected=0;
    for(int i=0;i<32;++i) assert(pp_keyboard_input(&k,PP_OK)==PP_KB_EDIT);
    assert(pp_keyboard_input(&k,PP_OK)==PP_KB_FULL && strlen(k.value)==32);
    assert(pp_wifi_credentials_valid(k.value,"12345678",false));
    assert(pp_wifi_credentials_valid("Open","",true));
    assert(!pp_wifi_credentials_valid("Secure","",false));
    assert(!pp_wifi_credentials_valid("Secure","1234567",false));
    assert(!pp_wifi_credentials_valid("Open","12345678",true));
    assert(!pp_wifi_credentials_valid("","12345678",false));
    assert(!pp_wifi_credentials_valid("bad\nname","12345678",false));
    char long_ssid[34]; memset(long_ssid,'x',33); long_ssid[33]=0;
    assert(!pp_wifi_credentials_valid(long_ssid,"12345678",false));
    char full[65]; memset(full,'x',64); full[64]=0;
    assert(!pp_wifi_credentials_valid("SSID",full,false)); full[63]=0;
    assert(pp_wifi_credentials_valid("SSID",full,false));
    for(unsigned seed=1,round=0;round<50000;++round) {
        seed=seed*1664525U+1013904223U;
        pp_keyboard_input(&k,(pp_key_t)(seed%5));
        assert(strlen(k.value)<=32 && k.selected<pp_keyboard_count(k.page));
    }
}
static void screen(void)
{
    assert(pp_screen_off(true,true)==PP_SCREEN_RUNNING);
    assert(pp_screen_off(false,true)==PP_SCREEN_PAUSED);
    assert(pp_screen_off(true,false)==PP_SCREEN_PAUSED);
    assert(pp_screen_input(PP_SCREEN_RUNNING,false)==PP_SCREEN_PASS);
    assert(pp_screen_input(PP_SCREEN_RUNNING,true)==PP_SCREEN_MENU);
    assert(pp_screen_input(PP_SCREEN_PAUSED,false)==PP_SCREEN_WAKE);
    assert(pp_screen_input(PP_SCREEN_PAUSED,true)==PP_SCREEN_WAKE);
    pp_input_t in={0};
    assert(pp_input_event(&in,PP_OK,PP_PRESS)==PP_NONE);
    assert(pp_input_event(&in,PP_OK,PP_LONG)==PP_MENU);
    assert(pp_input_event(&in,PP_OK,PP_RELEASE)==PP_NONE);
    pp_settings_t settings; pp_settings_defaults(&settings);
    assert(settings.screen_mode==0); settings.screen_mode=255; pp_settings_validate(&settings);
    assert(settings.screen_mode==0);
}
int main(void)
{
    keyboard(); screen();
    puts("Passport extensions: PASS (all printable ASCII, bounded keyboard fuzz, credentials, paused/running screen and wake input)");
}
