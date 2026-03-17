idf_component_register(
    SRCS "wifi_prov_mgr.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_event esp_netif esp_wifi freertos wifi_provisioning esp_hap_core esp_hap_platform nvs_flash)
