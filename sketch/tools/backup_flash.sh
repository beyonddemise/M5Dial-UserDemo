#!/usr/bin/env bash
# Read the complete 8 MB flash of the attached M5Dial into sketch/backup/ and
# write a .sha256 next to it. Read-only on the device. Run BEFORE the first upload.
# Usage: backup_flash.sh
set -euo pipefail
# shellcheck disable=SC1091
. "$(dirname "$0")/env.sh"
port="$("$PY" "$TOOLS_DIR/find_port.py")"
mkdir -p "$SKETCH_ROOT/backup"
out="$SKETCH_ROOT/backup/m5dial-factory-$(date +%Y%m%d-%H%M%S).bin"
"$PY" -m esptool --chip esp32s3 -p "$port" -b 460800 read_flash 0 0x800000 "$out"
size="$(wc -c < "$out" | tr -d ' ')"
[ "$size" -eq 8388608 ] || { echo "backup has $size bytes, expected 8388608" >&2; exit 1; }
( cd "$SKETCH_ROOT/backup" && shasum -a 256 "$(basename "$out")" > "$(basename "$out").sha256" )
echo "backup OK: $out ($size bytes)"
cat "$out.sha256"
