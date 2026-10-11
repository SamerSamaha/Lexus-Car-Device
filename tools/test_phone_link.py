# Verifies: REQ-029
"""Tests of the car-mode side of the phone link (LHU-053): the service configuration of each
session, the self-check item and the scripts. The server itself is tested in C++
(tests/unit/phone_link/)."""

import contextlib
import io
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
import car_session  # noqa: E402
import self_check  # noqa: E402

REPOSITORY = HERE.parent


class PhoneLinkSessionTest(unittest.TestCase):
    def setUp(self):
        self.root = Path(tempfile.mkdtemp())
        self.certificates = self.root / "phone-link"
        self.certificates.mkdir()
        self.repository = self.root / "repo"
        (self.repository / "deploy").mkdir(parents=True)
        (self.repository / "deploy" / "head_unit.conf").write_text("[source]\nkind = elm327\n",
                                                                   encoding="utf-8")

    def tearDown(self):
        shutil.rmtree(self.root)

    def make_certificates(self):
        for name in car_session.PHONE_LINK_FILES:
            (self.certificates / name).write_text("pem", encoding="ascii")

    def test_no_certificates_means_no_phone_link(self):
        self.assertEqual(car_session.phone_link_lines({}, self.certificates), [])

    def test_with_certificates_the_service_gets_the_port_and_the_files(self):
        self.make_certificates()
        lines = car_session.phone_link_lines({}, self.certificates)
        self.assertEqual(lines[0], "[phone_link]")
        self.assertIn("port = 8443", lines)
        self.assertIn(f"certificate = {self.certificates / 'server.crt'}", lines)
        self.assertIn(f"key = {self.certificates / 'server.key'}", lines)
        self.assertIn(f"ca_certificate = {self.certificates / 'ca.crt'}", lines)
        self.assertIn("port = 9443",
                      car_session.phone_link_lines({"phone_link.port": "9443"}, self.certificates))
        self.assertEqual(
            car_session.phone_link_lines({"phone_link.enabled": "off"}, self.certificates), [])

    def test_every_mode_writes_the_phone_link_into_the_session_configuration(self):
        self.make_certificates()
        for mode in ("car", "demo"):
            prepared = car_session.prepare(
                {"session.data_dir": str(self.root / "data"), "mode.kind": mode},
                self.repository, "/usr/local/bin/service", self.certificates)
            values = car_config.parse(
                (prepared["session"] / "head_unit.conf").read_text(encoding="utf-8"))
            self.assertEqual(values["phone_link.port"], "8443", mode)
            self.assertEqual(values["source.kind"], "elm327" if mode == "car" else "fake")


class PhoneLinkSelfCheckTest(unittest.TestCase):
    def setUp(self):
        self.root = Path(tempfile.mkdtemp())
        self.paths = self_check.Paths(phone_link=self.root)

    def tearDown(self):
        shutil.rmtree(self.root)

    def run_check(self, exit_code):
        report = self_check.Report()
        with contextlib.redirect_stdout(io.StringIO()):
            self_check.check_phone_link(report, self.paths, lambda command: (exit_code, ""))
        return report.rows[-1]

    def test_missing_certificates_fail(self):
        self.assertEqual(self.run_check(0)[0], self_check.FAIL)

    def test_present_and_valid_pass_and_ending_soon_fails(self):
        (self.root / "server.crt").write_text("pem", encoding="ascii")
        (self.root / "ca.crt").write_text("pem", encoding="ascii")
        self.assertEqual(self.run_check(0)[0], self_check.PASS)
        self.assertEqual(self.run_check(1)[0], self_check.FAIL)


class PhoneLinkScriptsTest(unittest.TestCase):
    def test_the_install_script_makes_the_certificates_and_installs_openssl(self):
        install = (REPOSITORY / "deploy" / "car" / "install_car_mode.sh").read_text(encoding="utf-8")
        self.assertIn("deploy/car/phone_link_certificates.sh", install)
        self.assertIn("openssl", install)

    def test_the_hub_shows_the_phone_link_address(self):
        session = (REPOSITORY / "deploy" / "car" / "lexus-hub-session").read_text(encoding="utf-8")
        self.assertIn("--phone-link-port 8443", session)

    def test_the_example_configuration_switches_the_phone_link_on(self):
        values = car_config.parse(
            (REPOSITORY / "deploy" / "car" / "car.conf.example").read_text(encoding="utf-8"))
        self.assertEqual(values["phone_link.enabled"], "on")
        self.assertEqual(values["phone_link.port"], "8443")

    @unittest.skipUnless(os.name == "posix" and shutil.which("openssl"), "needs openssl")
    def test_the_certificate_script_makes_an_authority_and_a_server_certificate_apple_accepts(self):
        folder = Path(tempfile.mkdtemp())
        try:
            script = REPOSITORY / "deploy" / "car" / "phone_link_certificates.sh"
            completed = subprocess.run(["sh", str(script), str(folder)], capture_output=True,
                                       text=True, check=False)
            self.assertEqual(completed.returncode, 0, completed.stderr)
            text = subprocess.run(["openssl", "x509", "-in", str(folder / "server.crt"), "-noout",
                                   "-text"], capture_output=True, text=True, check=True).stdout
            self.assertIn("TLS Web Server Authentication", text)
            self.assertIn("DNS:lexus-head-unit.local", text)
            self.assertIn("IP Address:172.20.10.1,", text)
            self.assertIn("IP Address:172.20.10.14", text)
            verified = subprocess.run(["openssl", "verify", "-CAfile", str(folder / "ca.crt"),
                                       str(folder / "server.crt")], capture_output=True,
                                      text=True, check=False)
            self.assertEqual(verified.returncode, 0, verified.stdout + verified.stderr)
            # Apple's limit is 398 days: at most 397 from now.
            self.assertEqual(subprocess.run(["openssl", "x509", "-in", str(folder / "server.crt"),
                                             "-noout", "-checkend", str(398 * 86400)],
                                            capture_output=True, check=False).returncode, 1)
            self.assertEqual(oct((folder / "server.key").stat().st_mode & 0o777), "0o600")
            self.assertEqual(oct((folder / "ca.key").stat().st_mode & 0o777), "0o600")
        finally:
            shutil.rmtree(folder)


if __name__ == "__main__":
    unittest.main()
