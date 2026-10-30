#!/usr/bin/env bash
# One-time (idempotent) toolchain setup for the Arduino port of the M5Dial demo.
#   1. arduino-cli (Homebrew on macOS; must already be on PATH elsewhere)
#   2. Python venv sketch/tools/.venv (pyserial, pillow, esptool, bleak)
#   3. wokwi-cli  -> sketch/tools/bin/        (pinned release binary)
#   4. wasi-sdk   -> sketch/tools/wasi-sdk/   (pinned release tarball)
#   5. M5Stack core + libraries from deps.txt (project-local, sketch/.arduino)
# Usage: sketch/tools/setup.sh
set -euo pipefail
# shellcheck disable=SC1091
. "$(dirname "$0")/env.sh"
cd "$SKETCH_ROOT"

# --- 1. arduino-cli ------------------------------------------------------------
if ! command -v arduino-cli >/dev/null; then
  if command -v brew >/dev/null; then brew install arduino-cli
  else echo "arduino-cli missing: https://arduino.github.io/arduino-cli/latest/installation/" >&2; exit 1; fi
fi

# --- 2. Python venv ---------------------------------------------------------------
if [ ! -x "$PY" ]; then python3 -m venv "$TOOLS_DIR/.venv"; fi
# why: old pip cannot build/select wheels for bleak's pyobjc dependency on macOS
"$PY" -m pip install -q -U pip
"$PY" -m pip install -q "pyserial==$PY_PYSERIAL" "pillow==$PY_PILLOW" \
  "esptool==$PY_ESPTOOL" "bleak==$PY_BLEAK"

# --- 3/4. host OS/arch mapping --------------------------------------------------------
os="$(uname -s)"; arch="$(uname -m)"
case "$os-$arch" in
  Darwin-arm64)   wokwi_t=macos-arm64;       wasi_t=arm64-macos ;;
  Darwin-x86_64)  wokwi_t=macos-x64;         wasi_t=x86_64-macos ;;
  Linux-x86_64)   wokwi_t=linuxstatic-x64;   wasi_t=x86_64-linux ;;
  Linux-aarch64)  wokwi_t=linuxstatic-arm64; wasi_t=arm64-linux ;;
  *) echo "unsupported host $os-$arch (Windows: use WSL2)" >&2; exit 1 ;;
esac

# verify_sha256 <file> <expected>: abort (and delete the file) on mismatch
verify_sha256() {
  local got; got="$(shasum -a 256 "$1" | cut -d' ' -f1)"
  if [ "$got" != "$2" ]; then rm -f "$1"; echo "sha256 mismatch for $(basename "$1"): $got != $2" >&2; exit 1; fi
}
# indirect lookup of pinned hashes from deps.txt (keys use '_' instead of '-')
pinned() { local k="$1_${2//-/_}"; echo "${!k:?no pinned sha256 $k in deps.txt}"; }

# --- 3. wokwi-cli -----------------------------------------------------------------------
# why: same release asset the official installer (wokwi.com/ci/install.sh) fetches, but
# pinned and kept inside the project instead of ~/.wokwi + a ~/bin symlink.
mkdir -p "$TOOLS_DIR/bin"
if [ "$("$TOOLS_DIR/bin/wokwi-cli" --short-version 2>/dev/null || true)" != "$WOKWI_CLI_VERSION" ]; then
  curl -fsSL -o "$TOOLS_DIR/bin/wokwi-cli" \
    "https://github.com/wokwi/wokwi-cli/releases/download/v$WOKWI_CLI_VERSION/wokwi-cli-$wokwi_t"
  verify_sha256 "$TOOLS_DIR/bin/wokwi-cli" "$(pinned WOKWI_SHA256 "$wokwi_t")"
  chmod +x "$TOOLS_DIR/bin/wokwi-cli"
fi

# --- 4. wasi-sdk ---------------------------------------------------------------------------
if [ ! -x "$WASI_SDK_PATH/bin/clang" ]; then
  tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
  curl -fsSL -o "$tmp/wasi.tgz" \
    "https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-$WASI_SDK_VERSION/wasi-sdk-$WASI_SDK_VERSION.0-$wasi_t.tar.gz"
  verify_sha256 "$tmp/wasi.tgz" "$(pinned WASI_SHA256 "$wasi_t")"
  mkdir -p "$WASI_SDK_PATH"
  tar -xzf "$tmp/wasi.tgz" -C "$WASI_SDK_PATH" --strip-components=1
fi

# --- 5. Arduino core + libraries -------------------------------------------------------------
acli() { arduino-cli --config-file "$ARDUINO_CLI_CONFIG" "$@"; }
acli core update-index
acli lib update-index
acli core install "$M5DIAL_CORE@$M5DIAL_CORE_VERSION"
for lib in "$LIB_M5DIAL" "$LIB_M5UNIFIED" "$LIB_M5GFX" "$LIB_NIMBLE" "$LIB_NEOPIXEL" "$LIB_LVGL"; do
  acli lib install "$lib"
done

# --- 5b. ctags on Apple Silicon ------------------------------------------
# why: arduino-cli's bundled ctags is x86_64 ("bad CPU type" without Rosetta, or a stale raw-ctags symlink from old setups). Builds then
# use tools/ctags/ctags, a universal-ctags wrapper selected per build by env.sh +
# build.sh (see env.sh); the toolchain itself is left untouched. Here we only make sure
# universal-ctags exists, and undo the binary swap earlier versions of this script did
# (bundled binary moved to ctags.x86_64, ctags replaced by a symlink/wrapper).
if [ "$os-$arch" = Darwin-arm64 ]; then
  [ -x /opt/homebrew/bin/ctags ] && /opt/homebrew/bin/ctags --version 2>/dev/null | grep -q Universal \
    || brew install universal-ctags
  shopt -s nullglob  # why: no ctags dir yet (fresh data dir) must not run the loop on a literal glob
  for d in "$ARDUINO_DIRECTORIES_DATA"/packages/builtin/tools/ctags/*/; do
    if [ -f "$d/ctags.x86_64" ]; then rm -f "$d/ctags"; mv "$d/ctags.x86_64" "$d/ctags"; fi
  done
  shopt -u nullglob
fi

echo "== versions =="
"$TOOLS_DIR/bin/wokwi-cli" --version
"$WASI_SDK_PATH/bin/clang" --version | head -1
acli lib list
