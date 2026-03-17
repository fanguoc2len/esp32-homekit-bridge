#pragma once

#include <stdbool.h>

#include "driver/gpio.h"

typedef struct {
    const char *id;
    const char *name;
    bool homekit_lightbulb;
    gpio_num_t gpio;
    bool active_high;
    bool boot_on;
    bool enabled;
} board_switch_profile_t;

typedef struct {
    const char *bridge_name;
    const char *manufacturer;
    const char *model;
    const char *serial_number;
    const char *fw_revision;
    board_switch_profile_t primary_switch;
} board_profile_t;

const board_profile_t *board_profile_get(void);
