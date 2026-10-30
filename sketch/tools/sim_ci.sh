#!/usr/bin/env bash
# CI runner for Wokwi scenarios: runs sim.sh for each <sketch-dir> <scenario.yaml> pair and
# turns wokwi-cli's output into a CI verdict.
#
# Usage: sim_ci.sh [--timeout-ms N] <sketch-dir> <scenario.yaml> [<sketch-dir> <scenario.yaml> ...]
#
# Verdict per scenario:
#   - pass (exit 0)                        -> "PASS <scenario>"
#   - Wokwi transport drop (code 1006)     -> retried once; the retry's result counts
#   - used-up Wokwi CI minute quota        -> SKIPPED, not failed: a billing condition, not a
#     test result. Emits ::warning::, appends "SKIPPED (Wokwi quota): <scenario>" to
#     $GITHUB_STEP_SUMMARY for this and every remaining scenario, and runs no more scenarios
#     (each would hit the same quota).
#   - anything else (mismatch, timeout 42, build error) -> "::error::FAIL", step stays red
# Exit code: 1 if any scenario failed, else 0 (also when the quota stopped the run).
#
# Env: SIM_SH overrides the sim runner (default: tools/sim.sh next to this script) - the unit
# tests in tests/test_sim_ci.py use a fake that prints canned wokwi-cli output.
set -uo pipefail

sim_sh="${SIM_SH:-$(dirname "$0")/sim.sh}"
summary="${GITHUB_STEP_SUMMARY:-/dev/null}"
timeout_args=()
if [ "${1:-}" = "--timeout-ms" ]; then
  timeout_args=(--timeout-ms "${2:?--timeout-ms needs a value}"); shift 2
fi
if [ $# -lt 2 ] || [ $(($# % 2)) -ne 0 ]; then
  echo "usage: sim_ci.sh [--timeout-ms N] <sketch-dir> <scenario.yaml> [...]" >&2; exit 64
fi

failed=()
out="$(mktemp)"
trap 'rm -f "$out"' EXIT
while [ $# -gt 0 ]; do
  sketch="$1"; scenario="$2"; shift 2
  for attempt in 1 2; do
    echo "::group::$scenario ($sketch) attempt $attempt"
    "$sim_sh" "$sketch" "$scenario" "${timeout_args[@]+"${timeout_args[@]}"}" 2>&1 | tee "$out"
    rc="${PIPESTATUS[0]}"
    echo "::endgroup::"
    # why: only a failed run is classified; wokwi-cli prints e.g. "You have used up your Free
    # plan monthly CI minute quota" and exits non-zero
    if [ "$rc" -ne 0 ] && grep -q 'CI minute quota' "$out"; then
      echo "::warning::Wokwi CI quota exhausted — scenario NOT run: $scenario"
      echo "SKIPPED (Wokwi quota): $scenario" >> "$summary"
      while [ $# -gt 0 ]; do
        echo "::warning::Wokwi CI quota exhausted — scenario NOT run: $2"
        echo "SKIPPED (Wokwi quota): $2" >> "$summary"
        shift 2
      done
      [ "${#failed[@]}" -eq 0 ] || { printf 'failed: %s\n' "${failed[@]}"; exit 1; }
      exit 0
    fi
    if [ "$rc" -ne 0 ] && [ "$attempt" -eq 1 ] && grep -q 'transport closed unexpectedly' "$out"; then
      echo "::warning::$scenario: Wokwi transport error 1006, retrying once"
      continue
    fi
    break
  done
  if [ "$rc" -eq 0 ]; then
    echo "PASS $scenario"
  else
    echo "::error::FAIL $scenario (exit $rc)"; failed+=("$scenario")
  fi
done
[ "${#failed[@]}" -eq 0 ] || { printf 'failed: %s\n' "${failed[@]}"; exit 1; }
