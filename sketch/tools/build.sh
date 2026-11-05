#!/usr/bin/env bash
# Compile a sketch for the M5Dial.
# Usage: build.sh <sketch-dir>
# Output: sketch/.build/<Name>/<Name>.ino.{elf,bin}, or $M5DIAL_BUILD_DIR if set
# (tools/sim.sh uses sketch/.build/sim/<Name> so the -DM5DD_SIM build never
# overwrites the device build)
# A sketch with its own partitions.csv (e.g. UserDemo) is size-checked against that table's
# first app partition instead of the board menu's PartitionScheme.
# A sketch with its own lv_conf.h (e.g. examples/LvglDemo) is built with LVGL's
# LV_CONF_INCLUDE_SIMPLE and the sketch dir on the include path, for C and C++ sources.
# Env M5DIAL_EXTRA_FLAGS: further compiler flags for C and C++ (merged with the above).
set -euo pipefail
# shellcheck disable=SC1091
. "$(dirname "$0")/env.sh"
[ $# -ge 1 ] || { echo "usage: build.sh <sketch-dir>" >&2; exit 64; }
dir="$(cd "$1" && pwd)"; name="$(basename "$dir")"

# why: the core's prebuild hook copies <sketch>/partitions.csv over the menu's table, but
# arduino-cli still checks the size against the menu's upload.maximum_size (huge_app: 3 MB);
# so derive the limit from the sketch's own (first) app partition. Type: `app` or its numeric
# value 0 (0x00); size: hex, decimal, or K/M suffix (as gen_esp32part accepts them).
max_size=""
if [ -f "$dir/partitions.csv" ]; then
  max_size="$(awk -F, '
    # hex (0x..) or decimal text -> number
    function num(s,   v, i) {
      if (s ~ /^0[xX]/) {
        v = 0; s = tolower(substr(s, 3))
        for (i = 1; i <= length(s); i++) v = v * 16 + index("0123456789abcdef", substr(s, i, 1)) - 1
        return v
      }
      return s + 0
    }
    /^[[:space:]]*(#|$)/ { next }
    {
      type = $2; gsub(/[[:space:]]/, "", type)
      if (type != "app" && !(type ~ /^(0[xX][0-9a-fA-F]+|[0-9]+)$/ && num(type) == 0)) next
      s = $5; gsub(/[[:space:]]/, "", s)
      mul = 1; suffix = toupper(substr(s, length(s)))
      if (suffix == "K") { mul = 1024; s = substr(s, 1, length(s) - 1) }
      else if (suffix == "M") { mul = 1048576; s = substr(s, 1, length(s) - 1) }
      printf "%d\n", num(s) * mul; exit
    }' "$dir/partitions.csv")"
  [ -n "$max_size" ] || { echo "build.sh: no app partition in $dir/partitions.csv" >&2; exit 65; }
fi

# why: LVGL 9 otherwise looks for lv_conf.h next to its own folder (sketch/libraries/), which
# would be shared by every sketch; the sketch-local file needs the define plus -I<sketch dir>.
# compiler.{c,cpp}.extra_flags are empty in the core's platform.txt (reserved for the user),
# so setting them clobbers nothing; C too, because LVGL itself is C. The quotes keep a path
# with spaces in one argument (arduino-cli splits recipes quote-aware).
# why M5DIAL_EXTRA_FLAGS (e.g. sim.sh's -DM5DD_SIM) instead of a caller --build-property:
# a second value for the same key would replace these, not add to them.
extra="${M5DIAL_EXTRA_FLAGS:-}"
[ -f "$dir/lv_conf.h" ] && extra="${extra:+$extra }-DLV_CONF_INCLUDE_SIMPLE \"-I$dir\""
flag_args=()
[ -n "$extra" ] && flag_args=(--build-property "compiler.c.extra_flags=$extra"
                              --build-property "compiler.cpp.extra_flags=$extra")

exec arduino-cli compile --config-file "$ARDUINO_CLI_CONFIG" --fqbn "$M5DIAL_FQBN_FULL" \
  --build-path "${M5DIAL_BUILD_DIR:-$SKETCH_ROOT/.build/$name}" \
  ${M5DIAL_CTAGS_DIR:+--build-property "tools.ctags.path=$M5DIAL_CTAGS_DIR"} \
  ${max_size:+--build-property "upload.maximum_size=$max_size"} \
  ${flag_args[@]+"${flag_args[@]}"} "${@:2}" "$dir"
