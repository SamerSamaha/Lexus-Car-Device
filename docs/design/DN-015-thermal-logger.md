# DN-015: Thermal and power logger

| | |
|---|---|
| Ticket | LHU-015 |
| Requirements | none (measurement tool, D-013; thresholds D-025) |
| Author | implementer (build-out form, D-049) |
| Status | Implemented |
| Form | Light (one page: problem, public interface, failure cases, test plan) |
| Draft written | 2026-10-08, about 10 minutes |
| Design review | after merge, by the repository owner (D-049) |
| Approved | 2026-10-08 |

## 1. Problem

Power and heat are measured risks, not blockers (D-013): every Pi bring-up session and every on-car session logs the SoC temperature, the firmware's throttling and under-voltage flags, the Arm clock and the input voltage every 5 s to a CSV, and judges the session against the approved thresholds (D-025). `tools/measure/log_thermal_power.py` is that logger. It runs on the Pi only (it calls `vcgencmd`); its parsing and the verdict are pure functions tested at the desk against recorded command outputs.

## 2. Public interface

Command line: `log_thermal_power.py --label LABEL --power-source SOURCE [--ambient-celsius C] [--interval-seconds 5] [--duration-seconds N] [--output-dir docs/measurements/thermal_power]`. Stops after the duration or on Ctrl-C; prints the summary either way.

Each CSV row: `timestamp` (ISO 8601 wall clock), `seconds_since_boot` (from `/proc/uptime`), `temperature_celsius` (`vcgencmd measure_temp`), `throttled_hex` (`vcgencmd get_throttled` raw), eight flag columns `under_voltage_now`, `arm_frequency_capped_now`, `throttled_now`, `soft_temperature_limit_now`, `under_voltage_occurred`, `arm_frequency_capped_occurred`, `throttled_occurred`, `soft_temperature_limit_occurred` (bits 0, 1, 2, 3, 16, 17, 18, 19), `arm_clock_hz` (`vcgencmd measure_clock arm`), `input_volts` (`vcgencmd pmic_read_adc EXT5V_V`, empty if unreadable). A header comment line records the label, power source, ambient temperature, interval and the thresholds.

Summary printed and written beside the CSV as `<name>.summary.txt`: sample count, minimum, median and maximum temperature, count of samples at or above 75 °C and at or above 80 °C, every flag seen with its first time, minimum input voltage, and the verdict: **fail** on any under-voltage or throttle flag (bits 0, 1, 2, 16, 17, 18) or 80 °C or above; **warn** from 75 °C, on the soft temperature limit (bits 3, 19) or below 4.75 V; else **pass**.

Pure functions, each tested: `parse_temperature`, `parse_throttled` (hex to the eight flags), `parse_clock`, `parse_voltage`, `summarise(rows)`, `verdict(summary)`.

## 3. Failure cases

| # | Failure | How the design handles it |
|---|---|---|
| 1 | `vcgencmd` absent (the desk) | Exit 2 with the reason before writing anything |
| 2 | One command fails or prints something unexpected mid-session | That cell is left empty, the row is still written with the timestamp, the error is counted and printed in the summary; the session continues |
| 3 | `pmic_read_adc` unsupported | `input_volts` empty for every row; the voltage part of the verdict is skipped and the summary says so |
| 4 | Output folder missing | Created |
| 5 | Interrupted session | The CSV is complete up to the last row (flushed per row) and the summary is still produced |

## 4. Test plan

| # | Test | Requirement |
|---|---|---|
| 1 | Parsing of the four command outputs as recorded on the unit on 2026-10-05 (`temp=51.0'C`, `throttled=0x0`, `frequency(48)=1500000000`, `EXT5V_V volt(24)=5.24500000V`) and of malformed text | tool |
| 2 | `parse_throttled`: `0x0` gives no flags; `0x50005` sets bits 0, 2, 16, 18; each bit maps to its column | tool |
| 3 | `summarise` and `verdict`: a pass session; a warn session at 76 °C; a fail session with one under-voltage flag; a fail at 80.0 °C; a warn on 4.70 V; empty rows | tool |
| 4 | Logging loop with a fake command runner: rows written with the header comment, errors counted, duration honoured | tool |

Real sessions are LHU-016 (checklist step 3.1).

## Changes after the design review

| # | Change | Reason |
|---|---|---|
| 1 | None yet; the review happens after merge (D-049) | |

## Design vs. implementation

Merged as PR #57 (a1385a9). No deviations. The script has not yet run on the Pi; its first real session is LHU-016 (checklist step 3.1), and the Pi's clock must be checked with `timedatectl` before a session because the timestamps come from it. Status: Implemented.
