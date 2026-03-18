#include "sdkconfig.h"

#include "board_profile.h"

#ifdef CONFIG_SMARTHOME_SWITCH_ACTIVE_HIGH
#define SMARTHOME_CFG_SWITCH_ACTIVE_HIGH true
#else
#define SMARTHOME_CFG_SWITCH_ACTIVE_HIGH false
#endif

#ifdef CONFIG_SMARTHOME_OUTPUT_BOOT_ON
#define SMARTHOME_CFG_OUTPUT_BOOT_ON true
#else
#define SMARTHOME_CFG_OUTPUT_BOOT_ON false
#endif

#ifdef CONFIG_SMARTHOME_PRIMARY_OUTPUT_DRIVER_NEOPIXEL
#define SMARTHOME_CFG_OUTPUT_DRIVER 1
#else
#define SMARTHOME_CFG_OUTPUT_DRIVER 0
#endif

#ifdef CONFIG_SMARTHOME_PRIMARY_OUTPUT_SERVICE_LIGHT
#define SMARTHOME_CFG_SERVICE_KIND 1
#elif defined(CONFIG_SMARTHOME_PRIMARY_OUTPUT_SERVICE_FAN)
#define SMARTHOME_CFG_SERVICE_KIND 2
#elif defined(CONFIG_SMARTHOME_PRIMARY_OUTPUT_SERVICE_OUTLET)
#define SMARTHOME_CFG_SERVICE_KIND 4
#else
#define SMARTHOME_CFG_SERVICE_KIND 0
#endif

/*
 * The sample HomeKit switch path intentionally uses its own configurable GPIO.
 * This keeps the migration isolated from the existing Arduino sketch pins while
 * still giving us one real device path end-to-end in native HomeKit.
 */
static const board_profile_t s_board_profile = {
    .bridge_name = CONFIG_SMARTHOME_BRIDGE_NAME,
    .manufacturer = CONFIG_SMARTHOME_MANUFACTURER,
    .model = CONFIG_SMARTHOME_BRIDGE_MODEL,
    .serial_number = CONFIG_SMARTHOME_SERIAL_NUMBER,
    .fw_revision = CONFIG_SMARTHOME_FW_REVISION,
    .primary_switch = {
        .id = CONFIG_SMARTHOME_SWITCH_ID,
        .name = CONFIG_SMARTHOME_SWITCH_NAME,
        .service_kind = SMARTHOME_CFG_SERVICE_KIND,
        .gpio = CONFIG_SMARTHOME_SWITCH_GPIO,
        .output_driver = SMARTHOME_CFG_OUTPUT_DRIVER,
        .active_high = SMARTHOME_CFG_SWITCH_ACTIVE_HIGH,
        .boot_on = SMARTHOME_CFG_OUTPUT_BOOT_ON,
        .enabled = true,
    },
};

const board_profile_t *board_profile_get(void)
{
    return &s_board_profile;
}
