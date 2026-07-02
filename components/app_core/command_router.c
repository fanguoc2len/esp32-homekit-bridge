#include <string.h>

#include <math.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_check.h>
#include <esp_log.h>

#include "sdkconfig.h"

#include "command_router.h"
#include "device_registry.h"
#include "drv_gpio_switch.h"
#include "drv_neopixel.h"
#include "state_store.h"

static const char *TAG = "command_router";
static const uint16_t EFFECT_HUE_STEP = 4;
static const uint32_t EFFECT_INTERVAL_MS = 40;

#ifdef CONFIG_SMARTHOME_ENV_SENSOR_SIM_INTERVAL_MS
#define SENSOR_SIM_INTERVAL_MS CONFIG_SMARTHOME_ENV_SENSOR_SIM_INTERVAL_MS
#else
#define SENSOR_SIM_INTERVAL_MS 30000
#endif

typedef struct {
    bool used;
    const app_device_config_t *device;
    uint16_t hue_offset;
} neopixel_effect_slot_t;

static neopixel_effect_slot_t s_effect_slots[APP_MAX_DEVICES];

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

static neopixel_effect_slot_t *command_router_find_effect_slot(const char *device_id)
{
    size_t i;

    if (!device_id) {
        return NULL;
    }

    for (i = 0; i < APP_MAX_DEVICES; i++) {
        if (s_effect_slots[i].used && !strcmp(s_effect_slots[i].device->id, device_id)) {
            return &s_effect_slots[i];
        }
    }
    return NULL;
}

static neopixel_effect_slot_t *command_router_claim_effect_slot(const app_device_config_t *device)
{
    size_t i;

    if (!device) {
        return NULL;
    }

    for (i = 0; i < APP_MAX_DEVICES; i++) {
        if (s_effect_slots[i].used && s_effect_slots[i].device == device) {
            return &s_effect_slots[i];
        }
    }

    for (i = 0; i < APP_MAX_DEVICES; i++) {
        if (!s_effect_slots[i].used) {
            s_effect_slots[i].used = true;
            s_effect_slots[i].device = device;
            s_effect_slots[i].hue_offset = 0;
            return &s_effect_slots[i];
        }
    }
    return NULL;
}

static const app_device_config_t *command_router_find_effect_switch_for_light(const char *light_device_id)
{
    size_t i;

    if (!light_device_id) {
        return NULL;
    }

    for (i = 0; i < device_registry_count(); i++) {
        const app_device_config_t *candidate = device_registry_get(i);

        if (candidate && candidate->is_effect_switch && candidate->linked_device_id
            && !strcmp(candidate->linked_device_id, light_device_id)) {
            return candidate;
        }
    }

    return NULL;
}

static esp_err_t command_router_sync_effect_switch_state(const app_device_config_t *light_device,
                                                         bool effect_on,
                                                         app_state_source_t source)
{
    const app_device_config_t *effect_device;
    app_device_state_t effect_state = {0};

    if (!light_device || !light_device->supports_effect_rainbow) {
        return ESP_OK;
    }

    effect_device = command_router_find_effect_switch_for_light(light_device->id);
    if (!effect_device) {
        return ESP_OK;
    }

    effect_state.on = effect_on;
    return state_store_set_state(effect_device->id, &effect_state, source);
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

static float command_router_clamp_float(float value, float min_value, float max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
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

static esp_err_t command_router_render_neopixel_state(const app_device_config_t *device,
                                                      const app_device_state_t *state,
                                                      uint16_t hue_offset)
{
    drv_neopixel_config_t driver_config;
    app_device_state_t render_state;
    uint8_t red;
    uint8_t green;
    uint8_t blue;

    if (!device || !state) {
        return ESP_ERR_INVALID_ARG;
    }

    driver_config = make_neopixel_config(device);
    if (!state->on || state->brightness <= 0) {
        return drv_neopixel_clear(&driver_config);
    }

    render_state = *state;
    if (render_state.effect_rainbow) {
        render_state.hue = fmodf(render_state.hue + hue_offset, 360.0f);
        if (render_state.hue < 0.0f) {
            render_state.hue += 360.0f;
        }
        if (render_state.saturation < 1.0f) {
            render_state.saturation = 100.0f;
        }
    }

    command_router_hsv_to_rgb(&render_state, &red, &green, &blue);
    return drv_neopixel_set_rgb(&driver_config, red, green, blue);
}

static void command_router_neopixel_effect_task(void *arg)
{
    (void) arg;

    while (true) {
        size_t i;

        for (i = 0; i < APP_MAX_DEVICES; i++) {
            neopixel_effect_slot_t *slot = &s_effect_slots[i];
            app_device_state_t state = {0};

            if (!slot->used || !slot->device) {
                continue;
            }
            if (state_store_get(slot->device->id, &state) != ESP_OK) {
                continue;
            }
            if (!state.on || !state.effect_rainbow) {
                continue;
            }
            if (command_router_render_neopixel_state(slot->device, &state, slot->hue_offset) == ESP_OK) {
                slot->hue_offset = (uint16_t) ((slot->hue_offset + EFFECT_HUE_STEP) % 360);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(EFFECT_INTERVAL_MS));
    }
}

static void command_router_sensor_simulator_task(void *arg)
{
    uint32_t tick = 0;

    (void) arg;

    while (true) {
        size_t i;
        int temp_phase = (int) (tick % 10U) - 5;
        int humidity_phase = (int) ((tick + 3U) % 12U) - 6;

        for (i = 0; i < device_registry_count(); i++) {
            const app_device_config_t *device = device_registry_get(i);
            app_device_state_t state = {0};

            if (!device || device->kind != APP_DEVICE_KIND_SENSOR || !device->simulator_enabled) {
                continue;
            }
            if (state_store_get(device->id, &state) != ESP_OK) {
                continue;
            }

            if (device->supports_temperature) {
                state.temperature_c = command_router_clamp_float(
                    device->initial_temperature_c + (temp_phase * 0.2f),
                    -40.0f,
                    100.0f);
            }
            if (device->supports_humidity) {
                state.humidity_percent = command_router_clamp_float(
                    device->initial_humidity_percent + (humidity_phase * 0.5f),
                    0.0f,
                    100.0f);
            }
            command_router_update_sensor_state(device->id, &state, APP_STATE_SOURCE_DEVICE);
        }

        tick++;
        vTaskDelay(pdMS_TO_TICKS(SENSOR_SIM_INTERVAL_MS));
    }
}

static esp_err_t command_router_apply_effect_switch(const char *device_id,
                                                    bool on,
                                                    app_state_source_t source)
{
    const app_device_config_t *effect_device = device_registry_find(device_id);
    const app_device_config_t *light_device;
    app_device_state_t light_state = {0};
    app_device_state_t effect_state = {0};

    if (!effect_device || !effect_device->is_effect_switch || !effect_device->linked_device_id) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    light_device = device_registry_find(effect_device->linked_device_id);
    if (!light_device) {
        return ESP_ERR_NOT_FOUND;
    }

    if (state_store_get(light_device->id, &light_state) != ESP_OK) {
        light_state.on = light_device->boot_on;
        light_state.brightness = 100;
        light_state.hue = 0.0f;
        light_state.saturation = 0.0f;
        light_state.rotation_speed = 0;
        light_state.effect_rainbow = false;
    }

    effect_state.on = on;
    light_state.effect_rainbow = on;
    if (on) {
        light_state.on = true;
        if (light_state.brightness <= 0) {
            light_state.brightness = 100;
        }
    }

    ESP_RETURN_ON_ERROR(state_store_set_state(effect_device->id, &effect_state, source),
                        TAG, "Failed to update effect switch state");
    return command_router_apply_light_state(light_device->id, &light_state, APP_STATE_SOURCE_DEVICE);
}

esp_err_t command_router_init(void)
{
    size_t i;
    bool start_sensor_simulator = false;

    memset(s_effect_slots, 0, sizeof(s_effect_slots));
    for (i = 0; i < device_registry_count(); i++) {
        const app_device_config_t *device = device_registry_get(i);
        esp_err_t err;

        if (!device) {
            continue;
        }

        if (device->is_effect_switch) {
            err = command_router_sync_from_hardware(device->id, APP_STATE_SOURCE_BOOT);
            if (err != ESP_OK) {
                return err;
            }
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
                    if (device->supports_effect_rainbow && !command_router_claim_effect_slot(device)) {
                        return ESP_ERR_NO_MEM;
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
            case APP_DEVICE_KIND_FAN:
            {
                if (device->output_driver != APP_OUTPUT_DRIVER_GPIO_SWITCH) {
                    return ESP_ERR_NOT_SUPPORTED;
                }

                drv_gpio_switch_config_t driver_config = make_switch_config(device);

                err = drv_gpio_switch_init(&driver_config);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to init fan driver for %s", device->id);
                    return err;
                }
                err = command_router_sync_from_hardware(device->id, APP_STATE_SOURCE_BOOT);
                if (err != ESP_OK) {
                    return err;
                }
                break;
            }
            case APP_DEVICE_KIND_LOCK:
            case APP_DEVICE_KIND_SENSOR:
                err = command_router_sync_from_hardware(device->id, APP_STATE_SOURCE_BOOT);
                if (err != ESP_OK) {
                    return err;
                }
                if (device->kind == APP_DEVICE_KIND_SENSOR && device->simulator_enabled) {
                    start_sensor_simulator = true;
                }
                break;
            default:
                ESP_LOGW(TAG, "Device kind %d not wired yet for %s", device->kind, device->id);
                break;
        }
    }

    for (i = 0; i < APP_MAX_DEVICES; i++) {
        if (s_effect_slots[i].used) {
            if (xTaskCreate(command_router_neopixel_effect_task,
                            "neopixel_effect",
                            4096,
                            NULL,
                            4,
                            NULL) != pdPASS) {
                return ESP_ERR_NO_MEM;
            }
            break;
        }
    }

    if (start_sensor_simulator) {
        if (xTaskCreate(command_router_sensor_simulator_task,
                        "sensor_simulator",
                        4096,
                        NULL,
                        3,
                        NULL) != pdPASS) {
            return ESP_ERR_NO_MEM;
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
        esp_err_t err;
        app_device_state_t next_state = *state;

        if (!next_state.on) {
            next_state.effect_rainbow = false;
        }

        if (next_state.effect_rainbow && next_state.on) {
            neopixel_effect_slot_t *slot = command_router_find_effect_slot(device->id);

            err = state_store_set_state(device->id, &next_state, source);
            if (err != ESP_OK) {
                return err;
            }
            err = command_router_sync_effect_switch_state(device,
                                                          true,
                                                          APP_STATE_SOURCE_DEVICE);
            if (err != ESP_OK) {
                return err;
            }
            return command_router_render_neopixel_state(device,
                                                        &next_state,
                                                        slot ? slot->hue_offset : 0);
        }

        err = command_router_render_neopixel_state(device, &next_state, 0);
        if (err != ESP_OK) {
            return err;
        }
        err = state_store_set_state(device->id, &next_state, source);
        if (err != ESP_OK) {
            return err;
        }
        return command_router_sync_effect_switch_state(device,
                                                       false,
                                                       APP_STATE_SOURCE_DEVICE);
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

esp_err_t command_router_apply_lock_state(const char *device_id,
                                          const app_device_state_t *state,
                                          app_state_source_t source)
{
    const app_device_config_t *device = device_registry_find(device_id);
    app_device_state_t next_state;

    if (!device || !state) {
        return device ? ESP_ERR_INVALID_ARG : ESP_ERR_NOT_FOUND;
    }
    if (device->kind != APP_DEVICE_KIND_LOCK || !device->supports_lock) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    next_state = *state;
    if (next_state.lock_target_state != APP_LOCK_TARGET_UNSECURED) {
        next_state.lock_target_state = APP_LOCK_TARGET_SECURED;
    }
    next_state.lock_current_state = next_state.lock_target_state == APP_LOCK_TARGET_SECURED
        ? APP_LOCK_CURRENT_SECURED
        : APP_LOCK_CURRENT_UNSECURED;

    return state_store_set_state(device->id, &next_state, source);
}

esp_err_t command_router_update_sensor_state(const char *device_id,
                                             const app_device_state_t *state,
                                             app_state_source_t source)
{
    const app_device_config_t *device = device_registry_find(device_id);
    app_device_state_t next_state;

    if (!device || !state) {
        return device ? ESP_ERR_INVALID_ARG : ESP_ERR_NOT_FOUND;
    }
    if (device->kind != APP_DEVICE_KIND_SENSOR) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    next_state = *state;
    if (device->supports_temperature) {
        next_state.temperature_c = command_router_clamp_float(next_state.temperature_c, -40.0f, 100.0f);
    } else {
        next_state.temperature_c = 0.0f;
    }
    if (device->supports_humidity) {
        next_state.humidity_percent = command_router_clamp_float(next_state.humidity_percent, 0.0f, 100.0f);
    } else {
        next_state.humidity_percent = 0.0f;
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

    if (device->is_effect_switch) {
        return command_router_apply_effect_switch(device_id, on, source);
    }

    if (device->output_driver == APP_OUTPUT_DRIVER_NEOPIXEL) {
        if (state_store_get(device->id, &state) != ESP_OK) {
            state.on = on;
            state.brightness = 100;
            state.hue = 0.0f;
            state.saturation = 0.0f;
            state.rotation_speed = 0;
            state.effect_rainbow = false;
        } else {
            state.on = on;
            if (on && state.brightness <= 0) {
                state.brightness = 100;
            }
            if (!on) {
                state.effect_rainbow = false;
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

    if (device->kind == APP_DEVICE_KIND_LOCK && device->supports_lock) {
        if (state_store_get(device->id, &state) != ESP_OK) {
            state.lock_target_state = on ? APP_LOCK_TARGET_UNSECURED : APP_LOCK_TARGET_SECURED;
        } else {
            state.lock_target_state = on ? APP_LOCK_TARGET_UNSECURED : APP_LOCK_TARGET_SECURED;
        }
        return command_router_apply_lock_state(device->id, &state, source);
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

    if (device->is_effect_switch) {
        app_device_state_t linked_state = {0};
        app_device_state_t effect_state = {0};
        const app_device_config_t *light_device = device->linked_device_id
            ? device_registry_find(device->linked_device_id)
            : NULL;

        if (light_device && state_store_get(light_device->id, &linked_state) == ESP_OK) {
            effect_state.on = linked_state.effect_rainbow;
        }
        return state_store_set_state(device->id, &effect_state, source);
    }

    if (device->output_driver == APP_OUTPUT_DRIVER_NEOPIXEL) {
        if (state_store_get(device->id, &state) != ESP_OK) {
            state.on = device->boot_on;
            state.brightness = 100;
            state.hue = 0.0f;
            state.saturation = 0.0f;
            state.rotation_speed = 0;
            state.effect_rainbow = false;
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

    if (device->kind == APP_DEVICE_KIND_LOCK && device->supports_lock) {
        if (state_store_get(device->id, &state) != ESP_OK) {
            state.lock_target_state = device->initial_lock_target_state;
        }
        return command_router_apply_lock_state(device->id, &state, source);
    }

    if (device->kind == APP_DEVICE_KIND_SENSOR) {
        if (state_store_get(device->id, &state) != ESP_OK) {
            state.temperature_c = device->initial_temperature_c;
            state.humidity_percent = device->initial_humidity_percent;
        }
        return command_router_update_sensor_state(device->id, &state, source);
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
