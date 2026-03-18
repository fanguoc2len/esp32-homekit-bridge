#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "device_registry.h"
#include "state_store.h"

typedef struct {
    const app_device_config_t *device;
    app_device_state_t state;
    bool used;
} state_entry_t;

typedef struct {
    state_store_observer_t observer;
    void *ctx;
} observer_entry_t;

static SemaphoreHandle_t s_state_lock;
static state_entry_t s_entries[APP_MAX_DEVICES];
static observer_entry_t s_observers[APP_MAX_OBSERVERS];

static bool state_store_state_equal(const app_device_state_t *lhs, const app_device_state_t *rhs)
{
    if (!lhs || !rhs) {
        return false;
    }

    return lhs->on == rhs->on
        && lhs->brightness == rhs->brightness
        && lhs->hue == rhs->hue
        && lhs->saturation == rhs->saturation
        && lhs->rotation_speed == rhs->rotation_speed
        && lhs->temperature_c == rhs->temperature_c
        && lhs->humidity_percent == rhs->humidity_percent;
}

static state_entry_t *state_store_find_entry(const char *device_id)
{
    size_t i;

    for (i = 0; i < APP_MAX_DEVICES; i++) {
        if (s_entries[i].used && !strcmp(s_entries[i].device->id, device_id)) {
            return &s_entries[i];
        }
    }
    return NULL;
}

static void state_store_notify(const app_device_config_t *device,
                               const app_device_state_t *state,
                               app_state_source_t source)
{
    size_t i;

    for (i = 0; i < APP_MAX_OBSERVERS; i++) {
        if (s_observers[i].observer) {
            s_observers[i].observer(device, state, source, s_observers[i].ctx);
        }
    }
}

esp_err_t state_store_init(void)
{
    size_t i;

    memset(s_entries, 0, sizeof(s_entries));
    memset(s_observers, 0, sizeof(s_observers));

    if (!s_state_lock) {
        s_state_lock = xSemaphoreCreateMutex();
        if (!s_state_lock) {
            return ESP_ERR_NO_MEM;
        }
    }

    for (i = 0; i < device_registry_count() && i < APP_MAX_DEVICES; i++) {
        s_entries[i].device = device_registry_get(i);
        s_entries[i].used = (s_entries[i].device != NULL);
        if (s_entries[i].used) {
            s_entries[i].state.on = s_entries[i].device->boot_on;
            s_entries[i].state.brightness = 100;
            s_entries[i].state.hue = 0.0f;
            s_entries[i].state.saturation = 0.0f;
            s_entries[i].state.rotation_speed = s_entries[i].device->boot_on ? 100 : 0;
        }
    }

    return ESP_OK;
}

esp_err_t state_store_register_observer(state_store_observer_t observer, void *ctx)
{
    size_t i;

    if (!observer) {
        return ESP_ERR_INVALID_ARG;
    }

    for (i = 0; i < APP_MAX_OBSERVERS; i++) {
        if (!s_observers[i].observer) {
            s_observers[i].observer = observer;
            s_observers[i].ctx = ctx;
            return ESP_OK;
        }
    }
    return ESP_ERR_NO_MEM;
}

esp_err_t state_store_set_on(const char *device_id, bool on, app_state_source_t source)
{
    app_device_state_t state = {0};

    if (!device_id) {
        return ESP_ERR_INVALID_ARG;
    }

    if (state_store_get(device_id, &state) != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }
    state.on = on;
    return state_store_set_state(device_id, &state, source);
}

esp_err_t state_store_set_state(const char *device_id,
                                const app_device_state_t *state,
                                app_state_source_t source)
{
    state_entry_t *entry;
    app_device_state_t snapshot;
    const app_device_config_t *device;
    bool changed;

    if (!device_id || !state) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_state_lock, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    entry = state_store_find_entry(device_id);
    if (!entry) {
        xSemaphoreGive(s_state_lock);
        return ESP_ERR_NOT_FOUND;
    }

    changed = !state_store_state_equal(&entry->state, state);
    entry->state = *state;
    snapshot = entry->state;
    device = entry->device;

    xSemaphoreGive(s_state_lock);

    if (changed) {
        state_store_notify(device, &snapshot, source);
    }

    return ESP_OK;
}

esp_err_t state_store_get(const char *device_id, app_device_state_t *state_out)
{
    state_entry_t *entry;

    if (!device_id || !state_out) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_state_lock, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    entry = state_store_find_entry(device_id);
    if (!entry) {
        xSemaphoreGive(s_state_lock);
        return ESP_ERR_NOT_FOUND;
    }

    *state_out = entry->state;
    xSemaphoreGive(s_state_lock);
    return ESP_OK;
}
