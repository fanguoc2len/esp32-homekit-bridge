#include <math.h>

#include <esp_log.h>

#include "command_router.h"
#include "device_registry.h"
#include "drv_gpio_switch.h"
#include "drv_neopixel.h"
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

static drv_neopixel_config_t make_neopixel_config(const app_device_config_t *device)
{
    drv_neopixel_config_t config = {
        .gpio = device->gpio,
        .max_leds = 1,
        .pixel_index = 0,
        .boot_on = device->boot_on,
    };
    return config;
}

static uint8_t command_router_rgb_channel(float value)
{
    if (value <= 0.0f) {
        return 0;
    }
    if (value >= 255.0f) {
        return 255;
    }
    return (uint8_t) lroundf(value);
}

static void command_router_hsv_to_rgb(const app_device_state_t *state,
                                      uint8_t *red_out,
                                      uint8_t *green_out,
                                      uint8_t *blue_out)
{
    float hue;
    float saturation;
    float brightness;
    float chroma;
    float secondary;
    float match;
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;

    if (!state || !red_out || !green_out || !blue_out || !state->on || state->brightness <= 0) {
        if (red_out) {
            *red_out = 0;
        }
        if (green_out) {
            *green_out = 0;
        }
        if (blue_out) {
            *blue_out = 0;
        }
        return;
    }

    hue = fmodf(state->hue, 360.0f);
    if (hue < 0.0f) {
        hue += 360.0f;
    }
    saturation = state->saturation / 100.0f;
    if (saturation < 0.0f) {
        saturation = 0.0f;
    } else if (saturation > 1.0f) {
        saturation = 1.0f;
    }
    brightness = state->brightness / 100.0f;
    if (brightness < 0.0f) {
        brightness = 0.0f;
    } else if (brightness > 1.0f) {
        brightness = 1.0f;
    }

    chroma = brightness * saturation;
    secondary = chroma * (1.0f - fabsf(fmodf(hue / 60.0f, 2.0f) - 1.0f));
    match = brightness - chroma;

    if (hue < 60.0f) {
        red = chroma;
        green = secondary;
    } else if (hue < 120.0f) {
        red = secondary;
        green = chroma;
    } else if (hue < 180.0f) {
        green = chroma;
        blue = secondary;
    } else if (hue < 240.0f) {
        green = secondary;
        blue = chroma;
    } else if (hue < 300.0f) {
        red = secondary;
        blue = chroma;
    } else {
        red = chroma;
        blue = secondary;
    }

    *red_out = command_router_rgb_channel((red + match) * 255.0f);
    *green_out = command_router_rgb_channel((green + match) * 255.0f);
    *blue_out = command_router_rgb_channel((blue + match) * 255.0f);
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
                if (device->output_driver == APP_OUTPUT_DRIVER_NEOPIXEL) {
                    drv_neopixel_config_t driver_config = make_neopixel_config(device);

                    err = drv_neopixel_init(&driver_config);
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to init NeoPixel driver for %s", device->id);
                        return err;
                    }
                } else {
                    drv_gpio_switch_config_t driver_config = make_switch_config(device);

                    err = drv_gpio_switch_init(&driver_config);
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to init switch driver for %s", device->id);
                        return err;
                    }
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

esp_err_t command_router_apply_light_state(const char *device_id,
                                           const app_device_state_t *state,
                                           app_state_source_t source)
{
    const app_device_config_t *device = device_registry_find(device_id);

    if (!device || !state) {
        return device ? ESP_ERR_INVALID_ARG : ESP_ERR_NOT_FOUND;
    }

    switch (device->kind) {
        case APP_DEVICE_KIND_LIGHT:
        case APP_DEVICE_KIND_SWITCH:
        case APP_DEVICE_KIND_OUTLET:
            break;
        default:
            return ESP_ERR_NOT_SUPPORTED;
    }

    if (device->output_driver == APP_OUTPUT_DRIVER_NEOPIXEL) {
        drv_neopixel_config_t driver_config = make_neopixel_config(device);
        uint8_t red;
        uint8_t green;
        uint8_t blue;
        esp_err_t err;

        command_router_hsv_to_rgb(state, &red, &green, &blue);
        if (!state->on || state->brightness <= 0) {
            err = drv_neopixel_clear(&driver_config);
        } else {
            err = drv_neopixel_set_rgb(&driver_config, red, green, blue);
        }
        if (err != ESP_OK) {
            return err;
        }
        return state_store_set_state(device->id, state, source);
    }

    return command_router_apply_on(device->id, state->on, source);
}

esp_err_t command_router_apply_fan_state(const char *device_id,
                                         const app_device_state_t *state,
                                         app_state_source_t source)
{
    const app_device_config_t *device = device_registry_find(device_id);
    app_device_state_t next_state;
    drv_gpio_switch_config_t driver_config;
    esp_err_t err;

    if (!device || !state) {
        return device ? ESP_ERR_INVALID_ARG : ESP_ERR_NOT_FOUND;
    }
    if (device->kind != APP_DEVICE_KIND_FAN || !device->supports_rotation_speed) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (device->output_driver != APP_OUTPUT_DRIVER_GPIO_SWITCH) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    next_state = *state;
    if (next_state.rotation_speed < 0) {
        next_state.rotation_speed = 0;
    } else if (next_state.rotation_speed > 100) {
        next_state.rotation_speed = 100;
    }

    if (!next_state.on) {
        next_state.rotation_speed = 0;
    } else if (next_state.rotation_speed <= 0) {
        next_state.rotation_speed = 100;
    }
    next_state.on = (next_state.rotation_speed > 0);

    driver_config = make_switch_config(device);
    err = drv_gpio_switch_set_on(&driver_config, next_state.on);
    if (err != ESP_OK) {
        return err;
    }
    return state_store_set_state(device->id, &next_state, source);
}

esp_err_t command_router_apply_on(const char *device_id, bool on, app_state_source_t source)
{
    const app_device_config_t *device = device_registry_find(device_id);
    esp_err_t err;
    app_device_state_t state = {0};

    if (!device) {
        return ESP_ERR_NOT_FOUND;
    }

    if (device->output_driver == APP_OUTPUT_DRIVER_NEOPIXEL) {
        if (state_store_get(device->id, &state) != ESP_OK) {
            state.on = on;
            state.brightness = 100;
            state.hue = 0.0f;
            state.saturation = 0.0f;
            state.rotation_speed = 0;
        } else {
            state.on = on;
            if (on && state.brightness <= 0) {
                state.brightness = 100;
            }
        }
        return command_router_apply_light_state(device->id, &state, source);
    }

    if (device->kind == APP_DEVICE_KIND_FAN && device->supports_rotation_speed) {
        if (state_store_get(device->id, &state) != ESP_OK) {
            state.rotation_speed = on ? 100 : 0;
        } else {
            state.rotation_speed = on ? (state.rotation_speed > 0 ? state.rotation_speed : 100) : 0;
        }
        state.on = on;
        return command_router_apply_fan_state(device->id, &state, source);
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
    app_device_state_t state = {0};

    if (!device) {
        return ESP_ERR_NOT_FOUND;
    }

    if (device->output_driver == APP_OUTPUT_DRIVER_NEOPIXEL) {
        if (state_store_get(device->id, &state) != ESP_OK) {
            state.on = device->boot_on;
            state.brightness = 100;
            state.hue = 0.0f;
            state.saturation = 0.0f;
            state.rotation_speed = 0;
        }
        return command_router_apply_light_state(device->id, &state, source);
    }

    if (device->kind == APP_DEVICE_KIND_FAN && device->supports_rotation_speed) {
        if (state_store_get(device->id, &state) != ESP_OK) {
            state.on = device->boot_on;
            state.rotation_speed = state.on ? 100 : 0;
        }
        return command_router_apply_fan_state(device->id, &state, source);
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
