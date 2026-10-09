# Release notes: v0.1.0 Core

Released 2026-10-08 from `dev` into `main`. This is the first tagged release. It is a desk release: everything in it is built and tested on Debian 13 and in CI against fakes and the ELM327 emulator. Nothing in it has been run against the car yet, and no performance or resource number is claimed.

## What works

- **Signal model and service layer** (plain C++17, no Qt). `SignalSample` carries value, unit, monotonic timestamp and a status of NeverReceived, Valid or Stale. `SignalStore` keeps the latest sample per signal and rejects out-of-order samples; `StalenessMonitor` marks a signal Stale when no sample arrives within its window. Design notes DN-006, DN-008.
- **Connection state machine** with the four states Disconnected, Connecting, Connected and Error, an eight-row transition table, and a test of all 24 state and trigger pairs. DN-007.
- **Source interface.** Every data source implements `VehicleDataSource` and emits decoded signal samples and connection events. A fake source and one parameterised integration suite run against every source; a CI step checks that the service and HMI targets link to no concrete source. DN-008.
- **OBD-II decoding.** Mode 01 PID decoder for the eight signals of REQ-004 and supported-PID bitmaps (0x00, 0x20, 0x40). DN-009.
- **ELM327 path.** Byte transport interface with a fake and a file-descriptor transport for serial devices and pseudo-terminals; a command allowlist (Mode 01, 03, 09 and fixed `AT` setup commands; a test offers all 256 modes); a response parser tested on a 16-entry corpus and 100,000 random strings; `Elm327ObdSource` with handshake, supported-PID discovery, polling, link-loss detection and reconnect backoff; a key-value configuration file. DN-010, DN-012.
- **ELM327 emulator** in Python with fault injection over a control socket (silence, garbage, disconnect, slow replies) and a counter of forbidden requests. Scenario tests kill and restart it. DN-011.
- **HMI.** View models that are the only thing QML sees, a worker loop that hops samples to the UI thread, the application `lexus-head-unit`, a home screen and a vehicle-data screen with the 4 x 2 tile grid, sized in millimetres. QML tests run offscreen; a CI step checks that the HMI includes nothing past the view models. DN-013, DN-039.
- **Thermal and power logger** for the Pi (`tools/measure/log_thermal_power.py`): temperature, throttling flags and input voltage every 5 s to CSV, with a summary and a pass, warn or fail verdict against the approved thresholds. DN-015.

## Tests at release

| Suite | Count | Result |
|---|---|---|
| ctest, `sanitizers` preset, clean build folder | 153 | 153 passed |
| ctest, `release` preset, clean build folder | 153 | 153 passed |
| of which HMI: 7 view-model tests and 1 QML runner holding 12 test functions in 3 files | 8 | passed |
| of which integration | 9 | passed |
| of which scenario (real time against the emulator, about 61 s) | 6 | passed |
| Python tool tests, Debian 13 (Python 3.13.5, no git) | 98 | 95 passed, 3 skipped because git is not installed there |
| Python tool tests, Windows (Python 3.10, git present) | 98 | 98 passed |
| Requirements with a tagged test (`tools/check_traceability.py`) | 22 | 10 with evidence, 12 pending, 0 failing |
| Privacy check on the tree | 170 files | 0 findings |

Toolchain: g++ 14.2.0, CMake 3.31.6, Qt 6.8.2, GoogleTest 1.16.0.

## What is measured

Nothing. No startup time, latency, memory, request rate or thermal figure is claimed by this release. The measurement tool exists (LHU-015); the sessions that use it need the Pi (LHU-016).

## What is known not to be done

These tickets of the v0.1.0 milestone need the Pi or the car and are carried into the v0.2.0 milestone. Each one's steps are in `docs/release/PI_BRINGUP_CHECKLIST.md`.

| Ticket | Work | Checklist step |
|---|---|---|
| LHU-016 | Thermal and power log in every Pi and car session, CSVs committed | 3.1 |
| LHU-017 | Adapter pairing, first parked car session, supported PIDs and request rate | 3.2 |
| LHU-018 | Clean-build measurement on the 2 GB Pi | 3.3 |

Also not done: the home screen has not been checked on the panel (step 3.4), and the nine desk assumptions A1 to A9 in section 2 of the checklist are all unverified. A repository license has not been chosen (OQ-25); until one is added, the code is visible but grants no rights to reuse it.

## Changes since the previous release

None: this is the first release. `main` moves from the repository's initial commit to this release.
