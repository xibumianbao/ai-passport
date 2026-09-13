#include "passport_storage.h"
#include "passport_metrics.h"
#include "esp_flash.h"
#include "esp_partition.h"
#include "nvs.h"
#include <string.h>
/* The generator populates this exact fixed-width object between two links. */
const pp_build_metrics_t pp_build_metrics=PP_METRICS_INIT;
void pp_capacity_read(pp_capacity_t *out)
{
    memset(out,0,sizeof(*out));
    if(esp_flash_get_size(NULL,&out->total)!=ESP_OK) return;
    /* Below 0x9000 contains bootloader, partition table and mandatory gaps. */
    out->allocated=0x9000;
    esp_partition_iterator_t it=esp_partition_find(ESP_PARTITION_TYPE_ANY,ESP_PARTITION_SUBTYPE_ANY,NULL);
    while(it) {
        const esp_partition_t *p=esp_partition_get(it); out->allocated+=p->size;
        if(!strcmp(p->label,"factory")) out->program_limit=p->size;
        it=esp_partition_next(it);
    }
    out->unassigned=out->total>out->allocated ? out->total-out->allocated : 0;
    nvs_stats_t stats;
    if(nvs_get_stats("settings",&stats)==ESP_OK) { out->settings_total=stats.total_entries; out->settings_free=stats.free_entries; }
}
