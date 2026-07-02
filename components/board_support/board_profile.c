#include "sdkconfig.h"

#include "board_profile.h"

#ifndef CONFIG_SMARTHOME_LOCK_ID
#define CONFIG_SMARTHOME_LOCK_ID "front_door_lock"
#endif

#ifndef CONFIG_SMARTHOME_LOCK_NAME
#define CONFIG_SMARTHOME_LOCK_NAME "Front Door Lock"
#endif

#ifndef CONFIG_SMARTHOME_ENV_SENSOR_ID
#define CONFIG_SMARTHOME_ENV_SENSOR_ID "environment_sensor"
#endif

#ifndef CONFIG_SMARTHOME_ENV_SENSOR_NAME
#define CONFIG_SMARTHOME_ENV_SENSOR_NAME "Room Climate"
#endif

#ifndef CONFIG_SMARTHOME_ENV_TEMPERATURE_C_X10
#define CONFIG_SMARTHOME_ENV_TEMPERATURE_C_X10 275
#endif

#ifndef CONFIG_SMARTHOME_ENV_HUMIDITY_PERCENT_X10
#define CONFIG_SMARTHOME_ENV_HUMIDITY_PERCENT_X10 650
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

#ifdef CONFIG_SMARTHOME_ENABLE_VIRTUAL_LOCK
#define SMARTHOME_CFG_ENABLE_VIRTUAL_LOCK true
#else
#define SMARTHOME_CFG_ENABLE_VIRTUAL_LOCK false
#endif

#ifdef CONFIG_SMARTHOME_LOCK_BOOT_LOCKED
#define SMARTHOME_CFG_LOCK_BOOT_LOCKED true
#else
#define SMARTHOME_CFG_LOCK_BOOT_LOCKED false
#endif

#ifdef CONFIG_SMARTHOME_ENABLE_ENV_SENSOR
#define SMARTHOME_CFG_ENABLE_ENV_SENSOR true
#else
#define SMARTHOME_CFG_ENABLE_ENV_SENSOR false
#endif

#ifdef CONFIG_SMARTHOME_ENV_SENSOR_TEMPERATURE
#define SMARTHOME_CFG_ENV_SENSOR_TEMPERATURE true
#else
#define SMARTHOME_CFG_ENV_SENSOR_TEMPERATURE false
#endif

#ifdef CONFIG_SMARTHOME_ENV_SENSOR_HUMIDITY
#define SMARTHOME_CFG_ENV_SENSOR_HUMIDITY true
#else
#define SMARTHOME_CFG_ENV_SENSOR_HUMIDITY false
#endif

#ifdef CONFIG_SMARTHOME_ENV_SENSOR_SIMULATOR
#define SMARTHOME_CFG_ENV_SENSOR_SIMULATOR true
#else
#define SMARTHOME_CFG_ENV_SENSOR_SIMULATOR false
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
    .door_lock = {
        .id = CONFIG_SMARTHOME_LOCK_ID,
        .name = CONFIG_SMARTHOME_LOCK_NAME,
        .enabled = SMARTHOME_CFG_ENABLE_VIRTUAL_LOCK,
        .boot_locked = SMARTHOME_CFG_LOCK_BOOT_LOCKED,
    },
    .environment_sensor = {
        .id = CONFIG_SMARTHOME_ENV_SENSOR_ID,
        .name = CONFIG_SMARTHOME_ENV_SENSOR_NAME,
        .enabled = SMARTHOME_CFG_ENABLE_ENV_SENSOR,
        .supports_temperature = SMARTHOME_CFG_ENV_SENSOR_TEMPERATURE,
        .supports_humidity = SMARTHOME_CFG_ENV_SENSOR_HUMIDITY,
        .simulator_enabled = SMARTHOME_CFG_ENV_SENSOR_SIMULATOR,
        .initial_temperature_c = CONFIG_SMARTHOME_ENV_TEMPERATURE_C_X10 / 10.0f,
        .initial_humidity_percent = CONFIG_SMARTHOME_ENV_HUMIDITY_PERCENT_X10 / 10.0f,
    },
};

const board_profile_t *board_profile_get(void)
{
    return &s_board_profile;
}
