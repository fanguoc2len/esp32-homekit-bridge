# ESP32 Fan HomeKit Demo

This demo exposes one ESP32 output as a HomeKit `Fan` with native
`RotationSpeed` support in Apple Home.

## What this demo does

- Exposes one bridged HomeKit fan accessory
- Lets Apple Home toggle the fan on and off
- Lets Apple Home change `RotationSpeed`
- Maps any speed above zero to an active hardware output line
- Also exposes the virtual door lock and room climate scaffolds by default

## Hardware

- ESP32 dev board
- Simplest bench test: an onboard LED on GPIO 2 so you can at least see ON/OFF
- Real fan/relay test: your relay input or transistor gate on the GPIO you choose

Important note: this milestone is a HomeKit and state-model scaffold first.
With the current plain GPIO driver, `RotationSpeed` is preserved in HomeKit state,
but hardware still behaves like:

- speed `0` -> output off
- speed `1..100` -> output on

That is enough to prove Apple Home fan controls round-trip through the ESP32
before a dedicated PWM fan driver is added.

## Build flow

```bash
cd /path/to/esp32-homekit-bridge
export SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.fan_demo.defaults"
./idf.sh set-target esp32
./idf.sh menuconfig
./idf.sh build
./idf.sh -p /dev/ttyUSB0 flash monitor
```

## What to set in menuconfig

Open `DoAn2 HomeKit Config` and confirm:

- `Wi-Fi onboarding mode`: choose either hardcoded Wi-Fi or provisioning
- `Primary switch GPIO`: set this to your relay or test output pin
- `Primary output hardware driver`: `Plain GPIO on/off`
- `Primary output HomeKit service type`: `Fan`
- `Primary output boots ON`: optional, usually keep off
- `Virtual migration devices`: keep enabled for a complete multi-accessory bridge,
  or disable them if you only want the fan accessory

If you are only bench-testing with an onboard LED, leave the GPIO at `2` if your
board uses that pin for a controllable LED.

## Apple Home behavior

Inside Apple Home, the accessory should show up as a fan. You should be able to:

- tap the fan on and off
- adjust the speed slider

On this milestone, changing the slider updates HomeKit state on the ESP32 and logs
the selected speed, but the default GPIO driver only turns the output line on or off.
The virtual lock and room climate accessories should also appear unless disabled
under `Virtual migration devices`.

## When to use this demo

Use this demo if you want to validate:

- HomeKit `Fan` service integration
- `RotationSpeed` writes from Apple Home
- future migration path for the original DoAn2 fan devices

If you want full RGB color controls instead, use `NEOPIXEL_DEMO.md`.
