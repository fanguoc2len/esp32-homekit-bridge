#pragma once

#include "device_model.h"
#include "esp_err.h"

esp_err_t command_router_init(void);
esp_err_t command_router_apply_on(const char *device_id, bool on, app_state_source_t source);
esp_err_t command_router_apply_light_state(const char *device_id,
                                           const app_device_state_t *state,
                                           app_state_source_t source);
esp_err_t command_router_apply_fan_state(const char *device_id,
                                         const app_device_state_t *state,
                                         app_state_source_t source);
esp_err_t command_router_sync_from_hardware(const char *device_id, app_state_source_t source);
void command_router_sync_all_from_hardware(app_state_source_t source);
