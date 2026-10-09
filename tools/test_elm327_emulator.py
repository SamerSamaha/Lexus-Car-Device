#!/usr/bin/env python3
"""Tests of the emulator's command handler, platform independent.

Run: python3 -m unittest discover --start-directory tools --pattern "test_*.py"
"""

import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "elm327_emulator"))

import elm327_emulator  # noqa: E402
from elm327_emulator import (  # noqa: E402
    Elm327Emulator,
    VehicleModel,
    dtc_bytes,
    multi_frame_lines,
    supported_pid_bitmap,
)


def lines_of(reply):
    """Split a reply into its text lines without echo handling or the prompt."""
    body = reply[: -len(elm327_emulator.PROMPT)] if reply.endswith(elm327_emulator.PROMPT) else reply
    return [line for line in body.replace("\r\n", "\r").split("\r") if line]


class ConfiguredEmulator(Elm327Emulator):
    """Echo, spaces and linefeeds off, as the source configures the adapter."""

    def __init__(self, **keyword_arguments):
        super().__init__(**keyword_arguments)
        for setup in ("ATE0", "ATL0", "ATS0"):
            self.handle_line(setup, 0.0)


class AtCommandTest(unittest.TestCase):
    def test_reset_identifies_and_turns_echo_back_on(self):
        emulator = ConfiguredEmulator()
        self.assertEqual(lines_of(emulator.handle_line("ATZ", 0.0)), ["ELM327 v1.5"])
        self.assertTrue(emulator.echo)
        self.assertEqual(lines_of(emulator.handle_line("ATI", 0.0)), ["ATI", "ELM327 v1.5"])

    def test_setup_commands_answer_ok_and_change_state(self):
        emulator = Elm327Emulator()
        self.assertEqual(lines_of(emulator.handle_line("ATE0", 0.0)), ["ATE0", "OK"])
        self.assertFalse(emulator.echo)
        self.assertEqual(lines_of(emulator.handle_line("ATS0", 0.0)), ["OK"])
        self.assertFalse(emulator.spaces)
        self.assertEqual(lines_of(emulator.handle_line("ATL0", 0.0)), ["OK"])
        self.assertEqual(lines_of(emulator.handle_line("ATH0", 0.0)), ["OK"])
        self.assertEqual(lines_of(emulator.handle_line("ATSP0", 0.0)), ["OK"])
        self.assertEqual(lines_of(emulator.handle_line("ATRV", 0.0)), ["14.1V"])
        self.assertEqual(lines_of(emulator.handle_line("ATDPN", 0.0)), ["A6"])

    def test_unknown_at_command_answers_question_mark_and_is_counted(self):
        emulator = ConfiguredEmulator()
        self.assertEqual(lines_of(emulator.handle_line("ATSH7E0", 0.0)), ["?"])
        self.assertEqual(emulator.statistics.forbidden_requests, 1)

    def test_spaces_on_puts_spaces_between_bytes(self):
        emulator = Elm327Emulator()
        emulator.handle_line("ATE0", 0.0)
        reply = lines_of(emulator.handle_line("010D", 0.0))
        self.assertRegex(reply[0], r"^41 0D [0-9A-F]{2}$")

    def test_every_reply_ends_with_the_prompt(self):
        emulator = Elm327Emulator()
        self.assertTrue(emulator.handle_line("ATZ", 0.0).endswith("\r\r>"))
        self.assertTrue(emulator.handle_line("010D", 0.0).endswith("\r\r>"))


class Mode01Test(unittest.TestCase):
    def setUp(self):
        self.emulator = ConfiguredEmulator()
        self.model = VehicleModel()

    def test_bitmaps_follow_the_profile_with_the_chain_bit(self):
        first = lines_of(self.emulator.handle_line("0100", 0.0))[0]
        self.assertTrue(first.startswith("4100"))
        bitmap = int(first[4:], 16)
        self.assertTrue(bitmap & (1 << 31))  # PID 0x01
        self.assertTrue(bitmap & (1 << (32 - 0x0D)))  # vehicle speed
        self.assertTrue(bitmap & 1)  # 0x20 chain
        second = lines_of(self.emulator.handle_line("0120", 0.0))[0]
        self.assertTrue(int(second[4:], 16) & (1 << (32 - 0x0F)))  # 0x2F fuel level
        third = lines_of(self.emulator.handle_line("0140", 0.0))[0]
        self.assertTrue(int(third[4:], 16) & (1 << (32 - 0x02)))  # 0x42 voltage
        self.assertFalse(int(third[4:], 16) & 1)  # no 0x60 chain

    def test_no_fuel_level_profile_drops_pid_2f(self):
        emulator = ConfiguredEmulator(profile="no-fuel-level")
        second = lines_of(emulator.handle_line("0120", 0.0))[0]
        self.assertFalse(int(second[4:], 16) & (1 << (32 - 0x0F)))
        self.assertEqual(lines_of(emulator.handle_line("012F", 0.0)), ["NO DATA"])

    def test_each_pid_encodes_the_inverse_of_the_decoder_formula(self):
        seconds = 12.5
        self.emulator.set_time(seconds)
        expectations = {
            "010C": round(self.model.engine_rpm(seconds) * 4),
            "010D": round(self.model.vehicle_speed_kmh(seconds)),
            "0105": round(self.model.coolant_celsius(seconds)) + 40,
            "0104": round(self.model.engine_load_percent(seconds) * 255 / 100),
            "0111": round(self.model.throttle_percent(seconds) * 255 / 100),
            "010F": round(self.model.intake_air_celsius(seconds)) + 40,
            "0142": round(self.model.control_module_volts(seconds) * 1000),
            "012F": round(self.model.fuel_level_percent(seconds) * 255 / 100),
        }
        for command, expected_raw in expectations.items():
            with self.subTest(command=command):
                line = lines_of(self.emulator.handle_line(command, seconds))[0]
                self.assertEqual(line[:4], "41" + command[2:])
                self.assertEqual(int(line[4:], 16), expected_raw)

    def test_unsupported_pid_answers_no_data(self):
        self.assertEqual(lines_of(self.emulator.handle_line("0102", 0.0)), ["NO DATA"])

    def test_bitmap_beyond_the_chain_answers_no_data(self):
        self.assertEqual(lines_of(self.emulator.handle_line("0160", 0.0)), ["NO DATA"])


class OtherModesTest(unittest.TestCase):
    def test_mode03_with_zero_two_and_six_codes(self):
        self.assertEqual(lines_of(ConfiguredEmulator().handle_line("03", 0.0)), ["4300"])
        two = ConfiguredEmulator(trouble_codes=["P0133", "P0420"])
        self.assertEqual(lines_of(two.handle_line("03", 0.0)), ["430201330420"])
        six_codes = ["P0133", "P0420", "C1234", "B0001", "U0100", "P2002"]
        six = ConfiguredEmulator(trouble_codes=six_codes)
        lines = lines_of(six.handle_line("03", 0.0))
        self.assertEqual(lines[0], "00E")
        self.assertTrue(lines[1].startswith("0:4306"))
        payload = "".join(line.split(":", 1)[1] for line in lines[1:])
        self.assertEqual(payload[:4], "4306")
        self.assertEqual(payload[4:], "".join(dtc_bytes(code).hex().upper() for code in six_codes))

    def test_mode09_vin_is_multi_frame_and_decodes_back(self):
        emulator = ConfiguredEmulator(vin="1M8GDM9AXKP042788")
        lines = lines_of(emulator.handle_line("0902", 0.0))
        self.assertEqual(lines[0], "014")
        payload = bytes.fromhex("".join(line.split(":", 1)[1] for line in lines[1:]))
        self.assertEqual(payload[:3], bytes([0x49, 0x02, 0x01]))
        self.assertEqual(payload[3:].decode("ascii"), "1M8GDM9AXKP042788")
        self.assertEqual(lines_of(emulator.handle_line("0900", 0.0))[0][:4], "4900")

    def test_other_modes_answer_no_data_and_are_counted_as_forbidden(self):
        emulator = ConfiguredEmulator()
        self.assertEqual(lines_of(emulator.handle_line("04", 0.0)), ["NO DATA"])
        self.assertEqual(lines_of(emulator.handle_line("0200", 0.0)), ["NO DATA"])
        self.assertEqual(lines_of(emulator.handle_line("2201", 0.0)), ["NO DATA"])
        self.assertEqual(emulator.statistics.forbidden_requests, 3)
        self.assertEqual(emulator.statistics.per_command["04"], 1)

    def test_garbage_answers_question_mark(self):
        emulator = ConfiguredEmulator()
        self.assertEqual(lines_of(emulator.handle_line("hello", 0.0)), ["?"])
        self.assertEqual(lines_of(emulator.handle_line("010", 0.0)), ["?"])
        self.assertEqual(emulator.statistics.unknown_commands, 2)


class FaultInjectionTest(unittest.TestCase):
    def setUp(self):
        self.emulator = ConfiguredEmulator()

    def test_silence_suppresses_replies_until_the_time_passes(self):
        self.assertEqual(self.emulator.apply_control("silence 2", 10.0), "OK")
        self.assertIsNone(self.emulator.handle_line("010D", 11.0))
        self.assertIsNotNone(self.emulator.handle_line("010D", 12.5))

    def test_stale_pid_answers_no_data_until_unstale(self):
        self.assertEqual(self.emulator.apply_control("stale 0D", 0.0), "OK")
        self.assertEqual(lines_of(self.emulator.handle_line("010D", 0.0)), ["NO DATA"])
        self.assertTrue(lines_of(self.emulator.handle_line("010C", 0.0))[0].startswith("410C"))
        self.assertEqual(self.emulator.apply_control("unstale 0D", 0.0), "OK")
        self.assertTrue(lines_of(self.emulator.handle_line("010D", 0.0))[0].startswith("410D"))

    def test_corrupt_garbles_the_next_n_data_replies(self):
        self.assertEqual(self.emulator.apply_control("corrupt 2", 0.0), "OK")
        first = lines_of(self.emulator.handle_line("010C", 0.0))[0]
        second = lines_of(self.emulator.handle_line("010C", 0.0))[0]
        third = lines_of(self.emulator.handle_line("010C", 0.0))[0]
        hex_digits = set("0123456789ABCDEF")
        self.assertFalse(set(first) <= hex_digits)
        self.assertFalse(set(second) <= hex_digits)
        self.assertTrue(set(third) <= hex_digits)
        self.assertEqual(self.emulator.statistics.corrupted_replies, 2)

    def test_ignition_off_answers_unable_to_connect_for_obd_requests_only(self):
        self.assertEqual(self.emulator.apply_control("ignition off", 0.0), "OK")
        self.assertEqual(lines_of(self.emulator.handle_line("010D", 0.0)), ["UNABLE TO CONNECT"])
        self.assertEqual(lines_of(self.emulator.handle_line("0100", 0.0)), ["UNABLE TO CONNECT"])
        self.assertEqual(lines_of(self.emulator.handle_line("ATRV", 0.0)), ["14.1V"])
        self.assertEqual(self.emulator.apply_control("ignition on", 0.0), "OK")
        self.assertTrue(lines_of(self.emulator.handle_line("010D", 0.0))[0].startswith("410D"))

    def test_delay_stats_exit_and_errors(self):
        self.assertEqual(self.emulator.apply_control("delay 250", 0.0), "OK")
        self.assertAlmostEqual(self.emulator.delay_seconds, 0.25)
        self.emulator.handle_line("010D", 0.0)
        statistics = json.loads(self.emulator.apply_control("stats", 0.0))
        self.assertEqual(statistics["requests"], 4)  # three setup commands and one request
        self.assertEqual(statistics["forbidden_requests"], 0)
        self.assertEqual(self.emulator.apply_control("exit", 0.0), "OK exit")
        self.assertTrue(self.emulator.apply_control("bogus", 0.0).startswith("ERR"))
        self.assertTrue(self.emulator.apply_control("stale", 0.0).startswith("ERR"))
        self.assertTrue(self.emulator.apply_control("", 0.0).startswith("ERR"))


class HelperTest(unittest.TestCase):
    def test_bitmap_bit_positions(self):
        self.assertEqual(supported_pid_bitmap([0x01], 0x00), bytes.fromhex("80000000"))
        self.assertEqual(supported_pid_bitmap([0x20], 0x00), bytes.fromhex("00000001"))
        self.assertEqual(supported_pid_bitmap([0x21], 0x00), bytes.fromhex("00000001"))
        self.assertEqual(supported_pid_bitmap([0x21], 0x20), bytes.fromhex("80000000"))

    def test_dtc_bytes_cover_the_four_letters(self):
        self.assertEqual(dtc_bytes("P0133"), bytes.fromhex("0133"))
        self.assertEqual(dtc_bytes("C1234"), bytes.fromhex("5234"))
        self.assertEqual(dtc_bytes("B0001"), bytes.fromhex("8001"))
        self.assertEqual(dtc_bytes("U0100"), bytes.fromhex("C100"))
        with self.assertRaises(ValueError):
            dtc_bytes("X0001")

    def test_multi_frame_lines_split_six_then_seven_bytes(self):
        lines = multi_frame_lines(bytes(range(20)))
        self.assertEqual(lines[0], "014")
        self.assertEqual(lines[1], "0:000102030405")
        self.assertEqual(lines[2], "1:060708090A0B0C")
        self.assertEqual(lines[3], "2:0D0E0F10111213")
        self.assertEqual(multi_frame_lines(bytes([0x43, 0x00])), ["4300"])


if __name__ == "__main__":
    unittest.main()
