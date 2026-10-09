#!/usr/bin/env python3
"""Tests for tools/measure/log_thermal_power.py: parsing, summary, verdict and the logging loop."""

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "measure"))

import log_thermal_power as logger  # noqa: E402

# Command outputs as recorded on the unit on 2026-10-05.
TEMPERATURE_TEXT = "temp=51.0'C"
THROTTLED_TEXT = "throttled=0x0"
CLOCK_TEXT = "frequency(48)=1500000000"
VOLTAGE_TEXT = "EXT5V_V volt(24)=5.24500000V"


def row(temperature="51.0", flags=None, volts="5.245", timestamp="t"):
    result = {column: "" for column in logger.CSV_COLUMNS}
    result["timestamp"] = timestamp
    result["temperature_celsius"] = temperature
    result["input_volts"] = volts
    for name in flags or ():
        result[name] = "1"
    return result


class ParsingTest(unittest.TestCase):
    def test_recorded_outputs_parse(self):
        self.assertEqual(logger.parse_temperature(TEMPERATURE_TEXT), 51.0)
        self.assertEqual(logger.parse_clock(CLOCK_TEXT), 1500000000)
        self.assertAlmostEqual(logger.parse_voltage(VOLTAGE_TEXT), 5.245)
        decoded = logger.parse_throttled(THROTTLED_TEXT)
        self.assertEqual(decoded["throttled_hex"], "0x0")
        self.assertTrue(all(decoded[name] == 0 for name, _bit in logger.FLAG_COLUMNS))

    def test_throttled_bits_map_to_their_columns(self):
        decoded = logger.parse_throttled("throttled=0x50005")
        self.assertEqual(decoded["under_voltage_now"], 1)
        self.assertEqual(decoded["throttled_now"], 1)
        self.assertEqual(decoded["under_voltage_occurred"], 1)
        self.assertEqual(decoded["throttled_occurred"], 1)
        self.assertEqual(decoded["arm_frequency_capped_now"], 0)
        self.assertEqual(decoded["soft_temperature_limit_occurred"], 0)
        for name, bit in logger.FLAG_COLUMNS:
            with self.subTest(bit=bit):
                self.assertEqual(logger.parse_throttled(f"throttled=0x{1 << bit:x}")[name], 1)

    def test_malformed_outputs_raise(self):
        for function, text in (
            (logger.parse_temperature, "temp=hot"),
            (logger.parse_throttled, "throttled=zz"),
            (logger.parse_clock, "frequency=fast"),
            (logger.parse_voltage, "EXT5V_V volt(24)=unknown"),
        ):
            with self.subTest(function=function.__name__):
                with self.assertRaises(logger.ParseError):
                    function(text)

    def test_seconds_since_boot(self):
        self.assertEqual(logger.seconds_since_boot("1234.56 4000.00\n"), 1234.56)


class SummaryAndVerdictTest(unittest.TestCase):
    def test_pass_session(self):
        summary = logger.summarise([row("51.0"), row("59.3"), row("55.1")])
        self.assertEqual(summary.sample_count, 3)
        self.assertEqual(summary.minimum_celsius, 51.0)
        self.assertEqual(summary.median_celsius, 55.1)
        self.assertEqual(summary.maximum_celsius, 59.3)
        self.assertEqual(summary.samples_at_or_above_warn, 0)
        self.assertEqual(summary.flags_seen, {})
        self.assertTrue(summary.voltage_readable)
        self.assertEqual(logger.verdict(summary), "pass")

    def test_warn_at_seventy_six_degrees(self):
        summary = logger.summarise([row("70.0"), row("76.0")])
        self.assertEqual(summary.samples_at_or_above_warn, 1)
        self.assertEqual(logger.verdict(summary), "warn")

    def test_fail_at_eighty_degrees_and_on_an_under_voltage_flag(self):
        self.assertEqual(logger.verdict(logger.summarise([row("80.0")])), "fail")
        summary = logger.summarise([row("50.0"), row("50.0", flags=["under_voltage_occurred"], timestamp="second")])
        self.assertEqual(summary.flags_seen, {"under_voltage_occurred": "second"})
        self.assertEqual(logger.verdict(summary), "fail")

    def test_soft_limit_flag_and_low_voltage_warn(self):
        self.assertEqual(logger.verdict(logger.summarise([row("50.0", flags=["soft_temperature_limit_now"])])), "warn")
        self.assertEqual(logger.verdict(logger.summarise([row("50.0", volts="4.70")])), "warn")
        self.assertEqual(logger.summarise([row("50.0", volts="4.70")]).minimum_input_volts, 4.70)

    def test_unreadable_voltage_is_not_a_warning_and_empty_rows_are_handled(self):
        summary = logger.summarise([row("50.0", volts="")])
        self.assertFalse(summary.voltage_readable)
        self.assertEqual(logger.verdict(summary), "pass")
        empty = logger.summarise([])
        self.assertEqual(empty.sample_count, 0)
        self.assertEqual(logger.verdict(empty), "pass")
        self.assertIn("no readings", logger.format_summary(empty, "x"))


class LoggingLoopTest(unittest.TestCase):
    def test_rows_are_written_with_the_header_and_errors_are_counted(self):
        outputs = {
            ("vcgencmd", "measure_temp"): [TEMPERATURE_TEXT, "temp=52.5'C", "garbage"],
            ("vcgencmd", "get_throttled"): [THROTTLED_TEXT] * 3,
            ("vcgencmd", "measure_clock", "arm"): [CLOCK_TEXT] * 3,
            ("vcgencmd", "pmic_read_adc", "EXT5V_V"): [VOLTAGE_TEXT] * 3,
        }
        calls = {key: 0 for key in outputs}

        def runner(arguments):
            key = tuple(arguments)
            text = outputs[key][calls[key]]
            calls[key] += 1
            return text

        clock = {"now": 0.0}

        def monotonic():
            return clock["now"]

        def sleep(seconds):
            clock["now"] += seconds

        with tempfile.TemporaryDirectory() as directory:
            output_path = Path(directory) / "session.csv"
            summary = logger.log_session(
                output_path, "unit", "fake", 21.0, 5.0, 15.0,
                runner=runner,
                read_uptime=lambda: "100.0 200.0\n",
                now=lambda: f"t{clock['now']:.0f}",
                sleep=sleep,
                monotonic=monotonic,
            )
            text = output_path.read_text(encoding="ascii").splitlines()
            summary_text = output_path.with_suffix(".summary.txt").read_text(encoding="ascii")

        self.assertTrue(text[0].startswith("# label=unit; power_source=fake; ambient_celsius=21.0"))
        self.assertEqual(text[1].split(",")[:4], ["timestamp", "seconds_since_boot", "temperature_celsius", "throttled_hex"])
        self.assertEqual(len(text), 2 + 3)
        self.assertEqual(text[2].split(",")[:4], ["t0", "100.0", "51.0", "0x0"])
        self.assertEqual(text[4].split(",")[2], "")  # the garbage reading leaves the cell empty
        self.assertEqual(summary.sample_count, 3)
        self.assertEqual(summary.error_count, 1)
        self.assertEqual(summary.maximum_celsius, 52.5)
        self.assertIn("verdict: pass", summary_text)
        self.assertIn("1 command errors", summary_text)

    def test_main_refuses_to_run_without_vcgencmd(self):
        original = logger.shutil.which
        logger.shutil.which = lambda name: None
        try:
            exit_code = logger.main(["--label", "x", "--power-source", "y"])
        finally:
            logger.shutil.which = original
        self.assertEqual(exit_code, logger.EXIT_CODE_COULD_NOT_RUN)


if __name__ == "__main__":
    unittest.main()
