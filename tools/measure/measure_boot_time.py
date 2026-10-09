#!/usr/bin/env python3
"""Boot time to the hub's first frame, for REQ-013 (LHU-032).

After each cold boot of the Pi (power removed, then applied), run

    measure_boot_time.py record --mark /run/user/1000/lexus-first-frame.txt

It reads the mark the hub wrote on its first frame (`lexus-hub --first-frame-mark <file>`: the
time since the kernel started), the kernel and userspace times from `systemd-analyze`, and the
boot id, and appends one row to docs/measurements/boot_time/boots.csv. A boot already in the
file is refused, so no boot is counted twice. Then

    measure_boot_time.py summarise

prints every boot and minimum, median and maximum, with the provisional REQ-013 target of 15 s
judged once 10 or more boots exist. Time in the firmware and bootloader before the kernel is
not in the mark; the procedure measures it apart and it is reported beside, not hidden.

Standard library only.
"""

from __future__ import annotations

import argparse
import csv
import datetime
import re
import statistics
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Optional, Sequence

EXIT_CODE_OK = 0
EXIT_CODE_REFUSED = 1
EXIT_CODE_COULD_NOT_RUN = 2
PROVISIONAL_TARGET_MILLISECONDS = 15000
MINIMUM_BOOTS = 10
DEFAULT_OUTPUT = Path(__file__).resolve().parent.parent.parent / "docs" / "measurements" / "boot_time" / "boots.csv"
CSV_COLUMNS = ["utc_time", "boot_id", "first_frame_boottime_ms", "kernel_seconds", "userspace_seconds"]
MARK_PATTERN = re.compile(r"first_frame_boottime_ms=(\d+)")
ANALYZE_PATTERN = re.compile(r"((?:[\d.]+(?:min|ms|s)\s*)+)\((kernel|userspace)\)")
DURATION_PART_PATTERN = re.compile(r"([\d.]+)(min|ms|s)")


def parse_mark(text: str) -> int:
    match = MARK_PATTERN.search(text)
    if not match:
        raise ValueError(f"not a first-frame mark: {text!r}")
    return int(match.group(1))


def parse_systemd_analyze(text: str) -> Dict[str, float]:
    """Kernel and userspace seconds from 'Startup finished in 2.1s (kernel) + 5.4s (userspace) = ...'."""
    seconds: Dict[str, float] = {}
    factor = {"ms": 0.001, "s": 1.0, "min": 60.0}
    for duration, part in ANALYZE_PATTERN.findall(text):
        seconds[part] = sum(
            float(value) * factor[unit] for value, unit in DURATION_PART_PATTERN.findall(duration)
        )
    return seconds


def read_rows(path: Path) -> List[Dict[str, str]]:
    if not path.exists():
        return []
    with path.open(newline="", encoding="ascii") as boots_file:
        return list(csv.DictReader(boots_file))


def record(mark_text: str, analyze_text: str, boot_id: str, output: Path) -> Dict[str, str]:
    rows = read_rows(output)
    if any(row["boot_id"] == boot_id for row in rows):
        raise FileExistsError(f"boot {boot_id} is already recorded")
    times = parse_systemd_analyze(analyze_text)
    row = {
        "utc_time": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "boot_id": boot_id,
        "first_frame_boottime_ms": str(parse_mark(mark_text)),
        "kernel_seconds": f"{times['kernel']:.3f}" if "kernel" in times else "",
        "userspace_seconds": f"{times['userspace']:.3f}" if "userspace" in times else "",
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    new_file = not output.exists()
    with output.open("a", newline="", encoding="ascii") as boots_file:
        writer = csv.DictWriter(boots_file, fieldnames=CSV_COLUMNS, lineterminator="\n")
        if new_file:
            writer.writeheader()
        writer.writerow(row)
    return row


def summarise(rows: Sequence[Dict[str, str]]) -> str:
    values = [int(row["first_frame_boottime_ms"]) for row in rows]
    lines = [f"boots: {len(values)}"]
    lines.extend(f"  {row['utc_time']} {row['first_frame_boottime_ms']} ms" for row in rows)
    if values:
        lines.append(f"first frame (ms since kernel start): min {min(values)} median {statistics.median(values):g} max {max(values)}")
    if len(values) < MINIMUM_BOOTS:
        lines.append(f"REQ-013: too few boots ({len(values)} of {MINIMUM_BOOTS})")
    else:
        worst = max(values)
        verdict = "within" if worst <= PROVISIONAL_TARGET_MILLISECONDS else "over"
        lines.append(f"REQ-013 (provisional {PROVISIONAL_TARGET_MILLISECONDS} ms): every boot {verdict} the target, worst {worst} ms")
    return "\n".join(lines)


def main(argument_list: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Boot time to the hub's first frame.")
    parser.add_argument("action", choices=["record", "summarise"])
    parser.add_argument("--mark", type=Path, help="the file the hub wrote (record)")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    arguments = parser.parse_args(argument_list)
    try:
        if arguments.action == "summarise":
            print(summarise(read_rows(arguments.output)))
            return EXIT_CODE_OK
        if arguments.mark is None:
            print("measure_boot_time: record needs --mark", file=sys.stderr)
            return EXIT_CODE_COULD_NOT_RUN
        analyze = subprocess.run(["systemd-analyze"], capture_output=True, text=True, check=False).stdout
        boot_id = Path("/proc/sys/kernel/random/boot_id").read_text(encoding="ascii").strip()
        row = record(arguments.mark.read_text(encoding="ascii"), analyze, boot_id, arguments.output)
        print(f"recorded: {row}")
        return EXIT_CODE_OK
    except FileExistsError as error:
        print(f"measure_boot_time: {error}", file=sys.stderr)
        return EXIT_CODE_REFUSED
    except (OSError, ValueError) as error:
        print(f"measure_boot_time: error: {error}", file=sys.stderr)
        return EXIT_CODE_COULD_NOT_RUN


if __name__ == "__main__":
    sys.exit(main())
