# Lexus Head Unit

A plug-in, read-only infotainment platform for a 2013 Lexus GS350: a Raspberry Pi 5 with the official 5-inch touch display, a Qt Quick hub that launches apps, and a central vehicle-data service that reads live signals over a Bluetooth OBD-II adapter and hands them to any app over D-Bus. The vehicle-data app shows live and derived signals and diagnostics; web apps run in the system browser; audio goes to the car stereo over Bluetooth. It plugs into the diagnostic port and a USB-C power bank. No wiring, no soldering, nothing replaced in the car.

## Why this exists

A portfolio project built to show, with evidence, the skills an automotive infotainment developer uses: C++17 with a strict warning set; Qt 6 with QML and a clean boundary between the interface and the logic; Linux on constrained hardware (2GB of RAM, no cooler), with process boundaries, D-Bus and systemd; CAN and OBD-II integration behind a hardware abstraction layer; measured startup time, latency and memory per process; unit, integration and HMI tests that run without a vehicle; CMake, CI, static analysis and sanitizers on every pull request; and an ASPICE-inspired process with numbered requirements, a traceability matrix, written designs and written reviews. Every number quoted in this repository comes from a script in `tools/measure/` with its raw data committed.

The shape, a launcher, apps and one central service that owns vehicle data, is the shape of an automotive infotainment platform in Linux terms. The vehicle-data path is built first and is the core; the hub and the apps come second; content that merely runs in a browser is content, not product code.

## What it is not

It is **read-only**. The only things it ever sends toward the vehicle are standard OBD-II read requests from a fixed allowlist (Mode 01, 03, 09) and adapter setup commands; there is no code path that clears codes or writes to the bus, and a unit test checks all 256 modes. It does not replace the factory screen or cluster, which remain the authority for every reading. It is **not a safety-rated product** and claims compliance with no standard; the practices borrowed from ISO 26262 and ASPICE are practices, not certification. Details: `docs/safety/SAFETY_STATEMENT.md`.

It does not do turn-by-turn navigation or hands-free calling, and it contains no native client for any streaming service; those were considered and cut (plan revision 6).

## Status

Working toward **v0.1.0 Core**: build, CI, privacy check and the documentation baseline are merged; the service library is a skeleton; the hardware is assembled and brought up (Raspberry Pi OS Trixie desktop, landscape touch, thermal and power logging, all first-boot numbers inside the approved thresholds). The car has not been connected yet. Milestones instead of dates since 2026-10-05: v0.1.0 Core (vehicle-data path end to end), v0.2.0 Platform (hub, D-Bus service, web apps, audio, power status), v1.0.0 Head unit (CAN path, record and replay, diagnostics, analytics, measurements, demo), then a roadmap. The plan with estimates and actuals is `docs/planning/KICKOFF_PLAN.md`; the tickets are on the GitHub board.

## Hardware

| Part | Choice |
|---|---|
| Computer | Raspberry Pi 5, 2GB, no case, no active cooling (heat and power are measured, not assumed); mounted on the back of the display with the supplied standoffs |
| Display | Raspberry Pi Touch Display 2, 5-inch, 720 x 1280, portrait-native, used in landscape |
| Storage | 32GB microSD, Raspberry Pi OS 64-bit desktop (Debian 13 "Trixie", 2026-09-15 image) |
| Vehicle link | Vgate vLinker MC+ Bluetooth OBD-II adapter (ELM327-compatible command set; only base `AT` commands are used, so any ELM327 adapter should work) |
| Power, car | Anker Power Bank (20K, 87W, built-in USB-C cable), model A1383: 20,000 mAh, 72 Wh; USB-C 5 V / 3 A among its outputs; no pass-through charging. The Pi runs from it at 5 V / 3 A with the 600 mA USB peripheral limit. Whether the bank switches off at the Pi's idle current is unverified until the first parked session |
| Power, desk | 5 V / 3 A USB-C phone charger |
| Audio | None on the board (the Pi 5 has no headphone jack, the display has no speaker); planned path is Bluetooth to the car stereo, unverified |
| Vehicle | 2013 Lexus GS350 |

Nothing else is bought; no wiring, no soldering.

## Architecture in one paragraph

Data sources (a SocketCAN reader with a DBC decoder, an ELM327 OBD-II source, a replay source) all implement one interface that delivers **decoded signal samples**: value, unit, monotonic timestamp and a status of NeverReceived, Valid or Stale. A plain C++17 service layer with no Qt dependency stores the latest sample per signal, judges staleness, tracks the connection state machine and computes derived trip signals. That layer runs inside one vehicle-data service process, which publishes every signal over D-Bus. The hub and the vehicle-data app are separate Qt processes: their view models read a D-Bus client, and QML binds only to the view models. Why the interface sits at the signal level and not the frame level is `docs/adr/ADR-001-signal-level-interface.md`; the full picture is `docs/architecture/ARCHITECTURE.md`.

```
hub (QML launcher)   vehicle-data app (QML)   system browser (web apps)
        \                   |
         +------ D-Bus -----+
                 vehicle-data service: service layer (C++17)  ^ VehicleDataSource
                 SocketCAN + DBC  |  ELM327 OBD-II  |  replay  |  fake (tests)
```

## Building and testing

Development happens in Debian 13 on WSL2 (the same package versions as Raspberry Pi OS: g++ 14, CMake 3.31, Qt 6.8.2, GoogleTest 1.16). The source stays on the Windows drive; the build folder lives inside the Linux home folder.

```sh
cmake --preset debug            # configure (Ninja, warnings as errors)
cmake --build --preset debug    # build
ctest --preset debug            # run the unit tests
```

Other presets: `sanitizers` (AddressSanitizer and UndefinedBehaviorSanitizer; what CI runs), `release`, and `static-analysis` (clang++ with clang-tidy, any warning fails the build). Formatting is checked with `clang-format --dry-run --Werror` against `.clang-format`. Presets are in `CMakePresets.json`. Packages needed on Debian 13: `build-essential cmake ninja-build clang clang-tidy clang-format libgtest-dev libgmock-dev qt6-base-dev qt6-declarative-dev`.

CI (`.github/workflows/ci.yml`) runs three required checks on every pull request: **Build and unit tests** (sanitizers and release), **Static analysis** (clang-format, clang-tidy) and **Privacy check** (no VIN-shaped or Bluetooth-address-shaped string in the tree). Pull requests cannot merge into `dev` or `main` without them.

## Repository layout

```
docs/   requirements/   REQUIREMENTS.md       REQ-001 to REQ-022 with acceptance criteria
        architecture/   ARCHITECTURE.md       process view, layers, components, threads, sizing rules
        adr/            ADR-001-...           decisions with alternatives and consequences
        safety/         SAFETY_STATEMENT.md   read-only guarantee, what is not claimed, hazards
        test/           TEST_STRATEGY.md      test tiers, scenarios, measurements, tags
        traceability/   TRACEABILITY.md       requirement -> design element -> ticket -> test
        release/        RELEASE_CHECKLIST.md  gate from dev to main
        design/         DN-nnn-*.md           design note per component ticket, written before code
        review/         CODE_REVIEW_CHECKLIST.md
        planning/       KICKOFF_PLAN.md       the plan, estimates, risks, decisions
deploy/ PI_SETUP.md     first-boot checklist for the Pi (systemd units follow)
src/    service/        plain C++17 service layer (hardware/, service_dbus/, hmi/, hub/ and app/ follow)
tests/  unit/           GoogleTest (integration/, scenarios/, hmi/, vcan/ follow)
tools/  check_private_data.py and its tests; measure/, the ELM327 emulator and analysis/ follow
```

## Process

Work is tracked as tickets `LHU-nnn` on a public GitHub Projects board, grouped by milestone (v0.1.0, v0.2.0, v1.0.0, Roadmap) and worked in one-week sprints. Every change goes through a short-lived branch and a pull request into `dev` with the three CI checks green and a written review against `docs/review/CODE_REVIEW_CHECKLIST.md` posted as a comment. Every ticket that adds or changes a component has a design note reviewed and approved before code is written (`docs/design/README.md`). `dev` is promoted to `main` only through `docs/release/RELEASE_CHECKLIST.md`. Bugs are GitHub issues with reproduction steps, root cause and the regression test added.

## Privacy

The repository is public. The vehicle identification number, Bluetooth addresses and raw on-car recordings are never committed; CI enforces the first two and raw recordings stay in the ignored `local_recordings/` folder until scrubbed.

## License

Not yet chosen.
