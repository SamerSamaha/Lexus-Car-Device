#!/usr/bin/env python3
"""ELM327 touchscreen demo: a throwaway spike, not product code.

Purpose: prove the chain car -> Bluetooth OBD-II adapter -> Raspberry Pi ->
touchscreen end to end, and record what the adapter and the car answer. The
real application is built through the tickets and design notes; nothing in
this file is meant to be reused there.

What it does:
  * opens a Bluetooth RFCOMM socket to an ELM327-compatible adapter (or a fake
    one with --fake), sends the fixed ELM327 setup commands, reads the
    supported-PID bitmap, then polls a small fixed list of Mode 01 PIDs;
  * shows the values as large tiles in a full-screen window with two buttons;
  * writes every raw request and reply, with timestamps, to a log file so the
    session can be studied afterwards. The log stays in local_recordings/ and
    is never committed (it may contain Mode 09 responses on a real car; this
    spike never sends Mode 09, but the rule stands).

Read-only, enforced in code: every command passes through is_allowed()
before it is written, and only a fixed list of AT commands plus Mode 01
requests exist here. There is no Mode 04 and no raw frame sending.

Usage on the Raspberry Pi (adapter already paired and trusted):
    python3 elm327_touch_demo.py --name vLinker --fullscreen
    python3 elm327_touch_demo.py --address XX:XX:XX:XX:XX:XX --fullscreen
Usage anywhere, no adapter:
    python3 elm327_touch_demo.py --fake

--name looks the paired device up through bluetoothctl, so no Bluetooth address
has to be typed or stored anywhere.

Standard library only (tkinter must be installed: python3-tk on Debian).
"""

import argparse
import csv
import datetime
import math
import queue
import random
import socket
import sys
import threading
import time
import tkinter

# ---------------------------------------------------------------------------
# Allowlist (REQ-001 in spirit; the real one is CommandAllowlist, LHU-010)
# ---------------------------------------------------------------------------

ALLOWED_AT_COMMANDS = (
    "ATZ",    # reset the adapter
    "ATE0",   # echo off
    "ATL0",   # linefeeds off
    "ATS0",   # spaces off
    "ATH0",   # headers off
    "ATSP0",  # protocol: automatic
    "ATI",    # adapter identification
    "ATRV",   # adapter-measured battery voltage
    "ATDPN",  # describe protocol by number
)

ALLOWED_OBD_MODES = ("01",)


def is_allowed(command: str) -> bool:
    """Return True only for the fixed AT commands and Mode 01 requests."""
    if command in ALLOWED_AT_COMMANDS:
        return True
    return len(command) == 4 and command[:2] in ALLOWED_OBD_MODES and all(
        character in "0123456789ABCDEF" for character in command[2:]
    )


# ---------------------------------------------------------------------------
# PID decoding (SAE J1979 formulas for the PIDs of REQ-004)
# ---------------------------------------------------------------------------

class PidDefinition:
    def __init__(self, pid_hex, name, unit, byte_count, decode):
        self.pid_hex = pid_hex
        self.name = name
        self.unit = unit
        self.byte_count = byte_count
        self.decode = decode


PID_DEFINITIONS = [
    PidDefinition("0C", "Engine RPM", "rpm", 2, lambda data: (data[0] * 256 + data[1]) / 4.0),
    PidDefinition("0D", "Vehicle speed", "km/h", 1, lambda data: float(data[0])),
    PidDefinition("05", "Coolant", "C", 1, lambda data: float(data[0] - 40)),
    PidDefinition("04", "Engine load", "%", 1, lambda data: data[0] * 100.0 / 255.0),
    PidDefinition("11", "Throttle", "%", 1, lambda data: data[0] * 100.0 / 255.0),
    PidDefinition("0F", "Intake air", "C", 1, lambda data: float(data[0] - 40)),
    PidDefinition("42", "Module voltage", "V", 2, lambda data: (data[0] * 256 + data[1]) / 1000.0),
    PidDefinition("2F", "Fuel level", "%", 1, lambda data: data[0] * 100.0 / 255.0),
]

ERROR_REPLIES = ("NO DATA", "UNABLE TO CONNECT", "CAN ERROR", "BUFFER FULL",
                 "STOPPED", "BUS INIT", "ERROR", "?")


def decode_supported_pid_bitmap(reply_hex: str, first_pid: int = 0x01):
    """Decode a '41XXYYYYYYYY' reply into the set of supported PIDs first_pid..first_pid+31.

    first_pid is 0x01 for the reply to 0100, 0x21 for 0120, 0x41 for 0140.
    """
    expected_prefix = "41%02X" % (first_pid - 1)
    if not reply_hex.startswith(expected_prefix):
        return None
    payload = reply_hex[4:12]
    if len(payload) != 8:
        return None
    try:
        bitmap = int(payload, 16)
    except ValueError:
        return None
    supported = set()
    for bit_index in range(32):
        if bitmap & (1 << (31 - bit_index)):
            supported.add(first_pid + bit_index)
    return supported


def decode_mode01_reply(reply_hex: str, definition: PidDefinition):
    """Return the decoded value or None if the reply is not a clean Mode 01 answer."""
    expected_prefix = "41" + definition.pid_hex
    if not reply_hex.startswith(expected_prefix):
        return None
    data_hex = reply_hex[len(expected_prefix):len(expected_prefix) + 2 * definition.byte_count]
    if len(data_hex) != 2 * definition.byte_count:
        return None
    try:
        data_bytes = bytes.fromhex(data_hex)
    except ValueError:
        return None
    return definition.decode(data_bytes)


# ---------------------------------------------------------------------------
# Transports
# ---------------------------------------------------------------------------

def find_paired_device_address(name_fragment: str):
    """Return the address of the first paired Bluetooth device whose name contains name_fragment.

    Uses 'bluetoothctl devices Paired' (BlueZ 5.65 and later; falls back to 'devices').
    Each output line looks like 'Device XX:XX:XX:XX:XX:XX Some Name'.
    """
    import subprocess
    for arguments in (["bluetoothctl", "devices", "Paired"], ["bluetoothctl", "devices"]):
        try:
            output = subprocess.run(arguments, capture_output=True, text=True, timeout=10).stdout
        except (OSError, subprocess.TimeoutExpired):
            return None
        for line in output.splitlines():
            parts = line.strip().split(" ", 2)
            if len(parts) == 3 and parts[0] == "Device" and name_fragment.lower() in parts[2].lower():
                return parts[1]
    return None


class RfcommTransport:
    """Bluetooth classic serial link to the adapter."""

    def __init__(self, address: str, channel: int, timeout_seconds: float):
        self.socket = socket.socket(socket.AF_BLUETOOTH, socket.SOCK_STREAM, socket.BTPROTO_RFCOMM)
        self.socket.settimeout(timeout_seconds)
        self.socket.connect((address, channel))

    def write(self, data: bytes) -> None:
        self.socket.sendall(data)

    def read_some(self) -> bytes:
        try:
            return self.socket.recv(256)
        except socket.timeout:
            return b""

    def close(self) -> None:
        self.socket.close()


class FakeTransport:
    """Pretends to be an ELM327 on a running engine. For testing the demo without a car."""

    def __init__(self):
        self.pending_reply = b""
        self.started_at = time.monotonic()

    def write(self, data: bytes) -> None:
        command = data.decode("ascii", errors="replace").strip().upper()
        elapsed = time.monotonic() - self.started_at
        rpm = 800 + 400 * (1 + math.sin(elapsed / 3.0))
        speed_kmh = max(0.0, 40 + 30 * math.sin(elapsed / 7.0))
        coolant_c = min(90, 20 + elapsed * 2)
        replies = {
            "ATZ": "ELM327 v1.5 (fake)",
            "ATE0": "OK", "ATL0": "OK", "ATS0": "OK", "ATH0": "OK", "ATSP0": "OK",
            "ATI": "ELM327 v1.5 (fake)",
            "ATRV": "%.1fV" % (14.1 + random.uniform(-0.1, 0.1)),
            "ATDPN": "A6",
            "0100": "4100BE3FA813",  # last bit set: 0120 exists
            "0120": "41208007A001",  # bit for 0x2F set; last bit set: 0140 exists
            "0140": "4140FED00400",  # bit for 0x42 set; last bit clear: no 0160
            "0160": "NO DATA",
            "010C": "410C%04X" % int(rpm * 4),
            "010D": "410D%02X" % int(speed_kmh),
            "0105": "4105%02X" % int(coolant_c + 40),
            "0104": "4104%02X" % int(random.uniform(20, 60) * 255 / 100),
            "0111": "4111%02X" % int(random.uniform(10, 30) * 255 / 100),
            "010F": "410F%02X" % int(25 + 40),
            "0142": "4142%04X" % int(14100 + random.uniform(-100, 100)),
            "012F": "412F%02X" % int(0.63 * 255),
        }
        reply = replies.get(command, "?")
        self.pending_reply = (reply + "\r\r>").encode("ascii")

    def read_some(self) -> bytes:
        time.sleep(0.03)
        reply, self.pending_reply = self.pending_reply, b""
        return reply

    def close(self) -> None:
        pass


# ---------------------------------------------------------------------------
# ELM327 session
# ---------------------------------------------------------------------------

class Elm327Session:
    def __init__(self, transport, raw_log_writer, timeout_seconds: float):
        self.transport = transport
        self.raw_log_writer = raw_log_writer
        self.timeout_seconds = timeout_seconds

    def command(self, command: str) -> str:
        """Send one allowlisted command, return the reply text without the prompt."""
        if not is_allowed(command):
            raise ValueError("command refused by the allowlist: %r" % command)
        self.transport.write((command + "\r").encode("ascii"))
        deadline = time.monotonic() + self.timeout_seconds
        received = b""
        while time.monotonic() < deadline:
            chunk = self.transport.read_some()
            if chunk:
                received += chunk
                if b">" in received:
                    break
        text = received.decode("ascii", errors="replace")
        text = text.replace("\r", "\n").replace(">", "")
        lines = [line.strip() for line in text.split("\n") if line.strip()]
        if lines and lines[0].upper() == command.upper():  # echo, before ATE0
            lines = lines[1:]
        lines = [line for line in lines if line.upper() != "SEARCHING..."]
        reply = " ".join(lines)
        self.raw_log_writer.writerow([datetime.datetime.now().isoformat(timespec="milliseconds"),
                                      command, reply if received else "TIMEOUT"])
        if not received:
            return "TIMEOUT"
        return reply

    def initialise(self) -> dict:
        info = {}
        info["ATZ"] = self.command("ATZ")
        time.sleep(1.0)
        for setup_command in ("ATE0", "ATL0", "ATS0", "ATH0", "ATSP0"):
            info[setup_command] = self.command(setup_command)
        info["ATI"] = self.command("ATI")
        info["ATRV"] = self.command("ATRV")
        info["ATDPN"] = self.command("ATDPN")
        # Supported-PID bitmaps: 0100 covers 01..20; its last bit says whether 0120
        # exists, whose last bit says whether 0140 exists, and so on.
        supported = None
        for bitmap_pid in (0x00, 0x20, 0x40):
            request = "01%02X" % bitmap_pid
            reply = self.command(request).replace(" ", "")
            info[request] = reply
            decoded = decode_supported_pid_bitmap(reply, bitmap_pid + 1)
            if decoded is None:
                break
            supported = (supported or set()) | decoded
            if (bitmap_pid + 0x20) not in decoded:
                break
        info["supported"] = supported
        return info


# ---------------------------------------------------------------------------
# Polling thread
# ---------------------------------------------------------------------------

class Poller(threading.Thread):
    def __init__(self, make_transport, events: queue.Queue, raw_log_writer, timeout_seconds: float):
        super().__init__(daemon=True)
        self.make_transport = make_transport
        self.events = events
        self.raw_log_writer = raw_log_writer
        self.timeout_seconds = timeout_seconds
        self.stop_requested = threading.Event()

    def run(self):
        transport = None
        try:
            self.events.put(("state", "Connecting"))
            transport = self.make_transport()
            session = Elm327Session(transport, self.raw_log_writer, self.timeout_seconds)
            info = session.initialise()
            self.events.put(("info", info))
            supported = info["supported"]
            if supported is None:
                self.events.put(("state", "Error: no PID bitmap (0100 -> %s)" % info["0100"]))
                return
            polled = [definition for definition in PID_DEFINITIONS
                      if int(definition.pid_hex, 16) in supported]
            self.events.put(("state", "Connected, %d of %d PIDs supported" % (len(polled), len(PID_DEFINITIONS))))
            self.events.put(("polled", [definition.pid_hex for definition in polled]))
            error_count = 0
            while not self.stop_requested.is_set():
                for definition in polled:
                    if self.stop_requested.is_set():
                        break
                    reply = session.command("01" + definition.pid_hex).replace(" ", "")
                    value = decode_mode01_reply(reply, definition)
                    if value is None:
                        error_count += 1
                        self.events.put(("error", definition.pid_hex, reply, error_count))
                    else:
                        self.events.put(("sample", definition.pid_hex, value, time.monotonic()))
                if not polled:
                    time.sleep(0.5)
        except Exception as error:  # a spike: report everything on screen
            self.events.put(("state", "Error: %s" % error))
        finally:
            if transport is not None:
                transport.close()
            self.events.put(("disconnected", None))


# ---------------------------------------------------------------------------
# Window
# ---------------------------------------------------------------------------

STALE_AFTER_SECONDS = 2.0


class DemoWindow:
    def __init__(self, root: tkinter.Tk, make_transport, raw_log_writer, timeout_seconds: float, fullscreen: bool):
        self.root = root
        self.make_transport = make_transport
        self.raw_log_writer = raw_log_writer
        self.timeout_seconds = timeout_seconds
        self.events = queue.Queue()
        self.poller = None
        self.last_sample_time = {}
        self.value_labels = {}
        self.tile_frames = {}

        root.title("ELM327 touch demo (spike)")
        root.configure(bg="black")
        if fullscreen:
            root.attributes("-fullscreen", True)
        else:
            root.geometry("1280x720")

        self.status_label = tkinter.Label(root, text="Disconnected", fg="white", bg="black",
                                          font=("DejaVu Sans", 18), anchor="w")
        self.status_label.pack(fill="x", padx=16, pady=(12, 0))

        self.info_label = tkinter.Label(root, text="", fg="#9a9a9a", bg="black",
                                        font=("DejaVu Sans Mono", 12), anchor="w", justify="left")
        self.info_label.pack(fill="x", padx=16)

        grid = tkinter.Frame(root, bg="black")
        grid.pack(fill="both", expand=True, padx=16, pady=8)
        for column_index in range(4):
            grid.columnconfigure(column_index, weight=1, uniform="tile")
        for row_index in range(2):
            grid.rowconfigure(row_index, weight=1, uniform="tile")
        for index, definition in enumerate(PID_DEFINITIONS):
            frame = tkinter.Frame(grid, bg="#1e1e1e", highlightbackground="#3a3a3a", highlightthickness=2)
            frame.grid(row=index // 4, column=index % 4, sticky="nsew", padx=6, pady=6)
            tkinter.Label(frame, text=definition.name, fg="#bbbbbb", bg="#1e1e1e",
                          font=("DejaVu Sans", 14)).pack(pady=(10, 0))
            value_label = tkinter.Label(frame, text="--", fg="#666666", bg="#1e1e1e",
                                        font=("DejaVu Sans", 44, "bold"))
            value_label.pack(expand=True)
            tkinter.Label(frame, text=definition.unit, fg="#bbbbbb", bg="#1e1e1e",
                          font=("DejaVu Sans", 14)).pack(pady=(0, 10))
            self.value_labels[definition.pid_hex] = value_label
            self.tile_frames[definition.pid_hex] = frame

        buttons = tkinter.Frame(root, bg="black")
        buttons.pack(fill="x", padx=16, pady=(0, 12))
        # 10 mm touch targets on the 5-inch panel is 116 px; the buttons are taller than that.
        self.connect_button = tkinter.Button(buttons, text="Connect", command=self.toggle_connection,
                                             font=("DejaVu Sans", 20), height=2, width=14)
        self.connect_button.pack(side="left", padx=(0, 12))
        tkinter.Button(buttons, text="Quit", command=self.quit, font=("DejaVu Sans", 20),
                       height=2, width=10).pack(side="right")
        root.bind("<Escape>", lambda event: self.quit())

        self.root.after(100, self.drain_events)
        self.root.after(500, self.check_staleness)

    def toggle_connection(self):
        if self.poller is None or not self.poller.is_alive():
            self.poller = Poller(self.make_transport, self.events, self.raw_log_writer, self.timeout_seconds)
            self.poller.start()
            self.connect_button.configure(text="Disconnect")
        else:
            self.poller.stop_requested.set()
            self.connect_button.configure(text="Connect")

    def drain_events(self):
        while True:
            try:
                event = self.events.get_nowait()
            except queue.Empty:
                break
            kind = event[0]
            if kind == "state":
                self.status_label.configure(text=event[1])
            elif kind == "info":
                info = event[1]
                supported = info["supported"]
                supported_text = ("none" if not supported else
                                  " ".join("%02X" % pid for pid in sorted(supported)))
                bitmaps_text = "   ".join("%s: %s" % (request, info[request])
                                          for request in ("0100", "0120", "0140") if request in info)
                self.info_label.configure(text="ATI: %s   ATRV: %s   ATDPN: %s\n%s\nsupported PIDs: %s"
                                          % (info["ATI"], info["ATRV"], info["ATDPN"], bitmaps_text, supported_text))
            elif kind == "polled":
                for pid_hex, frame in self.tile_frames.items():
                    frame.configure(highlightbackground="#3a3a3a" if pid_hex in event[1] else "#7a2a2a")
            elif kind == "sample":
                _, pid_hex, value, sample_time = event
                self.last_sample_time[pid_hex] = sample_time
                text = "%.0f" % value if value >= 100 else "%.1f" % value
                self.value_labels[pid_hex].configure(text=text, fg="white")
            elif kind == "error":
                _, pid_hex, reply, error_count = event
                self.status_label.configure(text="Connected, %d bad replies, last %s -> %s" % (error_count, pid_hex, reply))
            elif kind == "disconnected":
                self.connect_button.configure(text="Connect")
                for label in self.value_labels.values():
                    label.configure(fg="#666666")
                if not self.status_label.cget("text").startswith("Error"):
                    self.status_label.configure(text="Disconnected")
        self.root.after(100, self.drain_events)

    def check_staleness(self):
        now = time.monotonic()
        for pid_hex, label in self.value_labels.items():
            last = self.last_sample_time.get(pid_hex)
            if last is not None and now - last > STALE_AFTER_SECONDS:
                label.configure(fg="#666666")
        self.root.after(500, self.check_staleness)

    def quit(self):
        if self.poller is not None:
            self.poller.stop_requested.set()
        self.root.after(200, self.root.destroy)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--address", help="Bluetooth address of the adapter (never commit it)")
    parser.add_argument("--name", help="part of the paired adapter's Bluetooth name, for example vLinker; "
                                       "the address is looked up through bluetoothctl")
    parser.add_argument("--channel", type=int, default=1, help="RFCOMM channel (default 1)")
    parser.add_argument("--fake", action="store_true", help="use a fake adapter instead of Bluetooth")
    parser.add_argument("--fullscreen", action="store_true")
    parser.add_argument("--timeout", type=float, default=3.0, help="per-command timeout in seconds")
    parser.add_argument("--log", default=None,
                        help="CSV of every request and reply (default local_recordings/spike_<timestamp>.csv)")
    arguments = parser.parse_args()

    if not arguments.fake and not arguments.address and not arguments.name:
        parser.error("one of --fake, --address or --name is required")

    log_path = arguments.log or ("local_recordings/spike_%s.csv"
                                 % datetime.datetime.now().strftime("%Y%m%d_%H%M%S"))
    import os
    os.makedirs(os.path.dirname(log_path) or ".", exist_ok=True)
    log_file = open(log_path, "w", newline="", encoding="ascii")
    raw_log_writer = csv.writer(log_file)
    raw_log_writer.writerow(["timestamp", "command", "reply"])

    if arguments.fake:
        make_transport = FakeTransport
    else:
        def make_transport():
            # Looked up at every connect, so pairing the adapter after the window
            # is already open still works.
            address = arguments.address or find_paired_device_address(arguments.name)
            if address is None:
                raise RuntimeError("no paired Bluetooth device whose name contains %r; pair it first" % arguments.name)
            # A paired but untrusted device can be refused on reconnect; trusting is idempotent.
            import subprocess
            try:
                subprocess.run(["bluetoothctl", "trust", address], capture_output=True, timeout=10)
            except (OSError, subprocess.TimeoutExpired):
                pass
            return RfcommTransport(address, arguments.channel, arguments.timeout)

    root = tkinter.Tk()
    DemoWindow(root, make_transport, raw_log_writer, arguments.timeout, arguments.fullscreen)
    try:
        root.mainloop()
    finally:
        log_file.close()
        print("raw log written to", log_path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
