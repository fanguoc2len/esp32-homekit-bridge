# DoAn2 HomeKit Roadmap

## Goal

Split Apple Home migration into its own repository so firmware work can move
without colliding with the original Flask/Firebase/Arduino workflow.

## Current Status

- ESP-IDF project boots and pairs with Apple Home
- One GPIO output already works as a HomeKit light/switch path
- NeoPixel RGB light path now builds with native HomeKit brightness and color characteristics
- Fan service path now builds with native HomeKit `RotationSpeed`
- Door lock service path now builds as a HomeKit `Lock Mechanism` scaffold
- Temperature and humidity service paths now build as HomeKit sensor scaffolds
- Wi-Fi onboarding supports hardcoded credentials and provisioning flow
- Build helper script `idf.sh` is included for WSL use
- GitHub Actions build workflow covers LED, fan, and NeoPixel presets

## Migration Order

1. Stabilize single-output LED demo
2. Validate NeoPixel RGB hardware flow on real boards
3. Validate real-board fan relay flow and add a dedicated PWM fan driver
4. Replace virtual lock scaffold with a real lock actuator driver if hardware is selected
5. Replace virtual climate sensor scaffold with a real DHT/SHT/BME-class driver
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
