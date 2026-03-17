#include "sdkconfig.h"

#include "board_profile.h"

#ifdef CONFIG_SMARTHOME_OUTPUT_AS_LIGHTBULB
#define SMARTHOME_CFG_OUTPUT_AS_LIGHTBULB true
#else
#define SMARTHOME_CFG_OUTPUT_AS_LIGHTBULB false
#endif

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
        .homekit_lightbulb = SMARTHOME_CFG_OUTPUT_AS_LIGHTBULB,
        .gpio = CONFIG_SMARTHOME_SWITCH_GPIO,
        .active_high = SMARTHOME_CFG_SWITCH_ACTIVE_HIGH,
        .boot_on = SMARTHOME_CFG_OUTPUT_BOOT_ON,
        .enabled = true,
    },
};

const board_profile_t *board_profile_get(void)
{
    return &s_board_profile;
}
