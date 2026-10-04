# Requirements

This file is the single list of what the Lexus Head Unit must do. Every other document points at these IDs: the architecture names the component that implements each one, the traceability matrix names the test that verifies each one, and every pull request lists the IDs it touches.

Baseline: REQ-001 to REQ-015, approved with the kickoff plan (`docs/planning/KICKOFF_PLAN.md`). Requirements from REQ-016 onward are added in sprint 2 planning.

## Rules for a requirement

1. **One ID, never reused.** `REQ-nnn`. A withdrawn requirement keeps its row, struck through, with the reason.
2. **One sentence of behaviour.** It says what the system does, not how. "The system" is the head unit software unless a component is named.
3. **A measurable acceptance criterion.** Someone who did not write the code must be able to run the check and get a pass or fail. Numbers carry units and sample sizes.
4. **A verification method**, one of: unit test, integration test, scenario test, HMI test, measurement on the Pi, manual on-car procedure, CI check.
5. **A status.** Approved; Provisional (the number will be re-set from a measured baseline, OQ-8); Withdrawn.

A requirement that cannot be written this way is not ready and stays out of this file.

## The requirements

| ID | Requirement | Acceptance criterion | Verification | Status |
|---|---|---|---|---|
| REQ-001 | The system sends nothing to the vehicle except allowlisted OBD-II read requests (Mode 01 current data, Mode 03 stored trouble codes, Mode 09 vehicle information) and a fixed list of ELM327 setup commands. No code path exists for any other mode; in particular Mode 04 (clear trouble codes) does not exist in the codebase | A unit test offers every one of the 256 possible mode values to the command layer; only the allowlisted modes reach the transport and 0 bytes are written for every other value. Mode 0x04 is one of the rejected values and the test names it explicitly. The SocketCAN source never calls a send function: the fake CAN frame reader fails the test if its write function is called | Unit test | Approved |
| REQ-002 | The service layer receives vehicle data only through the `VehicleDataSource` interface; which concrete source runs (SocketCAN with DBC, ELM327 over Bluetooth, replay, fake) is chosen at startup by configuration, not by recompiling | The same integration test suite passes against each source with no recompile. The service and HMI build targets have no link dependency on any concrete source library, checked by a CI step that inspects the CMake link graph | Integration test, CI check | Approved |
| REQ-003 | Every signal exposes a value, a unit, a monotonic timestamp with 1 ms resolution, and a status that is one of NeverReceived, Valid or Stale | Unit tests cover each field and every status transition (NeverReceived to Valid, Valid to Stale, Stale to Valid). A sample whose timestamp is earlier than the previous sample of the same signal is rejected and counted | Unit test | Approved |
| REQ-004 | The OBD source discovers which PIDs the vehicle supports from the PID 0x00 bitmap and polls only supported PIDs from this list: vehicle speed 0x0D, engine RPM 0x0C, coolant temperature 0x05, engine load 0x04, throttle position 0x11, intake air temperature 0x0F, control module voltage 0x42, fuel level 0x2F | For each of the 8 PIDs, decoded values equal the SAE J1979 formula at the minimum raw value, the maximum raw value and 3 mid-range vectors, in the unit stated for that PID. With a supported-PID bitmap that excludes a PID, that PID is never requested over 100 polling cycles against the emulator | Unit test, integration test | Approved |
| REQ-005 | The CAN source decodes every signal defined in `dbc/simulated_vehicle.dbc`, covering both byte orders, signed and unsigned values, scale and offset | 10,000 or more random frames generated from the DBC; 100% of decoded values agree with the Python `cantools` library within 1e-6 after scaling. Every mismatch is listed in full in the test output, never summarised | Unit test with an independent oracle | Approved |
| REQ-006 | A signal's status becomes Stale when no new sample for that signal has arrived within its staleness timeout. The default timeout is 1000 ms and each signal may be given its own timeout in configuration | With a controllable clock, a signal last updated at time T is Valid at T + timeout and Stale at T + timeout + 100 ms, for the default timeout and for a per-signal timeout of 250 ms. The HMI test shows that a Stale signal is never drawn in the live style (REQ-012) | Unit test, HMI test | Approved |
| REQ-007 | The connection to a data source is modelled by a state machine with exactly four states, Disconnected, Connecting, Connected and Error, and a written transition table | Every transition in the table is exercised by a unit test and produces the expected state; every pair of states not in the table is rejected and leaves the state unchanged. The HMI shows the new state within 500 ms of the transition, measured by the HMI test with a fake view model | Unit test, HMI test | Approved |
| REQ-008 | Loss of the link to the adapter is detected within 2 s, and reconnection is retried with exponential backoff of 1, 2, 4 s, capped at 10 s, without restarting the application | The ELM327 emulator is killed and restarted 20 times; in 20 of 20 trials the state machine reaches Error within 2 s of the kill and returns to Connected within 15 s of the emulator coming back. The recorded retry intervals match 1, 2, 4, 8, 10, 10 s within 10% | Scenario test | Approved |
| REQ-009 | Sample-to-screen latency, measured from the moment a decoded sample leaves the data source to the first rendered frame that shows its value, is 200 ms or less at the 95th percentile | Over 1,000 or more samples on the Pi, the 95th percentile is 200 ms or less. The report states minimum, median, 95th and 99th percentile and maximum, with the raw per-sample data committed under `docs/measurements/`. Adapter round-trip time is reported separately and is not part of this figure | Measurement on the Pi | Approved |
| REQ-010 | Malformed input never crashes the application and never produces a sample with status Valid. Malformed input includes the ELM327 responses `NO DATA`, `?`, `CAN ERROR`, `BUFFER FULL`, `STOPPED` and `UNABLE TO CONNECT`, truncated responses, non-hexadecimal text, and CAN frames whose length does not match the DBC | A named corpus test covers each listed response and frame fault. A randomised test feeds 100,000 random byte strings to the parsers under AddressSanitizer and UndefinedBehaviorSanitizer: 0 crashes, 0 sanitizer reports, 0 samples with status Valid. The source's error counter increases by exactly 1 per malformed input | Unit test under sanitizers | Approved |
| REQ-011 | QML code never touches hardware or service classes directly; it binds only to view models | The HMI build target links only to the view-model library. A CI step fails if any file under `src/hmi/qml/` or any view model includes a header from `src/hardware/` or `src/service/` other than through the view-model library's public headers | CI check | Approved |
| REQ-012 | The home screen and the vehicle data screen show, for each displayed signal, its value, its unit and its status | A Qt Quick Test with a fake view model sets each signal to a Valid value, then to Stale, then to NeverReceived, and checks the displayed text, the unit text and the styling for every signal on both screens. Sizes follow the millimetre rules in `docs/architecture/ARCHITECTURE.md` | HMI test | Approved |
| REQ-013 | Time from power-on to the first rendered frame of the home screen on the Raspberry Pi 5 | Provisional target 15 s or less. Measured over 10 or more cold boots; every value is reported, none dropped. The target is re-set once a baseline exists (OQ-8) | Measurement on the Pi | Provisional |
| REQ-014 | Resident memory of the application on the Raspberry Pi 5 | Provisional target 150 MB or less resident memory after 30 minutes of operation, and growth from minute 5 to minute 30 under 5%. Sampled once every 10 s; raw data committed. The target is re-set once a baseline exists (OQ-8) | Measurement on the Pi | Provisional |
| REQ-015 | Raw transport bytes can be recorded with timestamps and replayed through `ReplaySource`, so that a drive can be reproduced at the desk | Replaying a recording through the full service layer produces a sequence of signal samples identical in values and order to the sequence produced when the recording was made. A recording that contains a vehicle identification number is scrubbed before it leaves the ignored `local_recordings/` folder, including the hexadecimal encoding used by Mode 09 | Integration test, privacy check | Approved |

## Reading the acceptance criteria

- "Unit test" means a GoogleTest test in `tests/unit/`, run under AddressSanitizer and UndefinedBehaviorSanitizer in CI.
- "Integration test" means the service layer plus real source code against a fake transport, a fake CAN frame reader, or the ELM327 emulator.
- "Scenario test" means scripted fault injection through the emulator.
- "HMI test" means a Qt Quick Test on the offscreen platform.
- "Measurement on the Pi" means a script in `tools/measure/` whose raw output is committed under `docs/measurements/` with the sample size and method.
- "CI check" means a step in `.github/workflows/ci.yml` that fails the pull request.

Where each requirement is implemented and tested is in `docs/traceability/TRACEABILITY.md`. How each verification method runs is in `docs/test/TEST_STRATEGY.md`.

## Not requirements

These are properties of the project, not of the software, and are stated elsewhere so they are not confused with testable behaviour:

- The system is read-only toward the vehicle by design and policy, not only by REQ-001: see `docs/safety/SAFETY_STATEMENT.md`.
- The system is not a safety-rated product and claims compliance with no standard: see `docs/safety/SAFETY_STATEMENT.md`.
- No vehicle identification number, Bluetooth address or raw on-car recording is committed: enforced by `tools/check_private_data.py` in CI.
