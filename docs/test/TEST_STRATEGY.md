# Test strategy

How each kind of claim in this project is verified, where each test runs, and what is deliberately not tested. Every requirement in `docs/requirements/REQUIREMENTS.md` names one of the methods below; `docs/traceability/TRACEABILITY.md` names the test for each requirement.

## 1. Principles

1. **A claim without a test or a measurement is not made.** Performance and correctness statements come with the sample size, the method and the raw data.
2. **Hardware is not required to test the software.** The service layer and the HMI are tested against fakes; the ELM327 path is tested against an emulator; only the `vcan` tier and the measurements need the Pi, and only the manual procedure needs the car.
3. **Bad input is a test case, not an afterthought.** Every parser has a named corpus of malformed inputs and a randomised test under sanitizers (REQ-010).
4. **Tests fail when the code is wrong.** A reviewer asks of every new test: would it fail if the code under test were broken? A test that cannot fail is removed.
5. **Bad results are kept.** A failed measurement run is recorded with the rest; nothing is rounded away or dropped.

## 2. Tiers

| Tier | What | Framework | Runs on | Exists since |
|---|---|---|---|---|
| T1 Unit | Every non-UI class in isolation: service layer, decoders, protocol, allowlist, state machine. Built with `-Wall -Wextra -Werror` and, in CI, with AddressSanitizer and UndefinedBehaviorSanitizer | GoogleTest and GoogleMock | WSL, CI | LHU-005 (one smoke test) |
| T2 Integration | Service layer plus real source code against a fake transport, a fake CAN frame reader, and the ELM327 emulator over a pseudo-terminal | GoogleTest; emulator in Python | WSL, CI | LHU-012 (v0.1.0) |
| T3 vcan | A Python traffic generator writes frames to `vcan0`; the application decodes them live. ctest label `vcan` | GoogleTest plus Python | Pi only; CI if later proven | LHU-028 (v1.0.0) |
| T4 Scenarios | Scripted fault injection through the emulator: see section 3 | Python driving the emulator, GoogleTest asserting | WSL, CI; some Pi only | LHU-011, LHU-012 (v0.1.0) |
| T5 HMI | View models and QML screens with a fake view model on the offscreen platform | Qt Quick Test | WSL, CI | LHU-013 |
| T6 On-car manual | A written procedure, parked first, results recorded per run | `docs/test/MANUAL_ON_CAR_PROCEDURE.md`, results in `docs/test/results/` | Car | LHU-017 (first parked session), LHU-034 (procedure) |

Checks that are not tests of the software but run in CI on every pull request:

| Check | Tool | Since |
|---|---|---|
| Formatting | `clang-format --dry-run --Werror` on every tracked `.cpp` and `.h` | LHU-005 |
| Static analysis | `clang-tidy` on every source file; any warning fails the build | LHU-005 |
| Warnings as errors | `-Werror` in every preset | LHU-005 |
| Privacy | `tools/check_private_data.py`: no VIN-shaped or Bluetooth-address-shaped string in tracked files or file names; the script has its own unit tests | LHU-005 |
| Traceability | `tools/check_traceability.py`: every requirement ID in `REQUIREMENTS.md` appears in at least one test file tag (section 5) | LHU-006, with the first tagged test |
| Dependency rules | Service and HMI targets link to no concrete source; QML and view models include nothing from hardware or service directly (REQ-002, REQ-011) | LHU-012, LHU-013 |
| Thermal and power | `tools/measure/log_thermal_power.py` runs during every Pi bring-up and on-car session; judged against the thresholds of D-025 | LHU-015 (script), LHU-016 (sessions) |

## 3. Scenario coverage (T4)

Each scenario is a script that drives the emulator and a test that asserts what the service layer and HMI did.

| Scenario | How it is injected | Expected behaviour | Requirement |
|---|---|---|---|
| Adapter unplugged | Emulator closes the pseudo-terminal | Error state within 2 s; reconnect attempts with backoff; all signals Stale after their timeout | REQ-006, REQ-007, REQ-008 |
| Bluetooth drop mid-drive | Emulator stops answering, then returns end-of-file | Same as above; no crash; recovery when the emulator returns | REQ-008, REQ-010 |
| Stale data | Link up; one PID stops being answered | Only that signal goes Stale; the others stay Valid | REQ-006 |
| Corrupt frames and text | Emulator garbles bytes; wrong CAN frame length | No Valid sample from the bad input; error counter increments; no crash | REQ-010 |
| Ignition off and on | Emulator answers `UNABLE TO CONNECT` then `NO DATA`, then recovers | Error or Stale as appropriate, then Valid again without restart | REQ-007, REQ-008, REQ-010 |
| Cold boot | Pi powered on from off, 10 or more times | Time to first home-screen frame recorded for every boot | REQ-013 |
| Low power | Pi: `get_throttled` flags surfaced; desk: fake provider sets the flags | Flags shown on the hub status strip and the diagnostics screen within 5 s; logged | REQ-020 |

## 4. Measurements

One script per metric in `tools/measure/`. Each produces a CSV committed under `docs/measurements/<metric>/` with a short method note beside it: what was measured, how, on which build, how many samples, what the environment was.

| Metric | Script | Sample size | Reported |
|---|---|---|---|
| Sample-to-screen latency | `measure_latency.py` | 1,000 or more samples | min, median, p95, p99, max; raw per-sample data |
| Boot to first frame | `measure_boot_time.py` | 10 or more cold boots | every value |
| Resident memory | `measure_memory.py` | 1 sample per 10 s for 30 min | every sample; growth minute 5 to 30 |
| SoC temperature, throttling, under-voltage | `log_thermal_power.py` | 1 row per 5 s per session | count, min, median, max temperature; every flag seen |
| Build on the Pi (LHU-018) | `/usr/bin/time -v`, `vmstat` | 1 clean build per configuration | peak memory, swap activity, temperature, time |

Before-and-after comparisons (the optimisation pass of LHU-032) show both data sets, not a percentage alone. From v0.2.0 memory is measured per process (service, hub, vehicle-data app, browser), because the browser is expected to dominate and must not hide the application's own figure.

## 5. Requirement tags in tests

Every test file that verifies a requirement carries a tag line in its first comment block:

```cpp
// Verifies: REQ-003, REQ-006
```

The same line, with the comment marker of the language, is used in Python and QML test files. `tools/check_traceability.py` (delivered with LHU-006) collects every `REQ-nnn` tag under `tests/` and fails CI if a requirement in `REQUIREMENTS.md` with status Approved or Provisional has no tag anywhere. Requirements verified by measurement name their `tools/measure/` script in the traceability matrix instead of a test tag; the script checks that the named file exists.

## 6. Where tests run

| Environment | What runs | What cannot run |
|---|---|---|
| WSL2, Debian 13 | T1, T2, T4, T5; static checks; privacy check | T3 (no vcan module in the WSL kernel), measurements |
| CI, Debian 13 container | Everything WSL runs, with sanitizers on T1 | T3, T6, measurements |
| Raspberry Pi 5 | Everything plus T3 and the measurements | T6 |
| Car | T6 and the thermal and power log | — |

## 7. Deliberately not tested, and why

- **Real CAN frames from the GS350.** The DBC describes an invented vehicle and is labelled so. Decoding the real car's broadcast frames is not planned before v1.0.0.
- **The adapter's firmware.** Only the software's behaviour toward it is tested; the adapter is a black box exercised by the on-car procedure.
- **The display's touch under the final display stack**, until the bring-up spike decides the stack (OQ-6).
- **Safety.** There is no safety function to test (see `docs/safety/SAFETY_STATEMENT.md`). The REQ-001 tests verify read-only behaviour of this software, nothing more.

## 8. Test code conventions

- Test files live beside the layer they test: `tests/unit/service/`, `tests/unit/obd/`, `tests/unit/can/`, `tests/integration/`, `tests/scenarios/`, `tests/vcan/`, `tests/hmi/`.
- File names end in `_test.cpp`; the test suite name is the class under test; the test name says the behaviour, in words (`MarksSignalStaleAfterTimeout`), not the method name.
- Time is injected through the `Clock` interface; no test sleeps to wait for a timeout.
- `tests/.clang-tidy` relaxes exactly two checks for test code (magic numbers, non-private members in fixtures). Everything else applies.
- A test that needs the Pi carries the ctest label `vcan` or `pi` and is excluded in CI by label.
