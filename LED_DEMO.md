# ESP32 LED HomeKit Demo

This is the fastest path in this repository to get one ESP32 LED visible in Apple Home.

## What this demo does

- Exposes one GPIO output as a native HomeKit `Lightbulb`
- Lets Apple Home toggle that output on and off
- Prints pairing info to the serial monitor after Wi-Fi onboarding

If you want full RGB color controls instead of simple on/off, use `NEOPIXEL_DEMO.md`.

## Hardware

- ESP32 dev board
- Best case: a board with an onboard LED on GPIO 2
- Safer fallback: an external LED plus resistor on a GPIO you choose in `menuconfig`

If your board does not use GPIO 2 for its LED, keep the demo flow but change `Primary switch GPIO` in `menuconfig`.
If your board has no controllable onboard LED at all, then no software-only change can turn the power LED into a HomeKit LED.

## Build prerequisites

You still need:

1. ESP-IDF
2. Espressif `esp-homekit-sdk`
3. Both environment variables exported:

```bash
export IDF_PATH=/path/to/esp-idf
export ESP_HOMEKIT_SDK_PATH=/path/to/esp-homekit-sdk
```

## If you already use the ESP-IDF VS Code extension

That is helpful, but the extension only works after it has a valid ESP-IDF setup selected.

In VS Code:

1. Open `Command Palette`
2. Run `ESP-IDF: Open ESP-IDF Installation Manager`
3. Install ESP-IDF and tools if needed
4. Run `ESP-IDF: Select Current ESP-IDF Version`
5. Run `ESP-IDF: Doctor Command`

Only continue when the Doctor command reports a valid setup.

Then open:

```text
/home/fanguoc2len/code/DoAn2-homekit
```

After that you can either:

- use the ESP-IDF buttons in VS Code for `menuconfig`, `build`, `flash`, `monitor`
- or use the integrated terminal and run `idf.py` manually

## Flash the LED demo

From this repository:

```bash
cd /home/fanguoc2len/code/DoAn2-homekit
export SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.led_demo.defaults"
idf.py set-target esp32
idf.py menuconfig
idf.py build
idf.py flash monitor
```

## What to set in menuconfig

Open `DoAn2 HomeKit Config` and confirm:

- `Wi-Fi onboarding mode`: `Unified Provisioning`
- `Provisioning transport`: `SoftAP provisioning`
- `Primary switch GPIO`: `2` for the LED demo, unless your board uses another pin
- `Primary switch is active high`: keep enabled unless your LED or relay is active-low
- `Primary output HomeKit service type`: `Lightbulb`
- `Primary output boots ON`: disabled

## How pairing works

There are 2 separate steps:

1. Put the ESP32 onto Wi-Fi
2. Add the accessory in Apple Home

### Step 1: Wi-Fi onboarding

After boot, the serial monitor will print:

- provisioning service name
- provisioning POP
- provisioning QR URL

Use the Espressif provisioning flow to send Wi-Fi credentials to the ESP32.

### Step 2: Apple Home pairing

After Wi-Fi connects, the serial monitor will print:

- HomeKit setup code
- HomeKit setup ID
- HomeKit QR payload

On iPhone:

1. Open `Home`
2. Tap `+`
3. Tap `Add Accessory`
4. Scan the printed QR or enter the setup code

When pairing succeeds, toggling the accessory in Apple Home should toggle the LED.

## First test

1. Flash the demo
2. Provision Wi-Fi
3. Pair from Apple Home
4. Tap the accessory on and off
5. Confirm the LED follows the Home app state

## If you need to pair again

1. Remove the accessory from Apple Home
2. Run:

```bash
idf.py erase-flash
```

3. Flash again
4. Reprovision Wi-Fi
5. Pair again from Apple Home
