#!/usr/bin/env bash
# Compile a sketch for the M5Dial.
# Usage: build.sh <sketch-dir>
# Output: sketch/.build/<Name>/<Name>.ino.{elf,bin}, or $M5DIAL_BUILD_DIR if set
# (tools/sim.sh uses sketch/.build/sim/<Name> so the -DM5DD_SIM build never
# overwrites the device build)
# A sketch with its own partitions.csv (e.g. UserDemo) is size-checked against that table's
# first app partition instead of the board menu's PartitionScheme.
set -euo pipefail
# shellcheck disable=SC1091
. "$(dirname "$0")/env.sh"
[ $# -ge 1 ] || { echo "usage: build.sh <sketch-dir>" >&2; exit 64; }
dir="$(cd "$1" && pwd)"; name="$(basename "$dir")"

# why: the core's prebuild hook copies <sketch>/partitions.csv over the menu's table, but
# arduino-cli still checks the size against the menu's upload.maximum_size (huge_app: 3 MB);
# so derive the limit from the sketch's own app partition. Size: hex, decimal, or K/M suffix.
max_size=""
if [ -f "$dir/partitions.csv" ]; then
  max_size="$(awk -F, '
    /^[[:space:]]*(#|$)/ { next }
    {
      type = $2; gsub(/[[:space:]]/, "", type)
      if (type != "app") next
      s = $5; gsub(/[[:space:]]/, "", s)
      mul = 1; suffix = toupper(substr(s, length(s)))
      if (suffix == "K") { mul = 1024; s = substr(s, 1, length(s) - 1) }
      else if (suffix == "M") { mul = 1048576; s = substr(s, 1, length(s) - 1) }
      if (s ~ /^0[xX]/) {
        v = 0; s = tolower(substr(s, 3))
        for (i = 1; i <= length(s); i++) v = v * 16 + index("0123456789abcdef", substr(s, i, 1)) - 1
      } else v = s + 0
      printf "%d\n", v * mul; exit
    }' "$dir/partitions.csv")"
  [ -n "$max_size" ] || { echo "build.sh: no app partition in $dir/partitions.csv" >&2; exit 65; }
fi

exec arduino-cli compile --config-file "$ARDUINO_CLI_CONFIG" --fqbn "$M5DIAL_FQBN_FULL" \
  --build-path "${M5DIAL_BUILD_DIR:-$SKETCH_ROOT/.build/$name}" \
  ${M5DIAL_CTAGS_DIR:+--build-property "tools.ctags.path=$M5DIAL_CTAGS_DIR"} \
  ${max_size:+--build-property "upload.maximum_size=$max_size"} "${@:2}" "$dir"
