#pragma once

#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_err.h"

typedef struct {
    gpio_num_t gpio;
    bool active_high;
    bool boot_on;
} drv_gpio_switch_config_t;

esp_err_t drv_gpio_switch_init(const drv_gpio_switch_config_t *config);
esp_err_t drv_gpio_switch_set_on(const drv_gpio_switch_config_t *config, bool on);
esp_err_t drv_gpio_switch_get_on(const drv_gpio_switch_config_t *config, bool *on_out);
