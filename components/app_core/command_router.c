#include <esp_log.h>

#include "command_router.h"
#include "device_registry.h"
#include "drv_gpio_switch.h"
#include "state_store.h"

static const char *TAG = "command_router";

static drv_gpio_switch_config_t make_switch_config(const app_device_config_t *device)
{
    drv_gpio_switch_config_t config = {
        .gpio = device->gpio,
        .active_high = device->active_high,
        .boot_on = device->boot_on,
    };
    return config;
}

esp_err_t command_router_init(void)
{
    size_t i;

    for (i = 0; i < device_registry_count(); i++) {
        const app_device_config_t *device = device_registry_get(i);
        esp_err_t err;

        if (!device) {
            continue;
        }

        switch (device->kind) {
            case APP_DEVICE_KIND_SWITCH:
            case APP_DEVICE_KIND_LIGHT:
            case APP_DEVICE_KIND_OUTLET:
            {
                drv_gpio_switch_config_t driver_config = make_switch_config(device);
                err = drv_gpio_switch_init(&driver_config);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to init switch driver for %s", device->id);
                    return err;
                }
                err = command_router_sync_from_hardware(device->id, APP_STATE_SOURCE_BOOT);
                if (err != ESP_OK) {
                    return err;
                }
                break;
            }
            default:
                ESP_LOGW(TAG, "Device kind %d not wired yet for %s", device->kind, device->id);
                break;
        }
    }

    return ESP_OK;
}

esp_err_t command_router_apply_on(const char *device_id, bool on, app_state_source_t source)
{
    const app_device_config_t *device = device_registry_find(device_id);
    esp_err_t err;

    if (!device) {
        return ESP_ERR_NOT_FOUND;
    }

    switch (device->kind) {
        case APP_DEVICE_KIND_SWITCH:
        case APP_DEVICE_KIND_LIGHT:
        case APP_DEVICE_KIND_OUTLET:
        {
            drv_gpio_switch_config_t driver_config = make_switch_config(device);
            err = drv_gpio_switch_set_on(&driver_config, on);
            if (err != ESP_OK) {
                return err;
            }
            return state_store_set_on(device->id, on, source);
        }
        default:
            return ESP_ERR_NOT_SUPPORTED;
    }
}

esp_err_t command_router_sync_from_hardware(const char *device_id, app_state_source_t source)
{
    const app_device_config_t *device = device_registry_find(device_id);
    bool on = false;
    esp_err_t err;

    if (!device) {
        return ESP_ERR_NOT_FOUND;
    }

    switch (device->kind) {
        case APP_DEVICE_KIND_SWITCH:
        case APP_DEVICE_KIND_LIGHT:
        case APP_DEVICE_KIND_OUTLET:
        {
            drv_gpio_switch_config_t driver_config = make_switch_config(device);
            err = drv_gpio_switch_get_on(&driver_config, &on);
            if (err != ESP_OK) {
                return err;
            }
            return state_store_set_on(device->id, on, source);
        }
        default:
            return ESP_ERR_NOT_SUPPORTED;
    }
}

void command_router_sync_all_from_hardware(app_state_source_t source)
{
    size_t i;

    for (i = 0; i < device_registry_count(); i++) {
        const app_device_config_t *device = device_registry_get(i);

        if (!device) {
            continue;
        }
        command_router_sync_from_hardware(device->id, source);
    }
}
