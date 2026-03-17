#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"

typedef struct {
    gpio_num_t gpio;
    uint32_t max_leds;
    uint32_t pixel_index;
    bool boot_on;
} drv_neopixel_config_t;

esp_err_t drv_neopixel_init(const drv_neopixel_config_t *config);
esp_err_t drv_neopixel_set_rgb(const drv_neopixel_config_t *config,
                               uint8_t red,
                               uint8_t green,
                               uint8_t blue);
esp_err_t drv_neopixel_clear(const drv_neopixel_config_t *config);
