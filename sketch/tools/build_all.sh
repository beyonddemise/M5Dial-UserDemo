#!/usr/bin/env bash
# Build every sketch of the port (CI build loop; also handy locally).
#
# Usage: build_all.sh [<sketch-root>]      (default: sketch/, the parent of tools/)
#
# Sketches: <root>/UserDemo plus every <root>/examples/<Name>/ that holds <Name>.ino, sorted;
# found by glob, so a new example is built without touching CI.
# Each sketch is built with tools/build.sh (device build, sketch/.build/<Name>/). All sketches
# are attempted even if one fails. Per sketch: a ::group:: with the compiler output, then
# "PASS <Name>" or "::error::build failed: <Name>".
# Step summary ($GITHUB_STEP_SUMMARY, if set): a markdown table with the flash use from
# arduino-cli's "Sketch uses N bytes (P%) of program storage space. Maximum is M bytes." line.
# An examples/<Name>/ without <Name>.ino is skipped with a ::warning::.
# Exit code: 1 if any build failed, 2 if UserDemo/UserDemo.ino is missing, else 0.
#
# Env: BUILD_SH overrides the build script (default: tools/build.sh next to this script) - the
# unit tests in tests/test_build_all.py use a fake that prints canned arduino-cli output.
set -uo pipefail

tools="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "${1:-$tools/..}" && pwd)"
build_sh="${BUILD_SH:-$tools/build.sh}"
summary="${GITHUB_STEP_SUMMARY:-/dev/null}"

# why an error, not a skip: a renamed or moved UserDemo.ino would otherwise leave CI green
# while the main firmware is never compiled
[ -f "$root/UserDemo/UserDemo.ino" ] ||
  { echo "::error::build_all.sh: no UserDemo/UserDemo.ino under $root" >&2; exit 2; }
sketches=("$root/UserDemo")
for d in "$root"/examples/*/; do
  [ -d "$d" ] || continue  # no examples/ (the glob stayed literal)
  d="${d%/}"
  if [ -f "$d/$(basename "$d").ino" ]; then
    sketches+=("$d")
  else
    echo "::warning::build_all.sh: skipped examples/$(basename "$d") (no $(basename "$d").ino)" >&2
  fi
done

rows=(); failed=()
out="$(mktemp)"
trap 'rm -f "$out"' EXIT
for dir in "${sketches[@]}"; do
  name="$(basename "$dir")"
  echo "::group::build $name"
  "$build_sh" "$dir" 2>&1 | tee "$out"
  rc="${PIPESTATUS[0]}"
  echo "::endgroup::"
  # why: sed prints "<used> <pct> <max>" only from arduino-cli's size line; absent -> "?"
  size="$(sed -nE 's/^Sketch uses ([0-9]+) bytes \(([0-9]+)%\).* Maximum is ([0-9]+) bytes\..*/\1 \2 \3/p' "$out" | head -1)"
  if [ "$rc" -eq 0 ]; then
    echo "PASS $name"
    if [ -n "$size" ]; then
      read -r used pct max <<< "$size"
      rows+=("| $name | pass | $used B ($pct%) | $max B |")
    else
      rows+=("| $name | pass | ? | ? |")
    fi
  else
    echo "::error::build failed: $name (exit $rc)"
    failed+=("$name")
    rows+=("| $name | FAIL | - | - |")
  fi
done

{
  echo "### Sketch builds"
  echo
  echo "| Sketch | Result | Flash used | Flash max |"
  echo "| --- | --- | --- | --- |"
  printf '%s\n' "${rows[@]}"
  if [ "${#failed[@]}" -gt 0 ]; then
    echo
    echo "${#failed[@]} of ${#sketches[@]} sketch builds failed: ${failed[*]}"
  fi
} >> "$summary"

[ "${#failed[@]}" -eq 0 ] || { echo "failed: ${failed[*]}"; exit 1; }
echo "all ${#sketches[@]} sketches built"
