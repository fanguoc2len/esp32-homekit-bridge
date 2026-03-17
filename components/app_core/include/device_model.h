#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "driver/gpio.h"

#define APP_MAX_DEVICES 8
#define APP_MAX_OBSERVERS 4

typedef enum {
    APP_DEVICE_KIND_SWITCH = 0,
    APP_DEVICE_KIND_LIGHT,
    APP_DEVICE_KIND_FAN,
    APP_DEVICE_KIND_SENSOR,
    APP_DEVICE_KIND_OUTLET,
    APP_DEVICE_KIND_LOCK,
    APP_DEVICE_KIND_SPEAKER,
} app_device_kind_t;

typedef enum {
    APP_OUTPUT_DRIVER_GPIO_SWITCH = 0,
    APP_OUTPUT_DRIVER_NEOPIXEL,
} app_output_driver_t;

typedef enum {
    APP_STATE_SOURCE_BOOT = 0,
    APP_STATE_SOURCE_HOMEKIT,
    APP_STATE_SOURCE_DEVICE,
} app_state_source_t;

typedef struct {
    bool on;
    int brightness;
    float hue;
    float saturation;
    int rotation_speed;
    float temperature_c;
    float humidity_percent;
} app_device_state_t;

typedef struct {
    const char *id;
    const char *name;
    app_device_kind_t kind;
    gpio_num_t gpio;
    app_output_driver_t output_driver;
    bool active_high;
    bool boot_on;
    bool supports_on;
    bool supports_brightness;
    bool supports_hue;
    bool supports_saturation;
} app_device_config_t;
