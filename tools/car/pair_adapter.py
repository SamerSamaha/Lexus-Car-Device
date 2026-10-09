#!/usr/bin/env python3
"""Pairs the Bluetooth OBD-II adapter once and makes car mode use it (LHU-044).

Runs on the Pi, from an SSH session (a phone on the hotspot works) or from the hub's
"Pair adapter" tile, which opens it in a terminal on the touchscreen; nothing has to be typed.

1. If a paired device whose name contains the name fragment (default "vLinker") exists, it is
   used. Otherwise the script scans for up to --scan-seconds and pairs the first match,
   preferring the classic-Bluetooth name ending in "-Android" (the "-IOS" name is the BLE side,
   which the serial port does not use). The PIN (default 1234, from car.conf) answers a PIN
   prompt; a passkey confirmation is answered yes.
2. The device is trusted, so BlueZ reconnects to it after a reboot without asking.
3. Its address is written into car.conf ([adapter] address), the only file that holds it. That
   file lives in the home folder of the Pi and is never committed.
4. The binding unit (lexus-obd-bind.service, /dev/rfcomm0) and the vehicle-data service are
   restarted, so the service connects within its next retry.

bluetoothctl is driven through a pseudo-terminal, because it answers the pairing agent's
prompts only on a terminal. Only the last two bytes of the address are printed.
"""

import argparse
import os
import pty
import re
import select
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, List, Optional, Sequence, Tuple

sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))

import car_config  # noqa: E402

BIND_UNIT = "lexus-obd-bind.service"
SERVICE_UNIT = "lexus-vehicle-data-service.service"
EXIT_OK = 0
EXIT_NOT_FOUND = 2
EXIT_PAIRING_FAILED = 3
EXIT_RESTART_FAILED = 4

_ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]|\x01|\x02|\r")
_DEVICE = re.compile(r"Device ((?:[0-9A-F]{2}:){5}[0-9A-F]{2}) (.+)$")


def clean(text: str) -> str:
    return _ANSI.sub("", text)


def devices_in(text: str) -> List[Tuple[str, str]]:
    """(address, name) for every "Device <address> <name>" line, in order, without repeats."""
    found: List[Tuple[str, str]] = []
    for line in clean(text).splitlines():
        match = _DEVICE.search(line)
        if match and match.group(2).strip() and not match.group(2).startswith(("RSSI", "TxPower",
                                                                               "ManufacturerData",
                                                                               "UUIDs", "Connected",
                                                                               "Paired", "Trusted",
                                                                               "ServicesResolved")):
            pair = (match.group(1), match.group(2).strip())
            if pair[0] not in [address for address, _ in found]:
                found.append(pair)
    return found


def choose(devices: List[Tuple[str, str]], fragment: str) -> Optional[Tuple[str, str]]:
    matching = [device for device in devices if fragment.lower() in device[1].lower()]
    classic = [device for device in matching if device[1].lower().endswith("android")]
    not_ble = [device for device in matching if not device[1].lower().endswith("ios")]
    for group in (classic, not_ble, matching):
        if group:
            return group[0]
    return None


@dataclass
class Session:
    """bluetoothctl on a pseudo-terminal; send() a line, expect() one of several texts."""

    program: str
    process: Optional[subprocess.Popen] = None
    master: int = -1
    seen: str = ""
    transcript: List[str] = field(default_factory=list)

    def start(self) -> None:
        master, slave = pty.openpty()
        self.process = subprocess.Popen([self.program], stdin=slave, stdout=slave, stderr=slave,
                                        close_fds=True)
        os.close(slave)
        self.master = master

    def send(self, line: str, shown: Optional[str] = None) -> None:
        self.transcript.append("> " + (shown if shown is not None else line))
        os.write(self.master, (line + "\n").encode("ascii"))

    def read_for(self, seconds: float) -> str:
        deadline = time.monotonic() + seconds
        chunks = []
        while time.monotonic() < deadline:
            ready, _, _ = select.select([self.master], [], [], max(0.0, deadline - time.monotonic()))
            if not ready:
                break
            try:
                data = os.read(self.master, 4096)
            except OSError:
                break
            if not data:
                break
            chunks.append(data.decode("utf-8", "replace"))
        text = clean("".join(chunks))
        self.seen += text
        return text

    def expect(self, needles: Sequence[str], seconds: float) -> Optional[str]:
        """Reads until one of the needles appears after the current mark; returns it or None."""
        deadline = time.monotonic() + seconds
        start = len(self.seen)
        while True:
            for needle in needles:
                if needle in self.seen[start:]:
                    return needle
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return None
            self.read_for(min(0.2, remaining))

    def close(self) -> None:
        if self.process is None:
            return
        try:
            self.send("quit")
            self.process.wait(timeout=3)
        except (OSError, subprocess.TimeoutExpired):
            self.process.kill()
        os.close(self.master)


def pair(session: Session, address: str, pin: str, seconds: float) -> bool:
    session.send(f"pair {address}")
    prompts = ["Enter PIN code", "Confirm passkey", "Request confirmation", "Accept pairing",
               "Pairing successful", "AlreadyExists", "Failed to pair", "not available"]
    while True:
        found = session.expect(prompts, seconds)
        if found is None or found in ("Failed to pair", "not available"):
            return False
        if found in ("Pairing successful", "AlreadyExists"):
            return True
        if found == "Enter PIN code":
            session.send(pin, shown="(PIN)")
        else:
            session.send("yes")
        # The prompt text stays in what was seen; move the mark past it.
        session.seen += "\n"
        prompts = [prompt for prompt in prompts if prompt != found] + [found]


def find_and_pair(session: Session, fragment: str, pin: str, scan_seconds: float,
                  report: Callable[[str], None]) -> Tuple[Optional[str], str]:
    """The address of the adapter, paired and trusted, or None with the reason."""
    session.send("power on")
    session.send("agent KeyboardDisplay")
    session.send("default-agent")
    session.read_for(1.0)
    session.send("devices Paired")
    paired = choose(devices_in(session.read_for(1.5)), fragment)
    if paired is not None:
        address, name = paired
        report(f"already paired: {name} ({car_config.masked_address(address)})")
    else:
        report(f"scanning for a device named like '{fragment}' for up to {scan_seconds:.0f} s;"
               " the adapter must be in the port with the ignition on")
        session.send("scan on")
        deadline = time.monotonic() + scan_seconds
        chosen = None
        while chosen is None and time.monotonic() < deadline:
            session.read_for(1.0)
            chosen = choose(devices_in(session.seen), fragment)
        session.send("scan off")
        if chosen is None:
            return None, f"no device named like '{fragment}' found"
        address, name = chosen
        report(f"found {name} ({car_config.masked_address(address)}), pairing")
        if not pair(session, address, pin, 30.0):
            return None, "pairing failed (wrong PIN, or the adapter is paired to another device)"
    session.send(f"trust {address}")
    if session.expect(["trust succeeded", "Trusted: yes"], 10.0) is None:
        return None, "trust did not succeed"
    return address, "paired and trusted"


def restart_units(runner: Callable[[List[str]], int]) -> bool:
    bound = runner(["systemctl", "restart", BIND_UNIT]) == 0
    service = runner(["systemctl", "--user", "restart", SERVICE_UNIT]) == 0
    return bound and service


def main(argument_list: Optional[Sequence[str]] = None,
         runner: Callable[[List[str]], int] = lambda command: subprocess.run(
             command, check=False).returncode) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--config", type=Path, default=car_config.DEFAULT_PATH)
    parser.add_argument("--pin", help="PIN for legacy pairing (default: car.conf, then 1234)")
    parser.add_argument("--name-fragment", help="part of the adapter name (default: vLinker)")
    parser.add_argument("--scan-seconds", type=float, default=30.0)
    parser.add_argument("--bluetoothctl", default="bluetoothctl")
    parser.add_argument("--no-restart", action="store_true", help="do not restart the units")
    parser.add_argument("--wait-at-end", type=float, default=0.0,
                        help="seconds to keep the result on screen (touchscreen terminal)")
    arguments = parser.parse_args(argument_list)
    values = car_config.read(arguments.config)
    pin = arguments.pin or values.get("adapter.pin", "") or "1234"
    fragment = arguments.name_fragment or values.get("adapter.name_fragment", "") or "vLinker"

    def report(message: str) -> None:
        print("pair-adapter: " + message, flush=True)

    session = Session(arguments.bluetoothctl)
    session.start()
    try:
        address, reason = find_and_pair(session, fragment, pin, arguments.scan_seconds, report)
    finally:
        session.close()
    status = EXIT_OK
    if address is None:
        report("FAILED: " + reason)
        status = EXIT_NOT_FOUND if reason.startswith("no device") else EXIT_PAIRING_FAILED
    else:
        car_config.write_values(arguments.config, {"adapter.address": address})
        report(f"{reason}; address saved in {arguments.config}")
        if not arguments.no_restart:
            if restart_units(runner):
                report("binding and service restarted: the strip should say Live within a minute"
                       " with the ignition on")
            else:
                report("FAILED: restarting the binding or the service; reboot the Pi instead")
                status = EXIT_RESTART_FAILED
    if arguments.wait_at_end > 0:
        time.sleep(arguments.wait_at_end)
    return status


if __name__ == "__main__":
    sys.exit(main())
