# Release notes: v0.2.0 Platform

Released 2026-10-09 from `dev` into `main`. Like v0.1.0, a desk release: everything in it is built and tested on Debian 13 and in CI, against fakes, the ELM327 emulator, a private D-Bus daemon and real child processes. Nothing in it has been run against the car or on the Pi panel yet, and no performance or resource figure from the Pi is claimed.

## What works

- **App hub** (`lexus-hub`, DN-021): a 4 x 2 grid of apps from `deploy/hub.conf`, each started as its own process group with `posix_spawnp`, tracked with `waitpid`, stopped with SIGTERM then SIGKILL, restarted on failure at most 3 times in 60 s. The way back to the hub is stopping the app in front, from a panel launcher (`lexus-hub --send return`) or a local control socket. Commands are split without a shell.
- **Vehicle-data service over D-Bus** (`lexus-vehicle-data-service`, DN-022): the only process that reads the vehicle; publishes every sample and connection change, answers late clients with the full state, versioned interface checked against its introspection file. `lexus-head-unit --source dbus` reads it. systemd user units for the service and the hub.
- **Power status and clean shutdown** (DN-025): the firmware's under-voltage and throttling flags on the hub status strip within 5 s, and a two-tap shutdown button.
- **Web apps** (LHU-023, desk part): browser launch flags for the Wayland session and three public test apps; a process-tree memory sampler (RSS and PSS); the manual procedure.
- **Audio to the car** (LHU-024, desk part): an output script, a session log on the service (`--session-log`), a summariser for REQ-019, and the on-car procedure.
- Merged ahead of v1.0.0 and included here: the **DBC decoder** checked against `cantools` on 10,000 frames (DN-027), the **SocketCAN source** with a receive-only reader and Pi-only vcan tests (DN-028), and **record and replay** of the ELM327 bytes with a VIN scrub (DN-029).

## Tests at release

| Suite | Count | Result |
|---|---|---|
| ctest, `sanitizers` preset, clean build folder | 241 | 241 passed (2 skipped, below) |
| ctest, `release` preset, clean build folder | 241 | 241 passed (2 skipped, below) |
| of which skipped with a ticket reason (vcan0 absent at the desk) | 2 | `VcanTest`, LHU-028 |
| Python tool tests, Windows | 123 | 123 passed (Debian: 120 passed, 3 skipped for missing git) |
| Requirements with a tagged test (`tools/check_traceability.py`) | 22 | 15 with evidence, 7 pending, 0 failing |
| Privacy check on the tree | 293 files | 0 findings |

Desk figures recorded in the design notes (desk, not the Pi): hub exit to idle with a visible window, median 195 to 198 ms over 20 cycles (DN-021); D-Bus hop, median 0.18 ms and p99 0.36 ms over 1,000 samples (DN-022); DBC decoder 0 mismatches against `cantools` over 42,558 values (DN-027); record and replay identical over 10 runs (DN-029).

## What is measured

Nothing on the Pi or in the car. The figures above are desk figures and are labelled so.

## What is known not to be done

Carried into the v1.0.0 milestone; each needs the Pi or the car, and each has its steps in `docs/release/PI_BRINGUP_CHECKLIST.md` or `docs/test/`:

| Ticket | Work | Where |
|---|---|---|
| LHU-016 | Thermal and power log in every session | checklist 3.1 |
| LHU-017 | Adapter pairing, first parked session (now also recorded, LHU-029) | checklist 3.2 |
| LHU-018 | Clean-build measurement on the Pi | checklist 3.3 |
| LHU-020 | The way back to the hub on the panel (A10 to A12) | checklist 3.5 |
| LHU-023 | Web apps run on the Pi: video, protected audio, game, memory | `docs/test/MANUAL_WEB_APPS_PROCEDURE.md` |
| LHU-024 | Audio to the car stereo with the OBD link (REQ-019) | `docs/test/MANUAL_ON_CAR_PROCEDURE.md` |

Also not done: the service and hub as user units on the Pi (step 3.6, A13), the vcan run (3.7), the 10 shutdown cycles (3.8). A repository license has still not been chosen (OQ-25).

## Changes since v0.1.0

PRs #60 to #68 (LHU-021, 022, 027, 028, 029, 025, 023, 024, and this release). The vehicle-data app gained `--source dbus`, `--source can`, `--source replay` and `--record`; `ReconnectBackoff` moved to the service library; the hub registry gained web apps and power settings.
