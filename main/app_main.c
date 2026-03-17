#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include "sdkconfig.h"

#include "command_router.h"
#include "device_registry.h"
#include "hk_bridge.h"
#include "state_store.h"
#include "wifi_prov_mgr.h"

static const char *TAG = "smarthome_main";

static void local_state_monitor_task(void *arg)
{
    while (true) {
        command_router_sync_all_from_hardware(APP_STATE_SOURCE_DEVICE);
        vTaskDelay(pdMS_TO_TICKS(CONFIG_SMARTHOME_SWITCH_SYNC_INTERVAL_MS));
    }
}

#if CONFIG_SMARTHOME_LOCAL_STATE_SELF_TEST
static void local_state_self_test_task(void *arg)
{
    const app_device_config_t *device = device_registry_get(0);

    if (!device) {
        vTaskDelete(NULL);
        return;
    }

    while (true) {
        app_device_state_t state = {0};

        vTaskDelay(pdMS_TO_TICKS(CONFIG_SMARTHOME_LOCAL_STATE_SELF_TEST_INTERVAL_MS));
        if (state_store_get(device->id, &state) == ESP_OK) {
            ESP_LOGI(TAG, "Local self-test toggling %s -> %s",
                     device->id, state.on ? "OFF" : "ON");
            command_router_apply_on(device->id, !state.on, APP_STATE_SOURCE_DEVICE);
        }
    }
}
#endif

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_LOGI(TAG, "Booting native HomeKit migration firmware");

    ESP_ERROR_CHECK(device_registry_init());
    ESP_ERROR_CHECK(state_store_init());
    ESP_ERROR_CHECK(command_router_init());
    ESP_ERROR_CHECK(smarthome_wifi_connect());
    ESP_ERROR_CHECK(hk_bridge_start());

    ESP_LOGI(TAG, "Onboarding mode: %s", smarthome_wifi_onboarding_mode());
    ESP_LOGI(TAG, "Wi-Fi provisioned: %s", smarthome_wifi_is_provisioned() ? "yes" : "no");
    xTaskCreate(local_state_monitor_task, "local_state_monitor", 4096, NULL, 5, NULL);
#if CONFIG_SMARTHOME_LOCAL_STATE_SELF_TEST
    xTaskCreate(local_state_self_test_task, "local_state_self_test", 4096, NULL, 4, NULL);
#endif

    ESP_LOGI(TAG, "HomeKit bridge is running");
}
