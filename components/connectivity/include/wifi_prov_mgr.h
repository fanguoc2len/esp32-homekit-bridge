#pragma once

#include <stdbool.h>

#include "esp_err.h"

esp_err_t smarthome_wifi_connect(void);
bool smarthome_wifi_is_connected(void);
bool smarthome_wifi_is_provisioned(void);
const char *smarthome_wifi_onboarding_mode(void);
esp_err_t smarthome_wifi_reset_credentials(void);
