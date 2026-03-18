# ESP32 NeoPixel HomeKit Demo

This is the fastest path in this repository to expose one NeoPixel / WS2812 light
to Apple Home as a real RGB `Lightbulb`.

## What this demo does

- Exposes one bridged HomeKit light accessory
- Lets Apple Home toggle power on and off
- Lets Apple Home change `Brightness`
- Lets Apple Home change `Hue`
- Lets Apple Home change `Saturation`

## Hardware

- ESP32 dev board
- One NeoPixel / WS2812 / WS2812B pixel or strip
- One GPIO line for the NeoPixel data input

Recommended wiring:

- NeoPixel `DIN` -> chosen ESP32 GPIO
- NeoPixel `GND` -> ESP32 `GND`
- NeoPixel `5V` or `VCC` -> appropriate power source for your LED

If you use a full 5V strip, a level shifter is cleaner for production. For a
single-pixel desk demo, many ESP32 boards still work directly on the data line.

## Build flow

```bash
cd /home/fanguoc2len/code/DoAn2-homekit
export SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.neopixel_demo.defaults"
./idf.sh set-target esp32
./idf.sh menuconfig
./idf.sh build
./idf.sh -p /dev/ttyUSB0 flash monitor
```

## What to set in menuconfig

Open `DoAn2 HomeKit Config` and confirm:

- `Wi-Fi onboarding mode`: choose either hardcoded Wi-Fi or provisioning
- `Primary switch GPIO`: set this to the NeoPixel data pin you wired
- `Primary output hardware driver`: `NeoPixel / WS2812 RGB`
- `Primary output HomeKit service type`: `Lightbulb`
- `Primary output boots ON`: optional

`Primary switch is active high` does not matter for NeoPixel mode.

## Apple Home pairing note

If you already paired the old single on/off LED accessory, remove it from Apple
Home first. The NeoPixel path adds new HomeKit characteristics, so the accessory
database changes and the old pairing should not be reused.

Recommended clean flow:

1. Remove the accessory from Apple Home
2. Run `./idf.sh erase-flash`
3. Flash the NeoPixel firmware
4. Reprovision Wi-Fi if needed
5. Pair again from Apple Home

## What you should see

After Wi-Fi is ready, the serial monitor prints:

- HomeKit setup code
- HomeKit setup ID
- HomeKit QR

Inside Apple Home, the accessory should now support:

- tap on/off
- brightness slider
- color picker

When those controls change in Apple Home, the NeoPixel color should follow.
