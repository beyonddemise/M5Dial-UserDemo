#!/usr/bin/env bash
# Open a 115200 baud serial monitor on the M5Dial (Ctrl-C to quit).
# Usage: monitor.sh [port]   (port auto-detected by VID:PID 303A:1001)
set -euo pipefail
# shellcheck disable=SC1091
. "$(dirname "$0")/env.sh"
port="${1:-$("$PY" "$TOOLS_DIR/find_port.py")}"
exec arduino-cli monitor --config-file "$ARDUINO_CLI_CONFIG" --port "$port" \
  --config baudrate=115200
