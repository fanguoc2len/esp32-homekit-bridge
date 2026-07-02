# Testing Guide

This project supports two verification levels: hardware-free checks for code
quality and firmware builds, then real-board checks when an ESP32 kit is
available.

## Hardware-Free Checks

Run from the repository root:

```bash
python3 scripts/check_no_secrets.py
./idf.sh build
SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.fan_demo.defaults" \
  ./idf.sh -B build-fan -DSDKCONFIG=/tmp/doan2-fan.sdkconfig set-target esp32
SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.fan_demo.defaults" \
  ./idf.sh -B build-fan -DSDKCONFIG=/tmp/doan2-fan.sdkconfig build
SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.led_demo.defaults;sdkconfig.neopixel_demo.defaults" \
  ./idf.sh -B build-neopixel -DSDKCONFIG=/tmp/doan2-neopixel.sdkconfig set-target esp32
SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.led_demo.defaults;sdkconfig.neopixel_demo.defaults" \
  ./idf.sh -B build-neopixel -DSDKCONFIG=/tmp/doan2-neopixel.sdkconfig build
```

Expected result:

- Secret scan passes.
- LED/default firmware links successfully.
- Fan preset links successfully and includes the HomeKit `RotationSpeed` path.
- NeoPixel preset links successfully and includes brightness, hue, saturation,
  and rainbow effect paths.
- Partition check reports enough free app space.

## Apple Home Checks With Hardware

1. Flash the selected preset.
2. Provision Wi-Fi using the serial QR URL or hardcoded bench credentials.
3. Pair with Apple Home using the configured setup code.
4. Confirm the bridge exposes the expected accessories:

| Accessory | Expected behavior |
| --- | --- |
| Primary output | On/off writes update the GPIO or NeoPixel output |
| RGB light | Brightness, hue, and saturation update the NeoPixel color |
| Rainbow switch | Turning on enables cycling color output |
| Fan | `RotationSpeed` above zero turns the relay output on |
| Front Door Lock | Target state writes immediately update current lock state |
| Room Climate | Temperature and humidity change periodically when simulator is on |

## Known Hardware Gaps

- The fan path currently maps speed to relay on/off. A PWM driver can replace
  this without changing the HomeKit service contract.
- The lock and climate sensor are virtual scaffolds until a real lock actuator
  and sensor module are selected.
- Face recognition, PIN login, and custom voice workflows remain outside native
  HomeKit by design.
