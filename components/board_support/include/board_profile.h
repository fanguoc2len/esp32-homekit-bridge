#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"

typedef struct {
    const char *id;
    const char *name;
    uint8_t service_kind;
    gpio_num_t gpio;
    uint8_t output_driver;
    bool active_high;
    bool boot_on;
    bool enabled;
} board_switch_profile_t;

typedef struct {
    const char *id;
    const char *name;
    bool enabled;
    bool boot_locked;
} board_lock_profile_t;

typedef struct {
    const char *id;
    const char *name;
    bool enabled;
    bool supports_temperature;
    bool supports_humidity;
    bool simulator_enabled;
    float initial_temperature_c;
    float initial_humidity_percent;
} board_sensor_profile_t;

typedef struct {
    const char *bridge_name;
    const char *manufacturer;
    const char *model;
    const char *serial_number;
    const char *fw_revision;
    board_switch_profile_t primary_switch;
    board_lock_profile_t door_lock;
    board_sensor_profile_t environment_sensor;
} board_profile_t;

const board_profile_t *board_profile_get(void);
