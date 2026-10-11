# Verifies: REQ-031
"""Tests of the web apps of car mode (LHU-055): the browser wrapper, the Hub button extension
and the car registry. The extension's behaviour inside Chromium is checked on the Pi (checklist
step 3.20); here its files are checked for what the design relies on."""

import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "car"))

import car_config  # noqa: E402

REPOSITORY = HERE.parent
EXTENSION = REPOSITORY / "deploy" / "car" / "browser_return"
WRAPPER = REPOSITORY / "deploy" / "car" / "lexus-browser"


class BrowserWrapperTest(unittest.TestCase):
    @unittest.skipUnless(os.name == "posix", "runs the shell script")
    def test_the_wrapper_starts_one_full_screen_window_with_the_extension_and_its_own_profile(self):
        home = Path(tempfile.mkdtemp())
        try:
            environment = dict(os.environ, HOME=str(home))
            environment.pop("LEXUS_BROWSER_PROFILE", None)
            environment.pop("LEXUS_BROWSER_EXTENSION", None)
            completed = subprocess.run(["sh", str(WRAPPER), "--print", "https://www.youtube.com/"],
                                       capture_output=True, text=True, check=False,
                                       env=environment)
            self.assertEqual(completed.returncode, 0, completed.stderr)
            words = completed.stdout.splitlines()
            self.assertEqual(words[0], "chromium")
            self.assertIn("--ozone-platform=wayland", words)
            self.assertIn(f"--user-data-dir={home}/.cache/lexus-car-browser", words)
            self.assertIn("--load-extension=/usr/local/share/lexus-head-unit/browser-return", words)
            self.assertIn("--disable-features=DisableLoadExtensionCommandLineSwitch", words)
            self.assertIn("--start-fullscreen", words)
            self.assertEqual(words[-1], "--app=https://www.youtube.com/")
            self.assertFalse((home / ".cache").exists(), "--print creates nothing")
        finally:
            shutil.rmtree(home)

    def test_the_wrapper_replaces_itself_with_the_browser(self):
        text = WRAPPER.read_text(encoding="utf-8")
        self.assertIn('exec "$@"', text, "the hub must track the browser's own process")

    @unittest.skipUnless(os.name == "posix", "runs the shell script")
    def test_a_url_with_spaces_or_quotes_stays_one_argument(self):
        completed = subprocess.run(["sh", str(WRAPPER), "--print", "https://a.example/?q=x y'z"],
                                   capture_output=True, text=True, check=False)
        self.assertEqual(completed.stdout.splitlines()[-1], "--app=https://a.example/?q=x y'z")


class ReturnExtensionTest(unittest.TestCase):
    def test_the_manifest_is_version_3_with_a_content_script_on_every_page(self):
        manifest = json.loads((EXTENSION / "manifest.json").read_text(encoding="utf-8"))
        self.assertEqual(manifest["manifest_version"], 3)
        self.assertEqual(manifest["background"]["service_worker"], "background.js")
        script = manifest["content_scripts"][0]
        self.assertEqual(script["matches"], ["<all_urls>"])
        self.assertEqual(script["js"], ["content.js"])
        self.assertNotIn("permissions", manifest, "the button needs no permission")

    def test_the_button_is_10_mm_on_top_follows_full_screen_and_asks_to_close(self):
        content = (EXTENSION / "content.js").read_text(encoding="utf-8")
        # 10 mm at 11.6 pixels per millimetre (D-024).
        self.assertIn("var sizePixels = 116;", content)
        self.assertIn("z-index:2147483647", content)
        self.assertIn("attachShadow", content)
        self.assertIn("fullscreenchange", content)
        self.assertIn("chrome.runtime.sendMessage({ lexusHubReturn: true })", content)
        background = (EXTENSION / "background.js").read_text(encoding="utf-8")
        self.assertIn("message.lexusHubReturn === true", background)
        self.assertIn("chrome.windows.remove(sender.tab.windowId)", background)

    @unittest.skipUnless(shutil.which("node"), "needs node")
    def test_the_scripts_parse(self):
        for name in ("content.js", "background.js"):
            completed = subprocess.run(["node", "--check", str(EXTENSION / name)],
                                       capture_output=True, text=True, check=False)
            self.assertEqual(completed.returncode, 0, completed.stderr)


class CarRegistryWebAppsTest(unittest.TestCase):
    def setUp(self):
        self.values = car_config.parse(
            (REPOSITORY / "deploy" / "car" / "hub_car.conf").read_text(encoding="utf-8"))

    def test_youtube_and_spotify_open_in_the_car_browser(self):
        apps = [name.strip() for name in self.values["hub.apps"].split(",")]
        self.assertIn("youtube", apps)
        self.assertIn("spotify", apps)
        self.assertLessEqual(len(apps), 12)
        self.assertEqual(self.values["hub.browser_command"], "lexus-browser {url}")
        self.assertEqual(self.values["app.youtube.kind"], "url")
        self.assertEqual(self.values["app.youtube.url"], "https://www.youtube.com/")
        self.assertEqual(self.values["app.spotify.url"], "https://open.spotify.com/")

    def test_the_installer_puts_the_extension_where_the_wrapper_loads_it(self):
        install = (REPOSITORY / "deploy" / "car" / "install_car_mode.sh").read_text(encoding="utf-8")
        self.assertIn("/usr/local/share/lexus-head-unit/browser-return", install)
        self.assertIn("deploy/car/lexus-browser", install)
        for name in ("manifest.json", "background.js", "content.js"):
            self.assertIn(name, install)


if __name__ == "__main__":
    unittest.main()
