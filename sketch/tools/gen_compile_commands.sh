#!/usr/bin/env bash
# Generate sketch/.build/compile_commands.json for IntelliSense/clangd.
# Usage: gen_compile_commands.sh [sketch-dir]   (default: examples/DialProbe)
set -euo pipefail
# shellcheck disable=SC1091
. "$(dirname "$0")/env.sh"
dir="$(cd "${1:-$SKETCH_ROOT/examples/DialProbe}" && pwd)"; name="$(basename "$dir")"
# why via build.sh: same flags as the real build (e.g. a sketch's lv_conf.h include path)
M5DIAL_BUILD_DIR="$SKETCH_ROOT/.build/$name" "$TOOLS_DIR/build.sh" "$dir" --only-compilation-database
# why: tools look for the database at a fixed, sketch-independent location
cp "$SKETCH_ROOT/.build/$name/compile_commands.json" "$SKETCH_ROOT/.build/compile_commands.json"
