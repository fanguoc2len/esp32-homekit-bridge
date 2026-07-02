# Architecture

DoAn2 HomeKit is split into small firmware layers so each new device type can
be migrated without rewriting the Apple Home bridge.

## Runtime Flow

```text
Apple Home write
  -> homekit_bridge
  -> command_router
  -> driver or virtual scaffold
  -> state_store
  -> homekit_bridge observer
  -> Apple Home event update
```

Local hardware or simulator updates travel through the same `state_store`
observer path, so HomeKit receives consistent characteristic updates no matter
where the state change started.

## Modules

| Module | Responsibility |
| --- | --- |
| `main` | Boot order, NVS init, Wi-Fi connection, HomeKit startup, monitor tasks |
| `components/connectivity` | Hardcoded Wi-Fi and Unified Provisioning onboarding |
| `components/board_support` | Kconfig-backed board profile and demo device profile |
| `components/app_core` | Device registry, normalized state storage, command routing |
| `components/drivers` | Hardware-specific GPIO and NeoPixel adapters |
| `components/homekit_bridge` | HomeKit accessories, services, characteristics, callbacks |

## Device Model

`device_registry` creates all logical devices from the board profile:

- Primary output: switch, light, fan, or outlet
- NeoPixel rainbow effect switch when RGB is enabled
- Virtual door lock for the HomeKit lock path
- Virtual room climate sensor for temperature and humidity paths

Every device has one `app_device_state_t` snapshot. Capabilities on
`app_device_config_t` decide which fields are meaningful for that device.

## HomeKit Bridge

`hk_bridge.c` maps each device kind to the matching Apple service:

- `Lightbulb`, `Switch`, `Outlet`, and `Fan` for output devices
- `Lock Mechanism` for the door lock scaffold
- `Temperature Sensor` and `Humidity Sensor` services on one climate accessory

HomeKit writes are validated, routed through `command_router`, then reflected
back to characteristics using the applied state from `state_store`.

## Hardware-Free Scaffolds

The lock and sensor paths are virtual on purpose. They prove the HomeKit service
contract, callback flow, and state synchronization before real actuators or
sensors are available. Replacing them with hardware later should only require a
new driver plus a small router branch, not a new bridge design.
