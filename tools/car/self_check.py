#!/usr/bin/env python3
"""Checks a car-mode install on the Pi and prints PASS, FAIL or SKIP per item (LHU-044).

Run at the end of deploy/car/install_car_mode.sh and any time afterwards:

    lexus-car-selfcheck              everything that can be checked at home
    lexus-car-selfcheck --with-demo  also switches to demo mode for up to 20 s, checks that live
                                     values come over D-Bus, then switches back

SKIP means the item cannot be checked yet (for example the adapter before it is paired in the
car); it is not a failure. The exit code is 1 when any item fails.
"""

import argparse
import os
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, List, Optional, Sequence, Tuple

sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))

import car_config  # noqa: E402

PASS, FAIL, SKIP = "PASS", "FAIL", "SKIP"
EXECUTABLES = ("lexus-hub", "lexus-head-unit", "lexus-vehicle-data-service")
SCRIPTS = ("lexus-car-session", "lexus-mode", "lexus-pair-adapter", "lexus-export-sessions",
           "lexus-bind-adapter", "lexus-hub-session", "lexus-car-selfcheck")
BUS_NAME = "io.github.samersamaha.LexusHeadUnit"
OBJECT_PATH = "/io/github/samersamaha/LexusHeadUnit/VehicleData"
INTERFACE = "io.github.samersamaha.LexusHeadUnit.VehicleData1"
LINK_DETAIL_NAMES = ("Idle", "SearchingForAdapter", "AdapterWithoutVehicle", "Live",
                     "LinkLostRetrying")
WRITEBACK = {"dirty_expire_centisecs": "500", "dirty_writeback_centisecs": "100"}
MINIMUM_FREE_BYTES = 1 << 30

Runner = Callable[[List[str]], Tuple[int, str]]


def run_command(command: List[str], environment: Optional[dict] = None) -> Tuple[int, str]:
    try:
        completed = subprocess.run(command, capture_output=True, text=True, check=False,
                                   timeout=30, env=environment)
    except FileNotFoundError:
        return 127, ""
    except subprocess.TimeoutExpired:
        return 124, ""
    return completed.returncode, completed.stdout + completed.stderr


@dataclass
class Paths:
    bin_directory: Path = Path("/usr/local/bin")
    car_config: Path = car_config.DEFAULT_PATH
    lightdm: Path = Path("/etc/lightdm/lightdm.conf")
    labwc_autostart: Path = Path.home() / ".config" / "labwc" / "autostart"
    cmdline: Path = Path("/boot/firmware/cmdline.txt")
    vm_directory: Path = Path("/proc/sys/vm")
    polkit_rule: Path = Path("/etc/polkit-1/rules.d/50-lexus-obd-bind.rules")
    hub_registry: Path = Path.home() / "Lexus-Car-Device" / "deploy" / "car" / "hub_car.conf"


@dataclass
class Report:
    rows: List[Tuple[str, str, str]] = field(default_factory=list)

    def add(self, result: str, item: str, detail: str = "") -> None:
        self.rows.append((result, item, detail))
        print(f"{result}  {item}" + (f": {detail}" if detail else ""), flush=True)

    def count(self, result: str) -> int:
        return sum(1 for row in self.rows if row[0] == result)


def busctl(method: str, runner: Runner) -> Tuple[int, str]:
    return runner(["busctl", "--user", "call", BUS_NAME, OBJECT_PATH, INTERFACE, method])


def link_detail_name(output: str) -> Optional[str]:
    parts = output.split()
    if len(parts) == 2 and parts[0] == "u" and parts[1].isdigit():
        number = int(parts[1])
        if number < len(LINK_DETAIL_NAMES):
            return LINK_DETAIL_NAMES[number]
    return None


def valid_sample_count(get_samples_output: str) -> int:
    """GetSamples prints 'a(uduxu) N  id value unit time status ...'; counts status 1 (Valid)."""
    words = get_samples_output.split()
    if len(words) < 2 or not words[0].startswith("a("):
        return 0
    fields = words[2:]
    return sum(1 for index in range(4, len(fields), 5) if fields[index] == "1")


def autologin_user(lightdm_text: str) -> Optional[str]:
    for line in lightdm_text.splitlines():
        stripped = line.strip()
        if stripped.startswith("autologin-user=") and not stripped.startswith("#"):
            return stripped.split("=", 1)[1].strip() or None
    return None


def check_executables(report: Report, paths: Paths, runner: Runner) -> None:
    missing = [name for name in EXECUTABLES
               if not os.access(paths.bin_directory / name, os.X_OK)]
    report.add(FAIL if missing else PASS, "executables installed",
               ("missing " + ", ".join(missing)) if missing else str(paths.bin_directory))
    if missing:
        return
    unresolved = []
    for name in EXECUTABLES:
        _, output = runner(["ldd", str(paths.bin_directory / name)])
        unresolved += [line.split()[0] for line in output.splitlines() if "not found" in line]
    report.add(FAIL if unresolved else PASS, "shared libraries resolve",
               ("not found: " + ", ".join(sorted(set(unresolved)))) if unresolved else "")
    code, _ = runner(["env", "QT_QPA_PLATFORM=offscreen", "timeout", "8",
                      str(paths.bin_directory / "lexus-head-unit"), "--source", "fake"])
    report.add(PASS if code == 124 else FAIL, "vehicle-data app starts (offscreen, demo source)",
               "ran 8 s" if code == 124 else f"exited with {code} before 8 s")


def check_scripts(report: Report, paths: Paths) -> None:
    missing = [name for name in SCRIPTS if not os.access(paths.bin_directory / name, os.X_OK)]
    report.add(FAIL if missing else PASS, "car-mode scripts installed",
               ("missing " + ", ".join(missing)) if missing else "")


def check_configuration(report: Report, paths: Paths, runner: Runner) -> Optional[str]:
    if not paths.car_config.is_file():
        report.add(FAIL, "car.conf present", str(paths.car_config))
        return None
    private = (paths.car_config.stat().st_mode & 0o077) == 0
    report.add(PASS if private else FAIL, "car.conf present and private",
               str(paths.car_config) + ("" if private else " is readable by others"))
    values = car_config.read(paths.car_config)
    report.add(PASS, "mode", car_config.mode_of(values))
    address = car_config.normalised_address(values.get("adapter.address", ""))
    if address is None:
        report.add(SKIP, "adapter address", "not paired yet: run lexus-pair-adapter in the car")
        return None
    report.add(PASS, "adapter address", car_config.masked_address(address))
    _, info = runner(["bluetoothctl", "info", address])
    paired = "Paired: yes" in info and "Trusted: yes" in info
    report.add(PASS if paired else FAIL, "adapter paired and trusted in BlueZ",
               "" if paired else "run lexus-pair-adapter again")
    return address


def check_units(report: Report, address: Optional[str], runner: Runner) -> None:
    code, output = runner(["systemctl", "is-enabled", "lexus-obd-bind.service"])
    report.add(PASS if output.strip() == "enabled" else FAIL, "adapter binding unit enabled",
               output.strip())
    if address is None:
        report.add(SKIP, "/dev/rfcomm0 bound", "no adapter address yet")
    else:
        _, output = runner(["rfcomm"])
        bound = "rfcomm0:" in output
        report.add(PASS if bound else FAIL, "/dev/rfcomm0 bound", "" if bound else output.strip())
    _, groups = runner(["id", "-nG"])
    report.add(PASS if "dialout" in groups.split() else FAIL, "user may open /dev/rfcomm0",
               "member of dialout" if "dialout" in groups.split() else "not in dialout")
    for unit in ("lexus-vehicle-data-service.service", "lexus-elm327-emulator.service"):
        code, output = runner(["systemctl", "--user", "is-enabled", unit])
        expected = "enabled" if unit.startswith("lexus-vehicle") else ("disabled", "static",
                                                                        "enabled")
        state = output.strip()
        good = state == expected if isinstance(expected, str) else state in expected
        report.add(PASS if good else FAIL, f"user unit {unit} installed", state)
    code, output = runner(["systemctl", "--user", "is-active", "lexus-vehicle-data-service"])
    report.add(PASS if output.strip() == "active" else FAIL, "vehicle-data service running",
               output.strip())
    code, output = busctl("GetInterfaceVersion", runner)
    report.add(PASS if output.strip() == "u 1" else FAIL, "service answers on D-Bus",
               output.strip())
    code, output = busctl("GetLinkDetail", runner)
    name = link_detail_name(output.strip())
    report.add(PASS if name else FAIL, "link detail readable", name or output.strip())


def check_session(report: Report, paths: Paths) -> None:
    autostart = paths.labwc_autostart.read_text(encoding="utf-8") \
        if paths.labwc_autostart.is_file() else ""
    report.add(PASS if "lexus-hub-session" in autostart else FAIL, "hub starts with the desktop",
               str(paths.labwc_autostart))
    lightdm = paths.lightdm.read_text(encoding="utf-8") if paths.lightdm.is_file() else ""
    user = autologin_user(lightdm)
    me = os.environ.get("USER", "")
    report.add(PASS if user and user == me else FAIL, "desktop logs in by itself",
               f"autologin-user={user}" if user else f"no autologin-user in {paths.lightdm}")
    registry = car_config.read(paths.hub_registry)
    apps = [app.strip() for app in registry.get("hub.apps", "").split(",") if app.strip()]
    missing = [app for app in apps if not registry.get(f"app.{app}.command")
               and registry.get(f"app.{app}.kind", "native") == "native"]
    report.add(PASS if apps and not missing else FAIL, "car hub registry",
               f"{len(apps)} apps" if apps and not missing else f"problem: {missing or 'no apps'}")


def check_power_safety(report: Report, paths: Paths, runner: Runner) -> None:
    values = {}
    for name in WRITEBACK:
        try:
            values[name] = (paths.vm_directory / name).read_text(encoding="ascii").strip()
        except OSError:
            values[name] = "unreadable"
    good = values == WRITEBACK
    report.add(PASS if good else FAIL, "writeback within about 5 s",
               ", ".join(f"{name}={value}" for name, value in values.items()))
    cmdline = paths.cmdline.read_text(encoding="ascii") if paths.cmdline.is_file() else ""
    report.add(PASS if "fsck.repair=yes" in cmdline.split() else FAIL,
               "file-system repair at boot", "fsck.repair=yes" if "fsck.repair=yes" in
               cmdline.split() else f"not in {paths.cmdline}")
    report.add(PASS if paths.polkit_rule.is_file() else FAIL, "pairing may restart the binding",
               str(paths.polkit_rule))
    code, output = runner(["vcgencmd", "get_throttled"])
    if code == 127:
        report.add(SKIP, "power flags now", "vcgencmd not found")
    else:
        report.add(PASS if output.strip() == "throttled=0x0" else FAIL, "power flags now",
                   output.strip())


def check_data(report: Report, paths: Paths, runner: Runner) -> None:
    values = car_config.read(paths.car_config)
    data = car_config.data_directory(values)
    writable = data.is_dir() and os.access(data, os.W_OK)
    free = shutil.disk_usage(data).free if data.is_dir() else 0
    good = writable and free >= MINIMUM_FREE_BYTES
    report.add(PASS if good else FAIL, "session folder writable with 1 GiB free",
               f"{data}, {free / (1 << 30):.1f} GiB free")
    name = values.get("network.hotspot_connection", "hotspot") or "hotspot"
    _, output = runner(["nmcli", "-t", "-f", "NAME", "connection", "show"])
    report.add(PASS if name in output.splitlines() else SKIP, "phone hotspot saved",
               name if name in output.splitlines() else
               f"no connection named '{name}' (docs/release/CAR_DAY_GUIDE.md, at home)")


def check_demo(report: Report, paths: Paths, runner: Runner,
               sleep: Callable[[float], None] = time.sleep) -> None:
    values = car_config.read(paths.car_config)
    previous = car_config.mode_of(values)
    replay_file = values.get("mode.replay_file", "")
    code, _ = runner([str(paths.bin_directory / "lexus-mode"), "demo"])
    live, valid = None, 0
    for _ in range(20):
        sleep(1.0)
        _, output = busctl("GetLinkDetail", runner)
        live = link_detail_name(output.strip())
        _, samples = busctl("GetSamples", runner)
        valid = valid_sample_count(samples)
        if live == "Live" and valid > 0:
            break
    restore = [str(paths.bin_directory / "lexus-mode"), previous]
    if previous == "replay" and replay_file:
        restore.append(replay_file)
    runner(restore)
    good = code == 0 and live == "Live" and valid > 0
    report.add(PASS if good else FAIL, "demo mode shows live values over D-Bus",
               f"detail {live}, {valid} valid signals; mode restored to {previous}")


def main(argument_list: Optional[Sequence[str]] = None, runner: Runner = run_command,
         paths: Optional[Paths] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--with-demo", action="store_true")
    parser.add_argument("--registry", type=Path)
    arguments = parser.parse_args(argument_list)
    paths = paths or Paths()
    if arguments.registry:
        paths.hub_registry = arguments.registry
    report = Report()
    check_executables(report, paths, runner)
    check_scripts(report, paths)
    address = check_configuration(report, paths, runner)
    check_units(report, address, runner)
    check_session(report, paths)
    check_power_safety(report, paths, runner)
    check_data(report, paths, runner)
    if arguments.with_demo:
        check_demo(report, paths, runner)
    print(f"self-check: {report.count(PASS)} PASS, {report.count(FAIL)} FAIL, "
          f"{report.count(SKIP)} SKIP")
    return 1 if report.count(FAIL) else 0


if __name__ == "__main__":
    sys.exit(main())
