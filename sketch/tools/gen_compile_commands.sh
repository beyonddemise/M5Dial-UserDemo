#!/usr/bin/env bash
# Generate sketch/.build/compile_commands.json for IntelliSense/clangd.
# Usage: gen_compile_commands.sh [sketch-dir]   (default: examples/Smoke)
set -euo pipefail
# shellcheck disable=SC1091
. "$(dirname "$0")/env.sh"
dir="$(cd "${1:-$SKETCH_ROOT/examples/Smoke}" && pwd)"; name="$(basename "$dir")"
arduino-cli compile --config-file "$ARDUINO_CLI_CONFIG" --fqbn "$M5DIAL_FQBN_FULL" \
  --only-compilation-database --build-path "$SKETCH_ROOT/.build/$name" "$dir"
# why: tools look for the database at a fixed, sketch-independent location
cp "$SKETCH_ROOT/.build/$name/compile_commands.json" "$SKETCH_ROOT/.build/compile_commands.json"
