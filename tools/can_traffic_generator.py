#!/usr/bin/env python3
"""Writes simulated-vehicle CAN traffic to a SocketCAN interface (DN-028, LHU-028).

For manual runs and the Pi's vcan tests: the head unit's CAN source reads vcan0, and this
script plays the invented vehicle of dbc/simulated_vehicle.dbc on it, with the eight project
signals moving slowly. Standard library only (socket.AF_CAN), so it runs on the Pi without the
test-only environment. This is test tooling: the head unit itself never transmits (REQ-001).

    python3 tools/can_traffic_generator.py --interface vcan0 --rate-hz 50 --duration-seconds 60

The signal layouts below are a copy of the DBC; tools/test_can_traffic_generator.py fails if
they ever differ from dbc/simulated_vehicle.dbc.
"""

from __future__ import annotations

import argparse
import math
import socket
import struct
import sys
import time
from dataclasses import dataclass
from typing import Dict, List, Optional, Sequence

CAN_FRAME_FORMAT = "=IB3x8s"


@dataclass(frozen=True)
class SignalLayout:
    name: str
    start_bit: int
    length: int
    little_endian: bool
    signed: bool
    scale: float
    offset: float


@dataclass(frozen=True)
class MessageLayout:
    identifier: int
    length: int
    signals: List[SignalLayout]


MESSAGES: List[MessageLayout] = [
    MessageLayout(
        0x100,
        8,
        [
            SignalLayout("EngineSpeed", 0, 16, True, False, 0.25, 0.0),
            SignalLayout("CoolantTemperature", 16, 8, True, False, 1.0, -40.0),
            SignalLayout("EngineLoad", 24, 8, True, False, 0.392156862745098, 0.0),
            SignalLayout("ThrottlePosition", 32, 10, True, False, 0.1, 0.0),
            SignalLayout("IntakeAirTemperature", 42, 8, True, True, 1.0, 0.0),
        ],
    ),
    MessageLayout(0x200, 8, [SignalLayout("VehicleSpeed", 7, 16, False, False, 0.01, 0.0)]),
    MessageLayout(
        0x300,
        4,
        [
            SignalLayout("ControlModuleVoltage", 0, 16, True, False, 0.001, 0.0),
            SignalLayout("FuelLevel", 16, 8, True, False, 0.392156862745098, 0.0),
        ],
    ),
]


def bit_positions(signal: SignalLayout) -> List[int]:
    """Bit positions, most significant first, in the DBC numbering (bit n is bit n % 8 of byte n // 8)."""
    if signal.little_endian:
        return [signal.start_bit + index for index in range(signal.length - 1, -1, -1)]
    positions = []
    position = signal.start_bit
    for _ in range(signal.length):
        positions.append(position)
        position = position + 15 if position % 8 == 0 else position - 1
    return positions


def encode(message: MessageLayout, values: Dict[str, float]) -> bytes:
    data = bytearray(message.length)
    for signal in message.signals:
        if signal.name not in values:
            continue
        raw = round((values[signal.name] - signal.offset) / signal.scale)
        raw &= (1 << signal.length) - 1  # two's complement for negative signed values
        positions = bit_positions(signal)
        for index, position in enumerate(positions):
            if (raw >> (len(positions) - 1 - index)) & 1:
                data[position // 8] |= 1 << (position % 8)
    return bytes(data)


def vehicle_values(elapsed_seconds: float) -> Dict[str, float]:
    wave = math.sin(elapsed_seconds / 5.0)
    return {
        "EngineSpeed": 800.0 + 600.0 * (wave + 1.0),
        "CoolantTemperature": 88.0,
        "EngineLoad": 20.0 + 10.0 * (wave + 1.0),
        "ThrottlePosition": 12.0 + 8.0 * (wave + 1.0),
        "IntakeAirTemperature": 25.0,
        "VehicleSpeed": 40.0 + 30.0 * wave,
        "ControlModuleVoltage": 14.1,
        "FuelLevel": 62.0,
    }


def frame_bytes(identifier: int, data: bytes) -> bytes:
    return struct.pack(CAN_FRAME_FORMAT, identifier, len(data), data.ljust(8, b"\x00"))


def run(interface: str, rate_hz: float, duration_seconds: float) -> int:
    with socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW) as can_socket:
        can_socket.bind((interface,))
        start = time.monotonic()
        frame_count = 0
        while time.monotonic() - start < duration_seconds:
            values = vehicle_values(time.monotonic() - start)
            for message in MESSAGES:
                can_socket.send(frame_bytes(message.identifier, encode(message, values)))
                frame_count += 1
            time.sleep(1.0 / rate_hz)
    print(f"can_traffic_generator: {frame_count} frames written to {interface}")
    return frame_count


def main(argument_list: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Write simulated-vehicle traffic to a CAN interface.")
    parser.add_argument("--interface", default="vcan0")
    parser.add_argument("--rate-hz", type=float, default=50.0)
    parser.add_argument("--duration-seconds", type=float, default=60.0)
    arguments = parser.parse_args(argument_list)
    try:
        run(arguments.interface, arguments.rate_hz, arguments.duration_seconds)
    except OSError as error:
        print(f"can_traffic_generator: error on {arguments.interface}: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
