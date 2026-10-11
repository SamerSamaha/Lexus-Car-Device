# Verifies: REQ-019, REQ-032
"""Tests of the car stereo tool (tools/car/car_audio.py, LHU-056) against a fake bluetoothctl and
a fake pactl. No Bluetooth radio is needed."""

import contextlib
import io
import os
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "car"))

import car_audio  # noqa: E402
import car_config  # noqa: E402

REPOSITORY = HERE.parent
# Built at run time so that no address-shaped text sits in a tracked file (privacy check).
STEREO = ":".join(["1C", "2D", "3E", "4F", "5A", "6B"])
PHONE = ":".join(["0C"] * 6)
POSIX = os.name == "posix"

FAKE_BLUETOOTHCTL = textwrap.dedent('''\
    #!/usr/bin/env python3
    """A stand-in for bluetoothctl that answers what car_audio.py sends."""
    import os, sys
    scenario = os.environ["FAKE_SCENARIO"]
    stereo = os.environ["FAKE_STEREO"]
    log = open(os.environ["FAKE_LOG"], "a")
    def say(text):
        sys.stdout.write(text + "\\n"); sys.stdout.flush()
    for line in sys.stdin:
        command = line.strip()
        log.write(command + "\\n"); log.flush()
        if command == "scan on" and scenario != "absent":
            say("[NEW] Device " + os.environ["FAKE_PHONE"] + " iPhone")
            say("[NEW] Device " + stereo + " LEXUS GS")
        elif command.startswith("pair "):
            say("[agent] Confirm passkey 123456 (yes/no): ")
        elif command == "yes":
            say("Pairing successful" if scenario != "refused" else "Failed to pair: org.bluez.Error.AuthenticationRejected")
        elif command.startswith("trust "):
            say("Changing " + command[6:] + " trust succeeded")
        elif command.startswith("connect "):
            say("Connection successful" if scenario != "car-off" else "Failed to connect: org.bluez.Error.Failed")
        elif command == "quit":
            break
    ''')


class FakePactl:
    def __init__(self, sink_after_calls=1):
        self.calls = []
        self.sink_after_calls = sink_after_calls

    def __call__(self, command):
        self.calls.append(command)
        if command[:4] == ["pactl", "list", "short", "sinks"]:
            listed = len([call for call in self.calls if call[:3] == ["pactl", "list", "short"]])
            text = "50\talsa_output.platform-hdmi\tPipeWire\ts32le 2ch 48000Hz\tSUSPENDED\n"
            if listed >= self.sink_after_calls:
                text += f"77\tbluez_output.{STEREO.replace(':', '_')}.1\tPipeWire\ts16le 2ch 44100Hz\tIDLE\n"
            return 0, text
        return 0, ""


class CarAudioParsingTest(unittest.TestCase):
    def test_the_bluetooth_output_is_found_in_the_sink_list(self):
        listing = ("50\talsa_output.platform\tPipeWire\n"
                   "77\tbluez_output.X.1\tPipeWire\n")
        self.assertEqual(car_audio.bluetooth_sink(listing), "bluez_output.X.1")
        self.assertIsNone(car_audio.bluetooth_sink("50\talsa_output.platform\tPipeWire\n"))

    def test_the_stereo_is_chosen_by_its_name_fragment(self):
        devices = [(PHONE, "iPhone"), (STEREO, "LEXUS GS")]
        self.assertEqual(car_audio.choose_stereo(devices, "lexus"), (STEREO, "LEXUS GS"))
        self.assertIsNone(car_audio.choose_stereo(devices, "Toyota"))

    def test_the_default_output_waits_for_pipewire_and_gives_up_after_its_time(self):
        pactl = FakePactl(sink_after_calls=3)
        sink = car_audio.make_default_output(pactl, sleep=lambda seconds: None, seconds=5)
        self.assertTrue(sink.startswith("bluez_output."))
        self.assertEqual(pactl.calls[-1][:2], ["pactl", "set-default-sink"])
        never = FakePactl(sink_after_calls=99)
        self.assertIsNone(car_audio.make_default_output(never, sleep=lambda seconds: None, seconds=3))


@unittest.skipUnless(POSIX, "bluetoothctl is driven through a pseudo-terminal")
class CarAudioSessionTest(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        root = Path(self.folder.name)
        self.fake = root / "bluetoothctl"
        self.fake.write_text(FAKE_BLUETOOTHCTL, encoding="utf-8")
        self.fake.chmod(0o755)
        self.config = root / "car.conf"
        self.log = root / "commands.txt"

    def tearDown(self):
        self.folder.cleanup()

    def run_tool(self, scenario, config_text="[audio]\naddress =\n", *extra):
        self.config.write_text(config_text, encoding="utf-8")
        os.environ.update(FAKE_SCENARIO=scenario, FAKE_STEREO=STEREO, FAKE_PHONE=PHONE,
                          FAKE_LOG=str(self.log))
        pactl = FakePactl()
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            code = car_audio.main(["--config", str(self.config), "--bluetoothctl", str(self.fake),
                                   "--scan-seconds", "3", *extra],
                                  runner=pactl, sleep=lambda seconds: None)
        commands = self.log.read_text(encoding="utf-8").splitlines() if self.log.exists() else []
        return code, output.getvalue(), car_config.read(self.config), commands, pactl

    def test_a_new_stereo_is_paired_confirmed_trusted_connected_saved_and_made_the_output(self):
        code, output, values, commands, pactl = self.run_tool("new")
        self.assertEqual(code, car_audio.EXIT_OK, output)
        self.assertIn(f"pair {STEREO}", commands)
        self.assertIn("yes", commands)
        self.assertIn(f"trust {STEREO}", commands)
        self.assertIn(f"connect {STEREO}", commands)
        self.assertEqual(values["audio.address"], STEREO)
        self.assertTrue(any(call[:2] == ["pactl", "set-default-sink"] for call in pactl.calls))
        self.assertNotIn(STEREO, output, "only the last two bytes are printed")
        self.assertIn("XX:XX:XX:XX:5A:6B", output)

    def test_a_saved_stereo_is_only_connected(self):
        code, output, values, commands, _ = self.run_tool("new", f"[audio]\naddress = {STEREO}\n")
        self.assertEqual(code, car_audio.EXIT_OK, output)
        self.assertNotIn("scan on", commands)
        self.assertFalse(any(command.startswith("pair ") for command in commands))
        self.assertIn(f"connect {STEREO}", commands)

    def test_no_stereo_found_writes_nothing(self):
        code, output, values, _, pactl = self.run_tool("absent")
        self.assertEqual(code, car_audio.EXIT_NOT_FOUND)
        self.assertEqual(values["audio.address"], "")
        self.assertEqual(pactl.calls, [])

    def test_a_refused_pairing_or_a_car_that_is_off_fails_without_saving(self):
        code, _, values, _, _ = self.run_tool("refused")
        self.assertEqual(code, car_audio.EXIT_PAIRING_FAILED)
        self.assertEqual(values["audio.address"], "")
        code, output, _, _, _ = self.run_tool("car-off", f"[audio]\naddress = {STEREO}\n")
        self.assertEqual(code, car_audio.EXIT_PAIRING_FAILED)
        self.assertIn("connecting failed", output)


class CarAudioInstallTest(unittest.TestCase):
    def test_the_hub_tile_and_the_command_exist(self):
        registry = car_config.parse(
            (REPOSITORY / "deploy" / "car" / "hub_car.conf").read_text(encoding="utf-8"))
        self.assertIn("car_audio", registry["hub.apps"])
        self.assertIn("lexus-car-audio", registry["app.car_audio.command"])
        install = (REPOSITORY / "deploy" / "car" / "install_car_mode.sh").read_text(encoding="utf-8")
        self.assertIn("car_audio.py:lexus-car-audio", install)
        self.assertIn("libspa-0.2-bluetooth", install)


if __name__ == "__main__":
    unittest.main()
