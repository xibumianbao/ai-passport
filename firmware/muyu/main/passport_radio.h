#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#define PP_WIFI_NETWORKS 12
typedef enum { PP_WIFI_OFF, PP_WIFI_EMPTY, PP_WIFI_CONNECTING, PP_WIFI_ONLINE, PP_WIFI_ERROR } pp_wifi_state_t;
typedef enum { PP_RADIO_WIFI_ON, PP_RADIO_WIFI_OFF, PP_RADIO_SCAN, PP_RADIO_FORGET,
               PP_RADIO_CANCEL_JOIN } pp_radio_cmd_t;
typedef enum { PP_JOIN_IDLE, PP_JOIN_WAIT, PP_JOIN_OK, PP_JOIN_FAILED } pp_join_state_t;
typedef struct { char ssid[33]; int8_t rssi; bool open, supported; } pp_network_t;
typedef struct {
    pp_wifi_state_t wifi;
    bool scanning, saved;
    int error, networks;
    uint32_t join_id, scan_id;
    pp_join_state_t join;
    char ip[16], saved_ssid[33];
    pp_network_t nearby[PP_WIFI_NETWORKS];
} pp_radio_state_t;
esp_err_t pp_radio_init(bool wifi_on);
bool pp_radio_request(pp_radio_cmd_t command);
/* Queue a copy; save only after DHCP success. No password in a snapshot. */
bool pp_radio_join(const char *ssid, const char *password, bool open, uint32_t request_id);
bool pp_radio_scan(uint32_t request_id);
void pp_radio_snapshot(pp_radio_state_t *state);
