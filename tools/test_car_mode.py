# Verifies: REQ-025
"""Tests of the car-mode tools in tools/car/ (LHU-044)."""

import contextlib
import io
import os
import shutil
import stat
import subprocess
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "car"))
sys.path.insert(0, str(HERE))

import car_config  # noqa: E402
import car_mode  # noqa: E402
import car_session  # noqa: E402
import export_sessions  # noqa: E402
import log_thermal_power  # noqa: E402
import self_check  # noqa: E402

REPOSITORY = HERE.parent
# Built at run time so that no address-shaped text sits in a tracked file (privacy check).
ADDRESS = ":".join(["0A", "1B", "2C", "3D", "4E", "5F"])
OTHER_ADDRESS = ":".join(["0B"] * 6)
# The hypothetical VIN allowlisted in tools/private_data_allowlist.txt; not a real vehicle.
EXAMPLE_VIN = "1M8GDM9AXKP042788"
POSIX = os.name == "posix"


class CarConfigTest(unittest.TestCase):
    TEXT = textwrap.dedent("""\
        # comment kept
        [mode]
        kind = car

        [adapter]
        # the address
        address =
        channel = 1
        """)

    def test_parse_reads_section_keys_and_skips_comments(self):
        values = car_config.parse(self.TEXT)
        self.assertEqual(values, {"mode.kind": "car", "adapter.address": "", "adapter.channel": "1"})

    def test_values_are_replaced_added_to_their_section_or_in_a_new_section(self):
        text = car_config.with_values(self.TEXT, {"adapter.address": ADDRESS, "adapter.pin": "1234",
                                                  "session.data_dir": "/data"})
        self.assertIn("# comment kept", text)
        self.assertIn("# the address", text)
        self.assertEqual(car_config.parse(text)["adapter.address"], ADDRESS)
        lines = text.splitlines()
        self.assertLess(lines.index("pin = 1234"), lines.index("[session]"))
        self.assertGreater(lines.index("pin = 1234"), lines.index("channel = 1"))
        self.assertEqual(car_config.parse(text)["session.data_dir"], "/data")

    def test_write_values_keeps_the_file_private(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "sub" / "car.conf"
            car_config.write_values(path, {"adapter.address": ADDRESS})
            self.assertEqual(car_config.read(path)["adapter.address"], ADDRESS)
            if POSIX:
                self.assertEqual(stat.S_IMODE(path.stat().st_mode), 0o600)

    def test_addresses_are_normalised_masked_and_validated(self):
        self.assertEqual(car_config.normalised_address(ADDRESS.lower().replace(":", "-")), ADDRESS)
        self.assertIsNone(car_config.normalised_address("not an address"))
        self.assertEqual(car_config.masked_address(ADDRESS), "XX:XX:XX:XX:4E:5F")

    def test_unknown_mode_falls_back_to_car(self):
        self.assertEqual(car_config.mode_of({"mode.kind": "Demo"}), "demo")
        self.assertEqual(car_config.mode_of({"mode.kind": "race"}), "car")
        self.assertEqual(car_config.mode_of({}), "car")


class CarSessionTest(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.root = Path(self.folder.name)
        self.repository = self.root / "repo"
        (self.repository / "deploy").mkdir(parents=True)
        (self.repository / "deploy" / "head_unit.conf").write_text("[source]\nkind = elm327\n",
                                                                   encoding="utf-8")
        self.data = self.root / "data"

    def tearDown(self):
        self.folder.cleanup()

    def prepare(self, **extra):
        values = {"session.data_dir": str(self.data)}
        values.update(extra)
        return car_session.prepare(values, self.repository, "/usr/local/bin/service")

    def test_car_mode_numbers_the_folder_and_records_the_adapter(self):
        first = self.prepare(**{"mode.kind": "car"})
        second = self.prepare(**{"mode.kind": "car"})
        self.assertEqual(first["session"].name, "0001-car")
        self.assertEqual(second["session"].name, "0002-car")
        command = first["command"]
        self.assertEqual(command[:3], ["/usr/local/bin/service", "--config",
                                       str(first["session"] / "head_unit.conf")])
        self.assertIn("--record", command)
        self.assertEqual(command[command.index("--session-log") + 1],
                         str(first["session"] / "session_log.csv"))
        written = (first["session"] / "head_unit.conf").read_text(encoding="utf-8")
        self.assertTrue(written.rstrip().endswith("kind = elm327"))
        self.assertIn("mode car", (first["session"] / "session.txt").read_text(encoding="utf-8"))

    def test_demo_and_emulator_overrides(self):
        demo = self.prepare(**{"mode.kind": "demo"})
        self.assertNotIn("--record", demo["command"])
        self.assertIn("kind = fake", (demo["session"] / "head_unit.conf").read_text(encoding="utf-8"))
        emulator = self.prepare(**{"mode.kind": "emulator"})
        text = (emulator["session"] / "head_unit.conf").read_text(encoding="utf-8")
        self.assertIn("device = " + car_session.EMULATOR_LINK, text)
        self.assertIn("--record", emulator["command"])

    def test_replay_last_uses_the_newest_car_recording_and_falls_back_to_demo(self):
        fallback = self.prepare(**{"mode.kind": "replay"})
        self.assertEqual(fallback["mode"], "demo")
        self.assertTrue(fallback["notes"])
        older = self.prepare(**{"mode.kind": "car"})["session"]
        newer = self.prepare(**{"mode.kind": "car"})["session"]
        (older / "obd.rec").write_text("0\tOPEN_OK\n", encoding="ascii")
        (newer / "obd.rec").write_text("0\tOPEN_OK\n", encoding="ascii")
        replay = self.prepare(**{"mode.kind": "replay", "mode.replay_file": "last"})
        self.assertEqual(replay["mode"], "replay")
        text = (replay["session"] / "head_unit.conf").read_text(encoding="utf-8")
        self.assertIn(f"file = {newer / 'obd.rec'}", text)
        self.assertIn("timing = original", text)

    def test_the_counter_never_reuses_a_folder_number(self):
        sessions = self.data / "sessions"
        (sessions / "0005-car").mkdir(parents=True)
        self.assertEqual(car_session.next_session_number(sessions), 6)
        (sessions / car_session.COUNTER_FILE).write_text("6\n", encoding="ascii")
        (sessions / "0006-demo").mkdir()
        self.assertEqual(car_session.next_session_number(sessions), 7)

    def test_print_command_starts_nothing(self):
        config = self.root / "car.conf"
        config.write_text(f"[mode]\nkind = demo\n[session]\ndata_dir = {self.data}\n",
                          encoding="utf-8")
        output = io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(io.StringIO()):
            status = car_session.main(["--config", str(config), "--repository",
                                       str(self.repository), "--print-command"])
        self.assertEqual(status, 0)
        self.assertIn("service /usr/local/bin/lexus-vehicle-data-service --config", output.getvalue())
        self.assertIn("--label session0001", output.getvalue())


class CarModeTest(unittest.TestCase):
    def test_switching_writes_the_mode_and_restarts_the_service(self):
        with tempfile.TemporaryDirectory() as folder:
            config = Path(folder) / "car.conf"
            config.write_text("[mode]\nkind = car\n", encoding="utf-8")
            commands = []
            with contextlib.redirect_stdout(io.StringIO()):
                status = car_mode.main(["replay", "last", "--config", str(config)],
                                       runner=lambda command: commands.append(command) or 0)
            self.assertEqual(status, 0)
            values = car_config.read(config)
            self.assertEqual((values["mode.kind"], values["mode.replay_file"]), ("replay", "last"))
            self.assertEqual(commands, [
                ["systemctl", "--user", "stop", car_mode.EMULATOR_UNIT],
                ["systemctl", "--user", "restart", car_mode.SERVICE_UNIT]])
            commands.clear()
            with contextlib.redirect_stdout(io.StringIO()):
                car_mode.main(["emulator", "--config", str(config)],
                              runner=lambda command: commands.append(command) or 0)
            self.assertEqual(commands[0], ["systemctl", "--user", "start", car_mode.EMULATOR_UNIT])

    def test_a_file_only_with_replay(self):
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                car_mode.main(["demo", "x.rec"], runner=lambda command: 0)


@unittest.skipUnless(POSIX, "pseudo-terminals exist only on POSIX systems")
class PairAdapterTest(unittest.TestCase):
    FAKE = textwrap.dedent('''\
        #!/usr/bin/env python3
        """A stand-in for bluetoothctl: answers the commands pair_adapter.py sends."""
        import os, sys
        scenario = os.environ["FAKE_SCENARIO"]
        address = os.environ["FAKE_ADDRESS"]
        log = open(os.environ["FAKE_LOG"], "a")
        def say(text):
            sys.stdout.write(text + "\\n"); sys.stdout.flush()
        for line in sys.stdin:
            command = line.strip()
            log.write(command + "\\n"); log.flush()
            if command == "devices Paired":
                if scenario == "paired":
                    say("Device " + address + " vLinker MC-Android")
            elif command == "scan on":
                if scenario != "absent":
                    say("[NEW] Device " + os.environ["FAKE_OTHER"] + " vLinker MC-IOS")
                    say("[NEW] Device " + address + " vLinker MC-Android")
            elif command.startswith("pair "):
                if scenario == "wrong-pin":
                    say("[agent] Enter PIN code: ")
                else:
                    say("[agent] Enter PIN code: ")
            elif command == "1234" and scenario == "new":
                say("Pairing successful")
            elif command == "9999" or (command == "1234" and scenario == "wrong-pin"):
                say("Failed to pair: org.bluez.Error.AuthenticationFailed")
            elif command.startswith("trust "):
                say("Changing " + command[6:] + " trust succeeded")
            elif command == "quit":
                break
        ''')

    def run_pairing(self, scenario, scan_seconds="2"):
        folder = Path(self.folder.name)
        fake = folder / "bluetoothctl"
        fake.write_text(self.FAKE, encoding="utf-8")
        fake.chmod(0o755)
        config = folder / "car.conf"
        config.write_text("[adapter]\naddress =\npin = 1234\n", encoding="utf-8")
        log = folder / "commands.txt"
        environment = dict(os.environ, FAKE_SCENARIO=scenario, FAKE_ADDRESS=ADDRESS,
                           FAKE_OTHER=OTHER_ADDRESS, FAKE_LOG=str(log))
        result = subprocess.run(
            [sys.executable, str(HERE / "car" / "pair_adapter.py"), "--config", str(config),
             "--bluetoothctl", str(fake), "--scan-seconds", scan_seconds, "--no-restart"],
            capture_output=True, text=True, env=environment, timeout=60, check=False)
        commands = log.read_text(encoding="utf-8").splitlines() if log.exists() else []
        return result, car_config.read(config), commands

    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()

    def tearDown(self):
        self.folder.cleanup()

    def test_scans_prefers_the_classic_name_pairs_with_the_pin_trusts_and_saves(self):
        result, values, commands = self.run_pairing("new")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(values["adapter.address"], ADDRESS)
        self.assertIn(f"pair {ADDRESS}", commands)
        self.assertIn("1234", commands)
        self.assertIn(f"trust {ADDRESS}", commands)
        self.assertNotIn(ADDRESS, result.stdout)
        self.assertIn("4E:5F", result.stdout)

    def test_an_adapter_already_paired_is_only_trusted(self):
        result, values, commands = self.run_pairing("paired")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(values["adapter.address"], ADDRESS)
        self.assertFalse(any(command.startswith("pair ") for command in commands))
        self.assertNotIn("scan on", commands)

    def test_no_adapter_found_writes_nothing(self):
        result, values, _ = self.run_pairing("absent", scan_seconds="1")
        self.assertEqual(result.returncode, 2)
        self.assertEqual(values["adapter.address"], "")
        self.assertIn("FAILED", result.stdout)

    def test_a_refused_pin_writes_nothing(self):
        result, values, _ = self.run_pairing("wrong-pin")
        self.assertEqual(result.returncode, 3)
        self.assertEqual(values["adapter.address"], "")

    def test_device_lines_and_the_choice(self):
        import pair_adapter
        text = (f"[NEW] Device {OTHER_ADDRESS} vLinker MC-IOS\n"
                f"[CHG] Device {ADDRESS} RSSI: -60\n"
                f"[NEW] Device {ADDRESS} vLinker MC-Android\n")
        devices = pair_adapter.devices_in(text)
        self.assertEqual(devices, [(OTHER_ADDRESS, "vLinker MC-IOS"),
                                   (ADDRESS, "vLinker MC-Android")])
        self.assertEqual(pair_adapter.choose(devices, "vlinker"), (ADDRESS, "vLinker MC-Android"))
        self.assertIsNone(pair_adapter.choose(devices, "OBDLink"))


class ExportSessionsTest(unittest.TestCase):
    def test_export_scrubs_the_recording_never_copies_it_raw_and_rebuilds_the_thermal_summary(self):
        with tempfile.TemporaryDirectory() as folder:
            data = Path(folder)
            session = data / "sessions" / "0003-car"
            session.mkdir(parents=True)
            (session / "session.txt").write_text("session 0003\nmode car\n", encoding="utf-8")
            (session / "session_log.csv").write_text(
                "milliseconds,event,state,trigger,from,to,requests_sent,samples_emitted,"
                "malformed_inputs\n0,counters,Connected,,,,0,0,0\n"
                "10000,counters,Connected,,,,200,180,0\n"
                "12000,transition,Error,LinkLost,Connected,Error,,,\n", encoding="ascii")
            payload = "VIN " + EXAMPLE_VIN + "\r>"
            (session / "obd.rec").write_text(
                "5 OPEN_OK\n6 READ " + payload.encode("ascii").hex() + "\n", encoding="ascii")
            row = {column: "" for column in log_thermal_power.CSV_COLUMNS}
            row.update({"timestamp": "t", "temperature_celsius": "52.5", "throttled_hex": "0x0"})
            (session / "2026-10-09_120000_session0003.csv").write_text(
                ",".join(log_thermal_power.CSV_COLUMNS) + "\n"
                + ",".join(row[column] for column in log_thermal_power.CSV_COLUMNS) + "\n",
                encoding="ascii")
            with contextlib.redirect_stdout(io.StringIO()):
                status = export_sessions.export_all(data)
            target = data / "export" / "0003-car"
            self.assertEqual(status, 0)
            self.assertFalse((target / "obd.rec").exists())
            scrubbed = (target / "obd.scrubbed.rec").read_text(encoding="ascii")
            self.assertNotIn(EXAMPLE_VIN.encode("ascii").hex(), scrubbed)
            summary = (target / "session_summary.txt").read_text(encoding="ascii")
            self.assertIn("transitions out of Connected: 1", summary)
            thermal_summary = target / "2026-10-09_120000_session0003.summary.txt"
            self.assertIn("1 samples", thermal_summary.read_text(encoding="ascii"))
            self.assertTrue((target / "session.txt").exists())
            # A second run with nothing changed exports nothing again.
            (target / "session.txt").unlink()
            with contextlib.redirect_stdout(io.StringIO()):
                export_sessions.export_all(data)
            self.assertFalse((target / "session.txt").exists())


class SelfCheckTest(unittest.TestCase):
    def test_output_parsers(self):
        self.assertEqual(self_check.link_detail_name("u 3"), "Live")
        self.assertIsNone(self_check.link_detail_name("u 9"))
        self.assertIsNone(self_check.link_detail_name("Call failed"))
        samples = "a(uduxu) 3 0 42.0 0 100 1 1 800 1 101 1 2 20 2 102 0"
        self.assertEqual(self_check.valid_sample_count(samples), 2)
        self.assertEqual(self_check.autologin_user("[Seat:*]\n#autologin-user=x\nautologin-user=lexus\n"),
                         "lexus")
        self.assertIsNone(self_check.autologin_user("[Seat:*]\n"))

    def test_a_report_with_a_failure_exits_one_and_counts_each_result(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            bin_directory = root / "bin"
            bin_directory.mkdir()
            vm = root / "vm"
            vm.mkdir()
            (vm / "dirty_expire_centisecs").write_text("500\n", encoding="ascii")
            (vm / "dirty_writeback_centisecs").write_text("3000\n", encoding="ascii")
            config = root / "car.conf"
            config.write_text(f"[session]\ndata_dir = {root}\n", encoding="utf-8")
            if POSIX:
                config.chmod(0o600)
            paths = self_check.Paths(bin_directory=bin_directory, car_config=config,
                                     lightdm=root / "lightdm.conf",
                                     labwc_autostart=root / "autostart",
                                     cmdline=root / "cmdline.txt", vm_directory=vm,
                                     polkit_rule=root / "rule", hub_registry=root / "hub.conf")
            replies = {"busctl": (0, "u 1"), "systemctl": (0, "enabled"), "id": (0, "lexus dialout"),
                       "nmcli": (0, "hotspot\n"), "vcgencmd": (0, "throttled=0x0")}
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                status = self_check.main([], runner=lambda command: replies.get(command[0], (1, "")),
                                         paths=paths)
            text = output.getvalue()
            self.assertEqual(status, 1)
            self.assertIn("FAIL  executables installed", text)
            self.assertIn("SKIP  adapter address", text)
            self.assertIn("FAIL  writeback within about 5 s", text)
            self.assertIn("PASS  phone hotspot saved", text)
            self.assertRegex(text, r"self-check: \d+ PASS, \d+ FAIL, \d+ SKIP")


@unittest.skipUnless(POSIX and shutil.which("bash"), "the install script is a bash script")
class InstallScriptTest(unittest.TestCase):
    SCRIPT = REPOSITORY / "deploy" / "car" / "install_car_mode.sh"

    def test_the_scripts_parse(self):
        for script in (self.SCRIPT, REPOSITORY / "deploy" / "car" / "lexus-hub-session",
                       REPOSITORY / "deploy" / "bind_obd_adapter.sh"):
            result = subprocess.run(["bash", "-n", str(script)], capture_output=True, text=True,
                                    check=False)
            self.assertEqual(result.returncode, 0, f"{script}: {result.stderr}")

    def test_a_dry_run_changes_nothing_and_lists_every_step(self):
        with tempfile.TemporaryDirectory() as folder:
            artifact = Path(folder) / "artifact"
            artifact.mkdir()
            elf_header = bytes(18) + bytes([0xB7, 0x00])
            for name in ("lexus-hub", "lexus-head-unit", "lexus-vehicle-data-service"):
                (artifact / name).write_bytes(elf_header)
            home = Path(folder) / "home"
            home.mkdir()
            environment = dict(os.environ, HOME=str(home))
            result = subprocess.run(["bash", str(self.SCRIPT), "--dry-run", "--artifact",
                                     str(artifact)], capture_output=True, text=True,
                                    env=environment, check=False, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            for heading in ("== 1. Packages", "== 2. Executables", "== 3. Car-mode tools",
                            "== 4. The car-mode file", "== 5. System", "== 6. User units",
                            "== 7. Desktop", "== 8. Self-check"):
                self.assertIn(heading, result.stdout)
            self.assertIn("would run: sudo install -m 755", result.stdout)
            self.assertEqual(list(home.iterdir()), [])

    def test_an_executable_for_another_cpu_is_refused(self):
        with tempfile.TemporaryDirectory() as folder:
            artifact = Path(folder)
            for name in ("lexus-hub", "lexus-head-unit", "lexus-vehicle-data-service"):
                (artifact / name).write_bytes(bytes(18) + bytes([0x3E, 0x00]))
            result = subprocess.run(["bash", str(self.SCRIPT), "--dry-run", "--artifact",
                                     str(artifact)], capture_output=True, text=True, check=False,
                                    timeout=60)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("not built for AArch64", result.stderr)


@unittest.skipUnless(POSIX and shutil.which("sh") and shutil.which("awk"), "POSIX shell needed")
class BindScriptTest(unittest.TestCase):
    def test_no_address_yet_exits_zero_without_binding(self):
        with tempfile.TemporaryDirectory() as folder:
            config = Path(folder) / "car.conf"
            config.write_text("[adapter]\naddress =\nchannel = 1\n", encoding="utf-8")
            result = subprocess.run(["sh", str(REPOSITORY / "deploy" / "bind_obd_adapter.sh"),
                                     "--config", str(config)], capture_output=True, text=True,
                                    check=False)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("no adapter address", result.stdout)

    def test_a_malformed_address_is_refused(self):
        with tempfile.TemporaryDirectory() as folder:
            config = Path(folder) / "car.conf"
            config.write_text("[adapter]\naddress = 12:34\n", encoding="utf-8")
            result = subprocess.run(["sh", str(REPOSITORY / "deploy" / "bind_obd_adapter.sh"),
                                     "--config", str(config)], capture_output=True, text=True,
                                    check=False)
            self.assertEqual(result.returncode, 1)
            self.assertIn("not a Bluetooth address", result.stderr)


if __name__ == "__main__":
    unittest.main()
