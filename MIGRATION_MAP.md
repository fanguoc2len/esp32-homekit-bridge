# DoAn2 To Apple Home Migration Map

## Purpose

This document maps the original DoAn2 device model to Apple Home / HomeKit
service types so implementation can move device by device without guessing the
target shape every time.

## Existing DoAn2 Device Types

The original system currently contains these major feature groups:

- light
- fan
- speaker
- temperature / humidity sensor
- fixed doors
- face recognition
- PIN / admin auth
- voice control
- Firebase scenes and automation glue

## HomeKit Mapping

### 1. Light / NeoPixel

Original capabilities:

- on/off
- brightness
- color
- rainbow effect

Best HomeKit target:

- `Lightbulb`

Native characteristics that map well:

- `On`
- `Brightness`
- `Hue`
- `Saturation`

Not a clean native HomeKit match:

- `rainbow` / party mode

Recommendation:

- migrate on/off + brightness + color first
- keep custom effect modes as optional firmware-side extensions outside the
  standard Apple Home UI

### 2. Fan

Original capabilities:

- on/off
- speed 0..3

Best HomeKit target:

- `Fan`
- or `Fanv2` if the SDK path later supports the exact profile cleanly

Native characteristics that map well:

- `On`
- `RotationSpeed`

### 3. Door / Servo Door

Original capabilities:

- open / closed

Possible HomeKit targets:

- `LockMechanism` if this is logically a lock
- `GarageDoorOpener` if this behaves like a gate / garage door
- fallback `Switch` if we want the fastest prototype path first

Recommendation:

- choose the HomeKit service based on real product meaning, not just the servo

### 4. Temperature / Humidity

Original capabilities:

- temperature
- humidity

Best HomeKit targets:

- `TemperatureSensor`
- `HumiditySensor`

This is one of the cleanest migrations.

### 5. Speaker / Buzzer / JQ6500

Original capabilities:

- on/off
- volume
- track index

HomeKit fit:

- weak

Possible fallback:

- expose only a basic `Switch` or `Outlet` if needed

Recommendation:

- do not make this an early migration target

### 6. Face Recognition / PIN / Voice / Web Admin

These are application and backend workflows, not native Apple Home accessory
types.

Recommendation:

- keep them outside HomeKit
- let HomeKit cover device control
- let the original backend continue handling identity, auth, AI, and custom UX

## Proposed Migration Order

1. Single light path
2. NeoPixel RGB light path
3. Fan speed path
4. Temperature / humidity path
5. Door / lock path
6. Optional speaker fallback path

## Architecture Notes

- Apple Home expects a stable accessory database.
- The original DoAn2 firmware discovers devices dynamically from Firebase.
- For HomeKit, dynamic discovery should be replaced with a stable accessory
  definition per board profile or a controlled persisted registry.

## Decision Summary

- Good HomeKit candidates: lights, fans, sensors, doors/locks
- Weak HomeKit candidates: speakers/media behavior
- Non-HomeKit candidates: face recognition, PIN login, voice backend, admin UI
