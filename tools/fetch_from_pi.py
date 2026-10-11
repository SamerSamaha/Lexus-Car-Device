#!/usr/bin/env python3
"""Copies the car sessions and the screen recordings from the Pi to the laptop (LHU-049).

Run on the laptop, from the repository folder, with the Pi reachable over SSH:

    python tools/fetch_from_pi.py                 host lexus-pi, into local_recordings/
    python tools/fetch_from_pi.py --host lexus@172.20.10.10 --dry-run

One SSH call runs lexus-export-sessions on the Pi (VIN scrubbed, raw recordings never copied)
and lists the files; then each file that is missing on the laptop, or has a different size, is
copied with scp: lexus-data/export/ to local_recordings/car-sessions/ and lexus-data/screen/ to
local_recordings/screen/. The recording still being written is skipped. local_recordings/ is
ignored by git: nothing copied here is ever committed.
"""

import argparse
import subprocess
import sys
from pathlib import Path, PurePosixPath
from typing import Callable, Dict, List, Optional, Sequence, Tuple

MARKER = "@@lexus-files@@"
REMOTE_FOLDERS = {"export": "car-sessions", "screen": "screen"}
REMOTE_SCRIPT = (
    "lexus-export-sessions >&2; "
    'cat "/run/user/$(id -u)/lexus-screen-recording" 2>/dev/null; '
    f"echo {MARKER}; "
    "cd ~/lexus-data 2>/dev/null && find export screen -type f -printf '%p %s\\n' 2>/dev/null; "
    "true"
)

Runner = Callable[[List[str]], Tuple[int, str]]


def run_command(command: List[str]) -> Tuple[int, str]:
    try:
        completed = subprocess.run(command, stdout=subprocess.PIPE, text=True, check=False)
    except FileNotFoundError:
        return 127, ""
    return completed.returncode, completed.stdout


def parse_listing(output: str) -> Tuple[Optional[str], Dict[str, int]]:
    """The file being recorded (or None) and every remote file with its size in bytes."""
    before, _, after = output.partition(MARKER)
    in_progress = before.strip().splitlines()[-1].strip() if before.strip() else None
    files: Dict[str, int] = {}
    for line in after.splitlines():
        path, _, size = line.strip().rpartition(" ")
        if path and size.isdigit() and not PurePosixPath(path).is_absolute() and ".." not in path:
            files[path] = int(size)
    return in_progress, files


def local_path(destination: Path, remote: str) -> Optional[Path]:
    parts = PurePosixPath(remote).parts
    if len(parts) < 2 or parts[0] not in REMOTE_FOLDERS:
        return None
    return destination.joinpath(REMOTE_FOLDERS[parts[0]], *parts[1:])


def plan(destination: Path, in_progress: Optional[str], files: Dict[str, int]) -> List[Tuple[str, Path]]:
    copies = []
    for remote, size in sorted(files.items()):
        target = local_path(destination, remote)
        if target is None:
            continue
        if in_progress and PurePosixPath(in_progress).name == PurePosixPath(remote).name \
                and remote.startswith("screen/"):
            continue
        if target.is_file() and target.stat().st_size == size:
            continue
        copies.append((remote, target))
    return copies


def main(argument_list: Optional[Sequence[str]] = None, runner: Runner = run_command) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--host", default="lexus-pi")
    parser.add_argument("--destination", type=Path, default=Path("local_recordings"))
    parser.add_argument("--dry-run", action="store_true", help="list the copies, make none")
    arguments = parser.parse_args(argument_list)
    code, output = runner(["ssh", arguments.host, REMOTE_SCRIPT])
    if code != 0 or MARKER not in output:
        print(f"fetch_from_pi: ssh {arguments.host} failed ({code})", file=sys.stderr)
        return 1
    in_progress, files = parse_listing(output)
    already = 0
    for remote, size in files.items():
        target = local_path(arguments.destination, remote)
        if target is not None and target.is_file() and target.stat().st_size == size:
            already += 1
    copies = plan(arguments.destination, in_progress, files)
    failures = 0
    for remote, target in copies:
        print(("would copy " if arguments.dry_run else "copy ") + f"{remote} -> {target}")
        if arguments.dry_run:
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        status, _ = runner(["scp", "-q", f"{arguments.host}:lexus-data/{remote}", str(target)])
        if status != 0:
            failures += 1
            print(f"fetch_from_pi: copying {remote} failed ({status})", file=sys.stderr)
    if in_progress:
        print(f"skipped the recording still being written: {PurePosixPath(in_progress).name}")
    print(f"fetch_from_pi: {len(copies) - failures} copied, {already} already here, "
          f"{failures} failed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
