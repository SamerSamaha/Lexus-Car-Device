#!/usr/bin/env python3
"""Fail if the CAN code could transmit (REQ-001, DN-028).

Scans every C++ file under src/hardware/can/ for a call that writes to a socket or a file
descriptor: write(, send(, sendto(, sendmsg(, writev(, pwrite(. The CAN path only listens; the
compile-time test in tests/unit/can/socket_can_dbc_source_test.cpp checks the interfaces, and this
check covers the code behind them. Comments are ignored.

Exit 0 clean, 1 finding, 2 could not run.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import List, Optional, Sequence

EXIT_CODE_NO_FINDING = 0
EXIT_CODE_FINDING = 1
EXIT_CODE_CHECK_COULD_NOT_RUN = 2

DEFAULT_REPOSITORY_ROOT = Path(__file__).resolve().parent.parent
CAN_RELATIVE_PATH = Path("src/hardware/can")
CPP_SUFFIXES = (".h", ".hpp", ".cpp")
TRANSMIT_CALL_PATTERN = re.compile(r"(?<![A-Za-z0-9_])(?:::)?(write|send|sendto|sendmsg|writev|pwrite)\s*\(")
LINE_COMMENT_PATTERN = re.compile(r"//.*$")


def check_file(path: Path, label: str) -> List[str]:
    findings: List[str] = []
    for line_number, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), start=1):
        code = LINE_COMMENT_PATTERN.sub("", line)
        match = TRANSMIT_CALL_PATTERN.search(code)
        if match:
            findings.append(f"{label}:{line_number}: transmit call {match.group(1)}(): {line.strip()}")
    return findings


def check_can(repository_root: Path) -> List[str]:
    can_root = repository_root / CAN_RELATIVE_PATH
    if not can_root.is_dir():
        raise FileNotFoundError(f"{can_root} is not a directory")
    findings: List[str] = []
    for path in sorted(can_root.rglob("*")):
        if path.is_file() and path.suffix in CPP_SUFFIXES:
            findings.extend(check_file(path, path.relative_to(repository_root).as_posix()))
    return findings


def main(argument_list: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Fail if the CAN code could transmit.")
    parser.add_argument("--repository-root", type=Path, default=DEFAULT_REPOSITORY_ROOT)
    arguments = parser.parse_args(argument_list)
    try:
        findings = check_can(arguments.repository_root)
    except OSError as error:
        print(f"check_can_read_only: error: {error}", file=sys.stderr)
        return EXIT_CODE_CHECK_COULD_NOT_RUN
    for finding in findings:
        print(finding)
    print(f"check_can_read_only: {len(findings)} findings")
    if findings:
        print("check_can_read_only: FAILED.", file=sys.stderr)
        return EXIT_CODE_FINDING
    return EXIT_CODE_NO_FINDING


if __name__ == "__main__":
    sys.exit(main())
