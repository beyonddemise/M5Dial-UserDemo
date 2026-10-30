#!/usr/bin/env bash
# Compile a sketch for the M5Dial.
# Usage: build.sh <sketch-dir>
# Output: sketch/.build/<Name>/<Name>.ino.{elf,bin}, or $M5DIAL_BUILD_DIR if set
# (tools/sim.sh uses sketch/.build/sim/<Name> so the -DM5DD_SIM build never
# overwrites the device build)
set -euo pipefail
# shellcheck disable=SC1091
. "$(dirname "$0")/env.sh"
[ $# -ge 1 ] || { echo "usage: build.sh <sketch-dir>" >&2; exit 64; }
dir="$(cd "$1" && pwd)"; name="$(basename "$dir")"
exec arduino-cli compile --config-file "$ARDUINO_CLI_CONFIG" --fqbn "$M5DIAL_FQBN_FULL" \
  --build-path "${M5DIAL_BUILD_DIR:-$SKETCH_ROOT/.build/$name}" \
  ${M5DIAL_CTAGS_DIR:+--build-property "tools.ctags.path=$M5DIAL_CTAGS_DIR"} "${@:2}" "$dir"
