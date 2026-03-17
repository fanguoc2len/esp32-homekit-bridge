#pragma once

#include "device_model.h"
#include "esp_err.h"

typedef void (*state_store_observer_t)(const app_device_config_t *device,
                                       const app_device_state_t *state,
                                       app_state_source_t source,
                                       void *ctx);

esp_err_t state_store_init(void);
esp_err_t state_store_register_observer(state_store_observer_t observer, void *ctx);
esp_err_t state_store_set_state(const char *device_id,
                                const app_device_state_t *state,
                                app_state_source_t source);
esp_err_t state_store_set_on(const char *device_id, bool on, app_state_source_t source);
esp_err_t state_store_get(const char *device_id, app_device_state_t *state_out);
