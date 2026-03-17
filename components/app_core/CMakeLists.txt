idf_component_register(
    SRCS "device_registry.c" "state_store.c" "command_router.c"
    INCLUDE_DIRS "include"
    REQUIRES board_support drivers freertos)
