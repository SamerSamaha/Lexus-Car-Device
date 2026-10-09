#!/usr/bin/env python3
"""Summarise sample-to-screen latency for REQ-009 (LHU-032).

Input: the CSV written by `lexus-head-unit --latency-log <file>` (one row per sample: when the
source stamped it, when it reached the view models, when the first frame after it was
swapped; all on the same monotonic clock). Reports, over every row, minimum, median, 95th and
99th percentile and maximum of

  * sample to screen (frame - sample): the REQ-009 figure, target 200 ms at the 95th percentile
    over 1,000 or more samples;
  * sample to view model (arrival - sample): the service, the D-Bus hop and the queue;
  * view model to screen (frame - arrival): waiting for and drawing the next frame.

Percentiles are nearest-rank, over the raw values; nothing is dropped. Adapter round-trip time
is not in these figures (REQ-009 reports it separately). Standard library only.

    measure_latency.py docs/measurements/latency/<file>.csv
"""

from __future__ import annotations

import argparse
import csv
import math
import sys
from pathlib import Path
from typing import Dict, List, Optional, Sequence

EXIT_CODE_OK = 0
EXIT_CODE_COULD_NOT_RUN = 2
TARGET_P95_MILLISECONDS = 200.0
MINIMUM_SAMPLES = 1000


def percentile(sorted_values: Sequence[float], fraction: float) -> float:
    """Nearest-rank percentile of already sorted values."""
    rank = max(1, math.ceil(fraction * len(sorted_values)))
    return sorted_values[rank - 1]


def statistics_of(values: Sequence[float]) -> Dict[str, float]:
    ordered = sorted(values)
    return {
        "min": ordered[0],
        "median": percentile(ordered, 0.5),
        "p95": percentile(ordered, 0.95),
        "p99": percentile(ordered, 0.99),
        "max": ordered[-1],
    }


def read_rows(path: Path) -> List[Dict[str, int]]:
    with path.open(newline="", encoding="ascii") as latency_file:
        return [
            {key: int(row[key]) for key in ("sample_ms", "arrival_ms", "frame_ms")}
            for row in csv.DictReader(latency_file)
        ]


def summarise(rows: Sequence[Dict[str, int]]) -> str:
    if not rows:
        return "no samples"
    parts = {
        "sample to screen": [row["frame_ms"] - row["sample_ms"] for row in rows],
        "sample to view model": [row["arrival_ms"] - row["sample_ms"] for row in rows],
        "view model to screen": [row["frame_ms"] - row["arrival_ms"] for row in rows],
    }
    lines = [f"samples: {len(rows)}"]
    for name, values in parts.items():
        figures = statistics_of(values)
        lines.append(
            f"{name} (ms): min {figures['min']:g} median {figures['median']:g} "
            f"p95 {figures['p95']:g} p99 {figures['p99']:g} max {figures['max']:g}"
        )
    p95 = statistics_of(parts["sample to screen"])["p95"]
    if len(rows) < MINIMUM_SAMPLES:
        lines.append(f"REQ-009: too few samples ({len(rows)} of {MINIMUM_SAMPLES})")
    else:
        verdict = "pass" if p95 <= TARGET_P95_MILLISECONDS else "fail"
        lines.append(f"REQ-009: {verdict} (p95 {p95:g} ms, target {TARGET_P95_MILLISECONDS:g} ms)")
    return "\n".join(lines)


def main(argument_list: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Summarise sample-to-screen latency.")
    parser.add_argument("latency_log", type=Path)
    arguments = parser.parse_args(argument_list)
    try:
        rows = read_rows(arguments.latency_log)
    except (OSError, KeyError, ValueError) as error:
        print(f"measure_latency: error: {error}", file=sys.stderr)
        return EXIT_CODE_COULD_NOT_RUN
    print(summarise(rows))
    return EXIT_CODE_OK


if __name__ == "__main__":
    sys.exit(main())
