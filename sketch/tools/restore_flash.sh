#!/usr/bin/env bash
# Write a flash backup made by backup_flash.sh back to the M5Dial (DESTRUCTIVE:
# overwrites the whole 8 MB flash). Requires --yes; without it only prints what
# would be overwritten. Verifies size and .sha256 first.
# Usage: restore_flash.sh <sketch/backup/m5dial-factory-*.bin> [--yes]
set -euo pipefail
# shellcheck disable=SC1091
. "$(dirname "$0")/env.sh"
[ $# -ge 1 ] || { echo "usage: restore_flash.sh <backup.bin> [--yes]" >&2; exit 64; }
bin="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
[ "$(wc -c < "$bin" | tr -d ' ')" -eq 8388608 ] || { echo "$bin is not 8388608 bytes" >&2; exit 1; }
( cd "$(dirname "$bin")" && shasum -a 256 -c "$(basename "$bin").sha256" )
port="$("$PY" "$TOOLS_DIR/find_port.py")"
if [ "${2:-}" != "--yes" ]; then
  echo "Would OVERWRITE the entire 8 MB flash of the M5Dial on $port with $bin."
  echo "Re-run with --yes to proceed." >&2
  exit 1
fi
"$PY" -m esptool --chip esp32s3 -p "$port" -b 460800 write_flash 0 "$bin"
