#include "pp_pet_store.h"
#include "nvs.h"
#include "esp_random.h"
#include <string.h>
static nvs_handle_t handle;
static bool opened;
static int active_slot;
bool pp_pet_store_write(void *unused, const uint8_t bytes[PP_PET_SAVE_SIZE])
{
    (void)unused;
    if (!opened) return false;
    int next=1-active_slot; const char *key=next?"save_b":"save_a";
    esp_err_t err=nvs_set_blob(handle,key,bytes,PP_PET_SAVE_SIZE);
    if (err==ESP_OK) err=nvs_commit(handle);
    uint8_t check[PP_PET_SAVE_SIZE]; size_t n=sizeof(check);
    if (err==ESP_OK) err=nvs_get_blob(handle,key,check,&n);
    if (err!=ESP_OK || n!=PP_PET_SAVE_SIZE || memcmp(check,bytes,n)) return false;
    active_slot=next; return true;
}
bool pp_pet_store_open(pp_pet_save_t *save)
{
    if (nvs_open_from_partition("settings","app_pet",NVS_READWRITE,&handle)!=ESP_OK) return false;
    opened=true; uint8_t a[PP_PET_SAVE_SIZE],b[PP_PET_SAVE_SIZE]; size_t na=sizeof(a),nb=sizeof(b);
    esp_err_t ea=nvs_get_blob(handle,"save_a",a,&na),eb=nvs_get_blob(handle,"save_b",b,&nb);
    active_slot=pp_pet_choose_save(save,a,ea==ESP_OK?na:0,b,eb==ESP_OK?nb:0);
    if (active_slot>=0) return true;
    /* Missing is a first run; malformed/newer-version records are not empty. */
    if (ea!=ESP_ERR_NVS_NOT_FOUND || eb!=ESP_ERR_NVS_NOT_FOUND) { pp_pet_store_close(); return false; }
    pp_pet_defaults(save,esp_random()); save->sequence=1;
    uint8_t initial[PP_PET_SAVE_SIZE]; pp_pet_encode(save,initial); active_slot=1;
    if (pp_pet_store_write(NULL,initial)) return true;
    pp_pet_store_close(); return false;
}
void pp_pet_store_close(void) { if(opened) nvs_close(handle); opened=false; }
