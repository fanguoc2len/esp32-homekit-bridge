# HomeKit Pairing And Provisioning

## Design

This firmware uses the Espressif ESP HomeKit SDK as the native Apple Home path.

- Wi-Fi onboarding and HomeKit pairing are separate steps.
- First, the ESP32 must get onto Wi-Fi.
- After Wi-Fi is ready, the accessory can be added from Apple Home using the HomeKit setup code or QR payload.

For the open-source SDK path in this repository:

- Preferred onboarding: Espressif Unified Provisioning
- Fallback onboarding: hardcoded Wi-Fi credentials
- Not implemented here as the default path: Apple WAC provisioning, because that requires the MFi SDK path

## First Boot Flow

1. Flash the firmware.
2. Boot the ESP32 and open the serial monitor.
3. If `Unified Provisioning` is enabled:
   - The device prints the provisioning service name and POP.
   - The device prints a provisioning QR URL.
   - Use the Espressif provisioning app or provisioning flow supported by Espressif to send Wi-Fi credentials.
4. After Wi-Fi connects, the firmware prints:
   - HomeKit setup code
   - HomeKit setup ID
   - HomeKit QR payload URL
5. Open Apple Home and add the accessory using the printed setup code or QR payload.

## Re-Pair Flow

If the accessory was already paired and you want to add it again:

1. Remove the accessory from Apple Home first.
2. Clear the accessory's HomeKit pairing state.
3. If Wi-Fi credentials are also stale, clear Wi-Fi credentials too.
4. Reboot the device.
5. Re-run Wi-Fi onboarding if required.
6. Pair again from Apple Home.

## What Must Be Reset When Identity Changes

Do a full accessory reset if any of the following change:

- HomeKit bridge identity or setup information
- accessory database shape
- bridged accessory topology
- accessory category changes, such as `Switch` to `Lightbulb`
- serial-number strategy that changes how Apple Home identifies the accessory

In practice, the safest recovery path is:

1. Remove accessory from Apple Home.
2. Erase HomeKit state on device.
3. Erase Wi-Fi credentials if onboarding must run again.
4. Re-flash and pair as a fresh accessory.

## Practical Recovery Options

- Fastest full reset: `idf.py erase-flash`
- Wi-Fi only reset: call `smarthome_wifi_reset_credentials()` from firmware code or erase the Wi-Fi NVS area
- HomeKit only reset: use the HomeKit factory reset API path when you wire a recovery trigger

## Production Note

For production hardware, avoid relying on hardcoded setup code in source. Use factory NVS generation for setup data and follow Espressif's production guidance.
