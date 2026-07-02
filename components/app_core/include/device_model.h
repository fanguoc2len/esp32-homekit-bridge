#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "driver/gpio.h"

#define APP_MAX_DEVICES 12
#define APP_MAX_OBSERVERS 4

#define APP_LOCK_CURRENT_SECURED 0
#define APP_LOCK_CURRENT_UNSECURED 1
#define APP_LOCK_CURRENT_JAMMED 2
#define APP_LOCK_CURRENT_UNKNOWN 3

#define APP_LOCK_TARGET_SECURED 0
#define APP_LOCK_TARGET_UNSECURED 1

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
    bool effect_rainbow;
    float temperature_c;
    float humidity_percent;
    uint8_t lock_current_state;
    uint8_t lock_target_state;
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
    bool supports_rotation_speed;
    bool supports_effect_rainbow;
    bool supports_temperature;
    bool supports_humidity;
    bool supports_lock;
    bool simulator_enabled;
    bool is_effect_switch;
    const char *linked_device_id;
    float initial_temperature_c;
    float initial_humidity_percent;
    uint8_t initial_lock_target_state;
} app_device_config_t;
