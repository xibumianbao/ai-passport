#include "passport_radio.h"
#include "passport_core.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "nvs.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

typedef struct { int command; char ssid[33], password[64]; } radio_message_t;
enum { SAVE_CREDENTIALS = 100 };
static QueueHandle_t s_queue;
static SemaphoreHandle_t s_lock;
static pp_radio_state_t s_state;
static bool s_initialized, s_started, s_enabled;
static char s_ssid[33], s_password[64];
static httpd_handle_t s_server;
static atomic_bool s_connected, s_disconnected, s_scan_done;
static int64_t s_retry_at, s_portal_deadline;
static unsigned s_retries;
static nvs_handle_t s_nvs;
static bool s_storage_ok;
static void state_publish(void);

static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        atomic_store(&s_connected, false); atomic_store(&s_disconnected, true);
    }
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) atomic_store(&s_connected, true);
    if (base == IP_EVENT && id == IP_EVENT_STA_LOST_IP) {
        atomic_store(&s_connected, false); atomic_store(&s_disconnected, true);
    }
    if (base == WIFI_EVENT && id == WIFI_EVENT_SCAN_DONE) atomic_store(&s_scan_done, true);
}
static esp_err_t initialize_wifi_once(void)
{
    if (s_initialized) return ESP_OK;
    esp_err_t e = esp_netif_init(); if (e != ESP_OK) return e;
    e = esp_event_loop_create_default(); if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) return e;
    if (!esp_netif_create_default_wifi_sta() || !esp_netif_create_default_wifi_ap()) return ESP_ERR_NO_MEM;
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.nvs_enable = false;
    e = esp_wifi_init(&cfg); if (e != ESP_OK) return e;
    e = esp_wifi_set_storage(WIFI_STORAGE_RAM); if (e != ESP_OK) return e;
    e = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL);
    if (e != ESP_OK) return e;
    e = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL);
    if (e == ESP_OK) s_initialized = true;
    return e;
}
static esp_err_t initialize_wifi(void)
{
    /* A partially initialized driver/netif must not be created again on retry. */
    static bool attempted;
    static esp_err_t result = ESP_FAIL;
    if (!attempted) { attempted = true; result = initialize_wifi_once(); }
    return result;
}
static esp_err_t start_wifi(void)
{
    esp_err_t e = initialize_wifi(); if (e != ESP_OK) return e;
    e = esp_wifi_set_mode(s_state.portal ? WIFI_MODE_APSTA : WIFI_MODE_STA);
    if (e != ESP_OK) return e;
    if (!s_started) { e = esp_wifi_start(); s_started = e == ESP_OK; }
    return e;
}
static esp_err_t connect_saved(void)
{
    if (!s_enabled) return ESP_OK;
    esp_err_t e = start_wifi(); if (e != ESP_OK) return e;
    if (!s_ssid[0]) { s_state.wifi = PP_WIFI_EMPTY; return ESP_OK; }
    wifi_ap_record_t current_ap;
    if (!atomic_load(&s_connected) && esp_wifi_sta_get_ap_info(&current_ap) == ESP_OK)
        esp_wifi_disconnect(); /* Also recover a stuck/lost DHCP lease. */
    wifi_config_t cfg = {0};
    memcpy(cfg.sta.ssid, s_ssid, strlen(s_ssid));
    memcpy(cfg.sta.password, s_password, strlen(s_password));
    cfg.sta.threshold.authmode = s_password[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    cfg.sta.pmf_cfg.capable = true;
    e = esp_wifi_set_config(WIFI_IF_STA, &cfg);
    memset(&cfg, 0, sizeof(cfg));
    if (e == ESP_OK) e = esp_wifi_connect();
    s_state.wifi = e == ESP_OK ? PP_WIFI_CONNECTING : PP_WIFI_ERROR;
    s_retry_at = esp_timer_get_time() + 20000000;
    return e;
}
static bool local_portal(httpd_req_t *req)
{
    struct sockaddr_in local; socklen_t len = sizeof(local);
    if (getsockname(httpd_req_to_sockfd(req), (struct sockaddr *)&local, &len) != 0 ||
        local.sin_addr.s_addr != inet_addr("192.168.4.1")) return false;
    char host[40], origin[48];
    if (httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) != ESP_OK ||
        (strcmp(host, "192.168.4.1") && strcmp(host, "192.168.4.1:80"))) return false;
    if (httpd_req_get_hdr_value_len(req, "Origin") &&
        (httpd_req_get_hdr_value_str(req, "Origin", origin, sizeof(origin)) != ESP_OK ||
         (strcmp(origin, "http://192.168.4.1") && strcmp(origin, "http://192.168.4.1:80")))) return false;
    return true;
}
static esp_err_t portal_get(httpd_req_t *req)
{
    if (!local_portal(req)) return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Local setup only");
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "Content-Security-Policy", "default-src 'none'; style-src 'unsafe-inline'; form-action 'self'; frame-ancestors 'none'");
    return httpd_resp_sendstr(req,
        "<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
        "<title>Passport Wi-Fi</title><style>body{font:18px system-ui;max-width:420px;margin:40px auto;padding:20px;background:#f4f3ef}"
        "input,button{box-sizing:border-box;width:100%;padding:14px;margin:10px 0;font:inherit}button{background:#151515;color:white}</style>"
        "<h1>Passport Wi-Fi</h1><p>Connect your device to a 2.4 GHz network.</p>"
        "<form method=post action=/save><label>Wi-Fi name<input name=ssid maxlength=32 required autocomplete=off></label>"
        "<label>Password<input name=pass type=password maxlength=63 autocomplete=off></label>"
        "<button>Save and connect</button></form><p>Leave password empty only for an open network. Check the device for connection status.</p>");
}
static esp_err_t portal_post(httpd_req_t *req)
{
    if (!local_portal(req)) return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Local setup only");
    if (req->content_len == 0 || req->content_len > 384)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid form length");
    char body[385] = {0}; size_t used = 0;
    while (used < req->content_len) {
        int n = httpd_req_recv(req, body+used, req->content_len-used);
        if (n <= 0) { memset(body, 0, sizeof(body)); return ESP_FAIL; }
        used += (size_t)n;
    }
    radio_message_t message = {.command = SAVE_CREDENTIALS};
    bool valid = pp_parse_wifi_form(body, used, message.ssid, message.password);
    memset(body, 0, sizeof(body));
    if (!valid) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Use a 1-32 byte name and empty or 8-63 byte password");
    bool queued = xQueueSend(s_queue, &message, 0) == pdTRUE;
    memset(&message, 0, sizeof(message));
    if (!queued) return httpd_resp_send_err(req, HTTPD_503_SERVICE_UNAVAILABLE, "Busy; try again");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, "Connection requested. Check Passport for Wi-Fi status. The setup network closes after 5 minutes or when you choose Stop setup.");
}
static void stop_portal(void)
{
    if (s_server) { httpd_stop(s_server); s_server = NULL; }
    s_state.portal = false; memset(s_state.ap_password, 0, sizeof(s_state.ap_password));
    if (s_started) esp_wifi_set_mode(WIFI_MODE_STA);
}
static esp_err_t start_portal(void)
{
    if (s_state.portal) { s_portal_deadline = esp_timer_get_time()+300000000; return ESP_OK; }
    esp_err_t e = start_wifi(); if (e != ESP_OK) return e;
    wifi_config_t cfg = {0};
    snprintf(s_state.ap_name, sizeof(s_state.ap_name), "Passport-%04lX", (unsigned long)(esp_random() & 0xffff));
    snprintf(s_state.ap_password, sizeof(s_state.ap_password), "%08lX%04lX", (unsigned long)esp_random(), (unsigned long)(esp_random() & 0xffff));
    memcpy(cfg.ap.ssid, s_state.ap_name, strlen(s_state.ap_name));
    cfg.ap.ssid_len = strlen(s_state.ap_name);
    memcpy(cfg.ap.password, s_state.ap_password, strlen(s_state.ap_password));
    cfg.ap.authmode = WIFI_AUTH_WPA2_PSK; cfg.ap.max_connection = 2; cfg.ap.channel = 1;
    e = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (e == ESP_OK) e = esp_wifi_set_config(WIFI_IF_AP, &cfg);
    memset(&cfg, 0, sizeof(cfg));
    if (e == ESP_OK) {
        httpd_config_t hc = HTTPD_DEFAULT_CONFIG(); hc.stack_size = 4096;
        hc.max_open_sockets = 3; hc.lru_purge_enable = true; hc.recv_wait_timeout = 3;
        e = httpd_start(&s_server, &hc);
    }
    if (e == ESP_OK) {
        const httpd_uri_t get = {.uri="/", .method=HTTP_GET, .handler=portal_get};
        const httpd_uri_t post = {.uri="/save", .method=HTTP_POST, .handler=portal_post};
        e = httpd_register_uri_handler(s_server, &get);
        if (e == ESP_OK) e = httpd_register_uri_handler(s_server, &post);
    }
    if (e != ESP_OK) { stop_portal(); return e; }
    s_state.portal = true; s_portal_deadline = esp_timer_get_time()+300000000;
    return ESP_OK;
}
static void state_publish(void)
{
    s_state.ble_on = pp_ble_advertising(); s_state.ble_error = pp_ble_failed();
    /* Snapshot is a separate copy; readers never see partial strings. */
}
static pp_radio_state_t s_snapshot;
static void publish(void)
{
    state_publish(); xSemaphoreTake(s_lock, portMAX_DELAY); s_snapshot = s_state; xSemaphoreGive(s_lock);
}
static void radio_worker(void *arg)
{
    (void)arg;
    s_storage_ok = nvs_open_from_partition("settings", "pp_wifi", NVS_READWRITE, &s_nvs) == ESP_OK;
    if (s_storage_ok) {
        radio_message_t saved = {0}; size_t size = sizeof(saved);
        if (nvs_get_blob(s_nvs, "credentials", &saved, &size) == ESP_OK && size == sizeof(saved) &&
            saved.ssid[32] == 0 && saved.password[63] == 0) {
            memcpy(s_ssid, saved.ssid, sizeof(s_ssid)); memcpy(s_password, saved.password, sizeof(s_password));
        }
        memset(&saved, 0, sizeof(saved));
    }
    for (;;) {
        radio_message_t m;
        if (xQueueReceive(s_queue, &m, pdMS_TO_TICKS(100)) == pdTRUE) {
            esp_err_t e = ESP_OK;
            switch (m.command) {
            case PP_RADIO_WIFI_ON:
                s_enabled = true; s_retries = 0;
                if (!s_started) atomic_store(&s_connected, false);
                e = connect_saved(); break;
            case PP_RADIO_WIFI_OFF:
                s_enabled = false; stop_portal();
                if (s_started) { esp_wifi_scan_stop(); esp_wifi_clear_ap_list(); esp_wifi_stop(); }
                s_started = false; s_state.scanning = false; s_state.wifi = PP_WIFI_OFF;
                atomic_store(&s_connected, false); s_state.ip[0] = 0; break;
            case PP_RADIO_SETUP:
                s_enabled = true; e = start_portal();
                if (!s_ssid[0]) s_state.wifi = PP_WIFI_EMPTY;
                break;
            case PP_RADIO_SETUP_STOP: stop_portal(); break;
            case PP_RADIO_SCAN:
                if (s_started && !s_state.scanning && s_state.wifi != PP_WIFI_CONNECTING) {
                    e = esp_wifi_scan_start(NULL, false); s_state.scanning = e == ESP_OK;
                } else e = ESP_ERR_INVALID_STATE;
                break;
            case PP_RADIO_FORGET:
                if (!s_storage_ok) { e = ESP_ERR_INVALID_STATE; break; }
                e = nvs_erase_all(s_nvs); if (e == ESP_OK) e = nvs_commit(s_nvs);
                if (e == ESP_OK) {
                    memset(s_ssid, 0, sizeof(s_ssid)); memset(s_password, 0, sizeof(s_password));
                    if (s_started) esp_wifi_disconnect();
                    atomic_store(&s_connected, false); s_state.ip[0] = 0;
                    s_state.wifi = s_enabled ? PP_WIFI_EMPTY : PP_WIFI_OFF;
                } break;
            case SAVE_CREDENTIALS:
                if (!s_storage_ok) { e = ESP_ERR_INVALID_STATE; break; }
                e = nvs_set_blob(s_nvs, "credentials", &m, sizeof(m));
                if (e == ESP_OK) e = nvs_commit(s_nvs);
                if (e == ESP_OK) {
                    memcpy(s_ssid, m.ssid, sizeof(s_ssid)); memcpy(s_password, m.password, sizeof(s_password));
                    if (s_started) esp_wifi_disconnect();
                    atomic_store(&s_connected, false); s_retries = 0; s_enabled = true; e = connect_saved();
                } break;
            case PP_RADIO_BLE_ON: e = pp_ble_enable(); break;
            case PP_RADIO_BLE_OFF: e = pp_ble_disable(); break;
            default: e = ESP_ERR_INVALID_ARG; break;
            }
            s_state.error = e;
            if (e != ESP_OK && m.command != PP_RADIO_BLE_ON && m.command != PP_RADIO_BLE_OFF &&
                s_enabled && !atomic_load(&s_connected)) s_state.wifi = PP_WIFI_ERROR;
            memset(&m, 0, sizeof(m));
        }
        int64_t now = esp_timer_get_time();
        if (s_state.portal && now >= s_portal_deadline) stop_portal();
        if (atomic_exchange(&s_scan_done, false)) {
            wifi_ap_record_t aps[4]; uint16_t count = 4;
            if (esp_wifi_scan_get_ap_records(&count, aps) == ESP_OK) {
                s_state.networks = count;
                for (int i = 0; i < count; ++i)
                    snprintf(s_state.nearby[i], sizeof(s_state.nearby[i]), "%.28s %d", (char *)aps[i].ssid, aps[i].rssi);
            } else { esp_wifi_clear_ap_list(); s_state.networks = 0; }
            s_state.scanning = false;
        }
        if (s_enabled && atomic_load(&s_connected)) {
            s_state.wifi = PP_WIFI_ONLINE; s_retries = 0;
            esp_netif_ip_info_t info;
            if (esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &info) == ESP_OK)
                snprintf(s_state.ip, sizeof(s_state.ip), IPSTR, IP2STR(&info.ip));
        } else if (s_enabled && s_ssid[0] && !s_state.scanning) {
            if (atomic_exchange(&s_disconnected, false)) {
                s_state.wifi = PP_WIFI_CONNECTING; s_state.ip[0] = 0;
                unsigned seconds = 1U << (s_retries < 5 ? s_retries : 5);
                s_retry_at = now + (int64_t)seconds * 1000000; ++s_retries;
            }
            if (now >= s_retry_at) {
                if (s_retries >= 5) s_state.wifi = PP_WIFI_ERROR;
                esp_err_t e = connect_saved();
                if (e != ESP_OK) { s_state.error = e; s_state.wifi = PP_WIFI_ERROR; }
                if (s_retries >= 5) s_state.wifi = PP_WIFI_ERROR;
            }
        }
        publish();
    }
}
esp_err_t pp_radio_init(bool wifi_on, bool ble_on)
{
    s_lock = xSemaphoreCreateMutex(); s_queue = xQueueCreate(4, sizeof(radio_message_t));
    if (!s_lock || !s_queue) return ESP_ERR_NO_MEM;
    if (xTaskCreate(radio_worker, "pp_radio", 6144, NULL, 3, NULL) != pdPASS) return ESP_ERR_NO_MEM;
    if (wifi_on) pp_radio_request(PP_RADIO_WIFI_ON);
    if (ble_on) pp_radio_request(PP_RADIO_BLE_ON);
    return ESP_OK;
}
bool pp_radio_request(pp_radio_cmd_t cmd)
{
    radio_message_t m = {.command = cmd}; return s_queue && xQueueSend(s_queue, &m, 0) == pdTRUE;
}
void pp_radio_snapshot(pp_radio_state_t *out)
{
    if (!s_lock) { memset(out, 0, sizeof(*out)); out->wifi = PP_WIFI_ERROR; return; }
    xSemaphoreTake(s_lock, portMAX_DELAY); *out = s_snapshot; xSemaphoreGive(s_lock);
}
