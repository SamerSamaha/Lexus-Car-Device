#!/usr/bin/env python3
"""Log the Raspberry Pi's SoC temperature, throttling flags, Arm clock and input voltage.

One CSV row every interval (default 5 s), written under docs/measurements/thermal_power/, and a
summary with the verdict against the approved thresholds: fail on any under-voltage or throttle
flag or at 80 C or above; warn from 75 C, on the soft temperature limit, or below 4.75 V.

Runs on the Pi (needs vcgencmd). Standard library only. Design: docs/design/DN-015-thermal-logger.md.

Usage:
    log_thermal_power.py --label desk_idle --power-source "5V3A charger" --ambient-celsius 21 \
        --duration-seconds 1800
"""

from __future__ import annotations

import argparse
import csv
import datetime
import os
import re
import shutil
import statistics
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Dict, List, Optional, Sequence

EXIT_CODE_OK = 0
EXIT_CODE_COULD_NOT_RUN = 2

DEFAULT_OUTPUT_DIRECTORY = Path(__file__).resolve().parent.parent.parent / "docs" / "measurements" / "thermal_power"
DEFAULT_INTERVAL_SECONDS = 5.0

FAIL_TEMPERATURE_CELSIUS = 80.0
WARN_TEMPERATURE_CELSIUS = 75.0
WARN_INPUT_VOLTS = 4.75

FLAG_COLUMNS = (
    ("under_voltage_now", 0),
    ("arm_frequency_capped_now", 1),
    ("throttled_now", 2),
    ("soft_temperature_limit_now", 3),
    ("under_voltage_occurred", 16),
    ("arm_frequency_capped_occurred", 17),
    ("throttled_occurred", 18),
    ("soft_temperature_limit_occurred", 19),
)
FAIL_FLAG_BITS = (0, 1, 2, 16, 17, 18)
WARN_FLAG_BITS = (3, 19)

CSV_COLUMNS = ("timestamp", "seconds_since_boot", "temperature_celsius", "throttled_hex") + tuple(
    name for name, _bit in FLAG_COLUMNS
) + ("arm_clock_hz", "input_volts")

TEMPERATURE_PATTERN = re.compile(r"temp=([0-9]+(?:\.[0-9]+)?)'C")
THROTTLED_PATTERN = re.compile(r"throttled=0x([0-9A-Fa-f]+)")
CLOCK_PATTERN = re.compile(r"frequency\(\d+\)=(\d+)")
VOLTAGE_PATTERN = re.compile(r"volt\(\d+\)=([0-9]+(?:\.[0-9]+)?)V")

CommandRunner = Callable[[Sequence[str]], str]


class ParseError(ValueError):
    """The command printed something the parser does not understand."""


def parse_temperature(text: str) -> float:
    match = TEMPERATURE_PATTERN.search(text)
    if not match:
        raise ParseError(f"not a measure_temp output: {text!r}")
    return float(match.group(1))


def parse_throttled(text: str) -> Dict[str, int]:
    """Return the raw hex and the eight flag columns decoded from get_throttled."""
    match = THROTTLED_PATTERN.search(text)
    if not match:
        raise ParseError(f"not a get_throttled output: {text!r}")
    value = int(match.group(1), 16)
    decoded: Dict[str, int] = {"throttled_hex": "0x" + match.group(1).lower()}
    for name, bit in FLAG_COLUMNS:
        decoded[name] = 1 if value & (1 << bit) else 0
    return decoded


def parse_clock(text: str) -> int:
    match = CLOCK_PATTERN.search(text)
    if not match:
        raise ParseError(f"not a measure_clock output: {text!r}")
    return int(match.group(1))


def parse_voltage(text: str) -> float:
    match = VOLTAGE_PATTERN.search(text)
    if not match:
        raise ParseError(f"not a pmic_read_adc output: {text!r}")
    return float(match.group(1))


def seconds_since_boot(uptime_text: str) -> float:
    return float(uptime_text.split()[0])


def run_command(arguments: Sequence[str]) -> str:
    completed = subprocess.run(list(arguments), capture_output=True, text=True, timeout=10, check=False)
    if completed.returncode != 0:
        raise ParseError(f"{' '.join(arguments)} exited {completed.returncode}: {completed.stderr.strip()}")
    return completed.stdout.strip()


@dataclass
class Sample:
    timestamp: str
    seconds_since_boot: str = ""
    temperature_celsius: Optional[float] = None
    throttled: Dict[str, int] = field(default_factory=dict)
    arm_clock_hz: Optional[int] = None
    input_volts: Optional[float] = None
    errors: List[str] = field(default_factory=list)

    def as_row(self) -> Dict[str, str]:
        row: Dict[str, str] = {column: "" for column in CSV_COLUMNS}
        row["timestamp"] = self.timestamp
        row["seconds_since_boot"] = self.seconds_since_boot
        if self.temperature_celsius is not None:
            row["temperature_celsius"] = f"{self.temperature_celsius:.1f}"
        for key, value in self.throttled.items():
            row[key] = str(value)
        if self.arm_clock_hz is not None:
            row["arm_clock_hz"] = str(self.arm_clock_hz)
        if self.input_volts is not None:
            row["input_volts"] = f"{self.input_volts:.3f}"
        return row


def take_sample(runner: CommandRunner, read_uptime: Callable[[], str], now: Callable[[], str]) -> Sample:
    """One row; a failing command leaves its cell empty and records the error."""
    sample = Sample(timestamp=now())
    try:
        sample.seconds_since_boot = f"{seconds_since_boot(read_uptime()):.1f}"
    except (OSError, ValueError, IndexError) as error:
        sample.errors.append(f"uptime: {error}")
    for name, arguments, parser in (
        ("temperature", ("vcgencmd", "measure_temp"), parse_temperature),
        ("throttled", ("vcgencmd", "get_throttled"), parse_throttled),
        ("clock", ("vcgencmd", "measure_clock", "arm"), parse_clock),
        ("voltage", ("vcgencmd", "pmic_read_adc", "EXT5V_V"), parse_voltage),
    ):
        try:
            value = parser(runner(arguments))
        except (ParseError, OSError, subprocess.SubprocessError) as error:
            sample.errors.append(f"{name}: {error}")
            continue
        if name == "temperature":
            sample.temperature_celsius = value
        elif name == "throttled":
            sample.throttled = value
        elif name == "clock":
            sample.arm_clock_hz = value
        else:
            sample.input_volts = value
    return sample


@dataclass
class Summary:
    sample_count: int = 0
    minimum_celsius: Optional[float] = None
    median_celsius: Optional[float] = None
    maximum_celsius: Optional[float] = None
    samples_at_or_above_warn: int = 0
    samples_at_or_above_fail: int = 0
    flags_seen: Dict[str, str] = field(default_factory=dict)
    minimum_input_volts: Optional[float] = None
    voltage_readable: bool = False
    error_count: int = 0


def summarise(rows: Sequence[Dict[str, str]], error_count: int = 0) -> Summary:
    summary = Summary(sample_count=len(rows), error_count=error_count)
    temperatures = [float(row["temperature_celsius"]) for row in rows if row.get("temperature_celsius")]
    if temperatures:
        summary.minimum_celsius = min(temperatures)
        summary.median_celsius = statistics.median(temperatures)
        summary.maximum_celsius = max(temperatures)
        summary.samples_at_or_above_warn = sum(1 for value in temperatures if value >= WARN_TEMPERATURE_CELSIUS)
        summary.samples_at_or_above_fail = sum(1 for value in temperatures if value >= FAIL_TEMPERATURE_CELSIUS)
    for row in rows:
        for name, _bit in FLAG_COLUMNS:
            if row.get(name) == "1" and name not in summary.flags_seen:
                summary.flags_seen[name] = row.get("timestamp", "")
    volts = [float(row["input_volts"]) for row in rows if row.get("input_volts")]
    if volts:
        summary.voltage_readable = True
        summary.minimum_input_volts = min(volts)
    return summary


def verdict(summary: Summary) -> str:
    """pass, warn or fail against the thresholds of D-025."""
    fail_flag_names = {name for name, bit in FLAG_COLUMNS if bit in FAIL_FLAG_BITS}
    warn_flag_names = {name for name, bit in FLAG_COLUMNS if bit in WARN_FLAG_BITS}
    if any(name in fail_flag_names for name in summary.flags_seen):
        return "fail"
    if summary.maximum_celsius is not None and summary.maximum_celsius >= FAIL_TEMPERATURE_CELSIUS:
        return "fail"
    if any(name in warn_flag_names for name in summary.flags_seen):
        return "warn"
    if summary.maximum_celsius is not None and summary.maximum_celsius >= WARN_TEMPERATURE_CELSIUS:
        return "warn"
    if summary.voltage_readable and summary.minimum_input_volts is not None and summary.minimum_input_volts < WARN_INPUT_VOLTS:
        return "warn"
    return "pass"


def format_summary(summary: Summary, label: str) -> str:
    lines = [f"session {label}: {summary.sample_count} samples, {summary.error_count} command errors"]
    if summary.maximum_celsius is None:
        lines.append("temperature: no readings")
    else:
        lines.append(
            f"temperature C: min {summary.minimum_celsius:.1f}, median {summary.median_celsius:.1f}, "
            f"max {summary.maximum_celsius:.1f}; samples >= {WARN_TEMPERATURE_CELSIUS:.0f}: "
            f"{summary.samples_at_or_above_warn}; samples >= {FAIL_TEMPERATURE_CELSIUS:.0f}: "
            f"{summary.samples_at_or_above_fail}"
        )
    if summary.flags_seen:
        lines.append("flags seen: " + ", ".join(f"{name} (first at {when})" for name, when in summary.flags_seen.items()))
    else:
        lines.append("flags seen: none")
    if summary.voltage_readable:
        lines.append(f"input volts: min {summary.minimum_input_volts:.3f}")
    else:
        lines.append("input volts: not readable on this unit")
    lines.append(f"verdict: {verdict(summary)}")
    return "\n".join(lines)


def log_session(
    output_path: Path,
    label: str,
    power_source: str,
    ambient_celsius: Optional[float],
    interval_seconds: float,
    duration_seconds: Optional[float],
    runner: CommandRunner = run_command,
    read_uptime: Callable[[], str] = lambda: Path("/proc/uptime").read_text(encoding="ascii"),
    now: Callable[[], str] = lambda: datetime.datetime.now().isoformat(timespec="seconds"),
    sleep: Callable[[float], None] = time.sleep,
    monotonic: Callable[[], float] = time.monotonic,
) -> Summary:
    """Write rows until the duration passes or KeyboardInterrupt; return the summary."""
    output_path.parent.mkdir(parents=True, exist_ok=True)
    rows: List[Dict[str, str]] = []
    error_count = 0
    started = monotonic()
    with open(output_path, "w", newline="", encoding="ascii") as csv_file:
        csv_file.write(
            f"# label={label}; power_source={power_source}; ambient_celsius="
            f"{'' if ambient_celsius is None else ambient_celsius}; interval_seconds={interval_seconds}; "
            f"thresholds: fail flags bits 0,1,2,16,17,18 or >= {FAIL_TEMPERATURE_CELSIUS:.0f} C; "
            f"warn >= {WARN_TEMPERATURE_CELSIUS:.0f} C, bits 3,19, or < {WARN_INPUT_VOLTS} V\n"
        )
        writer = csv.DictWriter(csv_file, fieldnames=CSV_COLUMNS)
        writer.writeheader()
        try:
            while True:
                sample = take_sample(runner, read_uptime, now)
                error_count += len(sample.errors)
                for error in sample.errors:
                    print(f"warning: {error}", file=sys.stderr)
                row = sample.as_row()
                rows.append(row)
                writer.writerow(row)
                csv_file.flush()
                elapsed = monotonic() - started
                if duration_seconds is not None and elapsed + interval_seconds >= duration_seconds:
                    break
                sleep(interval_seconds)
        except KeyboardInterrupt:
            pass
    summary = summarise(rows, error_count)
    output_path.with_suffix(".summary.txt").write_text(format_summary(summary, label) + "\n", encoding="ascii")
    return summary


def parse_arguments(argument_list: Optional[Sequence[str]]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--label", required=True, help="session label, used in the file name")
    parser.add_argument("--power-source", required=True, help="for example '5V3A charger' or 'power bank A1383'")
    parser.add_argument("--ambient-celsius", type=float, default=None)
    parser.add_argument("--interval-seconds", type=float, default=DEFAULT_INTERVAL_SECONDS)
    parser.add_argument("--duration-seconds", type=float, default=None, help="stop after this long (default: until Ctrl-C)")
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIRECTORY)
    return parser.parse_args(argument_list)


def main(argument_list: Optional[Sequence[str]] = None) -> int:
    arguments = parse_arguments(argument_list)
    if shutil.which("vcgencmd") is None:
        print("log_thermal_power: vcgencmd not found; this logger runs on the Raspberry Pi", file=sys.stderr)
        return EXIT_CODE_COULD_NOT_RUN
    safe_label = re.sub(r"[^A-Za-z0-9_-]+", "_", arguments.label)
    file_name = datetime.datetime.now().strftime("%Y-%m-%d_%H%M%S") + f"_{safe_label}.csv"
    output_path = arguments.output_dir / file_name
    summary = log_session(
        output_path,
        arguments.label,
        arguments.power_source,
        arguments.ambient_celsius,
        arguments.interval_seconds,
        arguments.duration_seconds,
    )
    print(format_summary(summary, arguments.label))
    print(f"written: {output_path}")
    return EXIT_CODE_OK


if __name__ == "__main__":
    sys.exit(main())
