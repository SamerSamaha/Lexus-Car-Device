#!/usr/bin/env python3
"""Records the touchscreen and the Pi's sound output to one .mkv per session (LHU-049, REQ-026).

    lexus-screen-record status   whether recording is on, and the file being written
    lexus-screen-record on       turn recording on (remembered in car.conf) and start it
    lexus-screen-record off      stop it and turn it off
    lexus-screen-record toggle   on when off, off when on: the hub's "Screen recording" tile
    lexus-screen-record run      the recorder loop that the user unit runs; idle when off

The setting is [recording] enabled in ~/.config/lexus-head-unit/car.conf, so it survives a
reboot: lexus-hub-session starts the unit at every login and "run" records only when it is on.
While a file is being written, its path is in the status file that the hub reads for its REC
indicator and for hiding the network address (REQ-026). The recorder is wf-recorder: the screen
(output DSI-2 by default) and the default sound output's monitor (assumption A24, A25).
"""

import argparse
import datetime
import os
import shlex
import shutil
import signal
import subprocess
import sys
import time
from pathlib import Path
from typing import Callable, Dict, List, Mapping, Optional, Sequence

sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))

import car_config  # noqa: E402

UNIT = "lexus-screen-recorder.service"
DEFAULTS = {
    "recording.enabled": "off",
    "recording.directory": "~/lexus-data/screen",
    "recording.output": "DSI-2",
    "recording.audio": "on",
    "recording.audio_device": "@DEFAULT_MONITOR@",
    "recording.framerate": "30",
    "recording.min_free_mb": "2048",
}
CHECK_INTERVAL_SECONDS = 1.0
STOP_WAIT_SECONDS = 10.0
BYTES_PER_MEGABYTE = 1 << 20
# Exit code when the recorder program is missing: the unit does not restart for it.
EXIT_RECORDER_MISSING = 3

Runner = Callable[[List[str]], int]


def run_command(command: List[str]) -> int:
    return subprocess.run(command, check=False).returncode


def settings(values: Mapping[str, str]) -> Dict[str, str]:
    merged = dict(DEFAULTS)
    merged.update({key: value for key, value in values.items()
                   if key.startswith("recording.") and value.strip()})
    return merged


def is_on(text: str) -> bool:
    return text.strip().lower() in ("on", "yes", "true", "1")


def status_path() -> Path:
    runtime = os.environ.get("XDG_RUNTIME_DIR") or f"/run/user/{os.getuid()}"
    return Path(runtime) / "lexus-screen-recording"


def recording_directory(merged: Mapping[str, str]) -> Path:
    return Path(os.path.expanduser(merged["recording.directory"]))


def next_file(directory: Path, now: datetime.datetime) -> Path:
    """screen-NNNN-<date>.mkv, NNNN one more than the highest number already there.

    The number orders the files even when the Pi's clock was wrong at boot (no clock battery)."""
    highest = 0
    if directory.is_dir():
        for entry in directory.iterdir():
            parts = entry.name.split("-")
            if len(parts) > 1 and parts[0] == "screen" and parts[1].isdigit():
                highest = max(highest, int(parts[1]))
    return directory / f"screen-{highest + 1:04d}-{now.strftime('%Y-%m-%dT%H-%M-%S')}.mkv"


def recorder_command(merged: Mapping[str, str], file: Path) -> List[str]:
    """The wf-recorder command line, or recording.command with {file} replaced when it is set
    (for a recorder that needs other options on the Pi, without a code change)."""
    custom = merged.get("recording.command", "").strip()
    if custom:
        return [word.replace("{file}", str(file)) for word in shlex.split(custom)]
    command = ["wf-recorder", "--file", str(file), "--codec", "libx264",
               "--codec-param", "preset=ultrafast", "--codec-param", "crf=26",
               "--framerate", merged["recording.framerate"], "--overwrite"]
    if merged["recording.output"].strip():
        command += ["--output", merged["recording.output"].strip()]
    if is_on(merged["recording.audio"]):
        command.append("--audio=" + merged["recording.audio_device"].strip())
    return command


def free_megabytes(directory: Path) -> int:
    return shutil.disk_usage(directory).free // BYTES_PER_MEGABYTE


class Recorder:
    """One recording: starts the recorder, watches it and the free space, stops it with SIGINT,
    which makes wf-recorder finish the file. The process factory and the clock are injected so
    the tests run without wf-recorder."""

    def __init__(self, merged: Mapping[str, str], status: Path,
                 start_process: Callable[[List[str]], subprocess.Popen] = subprocess.Popen,
                 free: Callable[[Path], int] = free_megabytes,
                 sleep: Callable[[float], None] = time.sleep,
                 now: Callable[[], datetime.datetime] = datetime.datetime.now) -> None:
        self.merged = merged
        self.status = status
        self.start_process = start_process
        self.free = free
        self.sleep = sleep
        self.now = now
        self.stop_requested = False
        self.process: Optional[subprocess.Popen] = None

    def request_stop(self, *_arguments) -> None:
        self.stop_requested = True

    def run(self) -> int:
        directory = recording_directory(self.merged)
        directory.mkdir(parents=True, exist_ok=True)
        minimum = int(self.merged["recording.min_free_mb"])
        if self.free(directory) < minimum:
            print(f"lexus-screen-record: less than {minimum} MB free in {directory}; not recording",
                  file=sys.stderr)
            return 0
        file = next_file(directory, self.now())
        command = recorder_command(self.merged, file)
        try:
            self.process = self.start_process(command)
        except OSError as error:
            print(f"lexus-screen-record: cannot start {command[0]}: {error}", file=sys.stderr)
            return EXIT_RECORDER_MISSING
        self.status.parent.mkdir(parents=True, exist_ok=True)
        self.status.write_text(str(file) + "\n", encoding="utf-8")
        print(f"lexus-screen-record: recording {file}", flush=True)
        reason = "stopped"
        try:
            while True:
                if self.stop_requested:
                    break
                code = self.process.poll()
                if code is not None:
                    print(f"lexus-screen-record: the recorder exited with {code}", file=sys.stderr)
                    # A recorder that failed at once leaves an empty file; the restart would
                    # otherwise add one every 5 s.
                    if file.is_file() and file.stat().st_size == 0:
                        file.unlink()
                    return code if code not in (0, EXIT_RECORDER_MISSING) else 1
                if self.free(directory) < minimum:
                    reason = f"less than {minimum} MB free"
                    break
                self.sleep(CHECK_INTERVAL_SECONDS)
            self.finish()
            print(f"lexus-screen-record: {reason}; {file}", flush=True)
            return 0
        finally:
            self.status.unlink(missing_ok=True)

    def finish(self) -> None:
        if self.process is None or self.process.poll() is not None:
            return
        self.process.send_signal(signal.SIGINT)
        try:
            self.process.wait(timeout=STOP_WAIT_SECONDS)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()


def set_enabled(config: Path, enabled: bool, runner: Runner) -> int:
    car_config.write_values(config, {"recording.enabled": "on" if enabled else "off"})
    status = runner(["systemctl", "--user", "restart" if enabled else "stop", UNIT])
    print(f"lexus-screen-record: recording {'on' if enabled else 'off'}"
          + ("" if status == 0 else f"; systemctl failed ({status})"))
    return status


def show_status(merged: Mapping[str, str], status: Path) -> int:
    current = status.read_text(encoding="utf-8").strip() if status.is_file() else ""
    line = f"recording {'on' if is_on(merged['recording.enabled']) else 'off'}"
    if current:
        size = Path(current).stat().st_size if Path(current).is_file() else 0
        line += f", writing {current} ({size // BYTES_PER_MEGABYTE} MB)"
    print(line)
    return 0


def main(argument_list: Optional[Sequence[str]] = None, runner: Runner = run_command,
         recorder_factory: Callable[[Mapping[str, str], Path], Recorder] = Recorder) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("command", choices=("status", "on", "off", "toggle", "run"))
    parser.add_argument("--config", type=Path, default=car_config.DEFAULT_PATH)
    parser.add_argument("--status-file", type=Path, default=None)
    arguments = parser.parse_args(argument_list)
    merged = settings(car_config.read(arguments.config))
    if arguments.command == "on":
        return set_enabled(arguments.config, True, runner)
    if arguments.command == "off":
        return set_enabled(arguments.config, False, runner)
    if arguments.command == "toggle":
        return set_enabled(arguments.config, not is_on(merged["recording.enabled"]), runner)
    status = arguments.status_file or status_path()
    if arguments.command == "status":
        return show_status(merged, status)
    if not is_on(merged["recording.enabled"]):
        print("lexus-screen-record: recording is off (lexus-screen-record on)")
        return 0
    recorder = recorder_factory(merged, status)
    signal.signal(signal.SIGTERM, recorder.request_stop)
    signal.signal(signal.SIGINT, recorder.request_stop)
    return recorder.run()


if __name__ == "__main__":
    sys.exit(main())
