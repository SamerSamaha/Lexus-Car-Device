# Lexus Head Unit

A plug-in, read-only infotainment platform for a 2013 Lexus GS350, built on a Raspberry Pi 5 with a 5-inch touchscreen. A Qt Quick hub launches apps; a central vehicle-data service reads live signals from the car over a Bluetooth OBD-II adapter and hands them to any app over D-Bus; the vehicle-data app shows live and derived signals and diagnostics; web apps run in the system browser; audio goes to the car stereo over Bluetooth. It plugs into the diagnostic port and a USB-C power bank. No wiring, no soldering, nothing replaced in the car.

It is a portfolio project: the point is to show, with evidence, how automotive infotainment software is built. C++17, Qt 6 and QML, Linux (processes, D-Bus, systemd, Bluetooth), CAN and OBD-II behind a hardware abstraction layer, measured startup time, latency and memory on 2 GB of RAM, tests that run without a vehicle, and an ASPICE-inspired process with numbered requirements and traceability. It is **not a safety-rated product** and claims compliance with no standard.

## Status

**v0.1.0 Core** is released (`docs/release/RELEASE_NOTES_v0.1.0.md`): the service layer (signal store, staleness, connection state machine), the ELM327 path (allowlist, protocol, serial transport, source with discovery, loss detection and backoff), an ELM327 emulator with fault injection, the home and vehicle-data screens with their view models, and a thermal and power logger exist and are tested at the desk, including scenario tests that kill and restart the emulator. It is a desk release: the hardware is assembled and running, but the car has not been connected yet and no measurement is claimed. Work now continues toward **v0.2.0 Platform**. Milestones: v0.1.0 Core (vehicle-data path end to end), v0.2.0 Platform (hub, D-Bus service, web apps, audio, power status), v1.0.0 Head unit (CAN path, record and replay, diagnostics, analytics, measurements). Progress is on the [GitHub project board](https://github.com/users/SamerSamaha/projects/1); the plan is `docs/planning/KICKOFF_PLAN.md`.

## Architecture

```
hub (QML launcher)      vehicle-data app (QML)      system browser (web apps)
        \                        |                   started and tracked by the hub
         +-------- D-Bus --------+
                      |
         vehicle-data service: plain C++17 service layer
           SignalStore . StalenessMonitor . ConnectionStateMachine . derived signals
                      |  VehicleDataSource interface (decoded signal samples)
           ELM327 OBD-II  |  SocketCAN + DBC  |  replay  |  fake (tests)
```

Every data source delivers **decoded signal samples**: value, unit, monotonic timestamp and a status of NeverReceived, Valid or Stale. A plain C++17 service layer with no Qt dependency stores the latest sample per signal, judges staleness, runs the connection state machine and computes derived trip signals. It runs in one vehicle-data service process and publishes over D-Bus; the hub and the apps are separate Qt processes whose view models read a D-Bus client, and QML binds only to view models. One integration suite runs against every source. The shape, a launcher, apps and one central vehicle-data service, is the shape of an automotive infotainment platform in Linux terms.

Why the interface sits at the signal level: `docs/adr/ADR-001-signal-level-interface.md`. The full picture: `docs/architecture/ARCHITECTURE.md`.

## Read-only, by construction

The only things ever sent toward the vehicle are OBD-II read requests from a fixed allowlist (Mode 01, 03, 09) and adapter setup commands. There is no code path that clears codes or writes to the bus; a unit test offers all 256 modes and only the allowlisted ones reach the transport; the CAN reader has no send function. The factory cluster remains the authority for every reading. Details and limits: `docs/safety/SAFETY_STATEMENT.md`.

## Hardware

| Part | Choice |
|---|---|
| Computer | Raspberry Pi 5, 2 GB, no case, no active cooling; heat and power are logged every 5 s and judged against written thresholds |
| Display | Raspberry Pi Touch Display 2, 5-inch, 720 x 1280, used in landscape |
| Vehicle link | Vgate vLinker MC+ Bluetooth OBD-II adapter; only base ELM327 `AT` commands are used, so any ELM327 adapter should work |
| Power | 20,000 mAh USB-C power bank in the car (5 V / 3 A); a 5 V / 3 A charger at the desk |
| Audio | Bluetooth from the Pi to the car stereo (planned; the Pi 5 has no headphone jack) |
| Vehicle | 2013 Lexus GS350 |

First-boot setup is `deploy/PI_SETUP.md`.

## Building and testing

Development happens in Debian 13 on WSL2 with the same package versions as Raspberry Pi OS (g++ 14, CMake 3.31, Qt 6.8.2, GoogleTest 1.16).

```sh
cmake --preset debug            # configure (Ninja, warnings as errors)
cmake --build --preset debug    # build
ctest --preset debug            # unit, integration, scenario and HMI tests (offscreen)
```

Run the vehicle-data application against the emulator (no hardware needed):

```sh
python3 tools/elm327_emulator/elm327_emulator.py --link /tmp/obd --control /tmp/obd.control &
# set elm327.device = /tmp/obd in deploy/head_unit.conf, then:
~/build/lexus-car-device/debug/src/app/lexus-head-unit --config deploy/head_unit.conf
# or without the emulator, with moving demo values:
~/build/lexus-car-device/debug/src/app/lexus-head-unit --source fake
# or the CAN path on a Pi with vcan0 (sudo deploy/setup_vcan.sh; python3 tools/can_traffic_generator.py):
~/build/lexus-car-device/debug/src/app/lexus-head-unit --source can
```

Run the platform as on the Pi: the vehicle-data service owns the source and publishes on the session bus, the hub launches the vehicle-data app, which reads the service (no hardware needed):

```sh
~/build/lexus-car-device/debug/src/service_dbus/lexus-vehicle-data-service --source fake &
PATH=~/build/lexus-car-device/debug/src/app:$PATH ~/build/lexus-car-device/debug/src/hub/app/lexus-hub --registry deploy/hub.conf &
~/build/lexus-car-device/debug/src/hub/app/lexus-hub --send status    # or: --send "launch vehicle_data", --send return
```

Other presets: `sanitizers` (AddressSanitizer and UndefinedBehaviorSanitizer; what CI runs), `release`, and `static-analysis` (clang++ with clang-tidy, any warning fails the build). Formatting is `clang-format --dry-run --Werror`. The DBC decoder's oracle test needs `cantools` in a test-only virtual environment: run `tools/setup_test_venv.sh` once (nothing is installed system-wide). Packages on Debian 13: `build-essential cmake ninja-build clang clang-tidy clang-format libgtest-dev libgmock-dev qt6-base-dev qt6-declarative-dev qml6-module-qtquick qml6-module-qtquick-window qml6-module-qttest`.

CI runs three required checks on every pull request: **Build and unit tests**, **Static analysis** and **Privacy check** (no vehicle identification number or Bluetooth address in the tree). Nothing merges without them.

## Repository layout

```
src/        service/ (plain C++17)  hardware/  service_dbus/  hmi/  hub/  app/
tests/      unit/  integration/  scenarios/  hmi/  vcan/
tools/      check_private_data.py, measurement scripts, ELM327 emulator, analysis
deploy/     Pi setup, systemd units, app registry
dbc/        simulated_vehicle.dbc (invented vehicle, labelled as such)
docs/       requirements/  architecture/  adr/  safety/  design/  test/
            traceability/  review/  release/  planning/  measurements/
```

Folders that do not exist yet are created by the ticket that needs them.

## Process

Work is tracked as `LHU-nnn` tickets on the project board, grouped by milestone. Every change is a short-lived branch and a pull request into `dev` with CI green and a written review against `docs/review/CODE_REVIEW_CHECKLIST.md`. Every component has a design note (`docs/design/`) reviewed and approved before code. Requirements have measurable acceptance criteria (`docs/requirements/REQUIREMENTS.md`) and each maps to a design element and a test (`docs/traceability/TRACEABILITY.md`). `dev` is promoted to `main` only through `docs/release/RELEASE_CHECKLIST.md`, tagged per milestone. Every number quoted in this repository comes from a script in `tools/measure/` with its raw data committed.

## Privacy

The repository is public. The vehicle identification number, Bluetooth addresses and raw on-car recordings are never committed; CI enforces the first two, and raw recordings stay in the ignored `local_recordings/` folder until scrubbed.

## License

Not yet chosen.
