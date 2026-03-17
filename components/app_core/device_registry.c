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
        s_devices[s_device_count++] = (app_device_config_t) {
            .id = profile->primary_switch.id,
            .name = profile->primary_switch.name,
            .kind = profile->primary_switch.homekit_lightbulb ? APP_DEVICE_KIND_LIGHT : APP_DEVICE_KIND_SWITCH,
            .gpio = profile->primary_switch.gpio,
            .active_high = profile->primary_switch.active_high,
            .boot_on = profile->primary_switch.boot_on,
            .supports_on = true,
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
