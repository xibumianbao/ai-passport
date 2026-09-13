#pragma once
#include <stdint.h>
#define PP_CAPACITY_APPS 16
typedef struct {
    uint32_t valid, image_bytes, app_flash[PP_CAPACITY_APPS], app_ram[PP_CAPACITY_APPS];
} pp_build_metrics_t;
extern const pp_build_metrics_t pp_build_metrics;
typedef struct { uint32_t total, allocated, unassigned, program_limit, settings_total, settings_free; } pp_capacity_t;
void pp_capacity_read(pp_capacity_t *out);
