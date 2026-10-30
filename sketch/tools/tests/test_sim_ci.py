"""Tests for tools/sim_ci.sh verdict logic, using a fake sim.sh (no real Wokwi call)."""
import os
import stat
import subprocess
import tempfile
import unittest

SIM_CI = os.path.join(os.path.dirname(__file__), "..", "sim_ci.sh")

QUOTA = "Error: You have used up your Free plan monthly CI minute quota. Upgrade at wokwi.com"
MISMATCH = "Error in step 2: expected 'boot: ok', got 'boot: fail'"
TRANSPORT = "Error: Connection to transport closed unexpectedly: code 1006"

# why: the fake pops one "<rc>|<text>" line per call from $FAKE_DIR/responses and logs its args,
# so a test scripts the exact wokwi-cli output sequence and can count invocations
FAKE = """#!/usr/bin/env bash
n=$(wc -l < "$FAKE_DIR/calls" 2>/dev/null || echo 0)
echo "$*" >> "$FAKE_DIR/calls"
line=$(sed -n "$((n + 1))p" "$FAKE_DIR/responses")
echo "${line#*|}"
exit "${line%%|*}"
"""


class SimCiTest(unittest.TestCase):
    def run_ci(self, responses, pairs, timeout=None):
        """Run sim_ci.sh against scripted responses; returns (proc, calls, summary)."""
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        d = tmp.name
        fake = os.path.join(d, "fake_sim.sh")
        with open(fake, "w") as f:
            f.write(FAKE)
        os.chmod(fake, os.stat(fake).st_mode | stat.S_IEXEC)
        with open(os.path.join(d, "responses"), "w") as f:
            f.write("".join(f"{rc}|{text}\n" for rc, text in responses))
        summary = os.path.join(d, "summary.md")
        env = dict(os.environ, SIM_SH=fake, FAKE_DIR=d, GITHUB_STEP_SUMMARY=summary)
        args = ["bash", SIM_CI] + (["--timeout-ms", str(timeout)] if timeout else [])
        for sketch, scenario in pairs:
            args += [sketch, scenario]
        proc = subprocess.run(args, env=env, capture_output=True, text=True)

        def read(name):
            p = os.path.join(d, name)
            if not os.path.exists(p):
                return []
            with open(p) as f:
                return f.read().splitlines()

        return proc, read("calls"), read("summary.md")

    def test_pass(self):
        proc, calls, summary = self.run_ci([(0, "ok")], [("examples/A", "a.yaml")], timeout=20000)
        self.assertEqual(proc.returncode, 0)
        self.assertIn("PASS a.yaml", proc.stdout)
        self.assertEqual(calls, ["examples/A a.yaml --timeout-ms 20000"])
        self.assertEqual(summary, [])

    def test_quota_skips_remaining_and_exits_zero(self):
        proc, calls, summary = self.run_ci(
            [(1, QUOTA)], [("examples/A", "a.yaml"), ("examples/B", "b.yaml")])
        self.assertEqual(proc.returncode, 0, proc.stdout)
        self.assertEqual(len(calls), 1, "no further scenarios after quota")
        self.assertIn("::warning::Wokwi CI quota exhausted — scenario NOT run", proc.stdout)
        self.assertEqual(summary, ["SKIPPED (Wokwi quota): a.yaml", "SKIPPED (Wokwi quota): b.yaml"])

    def test_quota_after_earlier_failure_stays_red(self):
        proc, calls, summary = self.run_ci(
            [(1, MISMATCH), (1, QUOTA)], [("examples/A", "a.yaml"), ("examples/B", "b.yaml")])
        self.assertEqual(proc.returncode, 1)
        self.assertEqual(summary, ["SKIPPED (Wokwi quota): b.yaml"])

    def test_mismatch_fails_without_retry(self):
        proc, calls, _ = self.run_ci([(1, MISMATCH), (0, "ok")], [("examples/A", "a.yaml")])
        self.assertEqual(proc.returncode, 1)
        self.assertEqual(len(calls), 1)
        self.assertIn("::error::FAIL a.yaml (exit 1)", proc.stdout)

    def test_timeout_fails(self):
        proc, _, _ = self.run_ci([(42, "Timeout: simulation did not finish")], [("A", "a.yaml")])
        self.assertEqual(proc.returncode, 1)

    def test_transport_drop_retried_once_then_pass(self):
        proc, calls, _ = self.run_ci([(1, TRANSPORT), (0, "ok")], [("examples/A", "a.yaml")])
        self.assertEqual(proc.returncode, 0, proc.stdout)
        self.assertEqual(len(calls), 2)
        self.assertIn("retrying once", proc.stdout)

    def test_transport_drop_twice_fails(self):
        proc, calls, _ = self.run_ci([(1, TRANSPORT), (1, TRANSPORT)], [("examples/A", "a.yaml")])
        self.assertEqual(proc.returncode, 1)
        self.assertEqual(len(calls), 2)

    def test_failure_does_not_stop_other_scenarios(self):
        proc, calls, _ = self.run_ci(
            [(1, MISMATCH), (0, "ok")], [("examples/A", "a.yaml"), ("examples/B", "b.yaml")])
        self.assertEqual(proc.returncode, 1)
        self.assertEqual(len(calls), 2)
        self.assertIn("PASS b.yaml", proc.stdout)

    def test_odd_args_usage(self):
        proc = subprocess.run(["bash", SIM_CI, "only-one"], capture_output=True, text=True)
        self.assertEqual(proc.returncode, 64)


if __name__ == "__main__":
    unittest.main()
