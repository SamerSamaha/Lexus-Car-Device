#!/usr/bin/env python3
"""ELM327 adapter emulator with fault injection, for desk and CI tests of the OBD-II path.

Serves the ELM327 text protocol on a pseudo-terminal (Linux) and takes fault commands on a
Unix domain control socket. The command handler (Elm327Emulator.handle_line) is a pure
function of the command text and the emulator state, so it is unit-tested on any platform.

Usage:
    elm327_emulator.py --link /tmp/obd --control /tmp/obd.control [--profile standard]
Then open /tmp/obd as a serial device. Control: `echo "stale 0D" | socat - UNIX:/tmp/obd.control`.

Standard library only. The vehicle identification number returned by Mode 09 is the documented
hypothetical value from tools/private_data_allowlist.txt unless --vin is given.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import os
import random
import select
import socket
import sys
import time
from collections import Counter
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Sequence

ADAPTER_IDENTITY = "ELM327 v1.5"
PROTOCOL_NUMBER = "A6"
PROMPT = "\r\r>"
DEFAULT_VIN = "1M8GDM9AXKP042788"

SETUP_COMMANDS_WITH_OK = ("ATE0", "ATE1", "ATL0", "ATL1", "ATS0", "ATS1", "ATH0", "ATH1", "ATSP0")
ALLOWED_AT_COMMANDS = ("ATZ", "ATI", "ATRV", "ATDPN") + SETUP_COMMANDS_WITH_OK
ALLOWED_MODES = (0x01, 0x03, 0x09)

PID_PROFILES: Dict[str, List[int]] = {
    # The 8 PIDs of REQ-004 plus the ones a 2013 engine controller typically reports.
    "standard": [0x01, 0x03, 0x04, 0x05, 0x06, 0x07, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11,
                 0x13, 0x15, 0x1C, 0x1F, 0x21, 0x2F, 0x31, 0x33, 0x3C, 0x42, 0x45, 0x46, 0x47,
                 0x49, 0x4A, 0x4C, 0x51, 0x5C],
    "no-fuel-level": [0x01, 0x03, 0x04, 0x05, 0x06, 0x07, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
                      0x11, 0x13, 0x15, 0x1C, 0x1F, 0x21, 0x31, 0x33, 0x3C, 0x42, 0x45, 0x46],
    "minimal": [0x0C, 0x0D],
}

MODE09_SUPPORTED_PIDS = [0x02, 0x04, 0x06, 0x0A]
MAXIMUM_SINGLE_FRAME_PAYLOAD = 7


@dataclass
class VehicleModel:
    """Deterministic engine values as functions of elapsed seconds."""

    def engine_rpm(self, seconds: float) -> float:
        return 800.0 + 400.0 * (1.0 + math.sin(seconds / 3.0))

    def vehicle_speed_kmh(self, seconds: float) -> float:
        return max(0.0, 40.0 + 30.0 * math.sin(seconds / 7.0))

    def coolant_celsius(self, seconds: float) -> float:
        return min(90.0, 20.0 + seconds * 2.0)

    def engine_load_percent(self, seconds: float) -> float:
        return 30.0 + 20.0 * (1.0 + math.sin(seconds / 5.0)) / 2.0

    def throttle_percent(self, seconds: float) -> float:
        return 12.0 + 10.0 * (1.0 + math.sin(seconds / 4.0)) / 2.0

    def intake_air_celsius(self, seconds: float) -> float:
        return 25.0

    def control_module_volts(self, seconds: float) -> float:
        return 14.1 + 0.1 * math.sin(seconds)

    def fuel_level_percent(self, seconds: float) -> float:
        return max(0.0, 63.0 - seconds / 600.0)

    def mass_air_flow_grams_per_second(self, seconds: float) -> float:
        return 3.0 + 10.0 * (1.0 + math.sin(seconds / 3.0)) / 2.0


def encode_pid(pid: int, model: VehicleModel, seconds: float) -> Optional[bytes]:
    """Inverse of the SAE J1979 formulas used by the decoder (DN-009)."""
    if pid == 0x04:
        return bytes([round(model.engine_load_percent(seconds) * 255 / 100)])
    if pid == 0x05:
        return bytes([round(model.coolant_celsius(seconds)) + 40])
    if pid == 0x0C:
        raw = round(model.engine_rpm(seconds) * 4)
        return bytes([raw >> 8, raw & 0xFF])
    if pid == 0x0D:
        return bytes([round(model.vehicle_speed_kmh(seconds))])
    if pid == 0x0F:
        return bytes([round(model.intake_air_celsius(seconds)) + 40])
    if pid == 0x10:
        raw = round(model.mass_air_flow_grams_per_second(seconds) * 100)
        return bytes([raw >> 8, raw & 0xFF])
    if pid == 0x11:
        return bytes([round(model.throttle_percent(seconds) * 255 / 100)])
    if pid == 0x2F:
        return bytes([round(model.fuel_level_percent(seconds) * 255 / 100)])
    if pid == 0x42:
        raw = round(model.control_module_volts(seconds) * 1000)
        return bytes([raw >> 8, raw & 0xFF])
    return None


def supported_pid_bitmap(supported: Sequence[int], base_pid: int) -> bytes:
    """The 4-byte bitmap for PIDs base_pid + 1 .. base_pid + 32; the last bit chains onward."""
    bits = 0
    for offset in range(1, 33):
        pid = base_pid + offset
        is_set = pid in supported
        if offset == 32:
            is_set = pid in supported or any(
                base_pid + 32 < candidate <= base_pid + 64 for candidate in supported)
        if is_set:
            bits |= 1 << (32 - offset)
    return bits.to_bytes(4, "big")


def dtc_bytes(code: str) -> bytes:
    """Encode a trouble code such as P0133 or U0100 as its two bytes."""
    letter_values = {"P": 0, "C": 1, "B": 2, "U": 3}
    letter = code[0].upper()
    if letter not in letter_values or len(code) != 5:
        raise ValueError(f"not a trouble code: {code}")
    digits = int(code[1:], 16)
    first = (letter_values[letter] << 6) | ((digits >> 8) & 0x3F)
    return bytes([first, digits & 0xFF])


def multi_frame_lines(payload: bytes) -> List[str]:
    """ISO 15765 text framing as the adapter shows it with headers off."""
    if len(payload) <= MAXIMUM_SINGLE_FRAME_PAYLOAD:
        return [payload.hex().upper()]
    lines = [f"{len(payload):03X}"]
    frame_index = 0
    position = 0
    first_frame_payload = 6
    lines.append(f"0:{payload[:first_frame_payload].hex().upper()}")
    position = first_frame_payload
    while position < len(payload):
        frame_index = (frame_index + 1) % 16
        chunk = payload[position:position + 7]
        lines.append(f"{frame_index:X}:{chunk.hex().upper()}")
        position += 7
    return lines


def spaced(hex_text: str) -> str:
    """Insert the adapter's spaces between bytes, keeping any 'N:' frame prefix."""
    prefix = ""
    body = hex_text
    if ":" in hex_text:
        prefix, body = hex_text.split(":", 1)
        prefix += ": "
    if len(body) % 2 != 0:
        return prefix + body
    return prefix + " ".join(body[index:index + 2] for index in range(0, len(body), 2))


@dataclass
class EmulatorStatistics:
    requests: int = 0
    replies: int = 0
    forbidden_requests: int = 0
    unknown_commands: int = 0
    corrupted_replies: int = 0
    per_command: Counter = field(default_factory=Counter)

    def as_dict(self) -> dict:
        return {
            "requests": self.requests,
            "replies": self.replies,
            "forbidden_requests": self.forbidden_requests,
            "unknown_commands": self.unknown_commands,
            "corrupted_replies": self.corrupted_replies,
            "per_command": dict(self.per_command),
        }


class Elm327Emulator:
    """The adapter's state and command handler."""

    def __init__(
        self,
        profile: str = "standard",
        vin: str = DEFAULT_VIN,
        trouble_codes: Sequence[str] = (),
        model: Optional[VehicleModel] = None,
        random_seed: int = 20261011,
    ) -> None:
        self.model = model or VehicleModel()
        self.supported_pids = list(PID_PROFILES[profile])
        self.vin = vin
        self.trouble_codes = list(trouble_codes)
        self.random = random.Random(random_seed)
        self.statistics = EmulatorStatistics()
        self.echo = True
        self.spaces = True
        self.linefeeds = True
        self.ignition_on = True
        self.silence_until_seconds = 0.0
        self.stale_pids: set = set()
        self.corrupt_remaining = 0
        self.delay_seconds = 0.0
        self.reset()

    def reset(self) -> None:
        self.echo = True
        self.spaces = True
        self.linefeeds = True

    # -- replies -------------------------------------------------------------------------------

    def handle_line(self, line: str, seconds: float) -> Optional[str]:
        """Return the full reply text (with echo and prompt), or None when silenced."""
        command = line.strip()
        self.statistics.requests += 1
        if seconds < self.silence_until_seconds:
            return None
        normalised = "".join(command.split()).upper()
        self.statistics.per_command[normalised] += 1
        echo_was_on = self.echo
        reply_lines = self._reply_lines(normalised)
        echo = command if echo_was_on else ""
        rendered = [spaced(text) if self.spaces and self._is_hex_line(text) else text
                    for text in reply_lines]
        separator = "\r\n" if self.linefeeds else "\r"
        body = separator.join(rendered)
        self.statistics.replies += 1
        if echo:
            return echo + separator + body + PROMPT
        return body + PROMPT

    @staticmethod
    def _is_hex_line(text: str) -> bool:
        body = text.split(":", 1)[1] if ":" in text else text
        return len(body) > 0 and all(character in "0123456789ABCDEF" for character in body)

    def _reply_lines(self, command: str) -> List[str]:
        if command == "":
            return [""]
        if command.startswith("AT"):
            return self._at_reply(command)
        if not all(character in "0123456789ABCDEF" for character in command) or len(command) % 2:
            self.statistics.unknown_commands += 1
            return ["?"]
        mode = int(command[:2], 16)
        if mode not in ALLOWED_MODES:
            self.statistics.forbidden_requests += 1
            return ["NO DATA"]
        if not self.ignition_on:
            return ["UNABLE TO CONNECT"]
        if mode == 0x01:
            return self._mode01_reply(command)
        if mode == 0x03:
            return self._mode03_reply(command)
        return self._mode09_reply(command)

    def _at_reply(self, command: str) -> List[str]:
        if command == "ATZ":
            self.reset()
            return [ADAPTER_IDENTITY]
        if command in SETUP_COMMANDS_WITH_OK:
            if command == "ATE0":
                self.echo = False
            elif command == "ATE1":
                self.echo = True
            elif command == "ATS0":
                self.spaces = False
            elif command == "ATS1":
                self.spaces = True
            elif command == "ATL0":
                self.linefeeds = False
            elif command == "ATL1":
                self.linefeeds = True
            return ["OK"]
        if command == "ATI":
            return [ADAPTER_IDENTITY]
        if command == "ATRV":
            return ["14.1V"]
        if command == "ATDPN":
            return [PROTOCOL_NUMBER]
        self.statistics.forbidden_requests += 1
        return ["?"]

    def _mode01_reply(self, command: str) -> List[str]:
        if len(command) != 4:
            return ["?"]
        pid = int(command[2:], 16)
        if pid % 0x20 == 0:
            has_pids_in_range = any(pid < candidate <= pid + 0x20 for candidate in self.supported_pids)
            if pid > 0 and not has_pids_in_range:
                return ["NO DATA"]
            bitmap = supported_pid_bitmap(self.supported_pids, pid)
            return [self._maybe_corrupt(f"41{pid:02X}" + bitmap.hex().upper())]
        if pid not in self.supported_pids or pid in self.stale_pids:
            return ["NO DATA"]
        data = encode_pid(pid, self.model, self._seconds)
        if data is None:
            return ["NO DATA"]
        return [self._maybe_corrupt(f"41{pid:02X}" + data.hex().upper())]

    def _mode03_reply(self, command: str) -> List[str]:
        if command != "03":
            return ["?"]
        payload = bytes([0x43, len(self.trouble_codes)])
        for code in self.trouble_codes:
            payload += dtc_bytes(code)
        if not self.trouble_codes:
            payload = bytes([0x43, 0x00])
        return [self._maybe_corrupt(line) for line in multi_frame_lines(payload)]

    def _mode09_reply(self, command: str) -> List[str]:
        if len(command) != 4:
            return ["?"]
        pid = int(command[2:], 16)
        if pid == 0x00:
            bitmap = supported_pid_bitmap(MODE09_SUPPORTED_PIDS, 0x00)
            return ["4900" + bitmap.hex().upper()]
        if pid == 0x02:
            payload = bytes([0x49, 0x02, 0x01]) + self.vin.encode("ascii")
            return multi_frame_lines(payload)
        return ["NO DATA"]

    def _maybe_corrupt(self, line: str) -> str:
        if self.corrupt_remaining <= 0 or len(line) < 5:
            return line
        self.corrupt_remaining -= 1
        self.statistics.corrupted_replies += 1
        position = self.random.randrange(4, len(line))
        replacement = self.random.choice("GHJKLMNPQRSTVWXYZ")
        return line[:position] + replacement + line[position + 1:]

    # -- faults --------------------------------------------------------------------------------

    _seconds: float = 0.0

    def set_time(self, seconds: float) -> None:
        self._seconds = seconds

    def apply_control(self, line: str, seconds: float) -> str:
        parts = line.strip().split()
        if not parts:
            return "ERR empty"
        verb = parts[0].lower()
        try:
            if verb == "silence":
                self.silence_until_seconds = seconds + float(parts[1])
            elif verb == "stale":
                self.stale_pids.add(int(parts[1], 16))
            elif verb == "unstale":
                self.stale_pids.discard(int(parts[1], 16))
            elif verb == "corrupt":
                self.corrupt_remaining = int(parts[1])
            elif verb == "delay":
                self.delay_seconds = float(parts[1]) / 1000.0
            elif verb == "ignition":
                self.ignition_on = parts[1].lower() == "on"
            elif verb == "stats":
                return json.dumps(self.statistics.as_dict())
            elif verb == "exit":
                return "OK exit"
            else:
                return f"ERR unknown control command {verb}"
        except (IndexError, ValueError) as error:
            return f"ERR {error}"
        return "OK"


# -- serving --------------------------------------------------------------------------------


def replace_symlink(target: str, link_path: str) -> None:
    temporary = link_path + ".tmp"
    if os.path.lexists(temporary):
        os.unlink(temporary)
    os.symlink(target, temporary)
    os.replace(temporary, link_path)


def serve(arguments: argparse.Namespace) -> int:
    import pty
    import tty

    emulator = Elm327Emulator(profile=arguments.profile, vin=arguments.vin,
                              trouble_codes=arguments.dtc or ())
    try:
        master_fd, slave_fd = pty.openpty()
    except OSError as error:
        print(f"elm327_emulator: cannot create a pseudo-terminal: {error}", file=sys.stderr)
        return 2
    tty.setraw(slave_fd)
    device_path = os.ttyname(slave_fd)
    if arguments.link:
        replace_symlink(device_path, arguments.link)
    print(device_path, flush=True)

    control_socket = None
    if arguments.control:
        if os.path.exists(arguments.control):
            os.unlink(arguments.control)
        control_socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        control_socket.bind(arguments.control)
        control_socket.listen(4)
        control_socket.setblocking(False)

    log_writer = None
    log_file = None
    if arguments.log:
        log_file = open(arguments.log, "w", newline="", encoding="ascii")
        log_writer = csv.writer(log_file)
        log_writer.writerow(["seconds", "direction", "text"])

    started = time.monotonic()
    pending = b""
    control_clients: List[socket.socket] = []
    exit_requested = False
    try:
        while not exit_requested:
            watched = [master_fd] + ([control_socket] if control_socket else []) + control_clients
            readable, _, _ = select.select(watched, [], [], 0.5)
            seconds = time.monotonic() - started
            for ready in readable:
                if ready is master_fd:
                    try:
                        chunk = os.read(master_fd, 256)
                    except OSError:
                        chunk = b""
                    if not chunk:
                        continue
                    pending += chunk
                    while b"\r" in pending or b"\n" in pending:
                        split_at = min(index for index in (pending.find(b"\r"), pending.find(b"\n"))
                                       if index >= 0)
                        line = pending[:split_at].decode("ascii", errors="replace")
                        pending = pending[split_at + 1:]
                        emulator.set_time(seconds)
                        if log_writer:
                            log_writer.writerow([f"{seconds:.3f}", "request", line])
                        if arguments.verbose:
                            print(f"< {line!r}", flush=True)
                        reply = emulator.handle_line(line, seconds)
                        if reply is None:
                            continue
                        if emulator.delay_seconds > 0:
                            time.sleep(emulator.delay_seconds)
                        if log_writer:
                            log_writer.writerow([f"{seconds:.3f}", "reply", reply])
                        if arguments.verbose:
                            print(f"> {reply!r}", flush=True)
                        try:
                            os.write(master_fd, reply.encode("ascii", errors="replace"))
                        except OSError:
                            pass
                elif ready is control_socket:
                    client, _ = control_socket.accept()
                    client.setblocking(False)
                    control_clients.append(client)
                else:
                    try:
                        data = ready.recv(1024)
                    except OSError:
                        data = b""
                    if not data:
                        control_clients.remove(ready)
                        ready.close()
                        continue
                    for control_line in data.decode("ascii", errors="replace").splitlines():
                        answer = emulator.apply_control(control_line, seconds)
                        if answer == "OK exit":
                            exit_requested = True
                        try:
                            ready.sendall((answer + "\n").encode("ascii"))
                        except OSError:
                            pass
    except KeyboardInterrupt:
        pass
    finally:
        for client in control_clients:
            client.close()
        if control_socket:
            control_socket.close()
            if arguments.control and os.path.exists(arguments.control):
                os.unlink(arguments.control)
        if log_file:
            log_file.close()
        os.close(slave_fd)
        os.close(master_fd)
    return 0


def parse_arguments(argument_list: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--link", help="symlink path that always points at the current device")
    parser.add_argument("--control", help="Unix domain socket path for fault injection")
    parser.add_argument("--profile", choices=sorted(PID_PROFILES), default="standard")
    parser.add_argument("--vin", default=DEFAULT_VIN)
    parser.add_argument("--dtc", nargs="*", help="stored trouble codes, for example P0133 P0420")
    parser.add_argument("--log", help="CSV of every request and reply")
    parser.add_argument("--verbose", action="store_true")
    return parser.parse_args(argument_list)


def main(argument_list: Optional[Sequence[str]] = None) -> int:
    return serve(parse_arguments(argument_list))


if __name__ == "__main__":
    sys.exit(main())
