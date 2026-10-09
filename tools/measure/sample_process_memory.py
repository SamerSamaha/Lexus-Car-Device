#!/usr/bin/env python3
"""Sample the memory of a process and all its descendants (REQ-018, REQ-014 input).

A browser is many processes, and resident set size (RSS) counts a shared page once in every
process that maps it, so the sum of RSS overstates what the browser costs. Every interval this
script records both sums for the process tree: RSS (VmRSS) and proportional set size (Pss from
smaps_rollup, shared pages divided among their users), which adds up correctly. One CSV row per
sample is written under docs/measurements/memory/, then a summary with minimum, median and
maximum of each.

Runs on the Pi (Linux /proc). Standard library only.

Usage:
    sample_process_memory.py --pid <browser pid> --label browser_video \\
        --interval-seconds 10 --duration-seconds 600
    sample_process_memory.py --name chromium --label browser_video
"""

from __future__ import annotations

import argparse
import csv
import datetime
import statistics
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Dict, List, Optional, Sequence, Set, Tuple

EXIT_CODE_OK = 0
EXIT_CODE_COULD_NOT_RUN = 2

DEFAULT_OUTPUT_DIRECTORY = Path(__file__).resolve().parent.parent.parent / "docs" / "measurements" / "memory"
DEFAULT_PROC_ROOT = Path("/proc")
CSV_COLUMNS = ["utc_time", "elapsed_seconds", "process_count", "rss_kilobytes", "pss_kilobytes"]


@dataclass
class ProcessEntry:
    parent_pid: int
    name: str


@dataclass
class MemorySample:
    process_count: int
    rss_kilobytes: int
    pss_kilobytes: Optional[int]


def read_process_table(proc_root: Path) -> Dict[int, ProcessEntry]:
    """pid -> (parent pid, command name) for every process visible under proc_root."""
    table: Dict[int, ProcessEntry] = {}
    for entry in proc_root.iterdir():
        if not entry.name.isdigit():
            continue
        try:
            stat = (entry / "stat").read_text(encoding="ascii", errors="replace")
        except OSError:
            continue
        # The command name is in parentheses and may itself contain spaces or parentheses.
        name_start = stat.find("(")
        name_end = stat.rfind(")")
        fields = stat[name_end + 2 :].split()
        if name_start < 0 or name_end < 0 or len(fields) < 2:
            continue
        table[int(entry.name)] = ProcessEntry(parent_pid=int(fields[1]), name=stat[name_start + 1 : name_end])
    return table


def process_tree(table: Dict[int, ProcessEntry], root_pids: Set[int]) -> Set[int]:
    """The root processes and every descendant of them."""
    children: Dict[int, List[int]] = {}
    for pid, entry in table.items():
        children.setdefault(entry.parent_pid, []).append(pid)
    tree: Set[int] = set()
    pending = [pid for pid in root_pids if pid in table]
    while pending:
        pid = pending.pop()
        if pid in tree:
            continue
        tree.add(pid)
        pending.extend(children.get(pid, []))
    return tree


def roots_named(table: Dict[int, ProcessEntry], name: str) -> Set[int]:
    """Processes with that command name whose parent does not have it (the top of each tree)."""
    named = {pid for pid, entry in table.items() if entry.name == name}
    return {pid for pid in named if table[pid].parent_pid not in named}


def read_kilobytes(path: Path, key: str) -> Optional[int]:
    try:
        for line in path.read_text(encoding="ascii", errors="replace").splitlines():
            if line.startswith(key + ":"):
                return int(line.split()[1])
    except (OSError, ValueError, IndexError):
        return None
    return None


def sample_tree(proc_root: Path, root_pids: Set[int], name: Optional[str]) -> MemorySample:
    table = read_process_table(proc_root)
    roots = set(root_pids) | (roots_named(table, name) if name else set())
    tree = process_tree(table, roots)
    rss_total = 0
    pss_total = 0
    pss_complete = True
    for pid in sorted(tree):
        rss = read_kilobytes(proc_root / str(pid) / "status", "VmRSS")
        pss = read_kilobytes(proc_root / str(pid) / "smaps_rollup", "Pss")
        rss_total += rss or 0
        if pss is None:
            pss_complete = False
        else:
            pss_total += pss
    return MemorySample(len(tree), rss_total, pss_total if pss_complete else None)


def summarise(rows: Sequence[Dict[str, str]]) -> str:
    def numbers(column: str) -> List[int]:
        return [int(row[column]) for row in rows if row.get(column) not in (None, "")]

    lines = [f"samples: {len(rows)}"]
    for column in ("process_count", "rss_kilobytes", "pss_kilobytes"):
        values = numbers(column)
        if values:
            lines.append(
                f"{column}: min {min(values)} median {statistics.median(values):g} max {max(values)}"
            )
        else:
            lines.append(f"{column}: no values")
    return "\n".join(lines)


def run_session(
    proc_root: Path,
    root_pids: Set[int],
    name: Optional[str],
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
        writer = csv.DictWriter(output_file, fieldnames=CSV_COLUMNS, lineterminator="\n")
        writer.writeheader()
        while True:
            elapsed = now() - start
            sample = sample_tree(proc_root, root_pids, name)
            row = {
                "utc_time": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
                "elapsed_seconds": f"{elapsed:.1f}",
                "process_count": str(sample.process_count),
                "rss_kilobytes": str(sample.rss_kilobytes),
                "pss_kilobytes": "" if sample.pss_kilobytes is None else str(sample.pss_kilobytes),
            }
            writer.writerow(row)
            output_file.flush()
            rows.append(row)
            if elapsed + interval_seconds > duration_seconds:
                break
            sleep(interval_seconds)
    summary = summarise(rows)
    output_path.with_suffix(".summary.txt").write_text(summary + "\n", encoding="ascii")
    return rows, summary


def main(argument_list: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Sample the memory of a process tree.")
    parser.add_argument("--pid", type=int, action="append", default=[], help="root process (repeatable)")
    parser.add_argument("--name", help="command name of the root processes, for example chromium")
    parser.add_argument("--label", required=True, help="used in the file name")
    parser.add_argument("--interval-seconds", type=float, default=10.0)
    parser.add_argument("--duration-seconds", type=float, default=600.0)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIRECTORY)
    parser.add_argument("--proc-root", type=Path, default=DEFAULT_PROC_ROOT)
    arguments = parser.parse_args(argument_list)
    if not arguments.pid and not arguments.name:
        print("sample_process_memory: give --pid or --name", file=sys.stderr)
        return EXIT_CODE_COULD_NOT_RUN
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H%M%SZ")
    output_path = arguments.output_dir / f"{stamp}_{arguments.label}.csv"
    try:
        _, summary = run_session(
            arguments.proc_root,
            set(arguments.pid),
            arguments.name,
            arguments.interval_seconds,
            arguments.duration_seconds,
            output_path,
        )
    except OSError as error:
        print(f"sample_process_memory: error: {error}", file=sys.stderr)
        return EXIT_CODE_COULD_NOT_RUN
    print(summary)
    print(f"written to {output_path}")
    return EXIT_CODE_OK


if __name__ == "__main__":
    sys.exit(main())
