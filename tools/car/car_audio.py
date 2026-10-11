#!/usr/bin/env python3
"""Plays the Pi's sound through the car stereo over Bluetooth (LHU-056, REQ-019).

The Pi is the audio source and the car stereo the sink (A2DP), so the YouTube and Spotify
pages, the mirrored phone and every other sound of the Pi come out of the car speakers. Runs on
the Pi from the hub's "Car audio" tile, in a terminal on the touchscreen; nothing has to be typed.

1. If car.conf names a stereo ([audio] address), it is connected. Otherwise, or with --pair,
   the script scans for up to --scan-seconds for a device whose name contains the name fragment
   ([audio] name_fragment, default "LEXUS", assumption A34), pairs it (a passkey shown on the
   car's screen is confirmed here; tap Yes on the car too), trusts it and connects.
2. When PipeWire has made the Bluetooth output (a sink named bluez_output.*), it becomes the
   default output, which every program then plays to.
3. The address is written into car.conf, the only file that holds it; only its last two bytes
   are printed.

Put the car stereo in pairing mode (on the car's screen: Setup, Bluetooth, add a device) before
the first run. The OBD adapter uses the same Bluetooth radio; whether both links hold together
is measured in the car (OQ-28, checklist step 3.21).
"""

import argparse
import os
import subprocess
import sys
import time
from pathlib import Path
from typing import Callable, List, Optional, Sequence, Tuple

sys.path.insert(0, os.path.dirname(os.path.realpath(__file__)))

import car_config  # noqa: E402
import pair_adapter  # noqa: E402

DEFAULT_FRAGMENT = "LEXUS"
SINK_PREFIX = "bluez_output."
EXIT_OK = 0
EXIT_NOT_FOUND = 2
EXIT_PAIRING_FAILED = 3
EXIT_NO_OUTPUT = 4

Runner = Callable[[List[str]], Tuple[int, str]]


def run_command(command: List[str]) -> Tuple[int, str]:
    try:
        completed = subprocess.run(command, capture_output=True, text=True, check=False, timeout=10)
    except (FileNotFoundError, subprocess.TimeoutExpired):
        return 127, ""
    return completed.returncode, completed.stdout


def choose_stereo(devices: List[Tuple[str, str]], fragment: str) -> Optional[Tuple[str, str]]:
    for address, name in devices:
        if fragment.lower() in name.lower():
            return address, name
    return None


def bluetooth_sink(pactl_output: str) -> Optional[str]:
    """The name of the first Bluetooth output in `pactl list short sinks`."""
    for line in pactl_output.splitlines():
        fields = line.split()
        if len(fields) > 1 and fields[1].startswith(SINK_PREFIX):
            return fields[1]
    return None


def connect(session: pair_adapter.Session, address: str, seconds: float) -> bool:
    session.send(f"connect {address}")
    return session.expect(["Connection successful", "Failed to connect"], seconds) == \
        "Connection successful"


def find_pair_and_connect(session: pair_adapter.Session, known: Optional[str], fragment: str,
                          scan_seconds: float, report: Callable[[str], None]
                          ) -> Tuple[Optional[str], str]:
    session.send("power on")
    session.send("agent KeyboardDisplay")
    session.send("default-agent")
    session.read_for(1.0)
    address = known
    if address is None:
        report(f"scanning for the car stereo (a name containing '{fragment}') for up to "
               f"{scan_seconds:.0f} s; put the car in Bluetooth pairing mode")
        session.send("scan on")
        deadline = time.monotonic() + scan_seconds
        chosen = None
        while chosen is None and time.monotonic() < deadline:
            session.read_for(1.0)
            chosen = choose_stereo(pair_adapter.devices_in(session.seen), fragment)
        session.send("scan off")
        if chosen is None:
            return None, f"no device named like '{fragment}' found (set [audio] name_fragment)"
        address, name = chosen
        report(f"found {name} ({car_config.masked_address(address)}), pairing: confirm on the car")
        if not pair_adapter.pair(session, address, "0000", 60.0):
            return None, "pairing failed (refused on the car, or timed out)"
        session.send(f"trust {address}")
        if session.expect(["trust succeeded", "Trusted: yes"], 10.0) is None:
            return None, "trust did not succeed"
    else:
        report(f"connecting the paired stereo ({car_config.masked_address(address)})")
    if not connect(session, address, 20.0):
        return None, "connecting failed (is the car on, and Bluetooth audio selected on the car?)"
    return address, "connected"


def make_default_output(runner: Runner, sleep: Callable[[float], None] = time.sleep,
                        seconds: int = 15) -> Optional[str]:
    """Waits for PipeWire's Bluetooth output and makes it the default; its name or None."""
    for _ in range(seconds):
        _, sinks = runner(["pactl", "list", "short", "sinks"])
        sink = bluetooth_sink(sinks)
        if sink is not None:
            code, _ = runner(["pactl", "set-default-sink", sink])
            return sink if code == 0 else None
        sleep(1.0)
    return None


def main(argument_list: Optional[Sequence[str]] = None, runner: Runner = run_command,
         sleep: Callable[[float], None] = time.sleep) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--config", type=Path, default=car_config.DEFAULT_PATH)
    parser.add_argument("--pair", action="store_true", help="pair a stereo even if one is saved")
    parser.add_argument("--scan-seconds", type=float, default=45.0)
    parser.add_argument("--bluetoothctl", default="bluetoothctl")
    parser.add_argument("--wait-at-end", type=float, default=0.0,
                        help="seconds to keep the result on screen (touchscreen terminal)")
    arguments = parser.parse_args(argument_list)
    values = car_config.read(arguments.config)
    known = None if arguments.pair else car_config.normalised_address(values.get("audio.address", ""))
    fragment = values.get("audio.name_fragment", "").strip() or DEFAULT_FRAGMENT

    def report(message: str) -> None:
        print("car-audio: " + message, flush=True)

    session = pair_adapter.Session(arguments.bluetoothctl)
    session.start()
    try:
        address, reason = find_pair_and_connect(session, known, fragment, arguments.scan_seconds,
                                                report)
    finally:
        session.close()
    status = EXIT_OK
    if address is None:
        report("FAILED: " + reason)
        status = EXIT_NOT_FOUND if reason.startswith("no device") else EXIT_PAIRING_FAILED
    else:
        if address != known:
            car_config.write_values(arguments.config, {"audio.address": address})
            report(f"address saved in {arguments.config}")
        sink = make_default_output(runner, sleep)
        if sink is None:
            report("FAILED: no Bluetooth output appeared in PipeWire")
            status = EXIT_NO_OUTPUT
        else:
            report("the car stereo is the default output: every sound of the Pi plays there")
    if arguments.wait_at_end > 0:
        sleep(arguments.wait_at_end)
    return status


if __name__ == "__main__":
    sys.exit(main())
