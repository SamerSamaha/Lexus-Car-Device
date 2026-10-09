#!/usr/bin/env python3
"""Starts one session of the vehicle-data service in car mode (LHU-044, REQ-025).

The user unit lexus-vehicle-data-service.service runs this instead of the service. It:

1. reads the mode from car.conf (car, demo, replay or emulator);
2. makes a numbered session folder under <data_dir>/sessions/, for example 0007-car (numbered,
   not dated: the Pi has no clock battery, so the date can be wrong before the network is up);
3. writes the configuration the service will use into that folder: deploy/head_unit.conf with
   the overrides for the mode appended, so a later line wins;
4. starts the thermal and power logger into the folder (when vcgencmd exists);
5. replaces itself with the service, which writes the session log and, in the adapter modes,
   the raw recording into the folder.

Nothing here talks to the car; the service and its allowlist do that. --print-command shows the
folder and the command without starting anything (used by the tests and the self-check).
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Optional, Sequence

sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))

import car_config  # noqa: E402

REPOSITORY = Path(os.path.realpath(__file__)).parent.parent.parent
SERVICE_BINARY = "/usr/local/bin/lexus-vehicle-data-service"
EMULATOR_LINK = "/tmp/lexus-obd-emulator"
COUNTER_FILE = "next_session_number"
RECORDING_FILE = "obd.rec"


def next_session_number(sessions: Path) -> int:
    sessions.mkdir(parents=True, exist_ok=True)
    counter = sessions / COUNTER_FILE
    number = 1
    try:
        number = max(1, int(counter.read_text(encoding="ascii").strip()))
    except (FileNotFoundError, ValueError):
        existing = [int(child.name[:4]) for child in sessions.iterdir()
                    if child.is_dir() and child.name[:4].isdigit()]
        number = max(existing, default=0) + 1
    while any(child.name.startswith(f"{number:04d}-") for child in sessions.iterdir()):
        number += 1
    counter.write_text(f"{number + 1}\n", encoding="ascii")
    return number


def latest_recording(sessions: Path, exclude: Optional[Path] = None) -> Optional[Path]:
    """The newest car session with a recording that is not empty."""
    candidates = sorted((child for child in sessions.glob("*-car") if child.is_dir()), reverse=True)
    for child in candidates:
        recording = child / RECORDING_FILE
        if child != exclude and recording.is_file() and recording.stat().st_size > 0:
            return recording
    return None


def overrides_for(mode: str, values: Dict[str, str], sessions: Path,
                  session: Path) -> Optional[List[str]]:
    """Lines appended to head_unit.conf for the mode; None when the mode cannot start."""
    if mode == "car":
        return ["[source]", "kind = elm327"]
    if mode == "emulator":
        return ["[source]", "kind = elm327", "[elm327]", f"device = {EMULATOR_LINK}"]
    if mode == "demo":
        return ["[source]", "kind = fake"]
    replay_file = values.get("mode.replay_file", "").strip() or "last"
    recording = latest_recording(sessions, exclude=session) if replay_file == "last" \
        else Path(os.path.expanduser(replay_file))
    if recording is None or not recording.is_file():
        return None
    timing = values.get("mode.replay_timing", "original").strip() or "original"
    return ["[source]", "kind = replay", "[replay]", f"file = {recording}", f"timing = {timing}"]


def prepare(values: Dict[str, str], repository: Path, service_binary: str) -> Dict[str, object]:
    mode = car_config.mode_of(values)
    sessions = car_config.data_directory(values) / "sessions"
    number = next_session_number(sessions)
    session = sessions / f"{number:04d}-{mode}"
    session.mkdir(parents=True)
    overrides = overrides_for(mode, values, sessions, session)
    notes = []
    if overrides is None:
        notes.append(f"mode {mode}: no recording to replay; running the demo source instead")
        mode = "demo"
        overrides = ["[source]", "kind = fake"]
    base = (repository / "deploy" / "head_unit.conf").read_text(encoding="utf-8")
    configuration = session / "head_unit.conf"
    configuration.write_text(
        base.rstrip("\n") + "\n\n# Added by car_session.py for this session (mode "
        + mode + ")\n" + "\n".join(overrides) + "\n", encoding="utf-8")
    command = [service_binary, "--config", str(configuration),
               "--session-log", str(session / "session_log.csv")]
    if mode in ("car", "emulator"):
        command += ["--record", str(session / RECORDING_FILE)]
    with open(session / "session.txt", "w", encoding="utf-8") as summary:
        summary.write(f"session {number:04d}\nmode {mode}\n")
        summary.write("started (Pi clock, may be wrong before the network is up) "
                      + subprocess.run(["date", "-Iseconds"], capture_output=True, text=True,
                                       check=False).stdout.strip() + "\n")
        boot_id = Path("/proc/sys/kernel/random/boot_id")
        if boot_id.exists():
            summary.write("boot " + boot_id.read_text(encoding="ascii").strip() + "\n")
        installed = Path("/usr/local/share/lexus-head-unit/installed.txt")
        if installed.exists():
            summary.write(installed.read_text(encoding="utf-8"))
        for note in notes:
            summary.write(note + "\n")
    return {"number": number, "mode": mode, "session": session, "command": command,
            "notes": notes}


def thermal_command(values: Dict[str, str], repository: Path, number: int,
                    session: Path) -> List[str]:
    return [sys.executable, str(repository / "tools" / "measure" / "log_thermal_power.py"),
            "--label", f"session{number:04d}",
            "--power-source", values.get("session.power_source", "not recorded") or "not recorded",
            "--interval-seconds", values.get("session.thermal_interval_seconds", "5") or "5",
            "--output-dir", str(session)]


def main(argument_list: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--config", type=Path, default=car_config.DEFAULT_PATH)
    parser.add_argument("--repository", type=Path, default=REPOSITORY)
    parser.add_argument("--service", default=SERVICE_BINARY)
    parser.add_argument("--print-command", action="store_true",
                        help="make the folder and print the commands, start nothing")
    arguments = parser.parse_args(argument_list)
    values = car_config.read(arguments.config)
    prepared = prepare(values, arguments.repository, arguments.service)
    session = prepared["session"]
    thermal = thermal_command(values, arguments.repository, prepared["number"], session)
    for note in prepared["notes"]:
        print("car_session: " + note, file=sys.stderr)
    if arguments.print_command:
        print("session " + str(session))
        print("service " + " ".join(prepared["command"]))
        print("thermal " + " ".join(thermal))
        return 0
    print(f"car_session: session {session.name}", file=sys.stderr)
    if shutil.which("vcgencmd") and values.get("session.thermal_log", "on").strip() != "off":
        log = open(session / "thermal_logger.txt", "w", encoding="utf-8")
        subprocess.Popen(thermal, stdout=log, stderr=subprocess.STDOUT)  # noqa: S603
    command = prepared["command"]
    os.execv(command[0], command)
    return 0


if __name__ == "__main__":
    sys.exit(main())
