#!/usr/bin/env bash
# Build the Tab5 scoreboard. src/ is the source of truth.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
"$ROOT/scripts/sync_arduino.sh"
FQBN="${FQBN:-m5stack:esp32:m5stack_tab5:ChipVariant=prev3,PSRAM=enabled,USBMode=hwcdc,CDCOnBoot=cdc}"
OUT="$ROOT/build/tab5"
arduino-cli compile --fqbn "$FQBN" --output-dir "$OUT" "$ROOT/arduino/PadelScoreboardTab5"
echo "OK. Flash with the tablet in download mode (hold Reset until the green LED blinks):"
echo "  arduino-cli upload -p <PORT> --fqbn \"$FQBN\" \"$ROOT/arduino/PadelScoreboardTab5\""
echo "  arduino-cli monitor -p <PORT> -c baudrate=115200"
