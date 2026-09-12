#include "passport_radio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include <stdatomic.h>
#include <string.h>
static SemaphoreHandle_t s_stopped;
static atomic_bool s_requested, s_advertising, s_failed;
static bool s_initialized;
static uint8_t s_address;
static int advertise(void)
{
    struct ble_hs_adv_fields f = {0};
    const char *name = "Passport";
    f.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    f.name = (const uint8_t *)name; f.name_len = strlen(name); f.name_is_complete = 1;
    int rc = ble_gap_adv_set_fields(&f);
    if (!rc) {
        struct ble_gap_adv_params p = {0};
        p.conn_mode = BLE_GAP_CONN_MODE_NON; p.disc_mode = BLE_GAP_DISC_MODE_GEN;
        rc = ble_gap_adv_start(s_address, NULL, BLE_HS_FOREVER, &p, NULL, NULL);
    }
    atomic_store(&s_advertising, rc == 0); atomic_store(&s_failed, rc != 0); return rc;
}
static void sync_host(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (!rc) rc = ble_hs_id_infer_auto(0, &s_address);
    if (!rc && atomic_load(&s_requested)) rc = advertise();
    if (rc) atomic_store(&s_failed, true);
}
static void reset_host(int reason)
{
    (void)reason; atomic_store(&s_advertising, false); atomic_store(&s_failed, true);
}
static void host_task(void *arg)
{
    (void)arg; nimble_port_run();
    xSemaphoreGive(s_stopped);
    nimble_port_freertos_deinit();
}
esp_err_t pp_ble_enable(void)
{
    if (s_initialized) return atomic_load(&s_requested) ? ESP_OK : ESP_ERR_INVALID_STATE;
    if (!s_stopped) s_stopped = xSemaphoreCreateBinary();
    if (!s_stopped) return ESP_ERR_NO_MEM;
    while (xSemaphoreTake(s_stopped, 0) == pdTRUE) {}
    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) { atomic_store(&s_failed, true); return err; }
    s_initialized = true;
    ble_svc_gap_init(); ble_svc_gatt_init(); ble_svc_gap_device_name_set("Passport");
    ble_hs_cfg.sync_cb = sync_host; ble_hs_cfg.reset_cb = reset_host;
    atomic_store(&s_requested, true); atomic_store(&s_failed, false);
    nimble_port_freertos_init(host_task); return ESP_OK;
}
esp_err_t pp_ble_disable(void)
{
    if (!s_initialized) return ESP_OK;
    atomic_store(&s_requested, false);
    ble_gap_adv_stop(); atomic_store(&s_advertising, false);
    if (nimble_port_stop() || xSemaphoreTake(s_stopped, pdMS_TO_TICKS(5000)) != pdTRUE) {
        atomic_store(&s_failed, true); return ESP_FAIL;
    }
    esp_err_t err = nimble_port_deinit();
    if (err == ESP_OK) s_initialized = false;
    atomic_store(&s_failed, err != ESP_OK); return err;
}
bool pp_ble_advertising(void) { return atomic_load(&s_advertising); }
bool pp_ble_failed(void) { return atomic_load(&s_failed); }
