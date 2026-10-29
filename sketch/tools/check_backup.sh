#!/usr/bin/env bash
# Gate: succeed only if <dir> holds a complete, verified factory flash backup, i.e. at
# least one m5dial-factory-*.bin of exactly 8388608 bytes whose .sha256 matches.
# Usage: check_backup.sh [backup-dir]   (default: sketch/backup)
# Exit: 0 = valid backup found, 1 = none (message on stderr).
set -euo pipefail
dir="${1:-$(cd "$(dirname "$0")/.." && pwd)/backup}"
shopt -s nullglob
for bin in "$dir"/m5dial-factory-*.bin; do
  [ -f "$bin.sha256" ] || continue
  [ "$(wc -c < "$bin" | tr -d ' ')" -eq 8388608 ] || continue
  # why: the .sha256 holds a bare file name, so verify from inside the directory
  ( cd "$dir" && shasum -a 256 -c "$(basename "$bin").sha256" >/dev/null 2>&1 ) && exit 0
done
echo "no verified flash backup (8388608 bytes + matching .sha256) in $dir: run tools/backup_flash.sh first" >&2
exit 1
