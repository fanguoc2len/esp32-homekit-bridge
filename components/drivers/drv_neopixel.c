#include <esp_check.h>
#include <led_strip.h>

#include "drv_neopixel.h"

typedef struct {
    bool used;
    drv_neopixel_config_t config;
    led_strip_handle_t handle;
} drv_neopixel_slot_t;

static drv_neopixel_slot_t s_slots[8];

static drv_neopixel_slot_t *drv_neopixel_find_slot(gpio_num_t gpio)
{
    size_t i;

    for (i = 0; i < sizeof(s_slots) / sizeof(s_slots[0]); i++) {
        if (s_slots[i].used && s_slots[i].config.gpio == gpio) {
            return &s_slots[i];
        }
    }
    return NULL;
}

static drv_neopixel_slot_t *drv_neopixel_claim_slot(const drv_neopixel_config_t *config)
{
    drv_neopixel_slot_t *slot = drv_neopixel_find_slot(config->gpio);
    size_t i;

    if (slot) {
        return slot;
    }

    for (i = 0; i < sizeof(s_slots) / sizeof(s_slots[0]); i++) {
        if (!s_slots[i].used) {
            s_slots[i].used = true;
            s_slots[i].config = *config;
            return &s_slots[i];
        }
    }
    return NULL;
}

esp_err_t drv_neopixel_init(const drv_neopixel_config_t *config)
{
    drv_neopixel_slot_t *slot;
    led_strip_config_t strip_config;
    led_strip_rmt_config_t rmt_config;

    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    slot = drv_neopixel_claim_slot(config);
    if (!slot) {
        return ESP_ERR_NO_MEM;
    }
    if (slot->handle) {
        return ESP_OK;
    }

    strip_config = (led_strip_config_t) {
        .strip_gpio_num = config->gpio,
        .max_leds = config->max_leds ? config->max_leds : 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags.invert_out = false,
    };
    rmt_config = (led_strip_rmt_config_t) {
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };

    ESP_RETURN_ON_ERROR(led_strip_new_rmt_device(&strip_config, &rmt_config, &slot->handle),
                        "drv_neopixel",
                        "Failed to create NeoPixel strip");
    return led_strip_clear(slot->handle);
}

esp_err_t drv_neopixel_set_rgb(const drv_neopixel_config_t *config,
                               uint8_t red,
                               uint8_t green,
                               uint8_t blue)
{
    drv_neopixel_slot_t *slot;

    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    slot = drv_neopixel_find_slot(config->gpio);
    if (!slot || !slot->handle) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_RETURN_ON_ERROR(led_strip_set_pixel(slot->handle, config->pixel_index, red, green, blue),
                        "drv_neopixel",
                        "Failed to set NeoPixel color");
    return led_strip_refresh(slot->handle);
}

esp_err_t drv_neopixel_clear(const drv_neopixel_config_t *config)
{
    drv_neopixel_slot_t *slot;

    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    slot = drv_neopixel_find_slot(config->gpio);
    if (!slot || !slot->handle) {
        return ESP_ERR_INVALID_STATE;
    }

    return led_strip_clear(slot->handle);
}
