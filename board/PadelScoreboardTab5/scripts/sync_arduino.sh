#!/usr/bin/env bash
# Copy src/ into the Arduino sketch folder. arduino-cli compiles the sketch root only.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/arduino/PadelScoreboardTab5"
rm -rf "$DEST"
mkdir -p "$DEST"
cp "$ROOT"/src/*.h "$ROOT"/src/*.cpp "$DEST/"
mv "$DEST/main.cpp" "$DEST/PadelScoreboardTab5.ino"
echo "Synced → $DEST"
