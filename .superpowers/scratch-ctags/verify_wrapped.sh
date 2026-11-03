#!/bin/bash
# Build the regression fixture and Smoke with the scratch-installed ctags-compat wrapper.
set -euo pipefail
S=/Users/stw/Code/M5Dial-UserDemo/.superpowers/scratch-ctags
WT=/Users/stw/Code/M5Dial-UserDemo/.superpowers/wt-pr24/sketch
P="tools.ctags.path=$S/data/packages/builtin/tools/ctags/5.8-arduino11"
bash "$WT/tools/build.sh" "$WT/tools/tests/fixtures/CtagsProto" --only-compilation-database \
  --build-property "$P" > "$S/fixture.log" 2>&1
grep -v '^#line' "$WT/.build/CtagsProto/sketch/CtagsProto.ino.cpp" | head -16
echo "--- Smoke (wrapper)"
bash "$WT/tools/build.sh" "$WT/examples/Smoke" --build-property "$P" 2>&1 | tail -3
grep -v '^#line' "$WT/.build/Smoke/sketch/Smoke.ino.cpp" | grep -n 'setup\|loop' | head
