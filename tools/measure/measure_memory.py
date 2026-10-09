#!/usr/bin/env python3
"""Memory per process of the running system, for REQ-014 (LHU-032).

Samples several process trees at once, each under a label, every interval (default 10 s for
30 min), using the same tree walk as sample_process_memory.py: PSS and RSS summed over a
process and its descendants. One CSV row per sample with three columns per label
(<label>_pss_kb, <label>_rss_kb, <label>_processes), written under docs/measurements/memory/,
then a summary per label with minimum, median, maximum and the PSS growth from minute 5 to the
last sample (REQ-014 asks for under 5 % from minute 5 to minute 30).

    measure_memory.py --process service=lexus-vehicle-da --process hub=lexus-hub \\
        --process app=lexus-head-unit --process browser=chromium --label system_30min

Process names are the kernel's command names, which are cut to 15 characters. Standard library
only; runs on the Pi.
"""

from __future__ import annotations

import argparse
import csv
import datetime
import statistics
import sys
import time
from pathlib import Path
from typing import Callable, Dict, List, Optional, Sequence, Tuple

sys.path.insert(0, str(Path(__file__).resolve().parent))
from sample_process_memory import DEFAULT_OUTPUT_DIRECTORY, sample_tree  # noqa: E402

EXIT_CODE_OK = 0
EXIT_CODE_COULD_NOT_RUN = 2
GROWTH_FROM_SECONDS = 300.0
GROWTH_LIMIT_PERCENT = 5.0


def parse_process_arguments(values: Sequence[str]) -> List[Tuple[str, str]]:
    processes = []
    for value in values:
        label, separator, name = value.partition("=")
        if not separator or not label or not name:
            raise ValueError(f"--process wants label=name, got {value!r}")
        processes.append((label, name))
    return processes


def columns_for(processes: Sequence[Tuple[str, str]]) -> List[str]:
    columns = ["utc_time", "elapsed_seconds"]
    for label, _ in processes:
        columns += [f"{label}_pss_kb", f"{label}_rss_kb", f"{label}_processes"]
    return columns


def growth_percent(rows: Sequence[Dict[str, str]], column: str) -> Optional[float]:
    """PSS growth from the first sample at or after minute 5 to the last sample."""
    values = [(float(row["elapsed_seconds"]), row[column]) for row in rows if row[column] != ""]
    later = [(seconds, int(value)) for seconds, value in values if seconds >= GROWTH_FROM_SECONDS]
    if len(later) < 2 or later[0][1] == 0:
        return None
    return (later[-1][1] - later[0][1]) / later[0][1] * 100.0


def summarise(rows: Sequence[Dict[str, str]], processes: Sequence[Tuple[str, str]]) -> str:
    lines = [f"samples: {len(rows)}"]
    for label, name in processes:
        pss = [int(row[f"{label}_pss_kb"]) for row in rows if row[f"{label}_pss_kb"] != ""]
        if not pss:
            lines.append(f"{label} ({name}): no PSS values (process absent or unreadable)")
            continue
        growth = growth_percent(rows, f"{label}_pss_kb")
        growth_text = "unknown (under 5 min of samples)" if growth is None else (
            f"{growth:+.2f} % ({'under' if growth < GROWTH_LIMIT_PERCENT else 'not under'} {GROWTH_LIMIT_PERCENT:g} %)"
        )
        lines.append(
            f"{label} ({name}) PSS kB: min {min(pss)} median {statistics.median(pss):g} max {max(pss)}; growth from minute 5: {growth_text}"
        )
    return "\n".join(lines)


def run_session(
    proc_root: Path,
    processes: Sequence[Tuple[str, str]],
    interval_seconds: float,
    duration_seconds: float,
    output_path: Path,
    sleep: Callable[[float], None] = time.sleep,
    now: Callable[[], float] = time.monotonic,
) -> Tuple[List[Dict[str, str]], str]:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    rows: List[Dict[str, str]] = []
    start = now()
    with output_path.open("w", newline="", encoding="ascii") as output_file:
        writer = csv.DictWriter(output_file, fieldnames=columns_for(processes), lineterminator="\n")
        writer.writeheader()
        while True:
            elapsed = now() - start
            row = {
                "utc_time": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
                "elapsed_seconds": f"{elapsed:.1f}",
            }
            for label, name in processes:
                sample = sample_tree(proc_root, set(), name)
                present = sample.process_count > 0
                row[f"{label}_pss_kb"] = str(sample.pss_kilobytes) if present and sample.pss_kilobytes is not None else ""
                row[f"{label}_rss_kb"] = str(sample.rss_kilobytes) if present else ""
                row[f"{label}_processes"] = str(sample.process_count)
            writer.writerow(row)
            output_file.flush()
            rows.append(row)
            if elapsed + interval_seconds > duration_seconds:
                break
            sleep(interval_seconds)
    summary = summarise(rows, processes)
    output_path.with_suffix(".summary.txt").write_text(summary + "\n", encoding="ascii")
    return rows, summary


def main(argument_list: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Memory per process of the running system.")
    parser.add_argument("--process", action="append", default=[], help="label=command name (repeatable)")
    parser.add_argument("--label", required=True)
    parser.add_argument("--interval-seconds", type=float, default=10.0)
    parser.add_argument("--duration-seconds", type=float, default=1800.0)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIRECTORY)
    parser.add_argument("--proc-root", type=Path, default=Path("/proc"))
    arguments = parser.parse_args(argument_list)
    try:
        processes = parse_process_arguments(arguments.process)
        if not processes:
            raise ValueError("give at least one --process label=name")
        stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H%M%SZ")
        output_path = arguments.output_dir / f"{stamp}_{arguments.label}.csv"
        _, summary = run_session(arguments.proc_root, processes, arguments.interval_seconds,
                                 arguments.duration_seconds, output_path)
    except (OSError, ValueError) as error:
        print(f"measure_memory: error: {error}", file=sys.stderr)
        return EXIT_CODE_COULD_NOT_RUN
    print(summary)
    print(f"written to {output_path}")
    return EXIT_CODE_OK


if __name__ == "__main__":
    sys.exit(main())
