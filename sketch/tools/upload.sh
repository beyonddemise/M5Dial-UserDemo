#!/usr/bin/env bash
# Build (if needed) and upload a sketch to the attached M5Dial.
# Usage: upload.sh <sketch-dir> [port]   (port auto-detected by VID:PID 303A:1001)
# Refuses to run until the factory flash backup exists (see backup_flash.sh).
set -euo pipefail
# shellcheck disable=SC1091
. "$(dirname "$0")/env.sh"
[ $# -ge 1 ] || { echo "usage: upload.sh <sketch-dir> [port]" >&2; exit 64; }
# why: guarantees the "no upload before backup" rule even for ad-hoc invocations
if ! ls "$SKETCH_ROOT"/backup/m5dial-factory-*.bin >/dev/null 2>&1; then
  echo "no flash backup in sketch/backup/: run tools/backup_flash.sh first" >&2; exit 3
fi
dir="$(cd "$1" && pwd)"; name="$(basename "$dir")"
port="${2:-$("$PY" "$TOOLS_DIR/find_port.py")}"
"$TOOLS_DIR/build.sh" "$dir"
exec arduino-cli upload --config-file "$ARDUINO_CLI_CONFIG" --fqbn "$M5DIAL_FQBN" \
  --port "$port" --input-dir "$SKETCH_ROOT/.build/$name"
