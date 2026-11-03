"""Unit tests for ble_check.py (bleak is never imported: the BLE side is faked)."""
import asyncio
import contextlib
import io
import os
import subprocess
import sys
import types
import unittest
from unittest import mock

TOOLS = os.path.join(os.path.dirname(__file__), "..")
sys.path.insert(0, TOOLS)
import ble_check  # noqa: E402


class ParseArgsTest(unittest.TestCase):
    def test_defaults(self):
        args = ble_check.parse_args(["--name", "M5-1A2B"])
        self.assertEqual(args.name, "M5-1A2B")
        self.assertEqual(args.notifications, 3)
        self.assertEqual(args.timeout, 30.0)

    def test_explicit_values(self):
        args = ble_check.parse_args(["--name", "x", "--notifications", "5", "--timeout", "7.5"])
        self.assertEqual(args.notifications, 5)
        self.assertEqual(args.timeout, 7.5)

    def test_name_is_required(self):
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            ble_check.parse_args([])

    def test_notifications_must_be_at_least_two(self):
        # why: one value cannot show an increasing counter
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            ble_check.parse_args(["--name", "x", "--notifications", "1"])


class ParseMeasurementTest(unittest.TestCase):
    def test_uint8_value_as_the_dial_sends_it(self):
        # flags 0x06: sensor contact supported + detected, 8-bit value
        self.assertEqual(ble_check.parse_heart_rate(bytes([0x06, 90])), 90)

    def test_uint16_value_when_flag_bit0_is_set(self):
        self.assertEqual(ble_check.parse_heart_rate(bytes([0x01, 0x2C, 0x01])), 300)

    def test_accepts_bytearray(self):
        self.assertEqual(ble_check.parse_heart_rate(bytearray([0x06, 159])), 159)

    def test_too_short_raises(self):
        for data in (b"", bytes([0x06]), bytes([0x01, 0x2C])):
            with self.assertRaises(ValueError):
                ble_check.parse_heart_rate(data)


class CounterTest(unittest.TestCase):
    def test_step_up_is_ok(self):
        self.assertTrue(ble_check.counter_advanced(90, 91))

    def test_skipped_values_still_increase(self):
        self.assertTrue(ble_check.counter_advanced(90, 93))

    def test_wrap_from_top_back_to_start_is_ok(self):
        self.assertTrue(ble_check.counter_advanced(159, 90))

    def test_repeat_or_decrease_fails(self):
        self.assertFalse(ble_check.counter_advanced(100, 100))
        self.assertFalse(ble_check.counter_advanced(100, 99))
        self.assertFalse(ble_check.counter_advanced(120, 90))  # a jump back, not the wrap

    def test_wrap_with_a_lost_value_is_ok(self):
        # why: 159 can be the lost notification; 158 -> 90 is still two steps forward
        self.assertTrue(ble_check.counter_advanced(158, 90))
        self.assertTrue(ble_check.counter_advanced(159, 92))

    def test_value_outside_the_dial_range_fails(self):
        self.assertFalse(ble_check.counter_advanced(90, 160))
        self.assertFalse(ble_check.counter_advanced(89, 91))

    def test_sequence_check(self):
        self.assertTrue(ble_check.counter_increasing([157, 158, 159, 90, 91]))
        self.assertFalse(ble_check.counter_increasing([90, 92, 91]))
        self.assertTrue(ble_check.counter_increasing([90]))


class _FakeClient:
    """Stand-in for bleak.BleakClient: replays `values` as 0x2A37 notifications."""

    def __init__(self, values, body_location=b"\x01", payloads=None, stop_error=None):
        self.values = values
        self.body_location = body_location
        self.payloads = payloads  # raw 0x2A37 payloads, replayed instead of `values`
        self.stop_error = stop_error  # raised by stop_notify (a Dial that disconnected)
        self.read_uuids = []
        self.notify_uuid = None

    def __call__(self, device):
        self.device = device
        return self

    async def __aenter__(self):
        return self

    async def __aexit__(self, *exc):
        return False

    async def read_gatt_char(self, uuid):
        self.read_uuids.append(uuid)
        return bytearray(self.body_location)

    async def start_notify(self, uuid, callback):
        self.notify_uuid = uuid
        payloads = self.payloads or [bytes([0x06, v]) for v in self.values]
        for data in payloads:
            callback(None, bytearray(data))
            await asyncio.sleep(0)

    async def stop_notify(self, uuid):
        if self.stop_error is not None:
            raise self.stop_error


class _FakeScanner:
    """Stand-in for bleak.BleakScanner (only find_device_by_name)."""

    def __init__(self, device):
        self.device = device
        self.names = []

    async def find_device_by_name(self, name, timeout):
        self.names.append(name)
        return self.device


def _run(argv, scanner, client):
    out, err = io.StringIO(), io.StringIO()
    args = ble_check.parse_args(argv)
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        rc = asyncio.run(ble_check.run(args, scanner, client))
    return rc, out.getvalue(), err.getvalue()


class RunTest(unittest.TestCase):
    def test_pass_after_n_increasing_notifications(self):
        scanner, client = _FakeScanner("dev"), _FakeClient([90, 91, 92, 93])
        rc, out, _ = _run(["--name", "M5-1A2B", "--timeout", "2"], scanner, client)
        self.assertEqual(rc, 0)
        self.assertEqual(scanner.names, ["M5-1A2B"])
        self.assertEqual(client.read_uuids, [ble_check.BODY_SENSOR_LOCATION_UUID])
        self.assertEqual(client.notify_uuid, ble_check.HEART_RATE_MEASUREMENT_UUID)
        self.assertIn("ble_check: body_sensor_location=1", out)
        self.assertIn("ble_check: hr=92", out)
        self.assertIn("ble_check: PASS", out)

    def test_not_found_exits_2(self):
        rc, _, err = _run(["--name", "M5-1A2B", "--timeout", "1"], _FakeScanner(None),
                          _FakeClient([]))
        self.assertEqual(rc, 2)
        self.assertIn("M5-1A2B not found", err)

    def test_non_increasing_counter_exits_1_without_waiting_for_more(self):
        # why --notifications 5 with 2 values: only the early verdict can end the wait
        rc, _, err = _run(["--name", "x", "--timeout", "2", "--notifications", "5"],
                          _FakeScanner("dev"), _FakeClient([90, 90]))
        self.assertEqual(rc, 1)
        self.assertIn("not increasing", err)

    def test_malformed_measurement_exits_1(self):
        rc, _, err = _run(["--name", "x", "--timeout", "2", "--notifications", "5"],
                          _FakeScanner("dev"), _FakeClient([], payloads=[b"\x06"]))
        self.assertEqual(rc, 1)
        self.assertIn("too short", err)

    def test_body_location_of_wrong_length_exits_1(self):
        rc, _, err = _run(["--name", "x", "--timeout", "2"], _FakeScanner("dev"),
                          _FakeClient([90, 91, 92], body_location=b"\x01\x02"))
        self.assertEqual(rc, 1)
        self.assertIn("body sensor location is 2 bytes", err)

    def test_stop_notify_failure_keeps_the_verdict(self):
        # why: a Dial that disconnected makes stop_notify raise; the timeout must stay exit 3
        rc, _, err = _run(["--name", "x", "--timeout", "0.2"], _FakeScanner("dev"),
                          _FakeClient([90], stop_error=OSError("disconnected")))
        self.assertEqual(rc, 3)
        self.assertIn("timeout", err)

    def test_too_few_notifications_times_out_exit_3(self):
        rc, _, err = _run(["--name", "x", "--timeout", "0.2"], _FakeScanner("dev"),
                          _FakeClient([90, 91]))
        self.assertEqual(rc, 3)
        self.assertIn("timeout", err)


class _FakeBleakError(Exception):
    pass


class MainTest(unittest.TestCase):
    """main(): exceptions from the BLE side become EXIT_BLE_ERROR, not a traceback."""

    def _main_with_run_raising(self, exc):
        bleak = types.ModuleType("bleak")
        bleak.BleakClient = bleak.BleakScanner = object
        bleak_exc = types.ModuleType("bleak.exc")
        bleak_exc.BleakError = _FakeBleakError

        async def raising_run(*_args):
            raise exc

        err = io.StringIO()
        with mock.patch.dict(sys.modules, {"bleak": bleak, "bleak.exc": bleak_exc}), \
                mock.patch.object(ble_check, "run", raising_run), \
                contextlib.redirect_stderr(err):
            rc = ble_check.main(["--name", "x"])
        return rc, err.getvalue()

    def test_bleak_error_exits_4(self):
        rc, err = self._main_with_run_raising(_FakeBleakError("adapter off"))
        self.assertEqual(rc, ble_check.EXIT_BLE_ERROR)
        self.assertIn("adapter off", err)

    def test_connect_timeout_exits_4(self):
        # why asyncio.TimeoutError: bleak's CoreBluetooth connect() re-raises it on timeout
        rc, err = self._main_with_run_raising(asyncio.TimeoutError())
        self.assertEqual(rc, ble_check.EXIT_BLE_ERROR)
        self.assertIn("timed out", err)

    def test_os_error_exits_4(self):
        rc, _ = self._main_with_run_raising(OSError("Bluetooth unavailable"))
        self.assertEqual(rc, ble_check.EXIT_BLE_ERROR)


class ImportTest(unittest.TestCase):
    def test_module_import_does_not_import_bleak(self):
        # why a subprocess: bleak may already be in this interpreter's sys.modules
        code = "import sys, ble_check; print('bleak' in sys.modules)"
        out = subprocess.run([sys.executable, "-c", code], cwd=TOOLS, capture_output=True,
                             text=True, check=True).stdout.strip()
        self.assertEqual(out, "False")


if __name__ == "__main__":
    unittest.main()
