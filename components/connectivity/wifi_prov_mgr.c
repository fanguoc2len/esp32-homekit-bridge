#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_event.h>
#include <esp_idf_version.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <wifi_provisioning/manager.h>

#if CONFIG_SMARTHOME_WIFI_PROV_TRANSPORT_BLE
#include <wifi_provisioning/scheme_ble.h>
#else
#include <wifi_provisioning/scheme_softap.h>
#endif

#include "sdkconfig.h"

#include "wifi_prov_mgr.h"

#ifdef CONFIG_SMARTHOME_WIFI_SSID
#define SMARTHOME_CFG_WIFI_SSID CONFIG_SMARTHOME_WIFI_SSID
#else
#define SMARTHOME_CFG_WIFI_SSID ""
#endif

#ifdef CONFIG_SMARTHOME_WIFI_PASSWORD
#define SMARTHOME_CFG_WIFI_PASSWORD CONFIG_SMARTHOME_WIFI_PASSWORD
#else
#define SMARTHOME_CFG_WIFI_PASSWORD ""
#endif

#ifdef CONFIG_SMARTHOME_WIFI_PROV_SERVICE_PREFIX
#define SMARTHOME_CFG_WIFI_PROV_SERVICE_PREFIX CONFIG_SMARTHOME_WIFI_PROV_SERVICE_PREFIX
#else
#define SMARTHOME_CFG_WIFI_PROV_SERVICE_PREFIX "DOAN2_"
#endif

static const char *TAG = "wifi_prov_mgr";
static EventGroupHandle_t s_wifi_event_group;
static bool s_wifi_connected;
static bool s_wifi_provisioned;
static bool s_wifi_started_provisioning;
static bool s_wifi_prov_cred_failed;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAILED_BIT    BIT1
#define WIFI_MAX_RETRIES   10

#define PROV_QR_VERSION "v1"
#define PROV_QR_BASE_URL "https://espressif.github.io/esp-jumpstart/qrcode.html"

static void wifi_prov_mgr_print_qr_url(const char *service_name, const char *pop)
{
#if CONFIG_SMARTHOME_WIFI_PROV_SHOW_QR
    char payload[160];

    if (!service_name || !pop) {
        return;
    }

    snprintf(payload, sizeof(payload),
             "{\"ver\":\"%s\",\"name\":\"%s\",\"pop\":\"%s\",\"transport\":\"%s\"}",
             PROV_QR_VERSION,
             service_name,
             pop,
#if CONFIG_SMARTHOME_WIFI_PROV_TRANSPORT_BLE
             "ble");
#else
             "softap");
#endif

    ESP_LOGI(TAG, "Provisioning QR URL:");
    ESP_LOGI(TAG, "%s?data=%s", PROV_QR_BASE_URL, payload);
#endif
}

static void wifi_prov_mgr_get_service_name(char *service_name, size_t max_len)
{
    uint8_t mac[6];

    memset(mac, 0, sizeof(mac));
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(service_name, max_len, "%s%02X%02X%02X",
             SMARTHOME_CFG_WIFI_PROV_SERVICE_PREFIX,
             mac[3], mac[4], mac[5]);
}

static esp_err_t wifi_prov_mgr_get_pop(char *pop, size_t max_len)
{
    uint8_t mac[6];
    esp_err_t err;

    if (!pop || max_len < 9) {
        return ESP_ERR_INVALID_ARG;
    }

    err = esp_wifi_get_mac(WIFI_IF_STA, mac);
    if (err != ESP_OK) {
        return err;
    }

    snprintf(pop, max_len, "%02x%02x%02x%02x", mac[2], mac[3], mac[4], mac[5]);
    return ESP_OK;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    static int retry_count;

    (void) arg;
    (void) event_data;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_connected = false;
        if (retry_count < WIFI_MAX_RETRIES) {
            retry_count++;
            esp_wifi_connect();
            ESP_LOGW(TAG, "Retrying Wi-Fi connection (%d/%d)", retry_count, WIFI_MAX_RETRIES);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAILED_BIT);
        }
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        retry_count = 0;
        s_wifi_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "Wi-Fi connected");
        return;
    }

    if (event_base == WIFI_PROV_EVENT) {
        switch (event_id) {
            case WIFI_PROV_START:
                ESP_LOGI(TAG, "Unified Provisioning started");
                break;
            case WIFI_PROV_CRED_RECV: {
                wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *) event_data;
                ESP_LOGI(TAG, "Received Wi-Fi credentials for SSID: %s", (const char *) wifi_sta_cfg->ssid);
                break;
            }
            case WIFI_PROV_CRED_FAIL: {
                wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *) event_data;
                s_wifi_prov_cred_failed = true;
                ESP_LOGE(TAG, "Provisioning failed: %s",
                         (*reason == WIFI_PROV_STA_AUTH_ERROR) ? "auth error" : "AP not found");
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAILED_BIT);
                break;
            }
            case WIFI_PROV_CRED_SUCCESS:
                ESP_LOGI(TAG, "Provisioning successful");
                s_wifi_prov_cred_failed = false;
                s_wifi_provisioned = true;
                break;
            case WIFI_PROV_END:
                wifi_prov_mgr_deinit();
                break;
            default:
                break;
        }
    }
}

static esp_err_t wifi_prov_mgr_base_init(void)
{
    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();

    if (!s_wifi_event_group) {
        s_wifi_event_group = xEventGroupCreate();
        if (!s_wifi_event_group) {
            return ESP_ERR_NO_MEM;
        }
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));
    return ESP_OK;
}

#if CONFIG_SMARTHOME_WIFI_ONBOARDING_HARDCODED
static esp_err_t wifi_prov_mgr_start_hardcoded(void)
{
    wifi_config_t wifi_cfg = {0};
    wifi_auth_mode_t min_auth = WIFI_AUTH_WPA2_PSK;

    if (!SMARTHOME_CFG_WIFI_SSID[0]) {
        ESP_LOGE(TAG, "Wi-Fi SSID is empty. Set CONFIG_SMARTHOME_WIFI_SSID in menuconfig.");
        return ESP_ERR_INVALID_STATE;
    }

    if (!SMARTHOME_CFG_WIFI_PASSWORD[0]) {
        min_auth = WIFI_AUTH_OPEN;
    }

    strlcpy((char *) wifi_cfg.sta.ssid, SMARTHOME_CFG_WIFI_SSID, sizeof(wifi_cfg.sta.ssid));
    strlcpy((char *) wifi_cfg.sta.password, SMARTHOME_CFG_WIFI_PASSWORD, sizeof(wifi_cfg.sta.password));
    wifi_cfg.sta.threshold.authmode = min_auth;
    wifi_cfg.sta.pmf_cfg.capable = true;
    wifi_cfg.sta.pmf_cfg.required = false;

    ESP_LOGI(TAG, "Wi-Fi onboarding mode: hardcoded credentials");
    s_wifi_provisioned = true;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    return ESP_OK;
}
#endif

#if CONFIG_SMARTHOME_WIFI_ONBOARDING_PROVISIONING
static esp_err_t wifi_prov_mgr_start_provisioning_flow(void)
{
    wifi_prov_mgr_config_t config = {
#if CONFIG_SMARTHOME_WIFI_PROV_TRANSPORT_BLE
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM,
#else
        .scheme = wifi_prov_scheme_softap,
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE,
#endif
    };
    bool provisioned = false;
    char service_name[20];
    char pop[9];

    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));
    s_wifi_provisioned = provisioned;
    s_wifi_started_provisioning = false;
    s_wifi_prov_cred_failed = false;

    if (provisioned) {
        ESP_LOGI(TAG, "Wi-Fi already provisioned, starting STA");
        wifi_prov_mgr_deinit();
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Wi-Fi onboarding mode: Unified Provisioning");
#if CONFIG_SMARTHOME_WIFI_PROV_TRANSPORT_SOFTAP
    esp_netif_create_default_wifi_ap();
#endif

    wifi_prov_mgr_get_service_name(service_name, sizeof(service_name));
    ESP_ERROR_CHECK(wifi_prov_mgr_get_pop(pop, sizeof(pop)));

#if CONFIG_SMARTHOME_WIFI_PROV_TRANSPORT_BLE
    {
        uint8_t custom_service_uuid[] = {
            0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf,
            0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02,
        };
        ESP_ERROR_CHECK(wifi_prov_scheme_ble_set_service_uuid(custom_service_uuid));
    }
#endif

    ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1, pop, service_name, NULL));
    s_wifi_started_provisioning = true;
    ESP_LOGI(TAG, "Provisioning service: %s", service_name);
    ESP_LOGI(TAG, "Provisioning POP: %s", pop);
    wifi_prov_mgr_print_qr_url(service_name, pop);
    return ESP_OK;
}
#endif

esp_err_t smarthome_wifi_connect(void)
{
    EventBits_t bits;

    s_wifi_connected = false;
    s_wifi_provisioned = false;
    s_wifi_started_provisioning = false;
    s_wifi_prov_cred_failed = false;

    ESP_ERROR_CHECK(wifi_prov_mgr_base_init());

#if CONFIG_SMARTHOME_WIFI_ONBOARDING_HARDCODED
    ESP_ERROR_CHECK(wifi_prov_mgr_start_hardcoded());
#else
    ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning_flow());
#endif

    bits = xEventGroupWaitBits(s_wifi_event_group,
                               WIFI_CONNECTED_BIT | WIFI_FAILED_BIT,
                               pdFALSE,
                               pdFALSE,
                               portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        return ESP_OK;
    }

#if CONFIG_SMARTHOME_WIFI_ONBOARDING_PROVISIONING
    if (s_wifi_started_provisioning && s_wifi_prov_cred_failed) {
        ESP_LOGW(TAG, "Provisioning credentials failed. Clearing stored provisioning data so the next boot can reprovision cleanly.");
        wifi_prov_mgr_reset_provisioning();
    }
#endif
    ESP_LOGE(TAG, "Wi-Fi connection failed after retries");
    return ESP_FAIL;
}

bool smarthome_wifi_is_connected(void)
{
    return s_wifi_connected;
}

bool smarthome_wifi_is_provisioned(void)
{
    return s_wifi_provisioned;
}

const char *smarthome_wifi_onboarding_mode(void)
{
#if CONFIG_SMARTHOME_WIFI_ONBOARDING_HARDCODED
    return "hardcoded";
#else
#if CONFIG_SMARTHOME_WIFI_PROV_TRANSPORT_BLE
    return "unified-ble";
#else
    return "unified-softap";
#endif
#endif
}

esp_err_t smarthome_wifi_reset_credentials(void)
{
    s_wifi_connected = false;
    s_wifi_provisioned = false;
#if CONFIG_SMARTHOME_WIFI_ONBOARDING_PROVISIONING
    return wifi_prov_mgr_reset_provisioning();
#else
    return esp_wifi_restore();
#endif
}
