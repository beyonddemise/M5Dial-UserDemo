#!/usr/bin/env python3
"""Check the M5Dial BLE heart-rate server (BleServer sketch / UserDemo slot 5) from this host.

Scans for the device by name, connects, reads Body Sensor Location (0x2A38), subscribes to
Heart Rate Measurement (0x2A37) and exits 0 once ``--notifications`` values arrived with an
increasing counter (the Dial counts 90..159, then wraps to 90).

Usage: ble_check.py --name M5-XXXX [--notifications 3] [--timeout 30]
Exit codes: 0 pass, 1 counter not increasing or bad payload, 2 device not found,
3 timeout waiting for notifications, 4 BLE error.
Output lines: ``ble_check: body_sensor_location=<n>``, ``ble_check: hr=<n>``,
``ble_check: PASS``. Needs bleak (installed by setup.sh); it is imported only in main(), so
the logic here is unit-testable without it (tests/test_ble_check.py).
"""
from __future__ import annotations

import argparse
import asyncio
import sys
from typing import Iterable, List, Optional

HEART_RATE_MEASUREMENT_UUID = "00002a37-0000-1000-8000-00805f9b34fb"
BODY_SENSOR_LOCATION_UUID = "00002a38-0000-1000-8000-00805f9b34fb"

# Simulated counter on the Dial (upstream ble_demo.c): starts at 90, 159 is followed by 90.
HR_START = 90
HR_LAST = 159

EXIT_PASS, EXIT_BAD, EXIT_NOT_FOUND, EXIT_TIMEOUT, EXIT_BLE_ERROR = 0, 1, 2, 3, 4


def _at_least_two(text: str) -> int:
    """argparse type: an int >= 2 (one value cannot show an increasing counter)."""
    value = int(text)
    if value < 2:
        raise argparse.ArgumentTypeError("must be at least 2")
    return value


def parse_args(argv: Optional[List[str]]) -> argparse.Namespace:
    """Parse the command line.

    Args:
        argv: arguments without the program name (None = sys.argv[1:]).
    """
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("--name", required=True, help="advertised name, e.g. M5-1A2B")
    p.add_argument("--notifications", type=_at_least_two, default=3,
                   help="notifications to receive before passing (default 3)")
    p.add_argument("--timeout", type=float, default=30.0,
                   help="seconds for the scan, and again for the notifications (default 30)")
    return p.parse_args(argv)


def parse_heart_rate(data: bytes) -> int:
    """Return the heart-rate value of a Heart Rate Measurement (0x2A37) payload.

    Flags bit 0 selects the value format: 0 = uint8 in byte 1 (what the Dial sends, with
    flags 0x06), 1 = uint16 little-endian in bytes 1-2.

    Raises:
        ValueError: payload too short for its format.
    """
    if len(data) < 2:
        raise ValueError(f"heart-rate payload too short: {bytes(data).hex()}")
    if data[0] & 0x01:
        if len(data) < 3:
            raise ValueError(f"uint16 heart-rate payload too short: {bytes(data).hex()}")
        return data[1] | (data[2] << 8)
    return data[1]


def counter_advanced(prev: int, cur: int) -> bool:
    """True if ``cur`` follows ``prev``: larger, or the Dial's wrap from 159 back to 90.

    why not strictly +1: a notification can be lost if this host is slow to subscribe or
    drain; the check is that the Dial's counter keeps moving forward.
    """
    if prev == HR_LAST and cur == HR_START:
        return True
    return cur > prev


def counter_increasing(values: Iterable[int]) -> bool:
    """True if every value in ``values`` advanced from its predecessor (counter_advanced)."""
    vals = list(values)
    return all(counter_advanced(a, b) for a, b in zip(vals, vals[1:]))


async def run(args: argparse.Namespace, scanner, client_cls) -> int:
    """Scan, connect, read 0x2A38, collect notifications of 0x2A37 and judge the counter.

    Args:
        args: parse_args() result.
        scanner: object with ``async find_device_by_name(name, timeout)`` (bleak.BleakScanner).
        client_cls: async-context-manager factory taking the device (bleak.BleakClient).
    Returns:
        process exit code (EXIT_*).
    """
    device = await scanner.find_device_by_name(args.name, timeout=args.timeout)
    if device is None:
        print(f"ble_check: {args.name} not found within {args.timeout:g} s", file=sys.stderr)
        return EXIT_NOT_FOUND

    values: List[int] = []
    errors: List[str] = []
    done = asyncio.Event()

    def on_notify(_sender, data: bytearray) -> None:
        # why every check here: the verdict must not wait for more values after a bad one
        try:
            hr = parse_heart_rate(data)
        except ValueError as e:
            errors.append(str(e))
            done.set()
            return
        print(f"ble_check: hr={hr}")
        if values and not counter_advanced(values[-1], hr):
            errors.append(f"counter not increasing: {values[-1]} -> {hr}")
            done.set()
        values.append(hr)
        if len(values) >= args.notifications:
            done.set()

    async with client_cls(device) as client:
        loc = await client.read_gatt_char(BODY_SENSOR_LOCATION_UUID)
        if len(loc) != 1:
            print(f"ble_check: body sensor location is {len(loc)} bytes, expected 1",
                  file=sys.stderr)
            return EXIT_BAD
        print(f"ble_check: body_sensor_location={loc[0]}")
        await client.start_notify(HEART_RATE_MEASUREMENT_UUID, on_notify)
        try:
            await asyncio.wait_for(done.wait(), timeout=args.timeout)
        except asyncio.TimeoutError:
            print(f"ble_check: timeout after {len(values)}/{args.notifications} notifications",
                  file=sys.stderr)
            return EXIT_TIMEOUT
        finally:
            await client.stop_notify(HEART_RATE_MEASUREMENT_UUID)

    if errors:
        print(f"ble_check: {errors[0]}", file=sys.stderr)
        return EXIT_BAD
    print("ble_check: PASS")
    return EXIT_PASS


def main(argv: Optional[List[str]] = None) -> int:
    """Command-line entry point; imports bleak here so the module imports without it."""
    args = parse_args(argv)
    try:
        from bleak import BleakClient, BleakScanner
        from bleak.exc import BleakError
    except ImportError:
        print("ble_check: bleak is not installed (run tools/setup.sh)", file=sys.stderr)
        return EXIT_BLE_ERROR
    try:
        return asyncio.run(run(args, BleakScanner, BleakClient))
    except BleakError as e:
        print(f"ble_check: BLE error: {e}", file=sys.stderr)
        return EXIT_BLE_ERROR


if __name__ == "__main__":
    sys.exit(main())
