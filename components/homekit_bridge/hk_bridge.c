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
    hap_char_t *on_char;
    char serial_number[32];
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

static void hk_state_observer(const app_device_config_t *device,
                              const app_device_state_t *state,
                              app_state_source_t source,
                              void *ctx)
{
    hk_output_binding_t *binding;
    hap_val_t value = {0};

    (void) ctx;

    if (!device || source == APP_STATE_SOURCE_HOMEKIT) {
        return;
    }

    binding = hk_find_output_binding(device->id);
    if (!binding || !binding->on_char) {
        return;
    }

    value.b = state->on;
    hap_char_update_val(binding->on_char, &value);
    ESP_LOGI(TAG, "Synced local state to HomeKit: %s -> %s", device->id, state->on ? "ON" : "OFF");
}

static int hk_output_write(hap_write_data_t write_data[], int count, void *serv_priv, void *write_priv)
{
    const app_device_config_t *device = serv_priv;
    int i;
    int ret = HAP_SUCCESS;

    if (hap_req_get_ctrl_id(write_priv)) {
        ESP_LOGI(TAG, "Received HomeKit write from %s", hap_req_get_ctrl_id(write_priv));
    }

    if (!device) {
        return HAP_FAIL;
    }

    for (i = 0; i < count; i++) {
        hap_write_data_t *write = &write_data[i];

        if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_ON)) {
            esp_err_t err = command_router_apply_on(device->id, write->val.b, APP_STATE_SOURCE_HOMEKIT);
            if (err == ESP_OK) {
                hap_char_update_val(write->hc, &(write->val));
                *(write->status) = HAP_STATUS_SUCCESS;
                ESP_LOGI(TAG, "HomeKit command applied: %s -> %s", device->id, write->val.b ? "ON" : "OFF");
            } else {
                *(write->status) = HAP_STATUS_RES_ABSENT;
                ret = HAP_FAIL;
                ESP_LOGE(TAG, "Failed to apply HomeKit command for %s", device->id);
            }
        } else {
            *(write->status) = HAP_STATUS_RES_ABSENT;
        }
    }

    return ret;
}

static hap_serv_t *hk_create_output_service(const app_device_config_t *device, bool initial_on)
{
    switch (device->kind) {
        case APP_DEVICE_KIND_LIGHT:
            return hap_serv_lightbulb_create(initial_on);
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
        case APP_DEVICE_KIND_OUTLET:
            return HAP_CID_OUTLET;
        case APP_DEVICE_KIND_SWITCH:
        default:
            return HAP_CID_SWITCH;
    }
}

static const char *hk_output_model(const app_device_config_t *device)
{
    switch (device->kind) {
        case APP_DEVICE_KIND_LIGHT:
            return "GPIO Light";
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

    hap_acc_add_serv(binding->accessory, binding->service);
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
            case APP_DEVICE_KIND_OUTLET:
                err = hk_add_output_accessory(profile, device);
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

    ESP_LOGI(TAG, "HomeKit bridge started with %u bridged output accessory(ies)",
             (unsigned int) s_output_binding_count);

    return ESP_OK;
}
