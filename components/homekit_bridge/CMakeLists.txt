idf_component_register(
    SRCS "hk_bridge.c" "hk_pairing_info.c"
    INCLUDE_DIRS "include"
    REQUIRES app_core board_support qrcode esp_hap_core esp_hap_apple_profiles)
