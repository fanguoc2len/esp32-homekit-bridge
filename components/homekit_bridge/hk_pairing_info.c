#include <stdlib.h>

#include <esp_log.h>
#include <hap.h>
#include <qrcode.h>

#include "hk_pairing_info.h"

static const char *TAG = "hk_pairing_info";
static const char *HAP_QR_BASE_URL = "https://espressif.github.io/esp-homekit-sdk/qrcode.html";

esp_err_t hk_pairing_info_print(const char *accessory_name,
                                const char *setup_code,
                                const char *setup_id,
                                hap_cid_t cid)
{
    char *setup_payload;

    if (!setup_code || !setup_id) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "HomeKit accessory: %s", accessory_name ? accessory_name : "unknown");
    ESP_LOGI(TAG, "HomeKit setup code: %s", setup_code);
    ESP_LOGI(TAG, "HomeKit setup id: %s", setup_id);

    setup_payload = esp_hap_get_setup_payload((char *) setup_code, (char *) setup_id, false, cid);
    if (!setup_payload) {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "----- QR Code for HomeKit -----");
    ESP_LOGI(TAG, "Scan this QR code from Apple Home after Wi-Fi onboarding is complete.");
    qrcode_display(setup_payload);
    ESP_LOGI(TAG, "If the QR is not visible, open:");
    ESP_LOGI(TAG, "%s?data=%s", HAP_QR_BASE_URL, setup_payload);

    free(setup_payload);
    return ESP_OK;
}
