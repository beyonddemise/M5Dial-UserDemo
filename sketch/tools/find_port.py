#!/usr/bin/env python3
"""Locate the serial port of an attached M5Dial by USB VID:PID.

The ESP32-S3 native USB (USB-Serial-JTAG) enumerates as 303A:1001 and the
device node differs on every plug-in (and between OSes), so tools must never
hard-code it. Prints the port and exits 0, or exits 2 with a hint on stderr.
"""
from __future__ import annotations

import argparse
import sys
from typing import Optional

import serial.tools.list_ports

DEFAULT_VIDPID = "303A:1001"


def find_port(vidpid: str) -> Optional[str]:
    """Return the device node of the first serial port matching ``vidpid``.

    Args:
        vidpid: ``"VVVV:PPPP"`` hex string, case-insensitive.

    Returns:
        The port path (e.g. ``/dev/cu.usbmodem21101``) or ``None``.
    """
    vid_s, pid_s = vidpid.split(":")
    vid, pid = int(vid_s, 16), int(pid_s, 16)
    for port in serial.tools.list_ports.comports():
        if port.vid == vid and port.pid == pid:
            return port.device
    return None


def main(argv: Optional[list] = None) -> int:
    """CLI entry point; returns the process exit code (0 found, 2 missing)."""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--vidpid", default=DEFAULT_VIDPID)
    args = parser.parse_args(argv)
    port = find_port(args.vidpid)
    if port is None:
        print(f"no M5Dial found (VID:PID {args.vidpid.upper()}). Plug in, or hold the "
              "knob button while plugging in for download mode.", file=sys.stderr)
        return 2
    print(port)
    return 0


if __name__ == "__main__":
    sys.exit(main())
