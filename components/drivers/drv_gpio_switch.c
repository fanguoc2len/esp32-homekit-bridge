#include <esp_check.h>

#include "drv_gpio_switch.h"

static int drv_gpio_switch_level_for_state(const drv_gpio_switch_config_t *config, bool on)
{
    return config->active_high ? on : !on;
}

esp_err_t drv_gpio_switch_init(const drv_gpio_switch_config_t *config)
{
    gpio_config_t io_config;

    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    io_config = (gpio_config_t) {
        .pin_bit_mask = (1ULL << config->gpio),
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_RETURN_ON_ERROR(gpio_config(&io_config), "drv_gpio_switch", "GPIO config failed");
    return drv_gpio_switch_set_on(config, config->boot_on);
}

esp_err_t drv_gpio_switch_set_on(const drv_gpio_switch_config_t *config, bool on)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    return gpio_set_level(config->gpio, drv_gpio_switch_level_for_state(config, on));
}

esp_err_t drv_gpio_switch_get_on(const drv_gpio_switch_config_t *config, bool *on_out)
{
    int level;

    if (!config || !on_out) {
        return ESP_ERR_INVALID_ARG;
    }

    level = gpio_get_level(config->gpio);
    *on_out = config->active_high ? (level != 0) : (level == 0);
    return ESP_OK;
}
