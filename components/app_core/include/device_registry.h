#pragma once

#include <stddef.h>

#include "device_model.h"
#include "esp_err.h"

esp_err_t device_registry_init(void);
size_t device_registry_count(void);
const app_device_config_t *device_registry_get(size_t index);
const app_device_config_t *device_registry_find(const char *device_id);
