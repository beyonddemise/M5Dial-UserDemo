"""Unit tests for the pure log-matching logic of device_test.py."""
import contextlib
import io
import os
import sys
import unittest
from types import SimpleNamespace
from unittest import mock

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import device_test  # noqa: E402

LOG = "I (12) M5Dial: boot: smoke\nI (30) app: step=1\nI (50) app: step=2\n"


class EvaluateTest(unittest.TestCase):
    def test_expects_in_order_pass(self):
        self.assertEqual(device_test.evaluate(LOG, ["boot: smoke", r"step=1", r"step=2"], []), [])

    def test_expects_out_of_order_fail(self):
        problems = device_test.evaluate(LOG, [r"step=2", r"step=1"], [])
        self.assertEqual(len(problems), 1)
        self.assertIn("step=1", problems[0])

    def test_missing_expect_reported(self):
        problems = device_test.evaluate(LOG, ["nope"], [])
        self.assertEqual(len(problems), 1)
        self.assertIn("expect", problems[0])

    def test_forbid_matches_across_lines_dotall(self):
        # why: --forbid uses DOTALL so one pattern can span several log lines
        problems = device_test.evaluate(LOG, [], [r"step=1.*step=2"])
        self.assertEqual(len(problems), 1)
        self.assertIn("forbid", problems[0])

    def test_forbid_absent_passes(self):
        self.assertEqual(device_test.evaluate(LOG, [], ["Guru Meditation"]), [])


def _two_boards():
    return [SimpleNamespace(device="/dev/ttyACM0", vid=0x303A, pid=0x1001),
            SimpleNamespace(device="/dev/ttyACM1", vid=0x303A, pid=0x1001)]


class AmbiguousPortTest(unittest.TestCase):
    def test_ambiguous_ports_exit_3_before_upload(self):
        err = io.StringIO()
        with mock.patch("serial.tools.list_ports.comports", return_value=_two_boards()), \
                mock.patch("subprocess.run") as run, contextlib.redirect_stderr(err):
            self.assertEqual(device_test.main(["sk"]), 3)
        run.assert_not_called()
        self.assertIn("ambiguous", err.getvalue())

    def test_explicit_port_bypasses_ambiguity(self):
        with mock.patch("serial.tools.list_ports.comports", return_value=_two_boards()), \
                mock.patch("subprocess.run") as run, \
                mock.patch.object(device_test, "capture", return_value="ok") as cap, \
                contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(device_test.main(["sk", "--port", "/dev/ttyACM1"]), 0)
        self.assertEqual(run.call_args[0][0][-1], "/dev/ttyACM1")
        self.assertEqual(cap.call_args[0][0], "/dev/ttyACM1")

    def test_ambiguity_after_reenumeration_exit_3(self):
        one = _two_boards()[:1]
        with mock.patch("serial.tools.list_ports.comports", side_effect=[one, _two_boards()]), \
                mock.patch("subprocess.run"), mock.patch("time.sleep"), \
                mock.patch.object(device_test, "capture") as cap, \
                contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(device_test.main(["sk"]), 3)
        cap.assert_not_called()


class WaitForPortTest(unittest.TestCase):
    def test_retries_until_port_reappears(self):
        board = _two_boards()[:1]
        sleeps = []
        with mock.patch("serial.tools.list_ports.comports", side_effect=[[], [], board]):
            port, code, _ = device_test.wait_for_port(
                timeout=10, interval=0.25, sleep=sleeps.append, clock=lambda: 0.0)
        self.assertEqual((port, code), ("/dev/ttyACM0", 0))
        self.assertEqual(sleeps, [0.25, 0.25])

    def test_gives_up_with_exit_2_after_timeout(self):
        now = [0.0]

        def sleep(d):
            now[0] += d
        with mock.patch("serial.tools.list_ports.comports", return_value=[]):
            port, code, _ = device_test.wait_for_port(
                timeout=1, interval=0.25, sleep=sleep, clock=lambda: now[0])
        self.assertEqual((port, code), (None, 2))

    def test_ambiguity_fails_immediately(self):
        sleeps = []
        with mock.patch("serial.tools.list_ports.comports", return_value=_two_boards()):
            _, code, _ = device_test.wait_for_port(
                timeout=10, interval=0.25, sleep=sleeps.append, clock=lambda: 0.0)
        self.assertEqual(code, 3)
        self.assertEqual(sleeps, [])

    def test_main_missing_after_upload_exits_2_not_stale_port(self):
        board = _two_boards()[:1]
        with mock.patch("serial.tools.list_ports.comports", side_effect=[board, []]), \
                mock.patch("subprocess.run"), mock.patch("time.sleep"), \
                mock.patch.object(device_test, "wait_for_port",
                                  wraps=lambda **k: (None, 2, "gone")), \
                mock.patch.object(device_test, "capture") as cap, \
                contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(device_test.main(["sk"]), 2)
        cap.assert_not_called()


if __name__ == "__main__":
    unittest.main()
