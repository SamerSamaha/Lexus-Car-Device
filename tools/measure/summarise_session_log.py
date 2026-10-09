#!/usr/bin/env python3
"""Summarise a session log of the vehicle-data service for REQ-019 (LHU-024).

Input: the CSV written by `lexus-vehicle-data-service --session-log <file>` (counters every
10 s, every connection transition with its cause). With the audio window given (the seconds
from the start of the log when audio started and stopped), it reports:

  * the OBD request rate (requests per second) outside and inside the audio window, and the
    change in percent (REQ-019: within 20 %);
  * every transition out of Connected, with its time, cause, and whether it fell in the audio
    window (REQ-019: none caused by the audio link; whether one was is the tester's judgement,
    recorded in the procedure's notes, so the tool lists them rather than deciding).

    summarise_session_log.py session.csv --audio-start-seconds 600 --audio-end-seconds 1800

Standard library only. Exit 0 printed, 2 could not run.
"""

from __future__ import annotations

import argparse
import csv
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional, Sequence, Tuple

EXIT_CODE_OK = 0
EXIT_CODE_COULD_NOT_RUN = 2
RATE_TOLERANCE_PERCENT = 20.0


@dataclass
class CounterRow:
    seconds: float
    requests: int


@dataclass
class Transition:
    seconds: float
    trigger: str
    origin: str
    target: str


def read_log(path: Path) -> Tuple[List[CounterRow], List[Transition]]:
    counters: List[CounterRow] = []
    transitions: List[Transition] = []
    with path.open(newline="", encoding="ascii") as log_file:
        rows = list(csv.DictReader(log_file))
    if not rows:
        return counters, transitions
    start = int(rows[0]["milliseconds"])
    for row in rows:
        seconds = (int(row["milliseconds"]) - start) / 1000.0
        if row["event"] == "counters":
            counters.append(CounterRow(seconds, int(row["requests_sent"])))
        elif row["event"] == "transition":
            transitions.append(Transition(seconds, row["trigger"], row["from"], row["to"]))
    return counters, transitions


def rate_between(counters: Sequence[CounterRow], start: float, end: float) -> Optional[float]:
    """Requests per second between the first and last counter rows inside [start, end]."""
    inside = [row for row in counters if start <= row.seconds <= end]
    if len(inside) < 2 or inside[-1].seconds <= inside[0].seconds:
        return None
    return (inside[-1].requests - inside[0].requests) / (inside[-1].seconds - inside[0].seconds)


def summarise(counters: Sequence[CounterRow], transitions: Sequence[Transition],
              audio_start: float, audio_end: float) -> str:
    log_end = counters[-1].seconds if counters else 0.0
    before = rate_between(counters, 0.0, audio_start)
    during = rate_between(counters, audio_start, audio_end)
    after = rate_between(counters, audio_end, log_end)
    without_audio = [rate for rate in (before, after) if rate is not None]
    baseline = sum(without_audio) / len(without_audio) if without_audio else None
    lines = [f"log: {log_end:.0f} s, {len(counters)} counter rows, {len(transitions)} transitions"]
    lines.append("request rate without audio: " + ("unknown" if baseline is None else f"{baseline:.2f}/s"))
    lines.append("request rate with audio: " + ("unknown" if during is None else f"{during:.2f}/s"))
    if baseline and during is not None:
        change = (during - baseline) / baseline * 100.0
        within = abs(change) <= RATE_TOLERANCE_PERCENT
        lines.append(f"change: {change:+.1f} % ({'within' if within else 'outside'} {RATE_TOLERANCE_PERCENT:.0f} %)")
    lost = [transition for transition in transitions if transition.origin == "Connected"]
    lines.append(f"transitions out of Connected: {len(lost)}")
    for transition in lost:
        in_window = audio_start <= transition.seconds <= audio_end
        lines.append(
            f"  {transition.seconds:.1f} s {transition.trigger} -> {transition.target}"
            f" ({'during audio' if in_window else 'without audio'})"
        )
    return "\n".join(lines)


def main(argument_list: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Summarise a session log for REQ-019.")
    parser.add_argument("log", type=Path)
    parser.add_argument("--audio-start-seconds", type=float, required=True)
    parser.add_argument("--audio-end-seconds", type=float, required=True)
    arguments = parser.parse_args(argument_list)
    try:
        counters, transitions = read_log(arguments.log)
    except (OSError, KeyError, ValueError) as error:
        print(f"summarise_session_log: error: {error}", file=sys.stderr)
        return EXIT_CODE_COULD_NOT_RUN
    print(summarise(counters, transitions, arguments.audio_start_seconds, arguments.audio_end_seconds))
    return EXIT_CODE_OK


if __name__ == "__main__":
    sys.exit(main())
