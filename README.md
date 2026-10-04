# Lexus Head Unit

A plug-in, read-only infotainment head unit for a 2013 Lexus GS350: a Raspberry Pi 5 with the official 5-inch touch display, reading live vehicle data over a Bluetooth OBD-II adapter and showing it in a Qt Quick interface. It plugs into the diagnostic port and a USB-C power bank. No wiring, no soldering, nothing replaced in the car.

## Why this exists

A portfolio project built to show, with evidence, the skills an automotive infotainment developer uses: C++17 with a strict warning set; Qt 6 with QML and a clean boundary between the interface and the logic; Linux on constrained hardware (2GB of RAM, no cooler); CAN and OBD-II integration behind a hardware abstraction layer; measured startup time, latency and memory; unit, integration and HMI tests that run without a vehicle; CMake, CI, static analysis and sanitizers on every pull request; and an ASPICE-inspired process with numbered requirements, a traceability matrix, written designs and written reviews. Every number quoted in this repository comes from a script in `tools/measure/` with its raw data committed.

## What it is not

It is **read-only**. The only things it ever sends toward the vehicle are standard OBD-II read requests from a fixed allowlist (Mode 01, 03, 09) and adapter setup commands; there is no code path that clears codes or writes to the bus, and a unit test checks all 256 modes. It does not replace the factory screen or cluster, which remain the authority for every reading. It is **not a safety-rated product** and claims compliance with no standard; the practices borrowed from ISO 26262 and ASPICE are practices, not certification. Details: `docs/safety/SAFETY_STATEMENT.md`.

## Status

Sprint 1 of 3 (desk work, no hardware needed). Build, CI, privacy check and the documentation baseline are in place; the service library is a skeleton. The hardware is purchased and not yet delivered. Releases: v0.1.0 planned at the end of sprint 1, v1.0.0 (MVP) planned for 2026-10-22. The plan with estimates and actuals is `docs/planning/KICKOFF_PLAN.md`.

## Hardware

| Part | Choice |
|---|---|
| Computer | Raspberry Pi 5, 2GB, no case, no active cooling (heat and power are measured, not assumed) |
| Display | Raspberry Pi Touch Display 2, 5-inch, 720 x 1280, portrait-native |
| Vehicle link | Vgate vLinker MC+ Bluetooth OBD-II adapter (ELM327-compatible command set; only base `AT` commands are used, so any ELM327 adapter should work) |
| Power | USB-C power bank in the car; a 5 V / 3 A charger at the desk |
| Vehicle | 2013 Lexus GS350 |

## Architecture in one paragraph

Data sources (a SocketCAN reader with a DBC decoder, an ELM327 OBD-II source, a replay source) all implement one interface that delivers **decoded signal samples**: value, unit, monotonic timestamp and a status of NeverReceived, Valid or Stale. A plain C++17 service layer with no Qt dependency stores the latest sample per signal, judges staleness and tracks the connection state machine. Qt view models receive updates through queued signals on the UI thread, and QML binds only to them. Why the interface sits at the signal level and not the frame level is `docs/adr/ADR-001-signal-level-interface.md`; the full picture is `docs/architecture/ARCHITECTURE.md`.

```
QML screens  ->  view models (Qt, UI thread)  ->  service layer (C++17, worker thread)
                                                      ^  VehicleDataSource interface
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
docs/   requirements/   REQUIREMENTS.md       REQ-001 to REQ-015 with acceptance criteria
        architecture/   ARCHITECTURE.md       layers, components, threads, sizing rules
        adr/            ADR-001-...           decisions with alternatives and consequences
        safety/         SAFETY_STATEMENT.md   read-only guarantee, what is not claimed, hazards
        test/           TEST_STRATEGY.md      test tiers, scenarios, measurements, tags
        traceability/   TRACEABILITY.md       requirement -> design element -> ticket -> test
        release/        RELEASE_CHECKLIST.md  gate from dev to main
        design/         DN-nnn-*.md           design note per component ticket, written before code
        review/         CODE_REVIEW_CHECKLIST.md
        planning/       KICKOFF_PLAN.md       the plan, estimates, risks, decisions
src/    service/        plain C++17 service layer (hardware/, hmi/ and app/ follow)
tests/  unit/           GoogleTest (integration/, scenarios/, hmi/, vcan/ follow)
tools/  check_private_data.py and its tests; measure/ and the ELM327 emulator follow
```

## Process

Work is tracked as tickets `LHU-nnn` on a public GitHub Projects board in one-week sprints. Every change goes through a short-lived branch and a pull request into `dev` with the three CI checks green and a written review against `docs/review/CODE_REVIEW_CHECKLIST.md` posted as a comment. Every ticket that adds or changes a component has a design note reviewed and approved before code is written (`docs/design/README.md`). `dev` is promoted to `main` only through `docs/release/RELEASE_CHECKLIST.md`. Bugs are GitHub issues with reproduction steps, root cause and the regression test added.

## Privacy

The repository is public. The vehicle identification number, Bluetooth addresses and raw on-car recordings are never committed; CI enforces the first two and raw recordings stay in the ignored `local_recordings/` folder until scrubbed.

## License

Not yet chosen.
