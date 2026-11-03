#!/bin/bash
# Preprocess the Repro sketch with the scratch ctags wrapper (tools.ctags.path override).
set -euo pipefail
S=/Users/stw/Code/M5Dial-UserDemo/.superpowers/scratch-ctags
. /Users/stw/Code/M5Dial-UserDemo/.superpowers/wt-pr24/sketch/tools/env.sh
rm -rf "$S/build2"
arduino-cli compile --config-file "$ARDUINO_CLI_CONFIG" --fqbn "$M5DIAL_FQBN_FULL" \
  --build-path "$S/build2" -v --only-compilation-database \
  --build-property "tools.ctags.path=$S/data/packages/builtin/tools/ctags/5.8-arduino11" \
  "$S/Repro" > "$S/log2.txt" 2>&1
grep -o '[^ ]*/ctags -u' "$S/log2.txt"
cat -n "$S/build2/sketch/Repro.ino.cpp"
