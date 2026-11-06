#!/usr/bin/env bash
# Host check for the match rules. Does not need the tablet.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/padel_scoreboard_test"
CXX="${CXX:-c++}"
if ! "$CXX" -std=c++17 -x c++ -fsyntax-only - <<< 'int main(){return 0;}' >/dev/null 2>&1; then
  if [ -d /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk ]; then
    export DEVELOPER_DIR=/Library/Developer/CommandLineTools
    export SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
    CXX=/usr/bin/clang++
  fi
fi
"$CXX" -std=c++17 -I "$ROOT/src" "$ROOT/tools/host_test.cpp" "$ROOT/src/game.cpp" -o "$OUT"
"$OUT"
rm -f "$OUT"
