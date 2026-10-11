# Verifies: REQ-026
"""Tests of the screen recorder (tools/car/screen_record.py) and the copy to the laptop
(tools/fetch_from_pi.py), LHU-049. No wf-recorder, ssh or Pi is needed."""

import contextlib
import datetime
import io
import os
import signal
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "car"))
sys.path.insert(0, str(HERE))

import car_config  # noqa: E402
import fetch_from_pi  # noqa: E402
import screen_record  # noqa: E402

REPOSITORY = HERE.parent
FIXED_TIME = datetime.datetime(2026, 10, 11, 14, 3, 22)


class FakeProcess:
    """Stands in for wf-recorder: running until it is sent SIGINT, or exits on its own."""

    def __init__(self, command, exit_after_polls=None):
        self.command = command
        self.signals = []
        self.returncode = None
        self.polls = 0
        self.exit_after_polls = exit_after_polls

    def poll(self):
        self.polls += 1
        if self.exit_after_polls is not None and self.polls >= self.exit_after_polls:
            self.returncode = 1
        return self.returncode

    def send_signal(self, number):
        self.signals.append(number)
        self.returncode = 0

    def wait(self, timeout=None):
        return self.returncode

    def kill(self):
        self.returncode = -9


class ScreenRecordTest(unittest.TestCase):
    def setUp(self):
        self.folder = Path(tempfile.mkdtemp())
        self.config = self.folder / "car.conf"
        self.status = self.folder / "run" / "lexus-screen-recording"
        self.commands = []

    def runner(self, command):
        self.commands.append(command)
        return 0

    def merged(self, **overrides):
        values = {"recording.enabled": "on", "recording.directory": str(self.folder / "screen")}
        values.update({f"recording.{key}": value for key, value in overrides.items()})
        return screen_record.settings(values)

    def quiet(self, function, *arguments, **keywords):
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            return function(*arguments, **keywords)

    def test_toggle_remembers_the_setting_in_car_conf_and_starts_or_stops_the_unit(self):
        self.config.write_text("[mode]\nkind = car\n", encoding="utf-8")
        self.quiet(screen_record.main, ["toggle", "--config", str(self.config)], runner=self.runner)
        self.assertEqual(car_config.read(self.config)["recording.enabled"], "on")
        self.assertEqual(car_config.read(self.config)["mode.kind"], "car")
        self.assertEqual(self.commands[-1], ["systemctl", "--user", "restart", screen_record.UNIT])
        self.quiet(screen_record.main, ["toggle", "--config", str(self.config)], runner=self.runner)
        self.assertEqual(car_config.read(self.config)["recording.enabled"], "off")
        self.assertEqual(self.commands[-1], ["systemctl", "--user", "stop", screen_record.UNIT])
        self.quiet(screen_record.main, ["on", "--config", str(self.config)], runner=self.runner)
        self.assertEqual(car_config.read(self.config)["recording.enabled"], "on")

    def test_run_records_nothing_while_the_setting_is_off(self):
        self.config.write_text("[recording]\nenabled = off\n", encoding="utf-8")
        made = []
        code = self.quiet(screen_record.main,
                          ["run", "--config", str(self.config), "--status-file", str(self.status)],
                          recorder_factory=lambda merged, status: made.append(1))
        self.assertEqual(code, 0)
        self.assertEqual(made, [])
        self.assertFalse(self.status.exists())

    def test_files_are_numbered_after_the_highest_one_present(self):
        directory = self.folder / "screen"
        directory.mkdir()
        self.assertEqual(screen_record.next_file(directory, FIXED_TIME).name,
                         "screen-0001-2026-10-11T14-03-22.mkv")
        (directory / "screen-0007-2020-01-01T00-00-00.mkv").write_bytes(b"")
        (directory / "notes.txt").write_bytes(b"")
        self.assertEqual(screen_record.next_file(directory, FIXED_TIME).name,
                         "screen-0008-2026-10-11T14-03-22.mkv")

    def test_the_command_records_the_screen_and_the_sound_output(self):
        file = Path("/data/screen-0001.mkv")
        command = screen_record.recorder_command(self.merged(), file)
        self.assertEqual(command[0], "wf-recorder")
        self.assertIn("--file", command)
        self.assertEqual(command[command.index("--file") + 1], str(file))
        self.assertEqual(command[command.index("--output") + 1], "DSI-2")
        self.assertIn("--audio=@DEFAULT_MONITOR@", command)
        self.assertNotIn("--audio=@DEFAULT_MONITOR@",
                         screen_record.recorder_command(self.merged(audio="off"), file))
        custom = screen_record.recorder_command(
            self.merged(command="my-recorder -o out {file}"), file)
        self.assertEqual(custom, ["my-recorder", "-o", "out", str(file)])

    def test_a_recording_writes_the_status_file_and_a_stop_finishes_the_file_with_sigint(self):
        processes = []

        def start(command):
            processes.append(FakeProcess(command))
            return processes[-1]

        recorder = screen_record.Recorder(self.merged(), self.status, start_process=start,
                                          free=lambda directory: 10000, now=lambda: FIXED_TIME)
        seen = []

        def sleep(_seconds):
            seen.append(self.status.read_text(encoding="utf-8").strip())
            recorder.request_stop()

        recorder.sleep = sleep
        self.assertEqual(self.quiet(recorder.run), 0)
        self.assertEqual(Path(seen[0]).name, "screen-0001-2026-10-11T14-03-22.mkv")
        self.assertEqual(processes[0].signals, [signal.SIGINT])
        self.assertFalse(self.status.exists(), "the status file goes when recording stops")

    def test_low_free_space_prevents_or_ends_a_recording(self):
        started = []
        recorder = screen_record.Recorder(self.merged(min_free_mb="2048"), self.status,
                                          start_process=lambda command: started.append(1),
                                          free=lambda directory: 2047)
        self.assertEqual(self.quiet(recorder.run), 0)
        self.assertEqual(started, [])
        free_values = iter([5000, 5000, 100])
        processes = []
        recorder = screen_record.Recorder(
            self.merged(), self.status,
            start_process=lambda command: processes.append(FakeProcess(command)) or processes[-1],
            free=lambda directory: next(free_values), sleep=lambda seconds: None,
            now=lambda: FIXED_TIME)
        self.assertEqual(self.quiet(recorder.run), 0)
        self.assertEqual(processes[0].signals, [signal.SIGINT])
        self.assertFalse(self.status.exists())

    def test_a_recorder_that_dies_is_a_failure_for_systemd_to_restart(self):
        def start(command):
            Path(command[command.index("--file") + 1]).write_bytes(b"")
            return FakeProcess(command, exit_after_polls=2)

        recorder = screen_record.Recorder(
            self.merged(), self.status, start_process=start,
            free=lambda directory: 10000, sleep=lambda seconds: None, now=lambda: FIXED_TIME)
        self.assertEqual(self.quiet(recorder.run), 1)
        self.assertFalse(self.status.exists())
        self.assertEqual(list((self.folder / "screen").iterdir()), [],
                         "the empty file of a failed start is removed")

    def test_a_missing_recorder_program_stops_the_restarts(self):
        def start(command):
            raise FileNotFoundError(command[0])

        recorder = screen_record.Recorder(self.merged(), self.status, start_process=start,
                                          free=lambda directory: 10000, now=lambda: FIXED_TIME)
        self.assertEqual(self.quiet(recorder.run), screen_record.EXIT_RECORDER_MISSING)
        self.assertFalse(self.status.exists())
        unit = (REPOSITORY / "deploy" / "car" / "systemd" / "user" /
                "lexus-screen-recorder.service").read_text(encoding="utf-8")
        self.assertIn(f"RestartPreventExitStatus={screen_record.EXIT_RECORDER_MISSING}", unit)

    def test_status_reports_the_file_being_written(self):
        self.status.parent.mkdir(parents=True)
        recording = self.folder / "screen-0002-x.mkv"
        recording.write_bytes(b"\0" * (3 << 20))
        self.status.write_text(str(recording) + "\n", encoding="utf-8")
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            screen_record.show_status(self.merged(), self.status)
        self.assertEqual(output.getvalue().strip(),
                         f"recording on, writing {recording} (3 MB)")

    def test_the_unit_and_the_session_script_start_the_recorder(self):
        unit = (REPOSITORY / "deploy" / "car" / "systemd" / "user" /
                "lexus-screen-recorder.service").read_text(encoding="utf-8")
        self.assertIn("ExecStart=/usr/local/bin/lexus-screen-record run", unit)
        self.assertIn("Restart=on-failure", unit)
        session = (REPOSITORY / "deploy" / "car" / "lexus-hub-session").read_text(encoding="utf-8")
        self.assertIn("import-environment WAYLAND_DISPLAY", session)
        self.assertIn("start lexus-screen-recorder.service", session)
        self.assertIn('--recording-status "$runtime/lexus-screen-recording"', session)
        registry = car_config.parse(
            (REPOSITORY / "deploy" / "car" / "hub_car.conf").read_text(encoding="utf-8"))
        self.assertIn("screen_recording", registry["hub.apps"])
        self.assertEqual(registry["app.screen_recording.command"], "lexus-screen-record toggle")
        example = car_config.parse(
            (REPOSITORY / "deploy" / "car" / "car.conf.example").read_text(encoding="utf-8"))
        self.assertEqual(example["recording.enabled"], "off")

    @unittest.skipUnless(os.name == "posix", "runs the script as a program")
    def test_the_script_runs_as_a_program_outside_the_repository(self):
        script = REPOSITORY / "tools" / "car" / "screen_record.py"
        completed = subprocess.run(
            [sys.executable, str(script), "status", "--config", str(self.config),
             "--status-file", str(self.status)],
            cwd=str(self.folder), capture_output=True, text=True, check=False)
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(completed.stdout.strip(), "recording off")


class FetchFromPiTest(unittest.TestCase):
    LISTING = ("exported 2 sessions\n/home/lexus/lexus-data/screen/screen-0003-b.mkv\n"
               f"{fetch_from_pi.MARKER}\n"
               "export/0007/session_log.csv 120\n"
               "export/0007/obd.scrubbed.rec 4000\n"
               "screen/screen-0002-a.mkv 900\n"
               "screen/screen-0003-b.mkv 50\n"
               "sessions/0007/obd.rec 9999\n"
               "/etc/passwd 10\n"
               "screen/../../x 1\n")

    def setUp(self):
        self.destination = Path(tempfile.mkdtemp())
        self.commands = []

    def runner(self, command):
        self.commands.append(command)
        if command[0] == "ssh":
            return 0, self.LISTING
        target = Path(command[-1])
        target.write_bytes(b"\0" * self.sizes[command[-2].split(":lexus-data/", 1)[1]])
        return 0, ""

    @property
    def sizes(self):
        return fetch_from_pi.parse_listing(self.LISTING)[1]

    def run_fetch(self, *extra):
        with contextlib.redirect_stdout(io.StringIO()) as output:
            code = fetch_from_pi.main(["--destination", str(self.destination), *extra],
                                      runner=self.runner)
        return code, output.getvalue()

    def test_the_listing_is_parsed_and_unsafe_paths_are_dropped(self):
        in_progress, files = fetch_from_pi.parse_listing(self.LISTING)
        self.assertEqual(in_progress, "/home/lexus/lexus-data/screen/screen-0003-b.mkv")
        self.assertNotIn("/etc/passwd", files)
        self.assertNotIn("screen/../../x", files)
        self.assertEqual(files["export/0007/session_log.csv"], 120)

    def test_one_ssh_call_then_only_missing_files_and_never_the_raw_recording(self):
        code, _ = self.run_fetch()
        self.assertEqual(code, 0)
        self.assertEqual(self.commands[0][:2], ["ssh", "lexus-pi"])
        self.assertIn("lexus-export-sessions", self.commands[0][2])
        copied = sorted(command[-2] for command in self.commands[1:])
        self.assertEqual(copied, ["lexus-pi:lexus-data/export/0007/obd.scrubbed.rec",
                                  "lexus-pi:lexus-data/export/0007/session_log.csv",
                                  "lexus-pi:lexus-data/screen/screen-0002-a.mkv"])
        self.assertTrue((self.destination / "car-sessions" / "0007" / "session_log.csv").is_file())
        self.assertTrue((self.destination / "screen" / "screen-0002-a.mkv").is_file())
        self.commands.clear()
        code, output = self.run_fetch()
        self.assertEqual(len(self.commands), 1, "the second run copies nothing")
        self.assertIn("0 copied, 3 already here, 0 failed", output)

    def test_a_dry_run_copies_nothing(self):
        code, output = self.run_fetch("--dry-run")
        self.assertEqual(code, 0)
        self.assertEqual(len(self.commands), 1)
        self.assertIn("would copy screen/screen-0002-a.mkv", output)

    def test_an_unreachable_pi_fails(self):
        with contextlib.redirect_stderr(io.StringIO()):
            code = fetch_from_pi.main(["--destination", str(self.destination)],
                                      runner=lambda command: (255, ""))
        self.assertEqual(code, 1)


if __name__ == "__main__":
    unittest.main()
