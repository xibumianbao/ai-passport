#pragma once
#include <stdbool.h>
#include "esp_err.h"
typedef enum { PP_WIFI_OFF, PP_WIFI_EMPTY, PP_WIFI_CONNECTING, PP_WIFI_ONLINE, PP_WIFI_ERROR } pp_wifi_state_t;
typedef enum { PP_RADIO_WIFI_ON, PP_RADIO_WIFI_OFF, PP_RADIO_SETUP, PP_RADIO_SETUP_STOP,
               PP_RADIO_SCAN, PP_RADIO_FORGET, PP_RADIO_BLE_ON, PP_RADIO_BLE_OFF } pp_radio_cmd_t;
typedef struct {
    pp_wifi_state_t wifi;
    bool portal, ble_on, ble_error, scanning;
    int error, networks;
    char ip[16], ap_name[24], ap_password[13], nearby[4][40];
} pp_radio_state_t;
esp_err_t pp_radio_init(bool wifi_on, bool ble_on);
bool pp_radio_request(pp_radio_cmd_t command);
void pp_radio_snapshot(pp_radio_state_t *state);
/* BLE functions are called only by the radio worker. */
esp_err_t pp_ble_enable(void);
esp_err_t pp_ble_disable(void);
bool pp_ble_advertising(void);
bool pp_ble_failed(void);
