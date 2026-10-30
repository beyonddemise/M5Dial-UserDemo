"""Unit tests for the pure log-matching logic of device_test.py."""
import contextlib
import io
import os
import subprocess
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


class ResetBoardTest(unittest.TestCase):
    def test_reset_runs_reset_script_with_port(self):
        with mock.patch("subprocess.run") as run:
            device_test.reset_board("/dev/ttyACM0")
        cmd = run.call_args[0][0]
        self.assertEqual(os.path.basename(cmd[0]), "reset.sh")
        self.assertEqual(cmd[-1], "/dev/ttyACM0")

    def test_reset_failure_raises_with_power_cycle_hint(self):
        err = subprocess.CalledProcessError(1, "reset.sh")
        with mock.patch("subprocess.run", side_effect=err):
            with self.assertRaises(device_test.ResetError) as cm:
                device_test.reset_board("/dev/ttyACM0")
        self.assertIn("power-cycle", str(cm.exception))

    def test_main_reset_failure_exits_1_with_message(self):
        err = io.StringIO()
        with mock.patch.object(device_test, "capture",
                               side_effect=device_test.ResetError("reset failed; power-cycle")), \
                contextlib.redirect_stderr(err):
            self.assertEqual(device_test.main(["sk", "--port", "/dev/x", "--no-upload"]), 1)
        self.assertIn("power-cycle", err.getvalue())

    def test_reset_timeout_raises_reset_error_with_hint(self):
        with mock.patch("subprocess.run",
                        side_effect=subprocess.TimeoutExpired("reset.sh", 60)) as run:
            with self.assertRaises(device_test.ResetError) as cm:
                device_test.reset_board("/dev/ttyACM0")
        self.assertIn("power-cycle", str(cm.exception))
        self.assertEqual(run.call_args[1]["timeout"], device_test.RESET_TIMEOUT)


class HazardTest(unittest.TestCase):
    def test_download_mode_detected(self):
        out = device_test.reset_hazards("boot:0x0 (DOWNLOAD(USB/UART0))\nwaiting for download\n")
        self.assertEqual(len(out), 1)
        self.assertIn("ROM download mode", out[0])

    def test_brownout_detected(self):
        out = device_test.reset_hazards("E BOD: Brownout detector was triggered\n")
        self.assertEqual(len(out), 1)
        self.assertIn("power-cycle", out[0])

    def test_clean_log_has_no_hazard(self):
        self.assertEqual(device_test.reset_hazards(LOG), [])

    def test_main_fails_on_hazard_even_if_expects_match(self):
        err = io.StringIO()
        with mock.patch.object(device_test, "capture",
                               return_value="boot: smoke\nwaiting for download\n"), \
                contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(err):
            self.assertEqual(device_test.main(
                ["sk", "--port", "/dev/x", "--no-upload", "--expect", "boot: smoke"]), 1)
        self.assertIn("#28", err.getvalue())


class RedetectTest(unittest.TestCase):
    def _run(self, redetect):
        ser = mock.MagicMock()
        ser.__enter__.return_value = ser
        ser.read.return_value = b"x"
        with mock.patch.object(device_test, "reset_board"), \
                mock.patch("serial.Serial", return_value=ser):
            device_test.capture("/dev/old", [], False, 0.01, redetect=redetect)
        return ser

    def test_redetected_port_is_opened(self):
        ser = self._run(lambda: ("/dev/new", 0, ""))
        self.assertEqual(ser.port, "/dev/new")

    def test_no_redetect_keeps_port(self):
        self.assertEqual(self._run(None).port, "/dev/old")

    def test_redetect_failure_raises_port_lost_with_code(self):
        with mock.patch.object(device_test, "reset_board"):
            with self.assertRaises(device_test.PortLost) as cm:
                device_test.capture("/dev/old", [], False, 1,
                                    redetect=lambda: (None, 3, "ambiguous"))
        self.assertEqual(cm.exception.code, 3)

    def test_main_passes_redetect_only_for_auto_detect(self):
        with mock.patch.object(device_test, "capture", return_value="ok") as cap, \
                contextlib.redirect_stdout(io.StringIO()):
            device_test.main(["sk", "--port", "/dev/x", "--no-upload"])
        self.assertIsNone(cap.call_args[1].get("redetect"))


class CaptureTest(unittest.TestCase):
    def _capture(self):
        ser = mock.MagicMock()
        ser.__enter__.return_value = ser
        ser.read.return_value = b"boot: smoke\n"
        calls = []
        with mock.patch.object(device_test, "reset_board", side_effect=lambda p: calls.append("reset")), \
                mock.patch("serial.Serial", return_value=ser) as ctor:
            device_test.capture("/dev/x", ["boot: smoke"], True, 1)
        return ctor, ser, calls

    def test_opens_with_dtr_high_rts_low_and_never_asserts_rts(self):
        ctor, ser, calls = self._capture()
        self.assertEqual(calls, ["reset"])
        # why: port is created closed, flags set, then opened (DTR must be up at open time)
        self.assertEqual(ser.port, "/dev/x")
        ctor.assert_called_once()
        self.assertIs(ser.dtr, True)
        self.assertIs(ser.rts, False)
        ser.open.assert_called_once()

    def test_no_upload_path_resets_via_capture(self):
        with mock.patch("subprocess.run") as run, \
                mock.patch.object(device_test, "capture", return_value="ok") as cap, \
                contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(device_test.main(["sk", "--port", "/dev/x", "--no-upload"]), 0)
        run.assert_not_called()
        cap.assert_called_once()


if __name__ == "__main__":
    unittest.main()
