#include "passport_radio.h"
#include "passport_keyboard.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

/* Preserve the v0.2 credential blob layout. */
typedef struct { int command; char ssid[33], password[64]; } credentials_t;
typedef struct { int command; uint32_t request_id; credentials_t credentials; } message_t;
enum { JOIN = 100 };
static QueueHandle_t s_queue;
static SemaphoreHandle_t s_lock;
static pp_radio_state_t s_state, s_snapshot;
static bool s_started, s_enabled, s_storage_ok;
static credentials_t s_saved, s_candidate;
static atomic_bool s_connected, s_disconnected, s_scan_done;
static int64_t s_retry_at, s_join_deadline;
static unsigned s_retries;
static nvs_handle_t s_nvs;

static void event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)data;
    if ((base==WIFI_EVENT && id==WIFI_EVENT_STA_DISCONNECTED) ||
        (base==IP_EVENT && id==IP_EVENT_STA_LOST_IP)) {
        atomic_store(&s_connected,false); atomic_store(&s_disconnected,true);
    }
    if (base==IP_EVENT && id==IP_EVENT_STA_GOT_IP) atomic_store(&s_connected,true);
    if (base==WIFI_EVENT && id==WIFI_EVENT_SCAN_DONE) atomic_store(&s_scan_done,true);
}
static esp_err_t initialize_once(void)
{
    esp_err_t e=esp_netif_init(); if(e!=ESP_OK) return e;
    e=esp_event_loop_create_default(); if(e!=ESP_OK && e!=ESP_ERR_INVALID_STATE) return e;
    if(!esp_netif_create_default_wifi_sta()) return ESP_ERR_NO_MEM;
    wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT(); cfg.nvs_enable=false;
    e=esp_wifi_init(&cfg); if(e!=ESP_OK) return e;
    e=esp_wifi_set_storage(WIFI_STORAGE_RAM); if(e!=ESP_OK) return e;
    e=esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,event_handler,NULL); if(e!=ESP_OK) return e;
    return esp_event_handler_register(IP_EVENT,ESP_EVENT_ANY_ID,event_handler,NULL);
}
static esp_err_t start_wifi(void)
{
    static bool attempted; static esp_err_t result=ESP_FAIL;
    if(!attempted) { attempted=true; result=initialize_once(); }
    if(result!=ESP_OK) return result;
    if(s_started) return ESP_OK;
    esp_err_t e=esp_wifi_set_mode(WIFI_MODE_STA);
    if(e==ESP_OK) e=esp_wifi_start();
    s_started=e==ESP_OK; return e;
}
static const credentials_t *target(void) { return s_state.join==PP_JOIN_WAIT ? &s_candidate : &s_saved; }
static esp_err_t connect_target(void)
{
    if(!s_enabled) return ESP_OK;
    esp_err_t e=start_wifi(); if(e!=ESP_OK) return e;
    const credentials_t *c=target();
    if(!c->ssid[0]) { s_state.wifi=PP_WIFI_EMPTY; return ESP_OK; }
    wifi_config_t cfg={0};
    memcpy(cfg.sta.ssid,c->ssid,strlen(c->ssid)); memcpy(cfg.sta.password,c->password,strlen(c->password));
    cfg.sta.threshold.authmode=c->password[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    cfg.sta.pmf_cfg.capable=true;
    e=esp_wifi_set_config(WIFI_IF_STA,&cfg); memset(&cfg,0,sizeof(cfg));
    if(e==ESP_OK) e=esp_wifi_connect();
    s_retry_at=esp_timer_get_time()+15000000;
    s_state.wifi=e==ESP_OK ? PP_WIFI_CONNECTING : PP_WIFI_ERROR;
    return e;
}
static void disconnect(void)
{
    if(s_started) esp_wifi_disconnect();
    atomic_store(&s_connected,false); atomic_store(&s_disconnected,false); s_state.ip[0]=0;
}
static void cancel_join(pp_join_state_t result)
{
    if(s_state.join!=PP_JOIN_WAIT) return;
    disconnect(); memset(&s_candidate,0,sizeof(s_candidate)); s_state.join=result;
    s_retry_at=esp_timer_get_time()+1000000;
    s_state.wifi=s_saved.ssid[0] ? PP_WIFI_CONNECTING : PP_WIFI_EMPTY;
}
static void publish(void)
{
    s_state.ble_on=pp_ble_advertising(); s_state.ble_error=pp_ble_failed();
    s_state.saved=s_saved.ssid[0]!=0;
    snprintf(s_state.saved_ssid,sizeof(s_state.saved_ssid),"%s",s_saved.ssid);
    xSemaphoreTake(s_lock,portMAX_DELAY); s_snapshot=s_state; xSemaphoreGive(s_lock);
}
static void collect_scan(void)
{
    wifi_ap_record_t aps[PP_WIFI_NETWORKS]; uint16_t count=PP_WIFI_NETWORKS;
    s_state.networks=0;
    esp_err_t e=esp_wifi_scan_get_ap_records(&count,aps);
    if(e==ESP_OK) for(unsigned i=0;i<count;++i) {
        if(!aps[i].ssid[0]) continue;
        bool duplicate=false;
        for(int j=0;j<s_state.networks;++j) if(!strcmp((char *)aps[i].ssid,s_state.nearby[j].ssid)) duplicate=true;
        if(duplicate) continue;
        pp_network_t *n=&s_state.nearby[s_state.networks++];
        memcpy(n->ssid,aps[i].ssid,32); n->ssid[32]=0; n->rssi=aps[i].rssi;
        n->open=aps[i].authmode==WIFI_AUTH_OPEN;
        n->supported=n->open || aps[i].authmode==WIFI_AUTH_WPA2_PSK || aps[i].authmode==WIFI_AUTH_WPA_WPA2_PSK ||
            aps[i].authmode==WIFI_AUTH_WPA3_PSK || aps[i].authmode==WIFI_AUTH_WPA2_WPA3_PSK;
    }
    esp_wifi_clear_ap_list(); s_state.scanning=false;
    if(e!=ESP_OK) s_state.error=e;
    s_retry_at=esp_timer_get_time()+1000000;
}
static void radio_worker(void *unused)
{
    (void)unused;
    s_storage_ok=nvs_open_from_partition("settings","pp_wifi",NVS_READWRITE,&s_nvs)==ESP_OK;
    if(s_storage_ok) {
        size_t size=sizeof(s_saved);
        if(nvs_get_blob(s_nvs,"credentials",&s_saved,&size)!=ESP_OK || size!=sizeof(s_saved) ||
            s_saved.ssid[32] || s_saved.password[63] || !pp_wifi_credentials_valid(s_saved.ssid,s_saved.password,!s_saved.password[0]))
            memset(&s_saved,0,sizeof(s_saved));
    }
    for(;;) {
        message_t m;
        if(xQueueReceive(s_queue,&m,pdMS_TO_TICKS(100))==pdTRUE) {
            esp_err_t e=ESP_OK;
            switch(m.command) {
            case PP_RADIO_WIFI_ON: s_enabled=true; s_retries=0; e=connect_target(); break;
            case PP_RADIO_WIFI_OFF:
                cancel_join(PP_JOIN_IDLE); s_enabled=false;
                if(s_started) { esp_wifi_scan_stop(); esp_wifi_clear_ap_list(); esp_wifi_stop(); }
                s_started=false; s_state.scanning=false; s_state.networks=0; s_state.wifi=PP_WIFI_OFF;
                atomic_store(&s_connected,false); s_state.ip[0]=0; break;
            case PP_RADIO_SCAN:
                s_state.scan_id=m.request_id;
                cancel_join(PP_JOIN_IDLE); s_enabled=true; e=start_wifi();
                if(e==ESP_OK && !s_state.scanning) {
                    if(!atomic_load(&s_connected)) { disconnect(); s_state.wifi=s_saved.ssid[0] ? PP_WIFI_CONNECTING : PP_WIFI_EMPTY; }
                    s_state.networks=0; atomic_store(&s_scan_done,false);
                    e=esp_wifi_scan_start(NULL,false); s_state.scanning=e==ESP_OK;
                } break;
            case PP_RADIO_CANCEL_JOIN: cancel_join(PP_JOIN_IDLE); break;
            case PP_RADIO_FORGET:
                cancel_join(PP_JOIN_IDLE);
                e=s_storage_ok ? nvs_erase_key(s_nvs,"credentials") : ESP_ERR_INVALID_STATE;
                if(e==ESP_ERR_NVS_NOT_FOUND) e=ESP_OK;
                if(e==ESP_OK) e=nvs_commit(s_nvs);
                if(e==ESP_OK) { disconnect(); memset(&s_saved,0,sizeof(s_saved)); s_state.wifi=s_enabled ? PP_WIFI_EMPTY : PP_WIFI_OFF; }
                break;
            case JOIN:
                cancel_join(PP_JOIN_IDLE); disconnect();
                if(s_state.scanning) { esp_wifi_scan_stop(); esp_wifi_clear_ap_list(); s_state.scanning=false; }
                s_candidate=m.credentials; s_state.join_id=m.request_id; s_state.join=PP_JOIN_WAIT;
                s_join_deadline=esp_timer_get_time()+25000000; s_enabled=true; s_retries=0;
                e=connect_target(); if(e!=ESP_OK) cancel_join(PP_JOIN_FAILED); break;
            case PP_RADIO_BLE_ON: e=pp_ble_enable(); break;
            case PP_RADIO_BLE_OFF: e=pp_ble_disable(); break;
            default: e=ESP_ERR_INVALID_ARG; break;
            }
            s_state.error=e; memset(&m,0,sizeof(m));
        }
        int64_t now=esp_timer_get_time();
        if(atomic_exchange(&s_scan_done,false) && s_state.scanning) collect_scan();
        if(s_enabled && atomic_load(&s_connected)) {
            wifi_ap_record_t ap;
            if(esp_wifi_sta_get_ap_info(&ap)==ESP_OK && !strncmp((char *)ap.ssid,target()->ssid,32)) {
                esp_netif_ip_info_t ip;
                if(esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"),&ip)==ESP_OK && ip.ip.addr) {
                    s_state.wifi=PP_WIFI_ONLINE; s_retries=0;
                    snprintf(s_state.ip,sizeof(s_state.ip),IPSTR,IP2STR(&ip.ip));
                    if(s_state.join==PP_JOIN_WAIT) {
                        esp_err_t e=s_storage_ok ? nvs_set_blob(s_nvs,"credentials",&s_candidate,sizeof(s_candidate)) : ESP_ERR_INVALID_STATE;
                        if(e==ESP_OK) e=nvs_commit(s_nvs);
                        if(e==ESP_OK) { s_saved=s_candidate; memset(&s_candidate,0,sizeof(s_candidate)); s_state.join=PP_JOIN_OK; s_state.error=0; }
                        else { s_state.error=e; cancel_join(PP_JOIN_FAILED); }
                    }
                }
            }
        } else if(s_enabled && target()->ssid[0] && !s_state.scanning) {
            if(atomic_exchange(&s_disconnected,false)) {
                s_state.wifi=PP_WIFI_CONNECTING; s_state.ip[0]=0;
                s_retry_at=now+((int64_t)1<<(s_retries<4?s_retries:4))*1000000; ++s_retries;
            }
            if(now>=s_retry_at) {
                esp_err_t e=connect_target(); if(e!=ESP_OK) s_state.error=e;
                if(e!=ESP_OK || s_retries>=5) s_state.wifi=PP_WIFI_ERROR;
            }
        }
        if(s_state.join==PP_JOIN_WAIT && now>=s_join_deadline) { s_state.error=ESP_ERR_TIMEOUT; cancel_join(PP_JOIN_FAILED); }
        publish();
    }
}
esp_err_t pp_radio_init(bool wifi,bool ble)
{
    s_lock=xSemaphoreCreateMutex(); s_queue=xQueueCreate(4,sizeof(message_t));
    if(!s_lock || !s_queue) return ESP_ERR_NO_MEM;
    if(xTaskCreate(radio_worker,"pp_radio",6144,NULL,3,NULL)!=pdPASS) return ESP_ERR_NO_MEM;
    if(wifi) pp_radio_request(PP_RADIO_WIFI_ON);
    if(ble) pp_radio_request(PP_RADIO_BLE_ON);
    return ESP_OK;
}
bool pp_radio_request(pp_radio_cmd_t cmd)
{
    message_t m={.command=cmd}; return s_queue && xQueueSend(s_queue,&m,0)==pdTRUE;
}
bool pp_radio_scan(uint32_t request_id)
{
    message_t m={.command=PP_RADIO_SCAN,.request_id=request_id};
    return s_queue && xQueueSend(s_queue,&m,0)==pdTRUE;
}
bool pp_radio_join(const char *ssid,const char *pass,bool open,uint32_t request_id)
{
    if(!pp_wifi_credentials_valid(ssid,pass,open) || !request_id) return false;
    message_t m={.command=JOIN,.request_id=request_id};
    memcpy(m.credentials.ssid,ssid,strlen(ssid)+1); memcpy(m.credentials.password,pass,strlen(pass)+1);
    bool ok=s_queue && xQueueSend(s_queue,&m,0)==pdTRUE; memset(&m,0,sizeof(m)); return ok;
}
void pp_radio_snapshot(pp_radio_state_t *out)
{
    if(!s_lock) { memset(out,0,sizeof(*out)); out->wifi=PP_WIFI_ERROR; return; }
    xSemaphoreTake(s_lock,portMAX_DELAY); *out=s_snapshot; xSemaphoreGive(s_lock);
}
