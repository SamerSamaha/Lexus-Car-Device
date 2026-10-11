# DN-050: US units on screen (light note)

| | |
|---|---|
| Ticket | LHU-050 |
| Requirements | REQ-027 (new); touches REQ-012, REQ-022 |
| Author | implementer (build-out form, D-049) |
| Status | Implemented |
| Draft written | 2026-10-11, about 15 minutes |
| Design review | after merge, by the repository owner (D-049) |
| Approved | 2026-10-11 |

## Problem

The owner and the audience of the demo read speed in mph, distance in miles, temperature in degrees Fahrenheit and fuel economy in miles per gallon, but every signal is metric from the decoder to D-Bus and the recordings. The screen must show US units in car mode, with a switch back to metric in car.conf, without changing any value that is stored, published or recorded.

## Public interface

| Piece | Interface | Notes |
|---|---|---|
| `DisplayUnits` (view models) | `Metric`, `UsCustomary` | Only the HMI knows it; the service layer stays metric (REQ-003, REQ-017 unchanged) |
| `displayValue(metric, unit, units)` | value in the shown unit, or nothing | km/h and km divided by 1.609344 (exact); degrees Celsius times 1.8 plus 32; mpg = 235.2145833 / (L/100 km), with 1 US gallon = 3.785411784 L (exact); nothing for 0 L/100 km or less. rpm, %, V, g/s and minutes unchanged |
| `displayUnitText(unit, units)` | "mph", "°F", "mpg", "mi"; the metric text otherwise | |
| `SignalTileModel(signalId, units)`, `VehicleDataViewModel(units)` | `unitText` and `valueText` in the chosen units; property `usUnits` | The old constructors stay and mean metric. Miles per gallon above 999 (coasting) shows ">999" |
| `chooseDisplayUnits(commandLine, carFile, fallback, valid)` | the first of: `--units`, `display.units` in car.conf, the fallback | The fallback is US when the car file exists (car mode) and metric when it does not (the desk), so the owner's existing car.conf, which has no `[display]` section, gives US without an edit |
| `lexus-head-unit --units us|metric --car-config <path>` | | `--car-config` defaults to `~/.config/lexus-head-unit/car.conf`; the app prints the choice on its error output, which the test reads |
| `deploy/car/car.conf.example` | `[display] units = us` | New installs say it explicitly |

## Failure cases

| Case | Handling |
|---|---|
| An unknown text in `--units` or `display.units` ("imperial") | Reported on the error output; the next source is used |
| Fuel economy 0 L/100 km or negative | "--" in mpg (no finite value) |
| Very low L/100 km while coasting | ">999" instead of a five-digit number |
| A metric value that is Stale | Converted like a Valid one and drawn in the stale style (REQ-006, REQ-012 unchanged) |

## Test plan

| Requirement | Test |
|---|---|
| REQ-027 | `tests/unit/hmi/display_units_test.cpp` (each factor to 1e-9, -40 °C equals -40 °F, units without a US form unchanged, zero economy, unit texts, the order command line then car file then fallback, the tiles of a US view model with the sample still metric, ">999") |
| REQ-027 | `tests/hmi/tst_us_units.qml` (unit texts of the grid and the trip screen, converted values on screen) |
| REQ-027 | `tests/installed/units_from_the_command_line.sh` (the executable: no car file gives metric, a car file without units gives US, `display.units = metric` gives metric, `--units` wins either way) |

## Alternative considered

Converting in the service and publishing US values over D-Bus was rejected: the recordings, the session logs, the trip analytics constants (DN-031) and the replay test would all have to know the display choice, and a metric client could no longer read the same service. Units are a presentation concern, so they live in the view models.
