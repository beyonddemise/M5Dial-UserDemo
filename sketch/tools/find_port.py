#!/usr/bin/env python3
"""Locate the serial port of an attached M5Dial by USB VID:PID.

The ESP32-S3 native USB (USB-Serial-JTAG) enumerates as 303A:1001 and the
device node differs on every plug-in (and between OSes), so tools must never
hard-code it. Prints the port and exits 0, exits 2 if none is found, or 3 if
several match (ambiguous; pass an explicit port to the tool instead).
"""
from __future__ import annotations

import argparse
import sys
from typing import Optional

import serial.tools.list_ports

DEFAULT_VIDPID = "303A:1001"


def find_ports(vidpid: str) -> list:
    """Return the device nodes of all serial ports matching ``vidpid``.

    Args:
        vidpid: ``"VVVV:PPPP"`` hex string, case-insensitive.
    """
    vid_s, pid_s = vidpid.split(":")
    vid, pid = int(vid_s, 16), int(pid_s, 16)
    return [p.device for p in serial.tools.list_ports.comports()
            if p.vid == vid and p.pid == pid]


def find_port(vidpid: str) -> Optional[str]:
    """Return the device node of the first matching port (or ``None``).

    Note: ``main`` (used by the shell tools) refuses ambiguous matches; this
    helper does not, so only use it where picking the first is harmless.
    """
    ports = find_ports(vidpid)
    return ports[0] if ports else None


def resolve_port(vidpid: str) -> tuple:
    """Pick the single matching port, refusing to guess.

    Returns:
        ``(port, exit_code, message)``: ``(port, 0, "")`` for exactly one match,
        ``(None, 2, msg)`` for none, ``(None, 3, msg)`` for several.
    """
    ports = find_ports(vidpid)
    if len(ports) > 1:
        # why: destructive tools (flash write/read) must never guess between boards
        return None, 3, (f"ambiguous: {len(ports)} devices match VID:PID {vidpid.upper()} "
                         f"({', '.join(ports)}). Unplug the others or pass an explicit port.")
    if not ports:
        return None, 2, (f"no M5Dial found (VID:PID {vidpid.upper()}). Plug in, or hold the "
                         "knob button while plugging in for download mode.")
    return ports[0], 0, ""


def main(argv: Optional[list] = None) -> int:
    """CLI entry point; returns the process exit code (0 found, 2 missing, 3 ambiguous)."""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--vidpid", default=DEFAULT_VIDPID)
    args = parser.parse_args(argv)
    port, code, msg = resolve_port(args.vidpid)
    if code:
        print(msg, file=sys.stderr)
        return code
    print(port)
    return 0


if __name__ == "__main__":
    sys.exit(main())
