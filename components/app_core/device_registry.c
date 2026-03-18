#include <string.h>

#include "board_profile.h"
#include "device_registry.h"

static app_device_config_t s_devices[APP_MAX_DEVICES];
static size_t s_device_count;

esp_err_t device_registry_init(void)
{
    const board_profile_t *profile = board_profile_get();

    memset(s_devices, 0, sizeof(s_devices));
    s_device_count = 0;

    if (profile->primary_switch.enabled && s_device_count < APP_MAX_DEVICES) {
        app_device_kind_t kind = (app_device_kind_t) profile->primary_switch.service_kind;
        bool supports_color = kind == APP_DEVICE_KIND_LIGHT
            && profile->primary_switch.output_driver == APP_OUTPUT_DRIVER_NEOPIXEL;
        bool supports_rotation_speed = kind == APP_DEVICE_KIND_FAN;

        s_devices[s_device_count++] = (app_device_config_t) {
            .id = profile->primary_switch.id,
            .name = profile->primary_switch.name,
            .kind = kind,
            .gpio = profile->primary_switch.gpio,
            .output_driver = profile->primary_switch.output_driver,
            .active_high = profile->primary_switch.active_high,
            .boot_on = profile->primary_switch.boot_on,
            .supports_on = true,
            .supports_brightness = supports_color,
            .supports_hue = supports_color,
            .supports_saturation = supports_color,
            .supports_rotation_speed = supports_rotation_speed,
        };
    }

    return ESP_OK;
}

size_t device_registry_count(void)
{
    return s_device_count;
}

const app_device_config_t *device_registry_get(size_t index)
{
    if (index >= s_device_count) {
        return NULL;
    }
    return &s_devices[index];
}

const app_device_config_t *device_registry_find(const char *device_id)
{
    size_t i;

    if (!device_id) {
        return NULL;
    }

    for (i = 0; i < s_device_count; i++) {
        if (!strcmp(s_devices[i].id, device_id)) {
            return &s_devices[i];
        }
    }
    return NULL;
}
