#include "passport_core.h"
#include "passport_ui.h"
#include "passport_radio.h"
#include "passport_audio.h"
#include "muyu_logic.h"
#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "esp_app_desc.h"
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

typedef enum { APP, ROOT, APPS, SETTINGS, WIFI, BLE, SOUND, DISPLAY, TIMEOUT,
               ABOUT, DIAGNOSTICS, SETUP, SCAN, FORGET } page_t;
typedef struct { unsigned key; pp_edge_t edge; } input_event_t;
static const char *TAG = "passport";
static QueueHandle_t s_input;
static atomic_bool s_input_lost, s_buttons_ok;
static bool s_battery_ok, s_nvs_ok, s_storage_error, s_radio_ok;
static pp_runtime_t s_runtime;
static pp_input_t s_input_state;
static pp_settings_t s_settings;
static page_t s_page = APPS;
static int s_selection;
static uint32_t s_count;
static bool s_strike, s_asleep, s_dirty_settings, s_boot_pending;
static int64_t s_last_key, s_save_at, s_stable_at;
static nvs_handle_t s_nvs;
static char s_notice[56];

static bool app_start(void *ctx) { (void)ctx; return true; }
static void app_stop(void *ctx) { (void)ctx; }
static void muyu_focus(void *ctx, bool focused)
{
    (void)ctx;
    if (focused && pp_audio_ok()) {
        if (pp_resource_acquire(&s_runtime, pp_audio_cancel, NULL) >= 0)
            pp_audio_begin(s_runtime.generation);
    }
}
static void muyu_key(void *ctx, pp_key_t key)
{
    (void)ctx; (void)key; s_count = muyu_count_add(s_count, 1); s_strike = true;
    pp_audio_knock(s_runtime.generation);
}
static const pp_app_t APPS_LIST[] = {
    {"muyu", "Muyu", "0.2.0", "AI Passport", app_start, app_stop, muyu_focus, muyu_key, NULL},
    {"device", "Device status", "0.2.0", "AI Passport", app_start, app_stop, NULL, NULL, NULL},
};
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
        uint8_t v;
        if (nvs_get_u8(s_nvs, "wifi", &v) == ESP_OK) s_settings.wifi = v != 0;
        if (nvs_get_u8(s_nvs, "ble", &v) == ESP_OK) s_settings.ble = v != 0;
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
    if (e == ESP_OK) e = nvs_set_u8(s_nvs, "wifi", s_settings.wifi);
    if (e == ESP_OK) e = nvs_set_u8(s_nvs, "ble", s_settings.ble);
    if (e == ESP_OK) e = nvs_commit(s_nvs);
    storage_result(e);
}
static void settings_changed(void)
{
    s_dirty_settings = true; s_save_at = esp_timer_get_time()+1000000;
    pp_audio_volume(s_settings.volume); bsp_display_backlight(s_settings.brightness);
}
static void change_page(page_t page) { s_page = page; s_selection = 0; s_notice[0] = 0; }
static void launch(int index)
{
    if (s_nvs_ok) { storage_result(nvs_set_u8(s_nvs, "pending", 1)); storage_result(nvs_commit(s_nvs)); }
    if (pp_activate(&s_runtime, index)) {
        change_page(APP); s_stable_at = esp_timer_get_time()+10000000; s_boot_pending = true;
        ESP_LOGI(TAG, "app=%s session=%lu", s_runtime.apps[index].id, (unsigned long)s_runtime.generation);
    } else { change_page(APPS); snprintf(s_notice, sizeof(s_notice), "App failed; choose another."); }
}
static void on_button(bsp_btn_t key, bsp_btn_ev_t event, void *ctx)
{
    (void)ctx; input_event_t item = {.key = (unsigned)key};
    if (event == BSP_BTN_PRESS) item.edge = PP_PRESS;
    else if (event == BSP_BTN_RELEASE) item.edge = PP_RELEASE;
    else if (event == BSP_BTN_LONG) item.edge = PP_LONG;
    else return;
    if (xQueueSend(s_input, &item, 0) != pdTRUE) atomic_store(&s_input_lost, true);
}
static int rows(void)
{
    switch (s_page) {
    case ROOT: return 4; case APPS: return sizeof(APPS_LIST)/sizeof(APPS_LIST[0]);
    case SETTINGS: return 6; case WIFI: return 4; case BLE: case SETUP: return 2;
    case FORGET: return 2; default: return 0;
    }
}
static void request_radio(pp_radio_cmd_t command)
{
    if (!s_radio_ok || !pp_radio_request(command)) snprintf(s_notice, sizeof(s_notice), "Radio busy / unavailable");
}
static void back(void)
{
    switch (s_page) {
    case APP: pp_menu_open(&s_runtime); change_page(ROOT); break;
    case ROOT:
        if (s_runtime.active >= 0) { pp_resume(&s_runtime); change_page(APP); }
        else change_page(APPS);
        break;
    case APPS: case SETTINGS: case ABOUT: change_page(ROOT); break;
    case SETUP: case SCAN: case FORGET: change_page(WIFI); break;
    default: change_page(SETTINGS); break;
    }
}
static void input(pp_key_t key)
{
    if (key == PP_NONE) return;
    if (key == PP_MENU) { back(); return; }
    if (s_page == APP) { pp_dispatch(&s_runtime, key); return; }
    if (s_page == SOUND || s_page == DISPLAY || s_page == TIMEOUT) {
        if (key == PP_OK) { save_settings(); change_page(SETTINGS); return; }
        int delta = key == PP_UP ? 5 : -5;
        if (s_page == SOUND) {
            int v = s_settings.volume + delta; s_settings.volume = v < 0 ? 0 : v > 100 ? 100 : v;
        } else if (s_page == DISPLAY) {
            int v = s_settings.brightness + delta; s_settings.brightness = v < 10 ? 10 : v > 100 ? 100 : v;
        } else s_settings.timeout = (s_settings.timeout + (key == PP_UP ? 1 : 3)) % 4;
        settings_changed(); return;
    }
    int n = rows();
    if (key != PP_OK) { if (n) s_selection = (s_selection+n+(key == PP_UP ? -1 : 1)) % n; return; }
    switch (s_page) {
    case ROOT:
        if (s_selection == 0) { if (s_runtime.active >= 0) { pp_resume(&s_runtime); change_page(APP); } }
        else change_page(s_selection == 1 ? APPS : s_selection == 2 ? SETTINGS : ABOUT);
        break;
    case APPS: launch(s_selection); break;
    case SETTINGS: {
        const page_t targets[] = {WIFI, BLE, SOUND, DISPLAY, TIMEOUT, DIAGNOSTICS}; change_page(targets[s_selection]); break;
    }
    case WIFI:
        if (s_selection == 0) {
            s_settings.wifi = !s_settings.wifi; request_radio(s_settings.wifi ? PP_RADIO_WIFI_ON : PP_RADIO_WIFI_OFF); settings_changed();
        } else if (s_selection == 1) {
            s_settings.wifi = true; settings_changed(); request_radio(PP_RADIO_SETUP); change_page(SETUP);
        } else if (s_selection == 2) { request_radio(PP_RADIO_SCAN); change_page(SCAN); }
        else change_page(FORGET);
        break;
    case BLE:
        if (s_selection == 0) { s_settings.ble = !s_settings.ble; request_radio(s_settings.ble ? PP_RADIO_BLE_ON : PP_RADIO_BLE_OFF); settings_changed(); }
        else back();
        break;
    case SETUP: if (s_selection == 0) request_radio(PP_RADIO_SETUP_STOP); back(); break;
    case FORGET: if (s_selection == 1) request_radio(PP_RADIO_FORGET); change_page(WIFI); break;
    default: back(); break;
    }
}
static void build_view(pp_view_t *v, const pp_radio_state_t *radio, int battery)
{
    memset(v, 0, sizeof(*v)); v->menu = s_page != APP; v->muyu = s_runtime.active == 0;
    v->audio_ok = pp_audio_ok(); v->buttons_ok = atomic_load(&s_buttons_ok); v->battery = battery;
    v->count = s_count; v->selected = s_selection; v->row_count = rows();
    snprintf(v->app, sizeof(v->app), "%s", s_runtime.active < 0 ? "PASSPORT" : s_runtime.apps[s_runtime.active].name);
    const char *wifi[] = {"OFF", "NOT SET", "CONNECTING", "ONLINE", "RETRY / ERROR"};
    snprintf(v->status, sizeof(v->status), "Wi-Fi %s | BLE %s", wifi[radio->wifi], radio->ble_on ? "ON" : "OFF");
    switch (s_page) {
    case APP:
        snprintf(v->detail, sizeof(v->detail), "Wi-Fi: %s\nIP: %s\nBLE beacon: %s\nVolume: %u%%\nBrightness: %u%%\nFree heap: %lu KB\nPassport %s", wifi[radio->wifi],
                 radio->ip[0] ? radio->ip : "--", radio->ble_on ? "ON" : "OFF", s_settings.volume, s_settings.brightness,
                 (unsigned long)(esp_get_free_heap_size()/1024), PP_VERSION); break;
    case ROOT:
        strcpy(v->title, "SYSTEM"); strcpy(v->rows[0], s_runtime.active < 0 ? "No active app" : "Resume app");
        strcpy(v->rows[1], "Applications"); strcpy(v->rows[2], "Settings"); strcpy(v->rows[3], "About");
        strcpy(v->detail, "One device. Your apps."); break;
    case APPS:
        strcpy(v->title, "APPLICATIONS");
        for (size_t i = 0; i < s_runtime.count; ++i)
            snprintf(v->rows[i], sizeof(v->rows[i]), "%s%s", s_runtime.active == (int)i ? "* " : "", s_runtime.apps[i].name);
        strcpy(v->detail, "Choose an app.\nLast app opens on boot."); break;
    case SETTINGS: {
        strcpy(v->title, "SETTINGS"); const char *labels[] = {"Wi-Fi", "Bluetooth LE", "Sound", "Display", "Screen timeout", "Diagnostics"};
        for (int i=0; i<6; ++i) strcpy(v->rows[i], labels[i]);
        break;
    }
    case WIFI:
        strcpy(v->title, "WI-FI"); snprintf(v->rows[0], 40, "Wi-Fi: %s", s_settings.wifi ? "ON" : "OFF");
        strcpy(v->rows[1], "Set up with phone"); strcpy(v->rows[2], "Scan nearby"); strcpy(v->rows[3], "Forget saved network");
        snprintf(v->detail, sizeof(v->detail), "%s\nIP: %s", wifi[radio->wifi], radio->ip[0] ? radio->ip : "--"); break;
    case BLE:
        strcpy(v->title, "BLUETOOTH LE"); snprintf(v->rows[0], 40, "Beacon: %s", s_settings.ble ? "ON" : "OFF"); strcpy(v->rows[1], "Back");
        snprintf(v->detail, sizeof(v->detail), "Name: Passport\n%s\n\nBroadcast only.\nNo pairing / audio.", radio->ble_error ? "BLE ERROR" : radio->ble_on ? "ADVERTISING" : s_settings.ble ? "STARTING" : "OFF"); break;
    case SOUND: case DISPLAY: case TIMEOUT:
        strcpy(v->title, s_page == SOUND ? "SOUND" : s_page == DISPLAY ? "DISPLAY" : "SCREEN TIMEOUT");
        if (s_page == TIMEOUT) snprintf(v->detail, sizeof(v->detail), "%u seconds\n0 = always on\n\nUP / DOWN: adjust\nOK: save and return", pp_timeout_seconds(s_settings.timeout));
        else snprintf(v->detail, sizeof(v->detail), "%u%%\n\nUP / DOWN: adjust\nOK: save and return", s_page == SOUND ? s_settings.volume : s_settings.brightness);
        break;
    case SETUP:
        strcpy(v->title, "WI-FI SETUP"); strcpy(v->rows[0], "Stop setup"); strcpy(v->rows[1], "Back (keep setup)");
        if (radio->portal) snprintf(v->detail, sizeof(v->detail), "Join:\n%s\nPassword:\n%s\nOpen 192.168.4.1\nCloses after 5 min.", radio->ap_name, radio->ap_password);
        else snprintf(v->detail, sizeof(v->detail), "%s", radio->error ? "Setup failed.\nReturn and retry." : "Starting / closed.\nReturn to Wi-Fi to start.");
        break;
    case SCAN:
        strcpy(v->title, "NEARBY WI-FI");
        if (radio->scanning) strcpy(v->detail, "Scanning...");
        else if (radio->networks) {
            v->row_count = radio->networks; v->selected = -1;
            for (int i=0; i<radio->networks; ++i) snprintf(v->rows[i], 40, "%s", radio->nearby[i]);
            strcpy(v->detail, "Use phone setup to join.\nOK: back");
        } else strcpy(v->detail, "No scan results.\nTurn Wi-Fi on; wait for\nconnection attempts.\nOK: back");
        break;
    case FORGET:
        strcpy(v->title, "FORGET WI-FI?"); strcpy(v->rows[0], "Cancel"); strcpy(v->rows[1], "Forget network");
        strcpy(v->detail, "Removes only saved\nWi-Fi credentials."); break;
    case DIAGNOSTICS:
        strcpy(v->title, "DIAGNOSTICS");
        snprintf(v->detail, sizeof(v->detail), "Audio: %s\nButtons: %s\nStorage: %s\nRadio error: %d\nHeap: %lu KB\nMinimum: %lu KB\n\nOK: back",
                 pp_audio_ok() ? "OK" : "ERROR", v->buttons_ok ? "OK" : "ERROR", s_storage_error ? "ERROR" : "OK", radio->error,
                 (unsigned long)(esp_get_free_heap_size()/1024), (unsigned long)(esp_get_minimum_free_heap_size()/1024)); break;
    case ABOUT:
        strcpy(v->title, "PASSPORT");
        snprintf(v->detail, sizeof(v->detail), "Platform %s\nESP32-C3 / 8 MB\nESP-IDF %s\n\n2 built-in apps\nBy AI Passport\n\nOK: back", PP_VERSION, esp_get_idf_version()); break;
    }
    if (s_notice[0]) snprintf(v->detail, sizeof(v->detail), "%s", s_notice);
}
static void control_worker(void *arg)
{
    (void)arg; int battery = -1; int64_t battery_at = 0, render_at = 0;
    s_last_key = esp_timer_get_time();
    for (;;) {
        input_event_t e;
        if (xQueueReceive(s_input, &e, pdMS_TO_TICKS(20)) == pdTRUE) {
            int64_t now = esp_timer_get_time();
            if (atomic_exchange(&s_input_lost, false)) {
                memset(&s_input_state, 0, sizeof(s_input_state)); pp_menu_open(&s_runtime); change_page(ROOT);
                snprintf(s_notice, sizeof(s_notice), "Input busy; press again.");
            }
            pp_key_t key = pp_input_event(&s_input_state, e.key, e.edge);
            if (s_asleep) {
                s_asleep = false; pp_input_cancel(&s_input_state);
                bsp_display_backlight(s_settings.brightness);
                if (s_page == APP) pp_resume(&s_runtime);
            } else input(key);
            s_last_key = now; render_at = 0;
        }
        int64_t now = esp_timer_get_time();
        if (s_dirty_settings && now >= s_save_at) save_settings();
        if (s_boot_pending && now >= s_stable_at) {
            s_boot_pending = false;
            if (s_nvs_ok && s_runtime.active >= 0) {
                esp_err_t result = nvs_set_str(s_nvs, "last_app", s_runtime.apps[s_runtime.active].id);
                if (result == ESP_OK) result = nvs_set_u8(s_nvs, "schema", 1);
                if (result == ESP_OK) result = nvs_set_u8(s_nvs, "pending", 0);
                if (result == ESP_OK) result = nvs_set_u8(s_nvs, "failures", 0);
                if (result == ESP_OK) result = nvs_commit(s_nvs);
                storage_result(result);
            }
        }
        pp_radio_state_t radio; pp_radio_snapshot(&radio);
        unsigned timeout = pp_timeout_seconds(s_settings.timeout);
        if (!s_asleep && timeout && !radio.portal && now-s_last_key >= (int64_t)timeout*1000000) {
            pp_menu_open(&s_runtime); s_asleep = true; bsp_display_backlight(0);
        }
        if (now >= battery_at) { battery = s_battery_ok ? bsp_battery_soc() : -1; battery_at = now+30000000; }
        if (!s_asleep && now >= render_at && bsp_lvgl_lock(20)) {
            pp_view_t view; build_view(&view, &radio, battery); pp_ui_render(&view, s_strike);
            bsp_lvgl_unlock(); s_strike = false; render_at = now+200000;
        }
    }
}
void app_main(void)
{
    ESP_LOGI(TAG, "Passport %s; hold OK %d ms for system menu", PP_VERSION, PP_LONG_PRESS_MS);
    char last[24]; bool safe;
    load_settings(last, &safe);
    pp_runtime_init(&s_runtime, APPS_LIST, sizeof(APPS_LIST)/sizeof(APPS_LIST[0]));
    if (bsp_i2c_init() != ESP_OK || bsp_display_init() != ESP_OK || !bsp_lvgl_init()) { ESP_LOGE(TAG, "display init failed"); return; }
    bsp_display_backlight(s_settings.brightness);
    if (!bsp_lvgl_lock(1000)) return;
    pp_ui_create(); bsp_lvgl_unlock();
    pp_audio_init(s_settings.volume); s_battery_ok = bsp_battery_init() == ESP_OK;
    s_radio_ok = pp_radio_init(s_settings.wifi, s_settings.ble) == ESP_OK;
    int last_index = pp_find_app(&s_runtime, last);
    if (!safe && last_index >= 0) launch(last_index);
    else if (safe) snprintf(s_notice, sizeof(s_notice), "Safe start. Choose an app.");
    s_input = xQueueCreate(32, sizeof(input_event_t));
    if (!s_input || xTaskCreate(control_worker, "pp_control", 6144, NULL, 3, NULL) != pdPASS) {
        ESP_LOGE(TAG, "control init failed"); return;
    }
    atomic_store(&s_buttons_ok, bsp_button_init(on_button, NULL) == ESP_OK);
    ESP_LOGI(TAG, "ready: buttons=%d audio=%d storage=%d radio=%d", atomic_load(&s_buttons_ok), pp_audio_ok(), s_nvs_ok, s_radio_ok);
}
