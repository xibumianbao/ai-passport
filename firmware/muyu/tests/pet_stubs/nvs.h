#pragma once
#include <stddef.h>
#include <stdint.h>
typedef int esp_err_t;
typedef unsigned nvs_handle_t;
#define ESP_OK 0
#define ESP_ERR_NVS_NOT_FOUND 1
#define ESP_ERR_NVS_INVALID_LENGTH 2
#define NVS_READWRITE 1
esp_err_t nvs_open_from_partition(const char *partition,const char *ns,int mode,nvs_handle_t *handle);
esp_err_t nvs_get_blob(nvs_handle_t handle,const char *key,void *value,size_t *length);
esp_err_t nvs_set_blob(nvs_handle_t handle,const char *key,const void *value,size_t length);
esp_err_t nvs_commit(nvs_handle_t handle);
void nvs_close(nvs_handle_t handle);
