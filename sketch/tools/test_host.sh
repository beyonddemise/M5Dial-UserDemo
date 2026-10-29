#!/usr/bin/env bash
# Build and run the M5DialDemo host unit tests (Arduino-free headers, plus the Arduino-free
# smooth_menu sources) with the host C++ compiler. Not part of any Arduino build.
# Usage: test_host.sh
# Exit code: 0 = all tests pass; non-zero = compile error or number of failed tests.
set -euo pipefail
lib="$(cd "$(dirname "$0")/../libraries/M5DialDemo" && pwd)"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT
# why: -Isrc only - the tested headers (and smooth_menu) must stay free of Arduino/M5 includes
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -I"$lib/src" "$lib"/test/host/*.cpp \
  "$lib"/src/smooth_menu/*.cpp "$lib"/src/smooth_menu/*/*.cpp -o "$out/host_tests"
"$out/host_tests"
