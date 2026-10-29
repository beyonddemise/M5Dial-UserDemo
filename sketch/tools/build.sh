#!/usr/bin/env bash
# Compile a sketch for the M5Dial.
# Usage: build.sh <sketch-dir>
# Output: sketch/.build/<Name>/<Name>.ino.{elf,bin} (same ELF is used for sim and device)
set -euo pipefail
# shellcheck disable=SC1091
. "$(dirname "$0")/env.sh"
[ $# -ge 1 ] || { echo "usage: build.sh <sketch-dir>" >&2; exit 64; }
dir="$(cd "$1" && pwd)"; name="$(basename "$dir")"
exec arduino-cli compile --config-file "$ARDUINO_CLI_CONFIG" --fqbn "$M5DIAL_FQBN_FULL" \
  --build-path "$SKETCH_ROOT/.build/$name" \
  ${M5DIAL_CTAGS_DIR:+--build-property "tools.ctags.path=$M5DIAL_CTAGS_DIR"} "${@:2}" "$dir"
