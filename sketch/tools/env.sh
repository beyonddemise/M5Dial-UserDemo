#!/usr/bin/env bash
# Sourced (never executed) by every tool script. Makes the project-local tools
# reachable without any manual shell setup after the one-time setup.sh:
#   - prepends sketch/tools/bin (wokwi-cli) to PATH
#   - exports WASI_SDK_PATH (for building Wokwi chips)
#   - loads the pinned versions from deps.txt
# It deliberately activates nothing else (no venv; scripts call $PY directly).

TOOLS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SKETCH_ROOT="$(cd "$TOOLS_DIR/.." && pwd)"
export TOOLS_DIR SKETCH_ROOT

export PATH="$TOOLS_DIR/bin:$PATH"
export WASI_SDK_PATH="$TOOLS_DIR/wasi-sdk"
PY="$TOOLS_DIR/.venv/bin/python"
export PY

# why: arduino-cli must use the project config so cores/libs stay in sketch/.arduino
ARDUINO_CLI_CONFIG="$SKETCH_ROOT/arduino-cli.yaml"
export ARDUINO_CLI_CONFIG
# why: absolute paths so every script works from any cwd (yaml paths are cwd-relative)
export ARDUINO_DIRECTORIES_DATA="$SKETCH_ROOT/.arduino/data"
export ARDUINO_DIRECTORIES_DOWNLOADS="$SKETCH_ROOT/.arduino/staging"
# why: /sketch is the Arduino sketchbook, so sketch/libraries/* (our M5DialDemo) is found
# automatically; third-party libs installed by setup.sh also land in sketch/libraries/
# (git-ignored there except M5DialDemo/).
export ARDUINO_DIRECTORIES_USER="$SKETCH_ROOT"

set -a
# shellcheck disable=SC1091
. "$TOOLS_DIR/deps.txt"
set +a
# FQBN including board options (e.g. USB CDC on boot) - use this for compile/upload
M5DIAL_FQBN_FULL="$M5DIAL_FQBN${M5DIAL_FQBN_OPTIONS:+:$M5DIAL_FQBN_OPTIONS}"
export M5DIAL_FQBN_FULL
