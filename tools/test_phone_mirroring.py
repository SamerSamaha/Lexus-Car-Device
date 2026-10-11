# Verifies: REQ-033
"""Tests of phone mirroring in car mode (LHU-057): the receiver command, the hub tile and the
installer. The mirroring itself needs the iPhone and the Pi (checklist step 3.22)."""

import os
import subprocess
import sys
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "car"))

import car_config  # noqa: E402

REPOSITORY = HERE.parent
WRAPPER = REPOSITORY / "deploy" / "car" / "lexus-mirroring"


@unittest.skipUnless(os.name == "posix", "runs the shell script")
class MirroringCommandTest(unittest.TestCase):
    def printed(self, **environment):
        variables = dict(os.environ)
        variables.pop("LEXUS_MIRRORING_OPTIONS", None)
        variables.update(environment)
        completed = subprocess.run(["sh", str(WRAPPER), "--print"], capture_output=True,
                                   text=True, check=False, env=variables)
        self.assertEqual(completed.returncode, 0, completed.stderr)
        return completed.stdout.splitlines()

    def test_the_receiver_is_named_full_screen_on_wayland_with_software_decoding(self):
        words = self.printed()
        self.assertEqual(words[0], "uxplay")
        self.assertEqual(words[words.index("-n") + 1], "Lexus Head Unit")
        for option in ("-nh", "-fs", "-avdec"):
            self.assertIn(option, words)
        self.assertEqual(words[words.index("-vs") + 1], "waylandsink")

    def test_the_options_can_be_replaced_on_the_pi_without_a_code_change(self):
        words = self.printed(LEXUS_MIRRORING_OPTIONS="-nh -vs autovideosink")
        self.assertEqual(words, ["uxplay", "-n", "Lexus Head Unit", "-nh", "-vs", "autovideosink"])


class MirroringInstallTest(unittest.TestCase):
    def test_the_wrapper_replaces_itself_with_the_receiver(self):
        self.assertIn('exec "$@"', WRAPPER.read_text(encoding="utf-8"))

    def test_the_hub_tile_is_labelled_as_mirroring(self):
        registry = car_config.parse(
            (REPOSITORY / "deploy" / "car" / "hub_car.conf").read_text(encoding="utf-8"))
        self.assertIn("mirroring", [name.strip() for name in registry["hub.apps"].split(",")])
        self.assertEqual(registry["app.mirroring.name"], "Phone mirroring")
        self.assertEqual(registry["app.mirroring.command"], "lexus-mirroring")
        self.assertEqual(registry["app.mirroring.restart"], "never")

    def test_the_installer_adds_the_receiver_and_its_video_plugins(self):
        install = (REPOSITORY / "deploy" / "car" / "install_car_mode.sh").read_text(encoding="utf-8")
        for package in ("uxplay", "gstreamer1.0-plugins-bad", "gstreamer1.0-libav", "avahi-daemon"):
            self.assertIn(package, install)
        self.assertIn("deploy/car/lexus-mirroring", install)


if __name__ == "__main__":
    unittest.main()
