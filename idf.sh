#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

export IDF_PATH="/home/fanguoc2len/esp/esp-idf-v5.5.2"
export IDF_TOOLS_PATH="/home/fanguoc2len/.espressif"
export ESP_HOMEKIT_SDK_PATH="/home/fanguoc2len/esp/esp-homekit-sdk"
# Default to the simplest LED demo, but let callers override this for
# NeoPixel, fan, or future hardware presets.
export SDKCONFIG_DEFAULTS="${SDKCONFIG_DEFAULTS:-sdkconfig.defaults;sdkconfig.led_demo.defaults}"
export XDG_CACHE_HOME="${XDG_CACHE_HOME:-/tmp/esp-idf-cache}"
export LD_LIBRARY_PATH="/home/fanguoc2len/.local/libusb-1.0-0/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

mkdir -p "$XDG_CACHE_HOME"

# shellcheck disable=SC1091
. "$IDF_PATH/export.sh" >/dev/null 2>&1

cd "$SCRIPT_DIR"
idf.py "$@"
