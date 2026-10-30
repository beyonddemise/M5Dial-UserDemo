"""Tests for tools/build_all.sh (sketch discovery, keep-going, step summary), using a fake build.sh."""
import os
import stat
import subprocess
import tempfile
import unittest

BUILD_ALL = os.path.join(os.path.dirname(__file__), "..", "build_all.sh")

# why: real arduino-cli compile output (size lines), so the parser is tested on the actual format
OK_OUT = ("Sketch uses {flash} bytes ({pct}%) of program storage space. Maximum is {max} bytes.\n"
          "Global variables use 54321 bytes (16%) of dynamic memory, leaving 273359 bytes for local "
          "variables. Maximum is 327680 bytes.\n")

# why: the fake logs each sketch dir it is called with and answers from $FAKE_DIR/<Name>.rc/.out,
# so a test scripts per-sketch results and checks call order and count
FAKE = """#!/usr/bin/env bash
name="$(basename "$1")"
echo "$1" >> "$FAKE_DIR/calls"
cat "$FAKE_DIR/$name.out" 2>/dev/null
exit "$(cat "$FAKE_DIR/$name.rc" 2>/dev/null || echo 0)"
"""


class BuildAllTest(unittest.TestCase):
    def run_all(self, sketches, results=None, extra_dirs=()):
        """Create a fake sketch root with `sketches` (relative dirs, each with <Name>.ino) and
        `extra_dirs` (no .ino), run build_all.sh on it; returns (proc, calls, summary, root)."""
        results = results or {}
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        d = tmp.name
        root = os.path.join(d, "root")
        for rel in sketches:
            p = os.path.join(root, rel)
            os.makedirs(p)
            open(os.path.join(p, os.path.basename(rel) + ".ino"), "w").close()
        for rel in extra_dirs:
            os.makedirs(os.path.join(root, rel))
        fake = os.path.join(d, "fake_build.sh")
        with open(fake, "w") as f:
            f.write(FAKE)
        os.chmod(fake, os.stat(fake).st_mode | stat.S_IEXEC)
        for name, (rc, out) in results.items():
            with open(os.path.join(d, name + ".rc"), "w") as f:
                f.write(str(rc))
            with open(os.path.join(d, name + ".out"), "w") as f:
                f.write(out)
        summary = os.path.join(d, "summary.md")
        env = dict(os.environ, BUILD_SH=fake, FAKE_DIR=d, GITHUB_STEP_SUMMARY=summary)
        proc = subprocess.run(["bash", BUILD_ALL, root], env=env, capture_output=True, text=True)

        def read(name):
            p = os.path.join(d, name)
            if not os.path.exists(p):
                return []
            with open(p) as f:
                return f.read().splitlines()

        calls = [os.path.relpath(c, root) for c in read("calls")]
        return proc, calls, read("summary.md"), root

    def test_discovers_userdemo_and_every_example_sorted(self):
        proc, calls, _, _ = self.run_all(
            ["UserDemo", "examples/Zeta", "examples/Alpha"], extra_dirs=["examples/NoIno"])
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertEqual(calls, ["UserDemo", "examples/Alpha", "examples/Zeta"])

    def test_summary_table_has_flash_use(self):
        out = OK_OUT.format(flash=1234567, pct=39, max=3145728)
        proc, _, summary, _ = self.run_all(["UserDemo"], {"UserDemo": (0, out)})
        self.assertEqual(proc.returncode, 0)
        self.assertEqual(summary[0], "### Sketch builds")
        self.assertIn("| Sketch | Result | Flash used | Flash max |", summary)
        self.assertIn("| UserDemo | pass | 1234567 B (39%) | 3145728 B |", summary)

    def test_failure_keeps_going_and_fails_job(self):
        proc, calls, summary, _ = self.run_all(
            ["UserDemo", "examples/A", "examples/B"],
            {"A": (1, "error: 'foo' was not declared in this scope\n"),
             "B": (0, OK_OUT.format(flash=100, pct=1, max=3145728))})
        self.assertEqual(proc.returncode, 1)
        self.assertEqual(len(calls), 3, "every sketch attempted after a failure")
        self.assertIn("::error::build failed: A", proc.stdout)
        self.assertIn("| A | FAIL | - | - |", summary)
        self.assertIn("| B | pass | 100 B (1%) | 3145728 B |", summary)
        self.assertIn("1 of 3 sketch builds failed: A", summary)

    def test_compiler_output_is_shown(self):
        proc, _, _, _ = self.run_all(["UserDemo"], {"UserDemo": (1, "error: boom\n")})
        self.assertIn("error: boom", proc.stdout)

    def test_no_sketches_is_an_error(self):
        proc, calls, _, _ = self.run_all([], extra_dirs=["examples"])
        self.assertNotEqual(proc.returncode, 0)
        self.assertEqual(calls, [])

    def test_missing_size_line_still_passes(self):
        proc, _, summary, _ = self.run_all(["UserDemo"], {"UserDemo": (0, "done\n")})
        self.assertEqual(proc.returncode, 0)
        self.assertIn("| UserDemo | pass | ? | ? |", summary)


if __name__ == "__main__":
    unittest.main()
