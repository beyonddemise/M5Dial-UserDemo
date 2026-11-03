# --- 5b. ctags on Apple Silicon without Rosetta ------------------------------------------
# why: arduino-cli's bundled ctags is x86_64 ("bad CPU type" without Rosetta). Builds then
# use tools/ctags/ctags, a universal-ctags wrapper selected per build by env.sh +
# build.sh (see env.sh); the toolchain itself is left untouched. Here we only make sure
# universal-ctags exists, and undo the binary swap earlier versions of this script did
# (bundled binary moved to ctags.x86_64, ctags replaced by a symlink/wrapper).
if [ "$os-$arch" = Darwin-arm64 ] && ! arch -x86_64 /usr/bin/true 2>/dev/null; then
  [ -x /opt/homebrew/bin/ctags ] && /opt/homebrew/bin/ctags --version 2>/dev/null | grep -q Universal \
    || brew install universal-ctags
  shopt -s nullglob  # why: no ctags dir yet (fresh data dir) must not run the loop on a literal glob
  for d in "$ARDUINO_DIRECTORIES_DATA"/packages/builtin/tools/ctags/*/; do
    if [ -f "$d/ctags.x86_64" ]; then rm -f "$d/ctags"; mv "$d/ctags.x86_64" "$d/ctags"; fi
  done
  shopt -u nullglob
fi
