# Thermal and power sessions

Raw CSVs written by `tools/measure/log_thermal_power.py` (DN-015), one row every 5 s, each with its `.summary.txt`. Every Pi bring-up session and every on-car session is logged (LHU-016) and judged against the thresholds of the plan: fail on any under-voltage or throttle flag or at 80 °C or above; warn from 75 °C, on the soft temperature limit, or below 4.75 V input.

Method per session: fresh boot (the "occurred" flags stay set until reboot), the logger started before the workload, label, power source and ambient temperature given on the command line, the CSV copied from the Pi unchanged. Bad runs are kept.

| Date | Label | Power source | Ambient | Samples | Max °C | Flags | Verdict | File |
|---|---|---|---|---|---|---|---|---|
| 2026-10-05 | first boot, desktop idle and package upgrade | 5 V / 3 A charger | basement room, not measured | two raw files on the Pi under `~/measurements/` (earlier ad-hoc logger, 5 s rows) | 59.3 (measured) | 0x0 throughout (measured) | pass | not yet copied to this folder |

No session logged with this script yet: the first is checklist step 3.1.
