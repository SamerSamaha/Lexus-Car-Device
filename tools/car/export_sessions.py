#!/usr/bin/env python3
"""Prepares the car sessions on the Pi for copying to the laptop (LHU-044, REQ-025).

For every folder under <data_dir>/sessions/ it writes <data_dir>/export/<session>/ with:

- session.txt, head_unit.conf, session_log.csv and the thermal CSV and summary, copied as
  they are (none of them holds the VIN or the adapter address); a thermal summary that a power
  cut prevented is rebuilt from the CSV rows;
- session_summary.txt: transitions out of Connected and the request rate, from
  tools/measure/summarise_session_log.py;
- obd.scrubbed.rec: the raw recording with the VIN replaced, in plain text and in the
  hexadecimal Mode 09 form, by tools/scrub_recording.py. If any VIN-shaped string remains the
  recording is not exported and the session is reported as failed.

The raw recording obd.rec never goes into the export folder. Copy the export folder to the
laptop into local_recordings/, which git ignores:

    scp -r lexus-pi:lexus-data/export local_recordings/car-sessions

A session already exported is exported again only when one of its files has changed.
"""

import argparse
import csv
import json
import os
import shutil
import sys
from pathlib import Path
from typing import Dict, List, Optional, Sequence

HERE = Path(os.path.realpath(__file__)).parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE.parent / "measure"))

import car_config  # noqa: E402
import log_thermal_power  # noqa: E402
import scrub_recording  # noqa: E402
import summarise_session_log  # noqa: E402

COPIED_AS_THEY_ARE = ("session.txt", "head_unit.conf", "session_log.csv")
MARKER = ".exported.json"


def fingerprint(session: Path) -> Dict[str, int]:
    return {child.name: child.stat().st_size for child in sorted(session.iterdir())
            if child.is_file()}


def export_session(session: Path, export_root: Path) -> List[str]:
    """Exports one session; returns the problems found (empty when it is complete)."""
    target = export_root / session.name
    marker = target / MARKER
    current = fingerprint(session)
    if marker.exists() and json.loads(marker.read_text(encoding="utf-8")) == current:
        return []
    target.mkdir(parents=True, exist_ok=True)
    problems: List[str] = []
    for name in COPIED_AS_THEY_ARE:
        if (session / name).is_file():
            shutil.copy2(session / name, target / name)
    for thermal in session.glob("*_session[0-9][0-9][0-9][0-9].csv"):
        shutil.copy2(thermal, target / thermal.name)
        summary = thermal.with_suffix(".summary.txt")
        if summary.is_file():
            shutil.copy2(summary, target / summary.name)
        else:
            # A power cut ends the logger before it writes its summary: rebuild it from the rows.
            with open(thermal, newline="", encoding="ascii") as rows:
                result = log_thermal_power.summarise(list(csv.DictReader(rows)))
            (target / summary.name).write_text(
                log_thermal_power.format_summary(result, thermal.stem) + "\n", encoding="ascii")
    log = session / "session_log.csv"
    if log.is_file():
        counters, transitions = summarise_session_log.read_log(log)
        (target / "session_summary.txt").write_text(
            summarise_session_log.summarise_whole_log(counters, transitions) + "\n",
            encoding="ascii")
    recording = session / "obd.rec"
    scrubbed_path = target / "obd.scrubbed.rec"
    if recording.is_file():
        try:
            text = recording.read_text(encoding="ascii")
        except (OSError, UnicodeDecodeError) as error:
            problems.append(f"{session.name}: recording not readable: {error}")
        else:
            output, replaced, remaining = scrub_recording.scrub_recording(text)
            if remaining:
                problems.append(f"{session.name}: {remaining} VIN-shaped strings remain;"
                                " recording not exported")
                if scrubbed_path.exists():
                    scrubbed_path.unlink()
            else:
                scrubbed_path.write_text(output, encoding="ascii")
                with open(target / "session_summary.txt", "a", encoding="ascii") as summary:
                    summary.write(f"recording: {replaced} VIN occurrences replaced\n")
    if not problems:
        marker.write_text(json.dumps(current), encoding="utf-8")
    return problems


def export_all(data_directory: Path) -> int:
    sessions = data_directory / "sessions"
    export_root = data_directory / "export"
    folders = sorted(child for child in sessions.glob("[0-9][0-9][0-9][0-9]-*") if child.is_dir()) \
        if sessions.is_dir() else []
    problems: List[str] = []
    for session in folders:
        problems.extend(export_session(session, export_root))
    print(f"export_sessions: {len(folders)} sessions, {len(problems)} problems, into {export_root}")
    for problem in problems:
        print("  " + problem)
    return 1 if problems else 0


def main(argument_list: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--config", type=Path, default=car_config.DEFAULT_PATH)
    parser.add_argument("--data-dir", type=Path, help="default: session.data_dir in car.conf")
    arguments = parser.parse_args(argument_list)
    data_directory = arguments.data_dir or car_config.data_directory(car_config.read(arguments.config))
    return export_all(data_directory)


if __name__ == "__main__":
    sys.exit(main())
