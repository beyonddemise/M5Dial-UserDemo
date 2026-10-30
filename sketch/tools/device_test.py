#!/usr/bin/env python3
"""Flash a sketch to the attached M5Dial, reset it and check its serial log.

Usage:
    device_test.py <sketch-dir> --expect REGEX... [--forbid REGEX...]
                   [--timeout 20] [--no-upload]

Passes (exit 0) when every ``--expect`` regex matches the captured log in the
given order and no ``--forbid`` regex matches anywhere in it. Exit 1 lists the
unmet conditions; exit 2 means no device was found.
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
import time
from pathlib import Path
from typing import List, Optional

import serial

import find_port as fp

TOOLS = Path(__file__).resolve().parent
BAUD = 115200


def evaluate(log: str, expects: List[str], forbids: List[str]) -> List[str]:
    """Check ``log`` against the expectations and return a list of problems.

    ``expects`` must each match, searched in order (every match starts where the
    previous one ended). ``forbids`` are matched against the whole log with
    DOTALL, so a single pattern may span several lines. An empty result means
    the test passed.
    """
    problems: List[str] = []
    pos = 0
    for rx in expects:
        m = re.compile(rx).search(log, pos)
        if not m:
            problems.append(f"expect not matched (in order): {rx}")
            break  # why: later expects are only meaningful after an earlier one matched
        pos = m.end()
    for rx in forbids:
        if re.compile(rx, re.DOTALL).search(log):
            problems.append(f"forbid matched: {rx}")
    return problems


def _reset(ser: "serial.Serial") -> None:
    """Pulse EN low through RTS to reboot the board (DTR kept low = run mode)."""
    ser.dtr = False
    ser.rts = True
    time.sleep(0.1)
    ser.rts = False


def capture(port: str, expects: List[str], stop_early: bool, timeout: float) -> str:
    """Reset the board and collect serial output for up to ``timeout`` seconds.

    With ``stop_early`` the capture ends as soon as all ``expects`` matched.
    """
    chunks: List[str] = []
    # why: USB-CDC re-enumerates after an upload/reset; retry opening briefly
    deadline_open = time.time() + 10
    while True:
        try:
            ser = serial.Serial(port, BAUD, timeout=0.2)
            break
        except serial.SerialException:
            if time.time() > deadline_open:
                raise
            time.sleep(0.3)
    with ser:
        _reset(ser)
        end = time.time() + timeout
        while time.time() < end:
            data = ser.read(4096)
            if data:
                chunks.append(data.decode("utf-8", errors="replace"))
                if stop_early and not evaluate("".join(chunks), expects, []):
                    break
    return "".join(chunks)


def wait_for_port(timeout: float = 10.0, interval: float = 0.25,
                  sleep=time.sleep, clock=time.monotonic) -> tuple:
    """Poll for the board's port until it appears (it re-enumerates after upload).

    Ambiguity (exit 3) fails immediately; absence is retried until ``timeout``.

    Returns:
        ``(port, exit_code, message)`` as :func:`find_port.resolve_port`.
    """
    deadline = clock() + timeout
    while True:
        port, code, msg = fp.resolve_port(fp.DEFAULT_VIDPID)
        # why: only "none found" (2) is transient; a port or ambiguity is final
        if code != 2 or clock() >= deadline:
            return port, code, msg
        sleep(interval)


def main(argv: Optional[list] = None) -> int:
    """CLI entry point."""
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("sketch")
    ap.add_argument("--expect", nargs="+", default=[], metavar="REGEX")
    ap.add_argument("--forbid", nargs="+", default=[], metavar="REGEX")
    ap.add_argument("--timeout", type=float, default=20)
    ap.add_argument("--no-upload", action="store_true")
    ap.add_argument("--port", help="explicit serial port (skips auto-detection)")
    args = ap.parse_args(argv)

    # why: never guess between boards -- upload overwrites flash
    port, code, msg = (args.port, 0, "") if args.port else fp.resolve_port(fp.DEFAULT_VIDPID)
    if code:
        print(msg, file=sys.stderr)
        return code
    if not args.no_upload:
        try:
            subprocess.run([str(TOOLS / "upload.sh"), args.sketch, port], check=True)
        except subprocess.CalledProcessError as exc:
            print(f"FAIL: upload failed (exit {exc.returncode})", file=sys.stderr)
            return 1
        # why: the port node can change after the post-upload hard reset
        time.sleep(1.5)
        if not args.port:
            port, code, msg = wait_for_port()
            if code:
                print(msg, file=sys.stderr)
                return code
    try:
        log = capture(port, args.expect, stop_early=not args.forbid, timeout=args.timeout)
    except serial.SerialException as exc:
        print(f"FAIL: serial port {port} unusable: {exc}", file=sys.stderr)
        return 1
    print(log)
    problems = evaluate(log, args.expect, args.forbid)
    if problems:
        print("FAIL:\n  " + "\n  ".join(problems), file=sys.stderr)
        return 1
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
