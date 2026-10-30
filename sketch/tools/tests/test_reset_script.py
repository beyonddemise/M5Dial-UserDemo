"""reset.sh must boot the board with an esptool watchdog reset (never an RTS pulse)."""
import os
import shutil
import stat
import subprocess
import tempfile
import unittest

TOOLS = os.path.join(os.path.dirname(__file__), "..")


def _exe(path, text):
    with open(path, "w") as f:
        f.write(text)
    os.chmod(path, os.stat(path).st_mode | stat.S_IXUSR)


class ResetScriptTest(unittest.TestCase):
    def run_reset(self, esptool_exit=0):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        tools = os.path.join(tmp.name, "tools")
        os.makedirs(tools)
        for f in ("reset.sh", "env.sh", "deps.txt"):
            shutil.copy(os.path.join(TOOLS, f), tools)
        log = os.path.join(tmp.name, "log")
        stub = os.path.join(tmp.name, "esptool")
        _exe(stub, '#!/usr/bin/env bash\necho "$*" >> "$LOG"\nexit %d\n' % esptool_exit)
        env = dict(os.environ, LOG=log, M5DIAL_ESPTOOL=stub)
        proc = subprocess.run([os.path.join(tools, "reset.sh"), "/dev/ttyX"],
                              env=env, capture_output=True, text=True)
        text = open(log).read() if os.path.exists(log) else ""
        return proc, text

    def test_uses_watchdog_reset_after_default_reset(self):
        proc, log = self.run_reset()
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn("--before default-reset", log)
        self.assertIn("--after watchdog-reset", log)
        self.assertIn("-p /dev/ttyX", log)
        self.assertIn("read-mac", log)

    def test_failure_propagates_with_hint(self):
        proc, _ = self.run_reset(esptool_exit=2)
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("power-cycle", proc.stderr)


if __name__ == "__main__":
    unittest.main()
