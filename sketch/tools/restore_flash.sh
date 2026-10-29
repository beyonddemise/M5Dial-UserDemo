#!/usr/bin/env bash
# Write a flash backup made by backup_flash.sh back to the M5Dial (DESTRUCTIVE:
# overwrites the whole 8 MB flash). Verifies the .sha256 first.
# Usage: restore_flash.sh <sketch/backup/m5dial-factory-*.bin>
set -euo pipefail
# shellcheck disable=SC1091
. "$(dirname "$0")/env.sh"
[ $# -eq 1 ] || { echo "usage: restore_flash.sh <backup.bin>" >&2; exit 64; }
bin="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
( cd "$(dirname "$bin")" && shasum -a 256 -c "$(basename "$bin").sha256" )
port="$("$PY" "$TOOLS_DIR/find_port.py")"
"$PY" -m esptool --chip esp32s3 -p "$port" -b 460800 write_flash 0 "$bin"
