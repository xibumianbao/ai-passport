#include "passport_core.h"
#include "passport_ui.h"
#include "passport_radio.h"
#include "passport_audio.h"
#include "passport_catalog.h"
#include "passport_keyboard.h"
#include "passport_screen.h"
#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

typedef enum { APP, ROOT, APPS, SETTINGS, WIFI, SOUND, DISPLAY, TIMEOUT, SCREEN_MODE,
               SCAN, PASSWORD, SSID, NETWORK_TYPE, CONNECTING, FORGET } page_t;
typedef struct { unsigned key; pp_edge_t edge; } input_event_t;
static const char *TAG="passport";
static QueueHandle_t s_input;
static atomic_bool s_input_lost,s_buttons_ok;
static bool s_battery_ok,s_nvs_ok,s_storage_error,s_radio_ok;
static pp_runtime_t s_runtime;
static pp_input_t s_input_state;
static pp_settings_t s_settings;
static page_t s_page=APPS;
static int s_selection,s_network_count;
static pp_network_t s_networks[PP_WIFI_NETWORKS];
static pp_keyboard_t s_keyboard;
static char s_join_ssid[33];
static bool s_join_open,s_dirty_settings,s_boot_pending,s_scan_waiting;
static pp_screen_state_t s_screen=PP_SCREEN_ON;
static uint32_t s_request_id;
static int64_t s_last_key,s_save_at,s_stable_at;
static nvs_handle_t s_nvs;
static char s_notice[80];
pp_runtime_t *pp_app_runtime(void) { return &s_runtime; }

static void storage_result(esp_err_t result)
{
    if (result != ESP_OK) { s_storage_error = true; ESP_LOGE(TAG, "settings storage error: %s", esp_err_to_name(result)); }
}
static void load_settings(char last[24], bool *safe)
{
    pp_settings_defaults(&s_settings); *safe = false; last[0] = 0;
    /* Never erase NVS automatically: card identity and other namespaces are not ours. */
    nvs_flash_init(); /* Optional default driver cache; never erase on failure. */
    esp_err_t err = nvs_flash_init_partition("settings");
    if (err != ESP_OK || nvs_open_from_partition("settings", "passport", NVS_READWRITE, &s_nvs) != ESP_OK) {
        s_storage_error = true; return;
    }
    s_nvs_ok = true;
    uint8_t schema = 0; nvs_get_u8(s_nvs, "schema", &schema);
    if (schema == 1) {
        nvs_get_u8(s_nvs, "volume", &s_settings.volume);
        nvs_get_u8(s_nvs, "brightness", &s_settings.brightness);
        nvs_get_u8(s_nvs, "timeout", &s_settings.timeout);
        nvs_get_u8(s_nvs, "screen_mode", &s_settings.screen_mode);
        uint8_t v;
        if (nvs_get_u8(s_nvs, "wifi", &v) == ESP_OK) s_settings.wifi = v != 0;
        size_t len = 24; nvs_get_str(s_nvs, "last_app", last, &len);
    }
    pp_settings_validate(&s_settings);
    uint8_t pending = 0, failed = 0;
    nvs_get_u8(s_nvs, "pending", &pending); nvs_get_u8(s_nvs, "failures", &failed);
    *safe = pp_boot_safe(pending != 0, failed, &failed);
    storage_result(nvs_set_u8(s_nvs, "failures", failed));
    storage_result(nvs_set_u8(s_nvs, "pending", 0)); storage_result(nvs_commit(s_nvs));
}
static void save_settings(void)
{
    s_dirty_settings = false;
    if (!s_nvs_ok) return;
    esp_err_t e = nvs_set_u8(s_nvs, "schema", 1);
    if (e == ESP_OK) e = nvs_set_u8(s_nvs, "volume", s_settings.volume);
    if (e == ESP_OK) e = nvs_set_u8(s_nvs, "brightness", s_settings.brightness);
    if (e == ESP_OK) e = nvs_set_u8(s_nvs, "timeout", s_settings.timeout);
    if (e == ESP_OK) e = nvs_set_u8(s_nvs, "screen_mode", s_settings.screen_mode);
    if (e == ESP_OK) e = nvs_set_u8(s_nvs, "wifi", s_settings.wifi);
    if (e == ESP_OK) e = nvs_commit(s_nvs);
    storage_result(e);
}
static void settings_changed(void)
{
    s_dirty_settings = true; s_save_at = esp_timer_get_time()+1000000;
    pp_audio_volume(s_settings.volume); bsp_display_backlight(s_settings.brightness);
}
static void change_page(page_t page)
{
    if((s_page==PASSWORD || s_page==SSID) && page!=PASSWORD && page!=SSID && page!=CONNECTING)
        memset(&s_keyboard,0,sizeof(s_keyboard));
    s_page=page; s_selection=0; s_notice[0]=0;
}
static void launch(int index)
{
    if(s_nvs_ok) { storage_result(nvs_set_u8(s_nvs,"pending",1)); storage_result(nvs_commit(s_nvs)); }
    if(pp_activate(&s_runtime,index)) {
        change_page(APP); s_stable_at=esp_timer_get_time()+10000000; s_boot_pending=true;
        ESP_LOGI(TAG,"app=%s session=%lu",s_runtime.apps[index].id,(unsigned long)s_runtime.generation);
    } else { change_page(APPS); snprintf(s_notice,sizeof(s_notice),"App failed; choose another."); }
}
static void on_button(bsp_btn_t key,bsp_btn_ev_t event,void *ctx)
{
    (void)ctx; input_event_t item={.key=(unsigned)key};
    if(event==BSP_BTN_PRESS) item.edge=PP_PRESS;
    else if(event==BSP_BTN_RELEASE) item.edge=PP_RELEASE;
    else if(event==BSP_BTN_LONG) item.edge=PP_LONG;
    else return;
    if(xQueueSend(s_input,&item,0)!=pdTRUE) atomic_store(&s_input_lost,true);
}
static int rows(void)
{
    switch(s_page) {
    case ROOT: return 4;
    case APPS: return PP_APP_COUNT;
    case SETTINGS: return 5;
    case WIFI: return 3;
    case FORGET: case SCREEN_MODE: case NETWORK_TYPE: return 2;
    case SCAN: return s_scan_waiting ? 0 : s_network_count+2;
    default: return 0;
    }
}
static bool request_radio(pp_radio_cmd_t cmd)
{
    if(s_radio_ok && pp_radio_request(cmd)) return true;
    snprintf(s_notice,sizeof(s_notice),"Radio busy / unavailable"); return false;
}
static void start_scan(void)
{
    change_page(SCAN); s_settings.wifi=true; settings_changed();
    s_network_count=0; s_scan_waiting=true;
    if(++s_request_id==0) ++s_request_id;
    if(!s_radio_ok || !pp_radio_scan(s_request_id)) {
        s_scan_waiting=false; snprintf(s_notice,sizeof(s_notice),"Radio busy. Select Scan again.");
    }
}
static void connect_network(void)
{
    if(!pp_wifi_credentials_valid(s_join_ssid,s_keyboard.value,s_join_open)) {
        snprintf(s_notice,sizeof(s_notice),"Password needs 8-63 chars"); return;
    }
    if(++s_request_id==0) ++s_request_id;
    if(!pp_radio_join(s_join_ssid,s_keyboard.value,s_join_open,s_request_id)) {
        snprintf(s_notice,sizeof(s_notice),"Radio busy. Try GO again."); return;
    }
    s_settings.wifi=true; settings_changed(); change_page(CONNECTING);
}
static void back(void)
{
    switch(s_page) {
    case APP: pp_menu_open(&s_runtime); change_page(ROOT); break;
    case ROOT:
        if(s_runtime.active>=0) { pp_resume(&s_runtime); change_page(APP); } else change_page(APPS);
        break;
    case APPS: case SETTINGS: change_page(ROOT); break;
    case PASSWORD: case SSID: case NETWORK_TYPE: change_page(SCAN); break;
    case CONNECTING:
        request_radio(PP_RADIO_CANCEL_JOIN); memset(&s_keyboard,0,sizeof(s_keyboard)); change_page(WIFI); break;
    case SCAN: case FORGET: change_page(WIFI); break;
    default: change_page(SETTINGS); break;
    }
}
static void turn_screen_off(bool keep_running)
{
    s_screen=pp_screen_off(keep_running,s_page==APP);
    if(s_screen==PP_SCREEN_PAUSED) pp_menu_open(&s_runtime);
    bsp_display_backlight(0);
}
static void input(pp_key_t key)
{
    if(key==PP_NONE) return;
    if(s_page==PASSWORD || s_page==SSID) {
        pp_kb_result_t result=pp_keyboard_input(&s_keyboard,key); s_notice[0]=0;
        if(result==PP_KB_CANCEL) back();
        else if(result==PP_KB_FULL) snprintf(s_notice,sizeof(s_notice),"Input is full; DEL removes");
        else if(result==PP_KB_SUBMIT) {
            if(s_page==SSID) {
                if(!s_keyboard.value[0]) { strcpy(s_notice,"Enter a network name"); return; }
                snprintf(s_join_ssid,sizeof(s_join_ssid),"%.32s",s_keyboard.value);
                change_page(NETWORK_TYPE);
            } else connect_network();
        }
        return;
    }
    if(key==PP_MENU) { back(); return; }
    if(s_page==APP) { pp_dispatch(&s_runtime,key); return; }
    if(s_page==SOUND || s_page==DISPLAY || s_page==TIMEOUT) {
        if(key==PP_OK) { save_settings(); change_page(SETTINGS); return; }
        int delta=key==PP_UP ? 5 : -5;
        if(s_page==SOUND) { int v=s_settings.volume+delta; s_settings.volume=v<0?0:v>100?100:v; }
        else if(s_page==DISPLAY) { int v=s_settings.brightness+delta; s_settings.brightness=v<10?10:v>100?100:v; }
        else s_settings.timeout=(s_settings.timeout+(key==PP_UP?1:3))%4;
        settings_changed(); return;
    }
    int n=rows();
    if(key!=PP_OK) { if(n) s_selection=(s_selection+n+(key==PP_UP?-1:1))%n; return; }
    switch(s_page) {
    case ROOT:
        if(s_selection==0) { if(s_runtime.active>=0) { pp_resume(&s_runtime); change_page(APP); } }
        else if(s_selection==1) change_page(APPS);
        else if(s_selection==2) {
            if(s_runtime.active>=0) { pp_resume(&s_runtime); change_page(APP); turn_screen_off(true); }
            else strcpy(s_notice,"Open an app first.");
        } else change_page(SETTINGS);
        break;
    case APPS: launch(s_selection); break;
    case SETTINGS: {
        const page_t targets[]={WIFI,SOUND,DISPLAY,TIMEOUT,SCREEN_MODE};
        change_page(targets[s_selection]); break;
    }
    case WIFI:
        if(s_selection==0) start_scan();
        else if(s_selection==1) {
            bool enable=!s_settings.wifi;
            if(request_radio(enable?PP_RADIO_WIFI_ON:PP_RADIO_WIFI_OFF)) { s_settings.wifi=enable; settings_changed(); }
        } else change_page(FORGET);
        break;
    case SCAN:
        if(s_scan_waiting) break;
        if(s_selection<s_network_count) {
            pp_network_t net=s_networks[s_selection];
            if(!net.supported) { strcpy(s_notice,"WEP / Enterprise unsupported"); break; }
            memcpy(s_join_ssid,net.ssid,sizeof(s_join_ssid)); s_join_open=net.open;
            pp_keyboard_init(&s_keyboard,63); change_page(PASSWORD);
            if(net.open) connect_network();
        } else if(s_selection==s_network_count) start_scan();
        else { pp_keyboard_init(&s_keyboard,32); change_page(SSID); }
        break;
    case NETWORK_TYPE:
        s_join_open=s_selection==1; pp_keyboard_init(&s_keyboard,63); change_page(PASSWORD);
        if(s_join_open) connect_network();
        break;
    case CONNECTING: back(); break;
    case SCREEN_MODE: s_settings.screen_mode=s_selection; settings_changed(); save_settings(); change_page(SETTINGS); break;
    case FORGET: if(s_selection==1) request_radio(PP_RADIO_FORGET); change_page(WIFI); break;
    default: back(); break;
    }
}
static const char *wifi_label(pp_wifi_state_t state)
{
    const char *labels[]={"OFF","NOT SET","CONNECTING","ONLINE","RETRY / ERROR"};
    return state<=PP_WIFI_ERROR?labels[state]:"ERROR";
}
static void build_view(pp_view_t *v,const pp_radio_state_t *radio,int battery)
{
    memset(v,0,sizeof(*v)); v->menu=s_page!=APP;
    v->buttons_ok=atomic_load(&s_buttons_ok); v->battery=battery;
    v->module=s_runtime.active>=0?pp_modules[s_runtime.active]:NULL;
    int total=rows(),first=s_selection>=6?s_selection-5:0;
    v->row_count=total-first>6?6:total-first; v->selected=s_selection-first;
    snprintf(v->app,sizeof(v->app),"%s",s_runtime.active<0?"PASSPORT":s_runtime.apps[s_runtime.active].name);
    switch(radio->wifi) {
    case PP_WIFI_OFF: v->wifi=PP_LINK_OFF; break;
    case PP_WIFI_EMPTY: v->wifi=PP_LINK_UNCONFIGURED; break;
    case PP_WIFI_CONNECTING: v->wifi=PP_LINK_BUSY; break;
    case PP_WIFI_ONLINE: v->wifi=PP_LINK_ON; break;
    default: v->wifi=PP_LINK_ERROR; break;
    }
    const char *wifi=wifi_label(radio->wifi);
    switch(s_page) {
    case APP: break;
    case ROOT: {
        strcpy(v->title,"SYSTEM");
        const char *labels[]={s_runtime.active<0?"No active app":"Resume app","Applications","Screen off (run)","Settings"};
        for(int i=0;i<v->row_count;++i) strcpy(v->rows[i],labels[i+first]);
        break;
    }
    case APPS:
        strcpy(v->title,"APPLICATIONS");
        for(int i=0;i<v->row_count;++i) snprintf(v->rows[i],48,"%s%s",s_runtime.active==i+first?"* ":"",pp_apps[i+first].name);
        break;
    case SETTINGS: {
        strcpy(v->title,"SETTINGS");
        const char *labels[]={"Wi-Fi","Sound","Brightness","Screen timeout","Screen-off mode"};
        for(int i=0;i<v->row_count;++i) strcpy(v->rows[i],labels[i+first]);
        break;
    }
    case WIFI:
        strcpy(v->title,"WI-FI"); strcpy(v->rows[0],"Find and join network");
        snprintf(v->rows[1],48,"Wi-Fi: %s",s_settings.wifi?"ON":"OFF"); strcpy(v->rows[2],"Forget saved network");
        snprintf(v->detail,sizeof(v->detail),"%s\nSaved: %.32s\nIP: %s",wifi,radio->saved?radio->saved_ssid:"None",radio->ip[0]?radio->ip:"--"); break;
    case SCAN:
        strcpy(v->title,"NEARBY WI-FI");
        if(s_scan_waiting) strcpy(v->detail,"Searching 2.4 GHz...\nHold OK: back");
        else {
            for(int i=0;i<v->row_count;++i) {
                int index=first+i;
                if(index<s_network_count) snprintf(v->rows[i],48,"%s %s",s_networks[index].supported?(s_networks[index].open?"O":"*"):"?",s_networks[index].ssid);
                else strcpy(v->rows[i],index==s_network_count?"Scan again":"Hidden / manual name");
            }
            if(v->row_count<4) strcpy(v->detail,"O open  * password\nSelect to connect.");
        } break;
    case PASSWORD: case SSID:
        v->keyboard=true; v->keyboard_page=s_keyboard.page; v->keyboard_selected=s_keyboard.selected;
        v->keyboard_length=strlen(s_keyboard.value); v->keyboard_secret=s_page==PASSWORD;
        strcpy(v->title,s_page==PASSWORD?"WI-FI PASSWORD":"WI-FI NAME");
        snprintf(v->input_name,sizeof(v->input_name),"%s",s_page==PASSWORD?s_join_ssid:"Hidden network");
        if(s_page==SSID) memcpy(v->input_text,s_keyboard.value,sizeof(v->input_text));
        strcpy(v->detail,s_page==PASSWORD?"8-63 chars; select GO":"1-32 bytes; select GO");
        strcpy(v->footer,"UP/DOWN: key   OK: type"); break;
    case NETWORK_TYPE:
        strcpy(v->title,"NETWORK TYPE"); strcpy(v->rows[0],"WPA2 / WPA3 password"); strcpy(v->rows[1],"Open (no password)");
        snprintf(v->detail,sizeof(v->detail),"%s",s_join_ssid); break;
    case CONNECTING:
        strcpy(v->title,"CONNECTING");
        snprintf(v->detail,sizeof(v->detail),"%.32s\n\nConnecting, up to 25 s.\nSaved after success.\n\nOK: cancel",s_join_ssid); break;
    case SOUND: case DISPLAY: case TIMEOUT:
        strcpy(v->title,s_page==SOUND?"SOUND":s_page==DISPLAY?"BRIGHTNESS":"SCREEN TIMEOUT");
        if(s_page==TIMEOUT) snprintf(v->detail,sizeof(v->detail),"%u seconds\n0 = always on\n\nUP / DOWN: adjust\nOK: save and return",pp_timeout_seconds(s_settings.timeout));
        else snprintf(v->detail,sizeof(v->detail),"%u%%\n\nUP / DOWN: adjust\nOK: save and return",s_page==SOUND?s_settings.volume:s_settings.brightness);
        break;
    case SCREEN_MODE:
        strcpy(v->title,"SCREEN-OFF MODE");
        snprintf(v->rows[0],48,"%sPause app",s_settings.screen_mode==0?"* ":"");
        snprintf(v->rows[1],48,"%sKeep app running",s_settings.screen_mode==1?"* ":"");
        strcpy(v->detail,"Keep running: voice,\nnetwork and keys stay.\nHold OK: light + menu.\nNot deep sleep."); break;
    case FORGET:
        strcpy(v->title,"FORGET WI-FI?"); strcpy(v->rows[0],"Cancel"); strcpy(v->rows[1],"Forget network");
        strcpy(v->detail,"Removes only saved\nWi-Fi credentials."); break;

    }
    if(s_notice[0]) snprintf(v->detail,sizeof(v->detail),"%s",s_notice);
}
static void control_worker(void *unused)
{
    (void)unused; int battery=-1; int64_t battery_at=0,render_at=0,logic_at=esp_timer_get_time();
    s_last_key=esp_timer_get_time();
    for(;;) {
        input_event_t e;
        if(xQueueReceive(s_input,&e,pdMS_TO_TICKS(20))==pdTRUE) {
            if(atomic_exchange(&s_input_lost,false)) {
                memset(&s_input_state,0,sizeof(s_input_state)); pp_menu_open(&s_runtime); change_page(ROOT);
                s_screen=PP_SCREEN_ON; bsp_display_backlight(s_settings.brightness);
                snprintf(s_notice,sizeof(s_notice),"Input busy; press again.");
            }
            pp_key_t key=pp_input_event(&s_input_state,e.key,e.edge);
            pp_screen_action_t action=pp_screen_input(s_screen,key==PP_MENU);
            if(action==PP_SCREEN_WAKE) {
                s_screen=PP_SCREEN_ON; pp_input_cancel(&s_input_state); bsp_display_backlight(s_settings.brightness);
                if(s_page==APP) pp_resume(&s_runtime);
            } else if(action==PP_SCREEN_MENU) {
                s_screen=PP_SCREEN_ON; bsp_display_backlight(s_settings.brightness); input(PP_MENU);
            } else input(key);
            s_last_key=esp_timer_get_time(); render_at=0;
        }
        int64_t now=esp_timer_get_time();
        if(s_dirty_settings && now>=s_save_at) save_settings();
        if(s_boot_pending && now>=s_stable_at) {
            s_boot_pending=false;
            if(s_nvs_ok && s_runtime.active>=0) {
                esp_err_t result=nvs_set_str(s_nvs,"last_app",s_runtime.apps[s_runtime.active].id);
                if(result==ESP_OK) result=nvs_set_u8(s_nvs,"schema",1);
                if(result==ESP_OK) result=nvs_set_u8(s_nvs,"pending",0);
                if(result==ESP_OK) result=nvs_set_u8(s_nvs,"failures",0);
                if(result==ESP_OK) result=nvs_commit(s_nvs);
                storage_result(result);
            }
        }
        pp_radio_state_t radio; pp_radio_snapshot(&radio);
        if(s_page==SCAN && s_scan_waiting && radio.scan_id==s_request_id && !radio.scanning) {
            s_scan_waiting=false; s_network_count=radio.networks;
            memcpy(s_networks,radio.nearby,sizeof(s_networks)); s_selection=0;
        }
        if(s_page==CONNECTING && radio.join_id==s_request_id) {
            if(radio.join==PP_JOIN_OK) {
                memset(&s_keyboard,0,sizeof(s_keyboard)); change_page(WIFI); strcpy(s_notice,"Connected and saved.");
            } else if(radio.join==PP_JOIN_FAILED) {
                change_page(PASSWORD); strcpy(s_notice,"Failed. Check password / GO");
            }
        }
        unsigned timeout=pp_timeout_seconds(s_settings.timeout);
        bool editing=s_page==SSID || s_page==PASSWORD || s_page==CONNECTING || s_page==SCAN;
        if(s_screen==PP_SCREEN_ON && timeout && !editing && now-s_last_key>=(int64_t)timeout*1000000)
            turn_screen_off(s_settings.screen_mode==1);
        if(s_runtime.active>=0) {
            const pp_app_module_t *module=pp_modules[s_runtime.active];
            uint32_t elapsed=(uint32_t)((now-logic_at)/1000);
            if(module->tick) module->tick(module->ctx,elapsed>1000?1000:elapsed,
                s_screen==PP_SCREEN_ON && s_page==APP && s_runtime.focused);
        }
        logic_at=now;
        if(now>=battery_at) { battery=s_battery_ok?bsp_battery_soc():-1; battery_at=now+30000000; }
        if(s_screen==PP_SCREEN_ON && now>=render_at && bsp_lvgl_lock(20)) {
            pp_view_t v; build_view(&v,&radio,battery); pp_ui_render(&v);
            bsp_lvgl_unlock(); render_at=now+200000;
        }
    }
}
void app_main(void)
{
    ESP_LOGI(TAG,"Passport %s; hold OK %d ms for system menu",PP_VERSION,PP_LONG_PRESS_MS);
    char last[24]; bool safe; load_settings(last,&safe);
    pp_catalog_init(); pp_runtime_init(&s_runtime,pp_apps,PP_APP_COUNT);
    pp_keyboard_init(&s_keyboard,63);
    if(bsp_i2c_init()!=ESP_OK || bsp_display_init()!=ESP_OK || !bsp_lvgl_init()) { ESP_LOGE(TAG,"display init failed"); return; }
    bsp_display_backlight(s_settings.brightness);
    if(!bsp_lvgl_lock(1000)) return;
    pp_ui_create(); bsp_lvgl_unlock();
    pp_audio_init(s_settings.volume); s_battery_ok=bsp_battery_init()==ESP_OK;
    s_radio_ok=pp_radio_init(s_settings.wifi)==ESP_OK;
    int index=pp_find_app(&s_runtime,last);
    if(!safe && index>=0) launch(index);
    else if(safe) snprintf(s_notice,sizeof(s_notice),"Safe start. Choose an app.");
    s_input=xQueueCreate(32,sizeof(input_event_t));
    if(!s_input || xTaskCreate(control_worker,"pp_control",8192,NULL,3,NULL)!=pdPASS) { ESP_LOGE(TAG,"control init failed"); return; }
    atomic_store(&s_buttons_ok,bsp_button_init(on_button,NULL)==ESP_OK);
    ESP_LOGI(TAG,"ready: buttons=%d audio=%d storage=%d radio=%d",atomic_load(&s_buttons_ok),pp_audio_ok(),s_nvs_ok,s_radio_ok);
}
