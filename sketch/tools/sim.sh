#!/usr/bin/env bash
# Run a sketch on the virtual M5Dial (Wokwi) against an automation scenario.
#
# Usage: sim.sh <sketch-dir> <scenario.yaml> [--timeout-ms 30000]
#
# Steps:
#   1. builds the sketch via build.sh (sim variant: -DM5DD_SIM, see docs/PORTING_NOTES.md)
#   2. builds every Wokwi chip in sim/chips/*/ (make -> <chip>/dist/chip.wasm)
#   3. renders sim/wokwi.toml from sim/wokwi.toml.in (firmware/elf of that sketch,
#      one [[chip]] entry per built chip)
#   4. runs wokwi-cli sim/ --scenario <yaml> --timeout <ms>
# Firmware: sketch/.build/sim/<Name>/ (separate from the device build in .build/<Name>/).
# Output (CI artifact): sketch/.build/sim-out/<Name>/serial.log; scenarios may save
# files there too (take-screenshot save-to is relative to the scenario file).
# Exit code = wokwi-cli's (0 pass, 42 timeout, other = failure).
set -euo pipefail
# shellcheck disable=SC1091
. "$(dirname "$0")/env.sh"

usage() { echo "usage: sim.sh <sketch-dir> <scenario.yaml> [--timeout-ms 30000]" >&2; exit 64; }
[ $# -ge 2 ] || usage
sketch_dir="$(cd "$1" && pwd)"; name="$(basename "$sketch_dir")"
scenario="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
shift 2
timeout_ms=30000
while [ $# -gt 0 ]; do
  case "$1" in
    --timeout-ms) timeout_ms="${2:?--timeout-ms needs a value}"; shift 2 ;;
    *) usage ;;
  esac
done

if [ -z "${WOKWI_CLI_TOKEN:-}" ]; then
  echo "sim.sh: WOKWI_CLI_TOKEN is not set (get one at https://wokwi.com/dashboard/ci)" >&2
  exit 2
fi

SIM_DIR="$SKETCH_ROOT/sim"

# 1. firmware
# why: SIM_DEBUG_LEVEL (0-5, diagnostics only - never set by scenarios/CI checks) turns on
# ESP-IDF/M5GFX logging, e.g. M5GFX's "[Autodetect] read cmd:04 = ..." line.
# why: -DM5DD_SIM marks the sim build; the code it enables is listed in docs/PORTING_NOTES.md
build_args=(--build-property "compiler.cpp.extra_flags=-DM5DD_SIM")
[ -n "${SIM_DEBUG_LEVEL:-}" ] && build_args+=(--build-property "build.code_debug=$SIM_DEBUG_LEVEL")
M5DIAL_BUILD_DIR="$SKETCH_ROOT/.build/sim/$name" "$TOOLS_DIR/build.sh" "$sketch_dir" "${build_args[@]}" >/dev/null
out_dir="$SKETCH_ROOT/.build/sim-out/$name"
mkdir -p "$out_dir"

# 2. chips (each chip folder is a self-contained Wokwi chip project with a Makefile)
chips_toml=""
for chip_dir in "$SIM_DIR"/chips/*/; do
  [ -f "$chip_dir/Makefile" ] || continue
  chip="$(basename "$chip_dir")"
  make -s -C "$chip_dir" WASI_SDK_PATH="$WASI_SDK_PATH" >/dev/null
  chips_toml+=$'\n[[chip]]\n'"name = '$chip'"$'\n'"binary = 'chips/$chip/dist/chip.wasm'"$'\n'
done

# 3. wokwi.toml (git-ignored, regenerated on every run)
# why: wokwi.toml paths are relative to sim/; .build is a sibling of sim/
firmware="../.build/sim/$name"
sed -e "s|@FIRMWARE@|$firmware/$name.ino.merged.bin|" \
    -e "s|@ELF@|$firmware/$name.ino.elf|" \
    "$SIM_DIR/wokwi.toml.in" > "$SIM_DIR/wokwi.toml"
printf '%s' "$chips_toml" >> "$SIM_DIR/wokwi.toml"

# 4. simulate
exec wokwi-cli "$SIM_DIR" --scenario "$scenario" --timeout "$timeout_ms" \
  --serial-log-file "$out_dir/serial.log"
