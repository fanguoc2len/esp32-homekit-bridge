#include <stdio.h>
#include <string.h>

#include <esp_log.h>

#include <hap.h>
#include <hap_apple_chars.h>
#include <hap_apple_servs.h>

#include "sdkconfig.h"

#include "board_profile.h"
#include "command_router.h"
#include "device_registry.h"
#include "hk_bridge.h"
#include "hk_pairing_info.h"
#include "state_store.h"

typedef struct {
    const app_device_config_t *device;
    hap_acc_t *accessory;
    hap_serv_t *service;
    hap_serv_t *humidity_service;
    hap_char_t *on_char;
    hap_char_t *brightness_char;
    hap_char_t *hue_char;
    hap_char_t *saturation_char;
    hap_char_t *rotation_speed_char;
    hap_char_t *lock_current_state_char;
    hap_char_t *lock_target_state_char;
    hap_char_t *temperature_char;
    hap_char_t *humidity_char;
    char serial_number[32];
    char temperature_service_name[64];
    char humidity_service_name[64];
} hk_output_binding_t;

static const char *TAG = "hk_bridge";
static hk_output_binding_t s_output_bindings[APP_MAX_DEVICES];
static size_t s_output_binding_count;

static int hk_bridge_identify(hap_acc_t *ha)
{
    ESP_LOGI(TAG, "Bridge identified");
    return HAP_SUCCESS;
}

static int hk_accessory_identify(hap_acc_t *ha)
{
    hap_serv_t *info_service = hap_acc_get_serv_by_uuid(ha, HAP_SERV_UUID_ACCESSORY_INFORMATION);
    hap_char_t *name_char;
    const hap_val_t *name;

    if (!info_service) {
        return HAP_FAIL;
    }

    name_char = hap_serv_get_char_by_uuid(info_service, HAP_CHAR_UUID_NAME);
    if (!name_char) {
        return HAP_FAIL;
    }

    name = hap_char_get_val(name_char);
    if (!name || !name->s) {
        return HAP_FAIL;
    }

    ESP_LOGI(TAG, "Accessory identified: %s", name->s);
    return HAP_SUCCESS;
}

static hk_output_binding_t *hk_find_output_binding(const char *device_id)
{
    size_t i;

    for (i = 0; i < s_output_binding_count; i++) {
        if (!strcmp(s_output_bindings[i].device->id, device_id)) {
            return &s_output_bindings[i];
        }
    }
    return NULL;
}

static void hk_update_binding_from_state(hk_output_binding_t *binding, const app_device_state_t *state)
{
    hap_val_t value = {0};

    if (!binding || !state) {
        return;
    }

    if (binding->on_char) {
        value = (hap_val_t) {
            .b = state->on,
        };
        hap_char_update_val(binding->on_char, &value);
    }
    if (binding->brightness_char) {
        value = (hap_val_t) {
            .i = state->brightness,
        };
        hap_char_update_val(binding->brightness_char, &value);
    }
    if (binding->hue_char) {
        value = (hap_val_t) {
            .f = state->hue,
        };
        hap_char_update_val(binding->hue_char, &value);
    }
    if (binding->saturation_char) {
        value = (hap_val_t) {
            .f = state->saturation,
        };
        hap_char_update_val(binding->saturation_char, &value);
    }
    if (binding->rotation_speed_char) {
        value = (hap_val_t) {
            .f = state->rotation_speed,
        };
        hap_char_update_val(binding->rotation_speed_char, &value);
    }
    if (binding->lock_current_state_char) {
        value = (hap_val_t) {
            .u = state->lock_current_state,
        };
        hap_char_update_val(binding->lock_current_state_char, &value);
    }
    if (binding->lock_target_state_char) {
        value = (hap_val_t) {
            .u = state->lock_target_state,
        };
        hap_char_update_val(binding->lock_target_state_char, &value);
    }
    if (binding->temperature_char) {
        value = (hap_val_t) {
            .f = state->temperature_c,
        };
        hap_char_update_val(binding->temperature_char, &value);
    }
    if (binding->humidity_char) {
        value = (hap_val_t) {
            .f = state->humidity_percent,
        };
        hap_char_update_val(binding->humidity_char, &value);
    }
}

static void hk_state_observer(const app_device_config_t *device,
                              const app_device_state_t *state,
                              app_state_source_t source,
                              void *ctx)
{
    hk_output_binding_t *binding;

    (void) ctx;

    if (!device || source == APP_STATE_SOURCE_HOMEKIT) {
        return;
    }

    binding = hk_find_output_binding(device->id);
    if (!binding) {
        return;
    }

    hk_update_binding_from_state(binding, state);
    ESP_LOGI(TAG,
             "Synced local state to HomeKit: %s -> on=%d brightness=%d hue=%.1f saturation=%.1f speed=%d rainbow=%d lock=%u/%u temp=%.1f humidity=%.1f",
             device->id,
             state->on,
             state->brightness,
             (double) state->hue,
             (double) state->saturation,
             state->rotation_speed,
             state->effect_rainbow,
             (unsigned int) state->lock_current_state,
             (unsigned int) state->lock_target_state,
             (double) state->temperature_c,
             (double) state->humidity_percent);
}

static int hk_output_write(hap_write_data_t write_data[], int count, void *serv_priv, void *write_priv)
{
    const app_device_config_t *device = serv_priv;
    app_device_state_t next_state = {0};
    int i;
    int ret = HAP_SUCCESS;

    if (hap_req_get_ctrl_id(write_priv)) {
        ESP_LOGI(TAG, "Received HomeKit write from %s", hap_req_get_ctrl_id(write_priv));
    }

    if (!device) {
        return HAP_FAIL;
    }

    if (state_store_get(device->id, &next_state) != ESP_OK) {
        next_state.on = device->boot_on;
        next_state.brightness = 100;
        next_state.hue = 0.0f;
        next_state.saturation = 0.0f;
        next_state.effect_rainbow = false;
    }

    for (i = 0; i < count; i++) {
        hap_write_data_t *write = &write_data[i];
        const char *uuid = hap_char_get_type_uuid(write->hc);

        *(write->status) = HAP_STATUS_VAL_INVALID;

        if (!strcmp(uuid, HAP_CHAR_UUID_ON)) {
            next_state.on = write->val.b;
            if (next_state.on && next_state.brightness <= 0) {
                next_state.brightness = 100;
            }
            *(write->status) = HAP_STATUS_SUCCESS;
        } else if (!strcmp(uuid, HAP_CHAR_UUID_BRIGHTNESS) && device->supports_brightness) {
            next_state.brightness = write->val.i;
            next_state.on = (next_state.brightness > 0);
            *(write->status) = HAP_STATUS_SUCCESS;
        } else if (!strcmp(uuid, HAP_CHAR_UUID_HUE) && device->supports_hue) {
            next_state.hue = write->val.f;
            *(write->status) = HAP_STATUS_SUCCESS;
        } else if (!strcmp(uuid, HAP_CHAR_UUID_SATURATION) && device->supports_saturation) {
            next_state.saturation = write->val.f;
            *(write->status) = HAP_STATUS_SUCCESS;
        } else if (!strcmp(uuid, HAP_CHAR_UUID_ROTATION_SPEED) && device->supports_rotation_speed) {
            next_state.rotation_speed = (int) write->val.f;
            next_state.on = (next_state.rotation_speed > 0);
            *(write->status) = HAP_STATUS_SUCCESS;
        } else {
            *(write->status) = HAP_STATUS_RES_ABSENT;
            ret = HAP_FAIL;
        }
    }

    if (ret != HAP_SUCCESS) {
        return ret;
    }

    if (device->kind == APP_DEVICE_KIND_LIGHT
        && (device->supports_brightness || device->supports_hue || device->supports_saturation)) {
        if (command_router_apply_light_state(device->id, &next_state, APP_STATE_SOURCE_HOMEKIT) != ESP_OK) {
            for (i = 0; i < count; i++) {
                *(write_data[i].status) = HAP_STATUS_RES_ABSENT;
            }
            ESP_LOGE(TAG, "Failed to apply HomeKit light state for %s", device->id);
            return HAP_FAIL;
        }
    } else if (device->kind == APP_DEVICE_KIND_FAN && device->supports_rotation_speed) {
        if (command_router_apply_fan_state(device->id, &next_state, APP_STATE_SOURCE_HOMEKIT) != ESP_OK) {
            for (i = 0; i < count; i++) {
                *(write_data[i].status) = HAP_STATUS_RES_ABSENT;
            }
            ESP_LOGE(TAG, "Failed to apply HomeKit fan state for %s", device->id);
            return HAP_FAIL;
        }
    } else if (command_router_apply_on(device->id, next_state.on, APP_STATE_SOURCE_HOMEKIT) != ESP_OK) {
        for (i = 0; i < count; i++) {
            *(write_data[i].status) = HAP_STATUS_RES_ABSENT;
        }
        ESP_LOGE(TAG, "Failed to apply HomeKit command for %s", device->id);
        return HAP_FAIL;
    }

    if (state_store_get(device->id, &next_state) == ESP_OK) {
        hk_update_binding_from_state(hk_find_output_binding(device->id), &next_state);
    } else {
        for (i = 0; i < count; i++) {
            hap_char_update_val(write_data[i].hc, &(write_data[i].val));
        }
    }
    ESP_LOGI(TAG, "HomeKit state applied: %s -> on=%d brightness=%d hue=%.1f saturation=%.1f speed=%d rainbow=%d",
             device->id,
             next_state.on,
             next_state.brightness,
             (double) next_state.hue,
             (double) next_state.saturation,
             next_state.rotation_speed,
             next_state.effect_rainbow);

    return HAP_SUCCESS;
}

static int hk_lock_write(hap_write_data_t write_data[], int count, void *serv_priv, void *write_priv)
{
    const app_device_config_t *device = serv_priv;
    app_device_state_t next_state = {0};
    int i;
    int ret = HAP_SUCCESS;

    if (hap_req_get_ctrl_id(write_priv)) {
        ESP_LOGI(TAG, "Received HomeKit lock write from %s", hap_req_get_ctrl_id(write_priv));
    }

    if (!device) {
        return HAP_FAIL;
    }

    if (state_store_get(device->id, &next_state) != ESP_OK) {
        next_state.lock_target_state = device->initial_lock_target_state;
        next_state.lock_current_state = next_state.lock_target_state == APP_LOCK_TARGET_SECURED
            ? APP_LOCK_CURRENT_SECURED
            : APP_LOCK_CURRENT_UNSECURED;
    }

    for (i = 0; i < count; i++) {
        hap_write_data_t *write = &write_data[i];
        const char *uuid = hap_char_get_type_uuid(write->hc);

        *(write->status) = HAP_STATUS_VAL_INVALID;

        if (!strcmp(uuid, HAP_CHAR_UUID_LOCK_TARGET_STATE) && device->supports_lock) {
            next_state.lock_target_state = (uint8_t) write->val.u;
            *(write->status) = HAP_STATUS_SUCCESS;
        } else {
            *(write->status) = HAP_STATUS_RES_ABSENT;
            ret = HAP_FAIL;
        }
    }

    if (ret != HAP_SUCCESS) {
        return ret;
    }

    if (command_router_apply_lock_state(device->id, &next_state, APP_STATE_SOURCE_HOMEKIT) != ESP_OK) {
        for (i = 0; i < count; i++) {
            *(write_data[i].status) = HAP_STATUS_RES_ABSENT;
        }
        ESP_LOGE(TAG, "Failed to apply HomeKit lock state for %s", device->id);
        return HAP_FAIL;
    }

    if (state_store_get(device->id, &next_state) == ESP_OK) {
        hk_update_binding_from_state(hk_find_output_binding(device->id), &next_state);
    }
    ESP_LOGI(TAG, "HomeKit lock applied: %s -> current=%u target=%u",
             device->id,
             (unsigned int) next_state.lock_current_state,
             (unsigned int) next_state.lock_target_state);

    return HAP_SUCCESS;
}

static hap_serv_t *hk_create_output_service(const app_device_config_t *device, bool initial_on)
{
    switch (device->kind) {
        case APP_DEVICE_KIND_LIGHT:
            return hap_serv_lightbulb_create(initial_on);
        case APP_DEVICE_KIND_FAN:
            return hap_serv_fan_create(initial_on);
        case APP_DEVICE_KIND_OUTLET:
            return hap_serv_outlet_create(initial_on, initial_on);
        case APP_DEVICE_KIND_SWITCH:
        default:
            return hap_serv_switch_create(initial_on);
    }
}

static hap_cid_t hk_output_category(const app_device_config_t *device)
{
    switch (device->kind) {
        case APP_DEVICE_KIND_LIGHT:
            return HAP_CID_LIGHTING;
        case APP_DEVICE_KIND_FAN:
            return HAP_CID_FAN;
        case APP_DEVICE_KIND_OUTLET:
            return HAP_CID_OUTLET;
        case APP_DEVICE_KIND_SWITCH:
        default:
            return HAP_CID_SWITCH;
    }
}

static const char *hk_output_model(const app_device_config_t *device)
{
    if (device->is_effect_switch) {
        return "NeoPixel Effect Switch";
    }

    switch (device->kind) {
        case APP_DEVICE_KIND_LIGHT:
            return device->output_driver == APP_OUTPUT_DRIVER_NEOPIXEL ? "NeoPixel RGB Light" : "GPIO Light";
        case APP_DEVICE_KIND_FAN:
            return "GPIO Fan";
        case APP_DEVICE_KIND_OUTLET:
            return "GPIO Outlet";
        case APP_DEVICE_KIND_SWITCH:
        default:
            return "GPIO Switch";
    }
}

static esp_err_t hk_add_output_accessory(const board_profile_t *profile, const app_device_config_t *device)
{
    hk_output_binding_t *binding;
    hap_acc_cfg_t accessory_cfg;
    app_device_state_t state = {0};

    if (s_output_binding_count >= APP_MAX_DEVICES) {
        return ESP_ERR_NO_MEM;
    }

    binding = &s_output_bindings[s_output_binding_count];
    memset(binding, 0, sizeof(*binding));
    binding->device = device;

    snprintf(binding->serial_number, sizeof(binding->serial_number), "%s-%u",
             profile->serial_number, (unsigned int) (s_output_binding_count + 1));

    accessory_cfg = (hap_acc_cfg_t) {
        .name = (char *) device->name,
        .manufacturer = (char *) profile->manufacturer,
        .model = (char *) hk_output_model(device),
        .serial_num = binding->serial_number,
        .fw_rev = (char *) profile->fw_revision,
        .hw_rev = NULL,
        .pv = "1.1.0",
        .identify_routine = hk_accessory_identify,
        .cid = hk_output_category(device),
    };

    binding->accessory = hap_acc_create(&accessory_cfg);
    if (!binding->accessory) {
        return ESP_ERR_NO_MEM;
    }

    if (state_store_get(device->id, &state) != ESP_OK) {
        state.on = device->boot_on;
        state.brightness = 100;
        state.hue = 0.0f;
        state.saturation = 0.0f;
        state.rotation_speed = state.on ? 100 : 0;
        state.effect_rainbow = false;
    }

    binding->service = hk_create_output_service(device, state.on);
    if (!binding->service) {
        return ESP_ERR_NO_MEM;
    }

    hap_serv_add_char(binding->service, hap_char_name_create((char *) device->name));
    hap_serv_set_priv(binding->service, (void *) device);
    hap_serv_set_write_cb(binding->service, hk_output_write);

    binding->on_char = hap_serv_get_char_by_uuid(binding->service, HAP_CHAR_UUID_ON);
    if (!binding->on_char) {
        return ESP_ERR_NOT_FOUND;
    }

    if (device->supports_brightness) {
        if (hap_serv_add_char(binding->service, hap_char_brightness_create(state.brightness)) != HAP_SUCCESS) {
            return ESP_ERR_NO_MEM;
        }
    }
    if (device->supports_hue) {
        if (hap_serv_add_char(binding->service, hap_char_hue_create(state.hue)) != HAP_SUCCESS) {
            return ESP_ERR_NO_MEM;
        }
    }
    if (device->supports_saturation) {
        if (hap_serv_add_char(binding->service, hap_char_saturation_create(state.saturation)) != HAP_SUCCESS) {
            return ESP_ERR_NO_MEM;
        }
    }
    if (device->supports_rotation_speed) {
        if (hap_serv_add_char(binding->service,
                              hap_char_rotation_speed_create((float) state.rotation_speed)) != HAP_SUCCESS) {
            return ESP_ERR_NO_MEM;
        }
    }

    binding->brightness_char = hap_serv_get_char_by_uuid(binding->service, HAP_CHAR_UUID_BRIGHTNESS);
    binding->hue_char = hap_serv_get_char_by_uuid(binding->service, HAP_CHAR_UUID_HUE);
    binding->saturation_char = hap_serv_get_char_by_uuid(binding->service, HAP_CHAR_UUID_SATURATION);
    binding->rotation_speed_char = hap_serv_get_char_by_uuid(binding->service, HAP_CHAR_UUID_ROTATION_SPEED);

    hap_acc_add_serv(binding->accessory, binding->service);
    hap_add_bridged_accessory(binding->accessory, hap_get_unique_aid(device->id));

    s_output_binding_count++;
    return ESP_OK;
}

static esp_err_t hk_add_lock_accessory(const board_profile_t *profile, const app_device_config_t *device)
{
    hk_output_binding_t *binding;
    hap_acc_cfg_t accessory_cfg;
    app_device_state_t state = {0};

    if (s_output_binding_count >= APP_MAX_DEVICES) {
        return ESP_ERR_NO_MEM;
    }

    binding = &s_output_bindings[s_output_binding_count];
    memset(binding, 0, sizeof(*binding));
    binding->device = device;

    snprintf(binding->serial_number, sizeof(binding->serial_number), "%s-%u",
             profile->serial_number, (unsigned int) (s_output_binding_count + 1));

    accessory_cfg = (hap_acc_cfg_t) {
        .name = (char *) device->name,
        .manufacturer = (char *) profile->manufacturer,
        .model = "Virtual Door Lock",
        .serial_num = binding->serial_number,
        .fw_rev = (char *) profile->fw_revision,
        .hw_rev = NULL,
        .pv = "1.1.0",
        .identify_routine = hk_accessory_identify,
        .cid = HAP_CID_LOCK,
    };

    binding->accessory = hap_acc_create(&accessory_cfg);
    if (!binding->accessory) {
        return ESP_ERR_NO_MEM;
    }

    if (state_store_get(device->id, &state) != ESP_OK) {
        state.lock_target_state = device->initial_lock_target_state;
        state.lock_current_state = state.lock_target_state == APP_LOCK_TARGET_SECURED
            ? APP_LOCK_CURRENT_SECURED
            : APP_LOCK_CURRENT_UNSECURED;
    }

    binding->service = hap_serv_lock_mechanism_create(state.lock_current_state, state.lock_target_state);
    if (!binding->service) {
        return ESP_ERR_NO_MEM;
    }

    hap_serv_add_char(binding->service, hap_char_name_create((char *) device->name));
    hap_serv_set_priv(binding->service, (void *) device);
    hap_serv_set_write_cb(binding->service, hk_lock_write);

    binding->lock_current_state_char = hap_serv_get_char_by_uuid(binding->service,
                                                                 HAP_CHAR_UUID_LOCK_CURRENT_STATE);
    binding->lock_target_state_char = hap_serv_get_char_by_uuid(binding->service,
                                                                HAP_CHAR_UUID_LOCK_TARGET_STATE);
    if (!binding->lock_current_state_char || !binding->lock_target_state_char) {
        return ESP_ERR_NOT_FOUND;
    }

    hap_acc_add_serv(binding->accessory, binding->service);
    hap_add_bridged_accessory(binding->accessory, hap_get_unique_aid(device->id));

    s_output_binding_count++;
    return ESP_OK;
}

static esp_err_t hk_add_sensor_accessory(const board_profile_t *profile, const app_device_config_t *device)
{
    hk_output_binding_t *binding;
    hap_acc_cfg_t accessory_cfg;
    app_device_state_t state = {0};

    if (s_output_binding_count >= APP_MAX_DEVICES) {
        return ESP_ERR_NO_MEM;
    }

    binding = &s_output_bindings[s_output_binding_count];
    memset(binding, 0, sizeof(*binding));
    binding->device = device;

    snprintf(binding->serial_number, sizeof(binding->serial_number), "%s-%u",
             profile->serial_number, (unsigned int) (s_output_binding_count + 1));

    accessory_cfg = (hap_acc_cfg_t) {
        .name = (char *) device->name,
        .manufacturer = (char *) profile->manufacturer,
        .model = "Virtual Climate Sensor",
        .serial_num = binding->serial_number,
        .fw_rev = (char *) profile->fw_revision,
        .hw_rev = NULL,
        .pv = "1.1.0",
        .identify_routine = hk_accessory_identify,
        .cid = HAP_CID_SENSOR,
    };

    binding->accessory = hap_acc_create(&accessory_cfg);
    if (!binding->accessory) {
        return ESP_ERR_NO_MEM;
    }

    if (state_store_get(device->id, &state) != ESP_OK) {
        state.temperature_c = device->initial_temperature_c;
        state.humidity_percent = device->initial_humidity_percent;
    }

    if (device->supports_temperature) {
        snprintf(binding->temperature_service_name,
                 sizeof(binding->temperature_service_name),
                 "%s Temperature",
                 device->name);
        binding->service = hap_serv_temperature_sensor_create(state.temperature_c);
        if (!binding->service) {
            return ESP_ERR_NO_MEM;
        }
        hap_serv_add_char(binding->service, hap_char_name_create(binding->temperature_service_name));
        binding->temperature_char = hap_serv_get_char_by_uuid(binding->service,
                                                              HAP_CHAR_UUID_CURRENT_TEMPERATURE);
        if (!binding->temperature_char) {
            return ESP_ERR_NOT_FOUND;
        }
        hap_acc_add_serv(binding->accessory, binding->service);
    }

    if (device->supports_humidity) {
        snprintf(binding->humidity_service_name,
                 sizeof(binding->humidity_service_name),
                 "%s Humidity",
                 device->name);
        binding->humidity_service = hap_serv_humidity_sensor_create(state.humidity_percent);
        if (!binding->humidity_service) {
            return ESP_ERR_NO_MEM;
        }
        hap_serv_add_char(binding->humidity_service, hap_char_name_create(binding->humidity_service_name));
        binding->humidity_char = hap_serv_get_char_by_uuid(binding->humidity_service,
                                                           HAP_CHAR_UUID_CURRENT_RELATIVE_HUMIDITY);
        if (!binding->humidity_char) {
            return ESP_ERR_NOT_FOUND;
        }
        hap_acc_add_serv(binding->accessory, binding->humidity_service);
    }

    if (!binding->service && !binding->humidity_service) {
        return ESP_ERR_INVALID_ARG;
    }

    hap_add_bridged_accessory(binding->accessory, hap_get_unique_aid(device->id));

    s_output_binding_count++;
    return ESP_OK;
}

esp_err_t hk_bridge_start(void)
{
    hap_acc_t *bridge;
    hap_acc_cfg_t bridge_cfg;
    const board_profile_t *profile = board_profile_get();
    size_t i;

    s_output_binding_count = 0;

    hap_init(HAP_TRANSPORT_WIFI);

    bridge_cfg = (hap_acc_cfg_t) {
        .name = (char *) profile->bridge_name,
        .manufacturer = (char *) profile->manufacturer,
        .model = (char *) profile->model,
        .serial_num = (char *) profile->serial_number,
        .fw_rev = (char *) profile->fw_revision,
        .hw_rev = NULL,
        .pv = "1.1.0",
        .identify_routine = hk_bridge_identify,
        .cid = HAP_CID_BRIDGE,
    };

    bridge = hap_acc_create(&bridge_cfg);
    if (!bridge) {
        return ESP_ERR_NO_MEM;
    }

    hap_acc_add_wifi_transport_service(bridge, 0);
    hap_add_accessory(bridge);

    for (i = 0; i < device_registry_count(); i++) {
        const app_device_config_t *device = device_registry_get(i);
        esp_err_t err;

        if (!device) {
            continue;
        }

        switch (device->kind) {
            case APP_DEVICE_KIND_SWITCH:
            case APP_DEVICE_KIND_LIGHT:
            case APP_DEVICE_KIND_FAN:
            case APP_DEVICE_KIND_OUTLET:
                err = hk_add_output_accessory(profile, device);
                if (err != ESP_OK) {
                    return err;
                }
                break;
            case APP_DEVICE_KIND_LOCK:
                err = hk_add_lock_accessory(profile, device);
                if (err != ESP_OK) {
                    return err;
                }
                break;
            case APP_DEVICE_KIND_SENSOR:
                err = hk_add_sensor_accessory(profile, device);
                if (err != ESP_OK) {
                    return err;
                }
                break;
            default:
                ESP_LOGW(TAG, "Skipping unmigrated device kind %d (%s)", device->kind, device->id);
                break;
        }
    }

    if (state_store_register_observer(hk_state_observer, NULL) != ESP_OK) {
        return ESP_FAIL;
    }

    hap_set_setup_code(CONFIG_SMARTHOME_HAP_SETUP_CODE);
    hap_set_setup_id(CONFIG_SMARTHOME_HAP_SETUP_ID);
    if (hk_pairing_info_print(profile->bridge_name,
                              CONFIG_SMARTHOME_HAP_SETUP_CODE,
                              CONFIG_SMARTHOME_HAP_SETUP_ID,
                              HAP_CID_BRIDGE) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to print HomeKit pairing payload");
    }
    hap_start();

    ESP_LOGI(TAG, "HomeKit bridge started with %u bridged accessory(ies)",
             (unsigned int) s_output_binding_count);

    return ESP_OK;
}
