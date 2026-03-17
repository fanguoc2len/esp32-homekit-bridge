# DoAn2 HomeKit

This is the standalone Apple Home migration repository for the DoAn2 smart home project.
It extracts the HomeKit firmware work into its own ESP-IDF repo so the migration can move
without being mixed into the original Flask, Firebase, and Arduino workflow.

## Scope today

- Phase 1: ESP-IDF + HomeKit project skeleton
- Phase 2: one real GPIO output path mapped to Apple Home as a native HomeKit light or switch
- Phase 3: remaining devices migrate one by one into the same architecture

## What stays in the old repo

- Existing firmware stays in `sketch_nov16c.ino`
- Existing backend/web workflow stays untouched
- Face recognition, PIN login, and custom voice logic stay outside HomeKit for now

## What lives in this repo

- ESP-IDF firmware for Apple Home migration
- Hardware drivers in `components/drivers` and `components/board_support`
- HomeKit service logic in `components/homekit_bridge`
- Pairing and onboarding docs in `PAIRING.md`
- LED quickstart in `LED_DEMO.md`
- Migration plan in `ROADMAP.md`

## Build prerequisites

1. Install ESP-IDF for ESP32.
2. Clone the official Espressif ESP HomeKit SDK.
3. Export both environment variables:

```bash
export IDF_PATH=/path/to/esp-idf
export ESP_HOMEKIT_SDK_PATH=/path/to/esp-homekit-sdk
```

## Build

```bash
./idf.sh set-target esp32
./idf.sh menuconfig
./idf.sh build
./idf.sh -p /dev/ttyUSB0 flash monitor
```

See `PAIRING.md` for first-boot onboarding, Home app pairing, and recovery/reset flow.
See `LED_DEMO.md` for the quickest Apple Home demo using a single ESP32 LED.
See `ROADMAP.md` for the migration sequence from the original DoAn2 system.

## Notes

- `main/Kconfig.projbuild` exposes Wi-Fi credentials, HomeKit setup code, and the sample switch GPIO.
- The primary output can be exposed as a HomeKit `Lightbulb` or `Switch`. The default is `Lightbulb`
  because the current project is light-centric.
- The sample GPIO switch is intentionally isolated from the current Arduino pins so the migration can
  be tested without breaking the existing runtime.
- `SMARTHOME_LOCAL_STATE_SELF_TEST` is an optional hardware test helper. When enabled, firmware
  toggles the output periodically so you can verify HomeKit receives device-side state changes.
- `components/connectivity/wifi_prov_mgr.c` now supports either hardcoded Wi-Fi or Espressif Unified
  Provisioning for first boot. Apple Home pairing happens only after Wi-Fi onboarding completes.
- The next migration target should reuse the same app core for lights, fan, relay appliances, and sensors.
