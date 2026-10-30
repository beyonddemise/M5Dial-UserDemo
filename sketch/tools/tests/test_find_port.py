"""Unit tests for find_port.py (serial enumeration is mocked)."""
import contextlib
import io
import os
import sys
import unittest
from types import SimpleNamespace
from unittest import mock

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import find_port  # noqa: E402


def _port(device, vid, pid):
    """Build a stand-in for serial.tools.list_ports_common.ListPortInfo."""
    return SimpleNamespace(device=device, vid=vid, pid=pid)


class FindPortTest(unittest.TestCase):
    def test_picks_303a_1001(self):
        ports = [_port("/dev/cu.Bluetooth", None, None),
                 _port("/dev/cu.usbmodem21101", 0x303A, 0x1001)]
        with mock.patch("serial.tools.list_ports.comports", return_value=ports):
            self.assertEqual(find_port.find_port("303A:1001"), "/dev/cu.usbmodem21101")

    def test_ignores_other_vidpid(self):
        ports = [_port("/dev/cu.usbserial-1", 0x10C4, 0xEA60)]
        with mock.patch("serial.tools.list_ports.comports", return_value=ports):
            self.assertIsNone(find_port.find_port("303A:1001"))

    def test_none_found_exits_2_with_message(self):
        err = io.StringIO()
        with mock.patch("serial.tools.list_ports.comports", return_value=[]), \
                contextlib.redirect_stderr(err):
            self.assertEqual(find_port.main([]), 2)
        self.assertIn("no M5Dial found (VID:PID 303A:1001). Plug in, or hold the knob "
                      "button while plugging in for download mode.", err.getvalue())

    def test_main_prints_port_exit_0(self):
        out = io.StringIO()
        ports = [_port("/dev/ttyACM0", 0x303A, 0x1001)]
        with mock.patch("serial.tools.list_ports.comports", return_value=ports), \
                contextlib.redirect_stdout(out):
            self.assertEqual(find_port.main([]), 0)
        self.assertEqual(out.getvalue().strip(), "/dev/ttyACM0")

    def test_find_ports_returns_all_matches(self):
        ports = [_port("/dev/ttyACM0", 0x303A, 0x1001), _port("/dev/ttyACM1", 0x303A, 0x1001)]
        with mock.patch("serial.tools.list_ports.comports", return_value=ports):
            self.assertEqual(find_port.find_ports("303A:1001"), ["/dev/ttyACM0", "/dev/ttyACM1"])

    def test_multiple_matches_exit_3_and_print_nothing(self):
        out, err = io.StringIO(), io.StringIO()
        ports = [_port("/dev/ttyACM0", 0x303A, 0x1001), _port("/dev/ttyACM1", 0x303A, 0x1001)]
        with mock.patch("serial.tools.list_ports.comports", return_value=ports), \
                contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            self.assertEqual(find_port.main([]), 3)
        self.assertEqual(out.getvalue(), "")
        self.assertIn("ambiguous", err.getvalue())
        self.assertIn("/dev/ttyACM1", err.getvalue())


if __name__ == "__main__":
    unittest.main()
