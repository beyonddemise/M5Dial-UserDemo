#!/usr/bin/env bash
# Read the complete 8 MB flash of the attached M5Dial into sketch/backup/ and
# write a .sha256 next to it. Read-only on the device. Run BEFORE the first upload.
# The image is written to <name>.bin.partial and only renamed into place after the
# size and checksum are verified, so a failed run never leaves a *.bin behind that
# upload.sh could mistake for a backup.
# Usage: backup_flash.sh
set -euo pipefail
# shellcheck disable=SC1091
. "$(dirname "$0")/env.sh"
port="$("$PY" "$TOOLS_DIR/find_port.py")"
mkdir -p "$SKETCH_ROOT/backup"
name="m5dial-factory-$(date +%Y%m%d-%H%M%S).bin"
out="$SKETCH_ROOT/backup/$name"
trap 'rm -f "$out.partial" "$out.sha256.partial"' EXIT
"$PY" -m esptool --chip esp32s3 -p "$port" -b 460800 read_flash 0 0x800000 "$out.partial"
size="$(wc -c < "$out.partial" | tr -d ' ')"
[ "$size" -eq 8388608 ] || { echo "backup has $size bytes, expected 8388608" >&2; exit 1; }
( cd "$SKETCH_ROOT/backup" && shasum -a 256 "$name.partial" | sed "s/$name.partial/$name/" > "$name.sha256.partial" )
mv "$out.partial" "$out"
mv "$out.sha256.partial" "$out.sha256"
echo "backup OK: $out ($size bytes)"
cat "$out.sha256"
