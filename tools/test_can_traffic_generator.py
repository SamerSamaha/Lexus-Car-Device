#!/usr/bin/env python3
"""Tests for can_traffic_generator.py: its layouts equal the DBC, and its encoding is right."""

import re
import unittest
from pathlib import Path

import can_traffic_generator

DBC_PATH = Path(__file__).resolve().parent.parent / "dbc" / "simulated_vehicle.dbc"
SIGNAL_PATTERN = re.compile(
    r"^\s*SG_ (\w+) : (\d+)\|(\d+)@([01])([+-]) \(([^,]+),([^)]+)\)", re.MULTILINE
)
MESSAGE_PATTERN = re.compile(r"^BO_ (\d+) \w+: (\d+)", re.MULTILINE)


class CanTrafficGeneratorTest(unittest.TestCase):
    def test_layouts_equal_the_dbc(self):
        text = DBC_PATH.read_text(encoding="ascii")
        dbc_signals = {
            match.group(1): (
                int(match.group(2)),
                int(match.group(3)),
                match.group(4) == "1",
                match.group(5) == "-",
                float(match.group(6)),
                float(match.group(7)),
            )
            for match in SIGNAL_PATTERN.finditer(text)
        }
        dbc_messages = {int(match.group(1)): int(match.group(2)) for match in MESSAGE_PATTERN.finditer(text)}
        for message in can_traffic_generator.MESSAGES:
            self.assertEqual(dbc_messages[message.identifier], message.length)
            for signal in message.signals:
                self.assertEqual(
                    dbc_signals[signal.name],
                    (signal.start_bit, signal.length, signal.little_endian, signal.signed, signal.scale, signal.offset),
                    signal.name,
                )
        generated = {signal.name for message in can_traffic_generator.MESSAGES for signal in message.signals}
        self.assertEqual(len(generated), 8)

    def test_hand_computed_encodings(self):
        engine, motion, power = can_traffic_generator.MESSAGES
        self.assertEqual(can_traffic_generator.encode(motion, {"VehicleSpeed": 100.0})[:2], bytes([0x27, 0x10]))
        self.assertEqual(can_traffic_generator.encode(engine, {"EngineSpeed": 2500.0})[:2], bytes([0x10, 0x27]))
        self.assertEqual(
            can_traffic_generator.encode(engine, {"IntakeAirTemperature": -1.0})[5:7], bytes([0xFC, 0x03])
        )
        self.assertEqual(can_traffic_generator.encode(power, {"ControlModuleVoltage": 12.52})[:2], bytes([0xE8, 0x30]))

    def test_frame_layout_is_the_kernel_can_frame(self):
        frame = can_traffic_generator.frame_bytes(0x300, bytes([1, 2, 3, 4]))
        self.assertEqual(len(frame), 16)
        self.assertEqual(frame[4], 4)
        self.assertEqual(frame[8:12], bytes([1, 2, 3, 4]))


if __name__ == "__main__":
    unittest.main()
