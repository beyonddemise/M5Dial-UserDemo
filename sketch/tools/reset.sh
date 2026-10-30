#!/usr/bin/env bash
# Boot the attached M5Dial into its app with an esptool watchdog reset.
# Usage: reset.sh <port>
# why: RTS/hard-reset (esptool default, arduino-cli upload) leaves the M5Dial in ROM download
# mode or a brownout loop; `--after watchdog-reset` boots the app cleanly. `read-mac` is just
# a harmless command that makes esptool connect, then reset.
set -euo pipefail
# shellcheck disable=SC1091
. "$(dirname "$0")/env.sh"
[ $# -eq 1 ] || { echo "usage: reset.sh <port>" >&2; exit 64; }
port="$1"
# why: use the esptool bundled with the core (same one arduino-cli upload runs); M5DIAL_ESPTOOL
# overrides it (tests). Its option spelling is the 5.x one (watchdog-reset, read-mac).
esptool="${M5DIAL_ESPTOOL:-}"
if [ -z "$esptool" ]; then
  esptool="$(ls -d "$ARDUINO_DIRECTORIES_DATA"/packages/m5stack/tools/esptool_py/*/esptool 2>/dev/null | sort -V | tail -n1 || true)"
fi
[ -n "$esptool" ] && [ -x "$esptool" ] || {
  echo "reset.sh: bundled esptool not found under $ARDUINO_DIRECTORIES_DATA (run setup.sh)" >&2; exit 1; }
if ! "$esptool" --chip esp32s3 -p "$port" --before default-reset --after watchdog-reset read-mac >&2; then
  echo "reset.sh: esptool watchdog reset failed on $port; power-cycle the board (unplug/replug USB) and retry" >&2
  exit 1
fi
