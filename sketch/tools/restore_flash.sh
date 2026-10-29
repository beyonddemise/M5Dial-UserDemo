#!/usr/bin/env bash
# Write a flash backup made by backup_flash.sh back to the M5Dial (DESTRUCTIVE:
# overwrites the whole 8 MB flash). Requires --yes; without it only prints what
# would be overwritten. Verifies size and the requested image's own sha256
# against its .sha256 sidecar first, and refuses an ambiguous/missing port.
# Usage: restore_flash.sh <sketch/backup/m5dial-factory-*.bin> [--yes] [port]
#   port: optional explicit serial port (default: auto-detect 303A:1001; fails
#   if zero or several devices match).
set -euo pipefail
# shellcheck disable=SC1091
. "$(dirname "$0")/env.sh"
[ $# -ge 1 ] || { echo "usage: restore_flash.sh <backup.bin> [--yes] [port]" >&2; exit 64; }
bin="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
[ "$(wc -c < "$bin" | tr -d ' ')" -eq 8388608 ] || { echo "$bin is not 8388608 bytes" >&2; exit 1; }
# why: `shasum -c` would hash whatever file the sidecar names; hash the requested
# image itself and compare to the first field so a copied sidecar cannot pass.
want="$(awk 'NR==1{print $1}' "$bin.sha256" 2>/dev/null || true)"
have="$(shasum -a 256 "$bin" | awk '{print $1}')"
[ -n "$want" ] && [ "$want" = "$have" ] || { echo "checksum mismatch (or missing sidecar) for $bin" >&2; exit 1; }
port="${3:-$("$PY" "$TOOLS_DIR/find_port.py")}"
if [ "${2:-}" != "--yes" ]; then
  echo "Would OVERWRITE the entire 8 MB flash of the M5Dial on $port with $bin."
  echo "Re-run with --yes to proceed." >&2
  exit 1
fi
echo "Restoring $bin to $port ..."
"$PY" -m esptool --chip esp32s3 -p "$port" -b 460800 write_flash 0 "$bin"
