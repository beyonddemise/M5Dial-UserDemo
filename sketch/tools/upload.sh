#!/usr/bin/env bash
# Build (if needed) and upload a sketch to the attached M5Dial.
# Usage: upload.sh <sketch-dir> [port]   (port auto-detected by VID:PID 303A:1001)
# Refuses to run until a verified factory flash backup exists (see check_backup.sh).
set -euo pipefail
# shellcheck disable=SC1091
. "$(dirname "$0")/env.sh"
[ $# -ge 1 ] || { echo "usage: upload.sh <sketch-dir> [port]" >&2; exit 64; }
# why: guarantees the "no upload before backup" rule even for ad-hoc invocations
"$TOOLS_DIR/check_backup.sh" "$SKETCH_ROOT/backup" || exit 3
dir="$(cd "$1" && pwd)"; name="$(basename "$dir")"
port="${2:-$("$PY" "$TOOLS_DIR/find_port.py")}"
# why: an inherited M5DIAL_BUILD_DIR (e.g. the -DM5DD_SIM build from sim.sh) must never be
# flashed to hardware; build and upload from the same, explicit device build dir.
build_dir="$SKETCH_ROOT/.build/$name"
M5DIAL_BUILD_DIR="$build_dir" "$TOOLS_DIR/build.sh" "$dir"
arduino-cli upload --config-file "$ARDUINO_CLI_CONFIG" --fqbn "$M5DIAL_FQBN_FULL" \
  --port "$port" --input-dir "$build_dir"
# why: upload ends in an RTS hard reset, which strands the M5Dial in download mode or a
# brownout loop; boot the fresh app with a watchdog reset instead.
exec "$TOOLS_DIR/reset.sh" "$port"
