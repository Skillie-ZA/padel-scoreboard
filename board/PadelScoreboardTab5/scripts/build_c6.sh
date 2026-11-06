#!/usr/bin/env bash
# Build the ESP32-C6 slave. The P4 flashes this image over SDIO.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IDF="${IDF_PATH:-/Users/frikkie/.espressif/v5.5.4/esp-idf}"
# shellcheck disable=SC1091
source "$IDF/export.sh"
cd "$ROOT/c6"
idf.py set-target esp32c6
idf.py build
mkdir -p "$ROOT/build/c6"
cp "$ROOT/c6/build/network_adapter.bin" "$ROOT/build/c6/network_adapter.bin"
echo "OK $ROOT/build/c6/network_adapter.bin"
ls -l "$ROOT/build/c6/network_adapter.bin"
