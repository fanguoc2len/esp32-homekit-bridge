# DoAn2 HomeKit Roadmap

## Goal

Split Apple Home migration into its own repository so firmware work can move
without colliding with the original Flask/Firebase/Arduino workflow.

## Current Status

- ESP-IDF project boots and pairs with Apple Home
- One GPIO output already works as a HomeKit light/switch path
- NeoPixel RGB light path now builds with native HomeKit brightness and color characteristics
- Wi-Fi onboarding supports hardcoded credentials and provisioning flow
- Build helper script `idf.sh` is included for WSL use

## Migration Order

1. Stabilize single-output LED demo
2. Validate NeoPixel RGB hardware flow on real boards
3. Add fan service with speed control
4. Add door/lock service path
5. Add temperature and humidity sensor services
6. Decide which non-HomeKit features stay in the old backend

## Features Likely To Stay Outside HomeKit

- face recognition
- admin/PIN login
- custom voice pipeline
- Firebase/web scenes that do not map cleanly to HomeKit

## Features That Map Well To HomeKit

- on/off lights
- brightness and color
- fan speed
- door/lock state
- temperature and humidity
