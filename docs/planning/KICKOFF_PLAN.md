# Lexus Head Unit — project plan

**Revision 7, 2026-10-08.**
- Revision 1: kickoff plan.
- Revision 2: hardware as purchased, public repository, review and protection model (D-011 to D-018).
- Revision 3: approvals D-019 to D-027 applied; desk environment installed and verified.
- Revision 4: design-first gate added (D-033); sprint 1 estimates and statuses updated; LHU-005 before LHU-004 (D-032).
- Revision 5: design-first gate made tiered and LHU-011, LHU-012 moved to sprint 2 (D-037); LHU-004 re-estimated to 4 h (D-039); actual hours of LHU-002, LHU-003 and LHU-005 recorded; CI status checks required on both branches.
- Revision 6: scope review closed (D-042 to D-047): the head unit becomes a platform (app hub, vehicle-data service over D-Bus, apps); milestones replace dates and the MVP date is withdrawn (D-043); no further purchases (D-044); desktop image and labwc as the display stack (D-045); REQ-016 to REQ-022 added; tickets LHU-016 to LHU-039 created with milestones; hardware as arrived and the Pi bring-up recorded; LHU-001 closed (power bank record).
- Revision 7: ticket statuses brought up to date (LHU-019 and LHU-040 done, LHU-041 added); design notes for the build-out written by the implementer and reviewed after merge (D-049); `docs/release/PI_BRINGUP_CHECKLIST.md` created as the single list of steps that need the Pi or the car.

The canonical copy of this file is `docs/planning/KICKOFF_PLAN.md` in the repository. Ticket LHU-004 split it into the other `docs/` files; those files are the detailed references and this file is the plan.

## Context

A plug-in, read-only infotainment platform for a 2013 Lexus GS350, built as a portfolio project to demonstrate infotainment software skills: C++17, Qt/QML, Linux (processes, D-Bus, systemd, Bluetooth, Wayland), CAN and OBD-II integration, system design, testing on bench hardware, performance on constrained hardware, and an ASPICE-inspired process. It is not a safety-rated product.

Standing decisions: both data paths behind a signal-level interface; vcan tests on the Pi with in-process fakes on WSL; public repository; power and heat measured, not assumed; read-only toward the vehicle (REQ-001) and no safety claim are non-negotiable; **milestones instead of dates since revision 6** (D-043), worked in one-week sprints at whatever hours are available.

## Scope decision, 2026-10-05 (D-042 to D-047)

The question raised on 2026-10-05: is the end product a vehicle-data head unit (the approved plan) or something closer to a phone-projection system with video, music, maps and games? The review asked what the product must do for an infotainment interview, for daily use and for the demo, costed every candidate, and decided the following.

**Decided shape (D-042):** a platform. A full-screen **hub** launches apps as separate processes; one **vehicle-data service** owns the vehicle link and publishes signals over D-Bus to any app; the **vehicle-data app** shows live and derived signals and diagnostics; **web apps** (video, audio, games) are URL entries opened in the system browser; **audio** goes from the Pi to the car stereo over Bluetooth. This is the shape of an automotive infotainment platform in Linux terms (launcher, apps, one central vehicle-data service), which is what a system-design interview probes. The vehicle-data path stays the core and is built first.

**Why web apps as URLs:** an icon that opens a video site is a bookmark and proves nothing on its own; it costs about 3 hours and is kept because the platform around it (process management, the service boundary, per-process measurements) is what earns the resume words. Protected audio depends on the browser's DRM module, verified on the unit as a fact (OQ-27).

**Cut, and why (D-042):** hands-free calling (weeks of audio-stack work that duplicates the factory unit); turn-by-turn navigation (no reproducible API; the phone has it); native clients for any streaming service and phone-projection systems (proprietary, not reproducible); native games (prove nothing; a URL is free); model training on the device (the data volume does not justify it: at a generous 100 samples per second the adapter yields 10 to 15 MB of CSV per hour, which is laptop data); any purchase (D-044).

**Audio and GPS without purchases (D-044):** the Pi 5 has no headphone jack and the display has no speaker, and the car's USB port is a host for storage devices, so audio reaches the car only over Bluetooth (Pi as A2DP source) or not at all. Position for a map can only come from the phone over the hotspot; the phone in use is an iPhone, and a free position-streaming app for it is unverified (OQ-31). GPS and maps are therefore on the roadmap, not in v1.0.0; turn-by-turn is out permanently.

**Statistics (D-047):** "a statistics-based model over the car data" means recording trips on the Pi (REQ-015), analysing them in Python on the laptop (`tools/analysis/`, LHU-037), and, if a model earns it, exporting coefficients that the service layer applies as a derived signal (REQ-022). Nothing is trained on the Pi.

**Candidate table as decided** (hours are effort for the author in beginner mode, including the design gate where it applies; estimates, unverified until the first code tickets give a velocity):

| Feature | Proves | Hours | Decision |
|---|---|---|---|
| App hub launcher (process manager, registry, return to hub) | System design, Linux processes, Qt/QML | 8 + 1.75 | v0.2.0, LHU-021 after the LHU-020 spike |
| Vehicle-data service over D-Bus | IPC, HAL separation, the central-service pattern | 6 + 1.75 | v0.2.0, LHU-022 |
| Web apps in the system browser | Little on its own | 3 | v0.2.0, LHU-023 |
| Audio to the car stereo over Bluetooth | Linux audio and Bluetooth stacks | 4 | v0.2.0, LHU-024 |
| Power status and clean shutdown | Constrained hardware, systemd | 4 + 0.75 | v0.2.0, LHU-025 |
| Diagnostics: trouble codes, vehicle information | OBD-II depth | 5 + 1.75 | v1.0.0, LHU-030 |
| Trip analytics as derived signals | Automotive domain, service design | 7 + 1.75 | v1.0.0, LHU-031 |
| SocketCAN + DBC path | CAN integration | 10 + 2.5 | v1.0.0, LHU-027, LHU-028 |
| Record and replay with scrub | Testability, privacy | 4 + 1.75 | v1.0.0, LHU-029 |
| Whole-system measurements | Startup, latency, memory | 6 | v1.0.0, LHU-032 |
| GPS from phone position, map tiles | HAL abstraction, Qt | 14 + 1.75 | Roadmap, LHU-036 |
| Offline statistics on recordings | Python, data handling | 10 | Roadmap, LHU-037 |
| Call and notification status | Bluetooth | 7 | Roadmap, LHU-038, optional |
| Bluetooth media app (Pi as sink from the phone) | Bluetooth, D-Bus | 10 to 12 | Not planned without a USB audio adapter; superseded by web audio plus LHU-024 |
| Call audio, navigation, native clients, native games, on-device training | — | — | Cut |

**Resume sentence this supports when v0.2.0 plus diagnostics is true:** a Linux infotainment platform for a 2013 Lexus GS350 on a Raspberry Pi 5 (2 GB): a Qt/QML launcher managing app processes, a central vehicle-data service exposing OBD-II and CAN signals over D-Bus to any app, modelled on the central-service pattern of automotive platforms, with web apps, Bluetooth audio to the car stereo, and measured boot time, latency and memory.

## Milestones (D-043)

Each milestone is a tagged release from `dev` to `main` after `docs/release/RELEASE_CHECKLIST.md` passes. The tickets of each milestone are on the GitHub board under the milestone of the same name; sprints remain a one-week cadence for choosing what to work on next.

| Milestone | Content | Tickets | Estimated hours |
|---|---|---|---|
| **v0.1.0 Core** | Service layer, connection state machine, source interface, PID decoder, ELM327 parser and allowlist, emulator, `Elm327ObdSource`, home and vehicle-data screens on the Pi, adapter paired, first parked car session, thermal logging, build measurement, this revision | LHU-001, 006 to 015, 016 to 019, 039, 040, 041 | 49.25 remaining (table below) |
| **v0.2.0 Platform** | Return-to-hub spike, hub launcher, vehicle-data service over D-Bus, web apps, Bluetooth audio to the car, power status and clean shutdown, release | LHU-020 to 026 | 32.75 |
| **v1.0.0 Head unit** | DBC decoder, SocketCAN source, record and replay, diagnostics screen, trip analytics, whole-system measurements and one optimisation pass, arm64 CI build, on-car procedure and drives, docs, demo video, license, release | LHU-027 to 035 | 48.75 |
| **Roadmap** | GPS from phone position and map tiles, offline statistics, call and notification status (optional) | LHU-036 to 038 | 32.75, not scheduled |

Total planned to v1.0.0: about 132 hours of remaining work. At the sprint 1 pace of 35 hours per week that is four weeks; at fewer hours it is longer. No date is attached (D-043). The one dated recommendation on record: the referral makes a "resume-ready" point worth naming, and that point is v0.2.0 plus LHU-030.

## Hardware as arrived (D-011, D-012, D-026, D-044)

| Part | Purchased | Arrived |
|---|---|---|
| Computer | Raspberry Pi 5, 2GB | 2026-10-05; assembled on the back of the display |
| Display | Official Touch Display 2, 5-inch, 720x1280 native portrait | 2026-10-05; used in landscape |
| Storage | TOPESEL 32GB microSD | 2026-10-04; flashed 2026-10-05 |
| OBD adapter | Vgate vLinker MC+ (Bluetooth, ELM327/ELM329/STN compatible) | 2026-10-04; not yet paired |
| Case | None; the Pi mounts to the back of the display | — |
| Cooling | None | — |
| Power, car | Anker Power Bank (20K, 87W, Built-In USB-C Cable), model A1383. 20,000 mAh, 72 Wh. USB-C cable and port: 5 V 3 A, 9 V 3 A, 10 V 2.25 A, 12 V 3 A, 15 V 3 A, 20 V 3.25 A, 65 W max; USB-A 5 V 3 A, 22.5 W max; 87 W total. No pass-through charging. Low-current auto-off: not found in any source, unverified until the first parked session (OQ-3 closed 2026-10-04, record carried by LHU-019 and closing LHU-001) | In hand |
| Power, desk | 5 V / 3 A USB-C phone charger | In hand |
| Audio | Nothing: the Pi 5 has no headphone jack and the display has no speaker. Planned path is Bluetooth to the car stereo (LHU-024), unverified | — |
| Accessories | USB microSD reader for the laptop (no card slot) | 2026-10-05 |

No further purchases (D-044).

## Evidence

| Item | Finding | Status |
|---|---|---|
| Dev machine | Windows 11 Home, Core Ultra 7 155U, 15.4 GB RAM | Verified |
| WSL2 | WSL 2.5.7, kernel 6.6.87.1, WSLg 1.0.66 | Verified |
| Debian distro | Debian GNU/Linux 13 (trixie), 13.5, amd64, systemd enabled | Verified |
| Desk toolchain | g++ 14.2.0, CMake 3.31.6, Ninja 1.12.1, gdb 16.3, clang / clang-tidy / clang-format 19.1.7, GoogleTest 1.16.0-1, Qt 6.8.2, Python 3.13.5 | Verified |
| Desk environment check | See "Desk environment" below | Verified, 1 run each |
| vcan in WSL2 | `modinfo vcan`: not found; no CONFIG_CAN_VCAN | Verified absent |
| Local repository | `origin/main` is one commit, 414ff66 "Initial commit"; `dev` is the default branch | Verified |
| GitHub protection | Rulesets `protect-main` and `protect-dev` active on a personal Free public repository: deletion and force pushes blocked, pull request required, and since 2026-10-02 the three CI checks "Build and unit tests", "Static analysis" and "Privacy check" required on both branches | Verified through the API, 2026-10-03 |
| CI on GitHub | First two runs (PR #17) green in 17 to 44 s per job inside a `debian:trixie` container: `actions/checkout@v7` and AddressSanitizer work on the hosted runner | Verified, 2 runs |
| GitHub arm64 runners | `ubuntu-24.04-arm` free for public repos | Unverified by us |
| vcan on GitHub-hosted runners | Not shipped; fragile to add | Unverified by us |
| vLinker MC+ | Dual mode, Bluetooth 3.0 classic plus BLE 4.0; classic name ends in "-Android"; BLE name ends in "-IOS" | Unverified until paired |
| Pi 5 power input | Raspberry Pi docs: 5 V at 5 A, or 5 V at 3 A with a 600 mA USB peripheral limit | Docs verified; on the desk charger the Pi booted and ran the upgrade with 0x0 throttle flags (measured 2026-10-05); the power bank unverified |
| Pi 5 thermal | Docs: progressive throttling from 80°C, full at 85°C. Raspberry Pi's test: uncooled idle about 65°C on an air-conditioned bench; uncooled stress settles just above 85°C, throttled | Docs verified. **Measured on our unit 2026-10-05, basement room, desk charger: idle 51°C on the desktop, 59.3°C peak during a 93-package upgrade, `get_throttled` 0x0 throughout, input 5.245 V.** Inside the pass band |
| `vcgencmd get_throttled` | Bits 0 to 3: under-voltage, frequency capped, throttled, soft temperature limit (now). Bits 16 to 19: the same, "has occurred" since boot | Verified from docs |
| `vcgencmd pmic_read_adc EXT5V_V` | Reads the 5 V input on the Pi 5 | Verified on our unit (5.245 V) |
| Raspberry Pi OS swap | Trixie ships zram swap, 2 GiB on this unit (`swapon --show`) | Verified on our unit |
| Raspberry Pi OS image | Desktop 64-bit, Debian 13 Trixie, image 2026-09-15, kernel 6.18; Qt 6.8.2 (`6.8.2+dfsg-9+deb13u2`), BlueZ 5.82, `rfcomm` present, `python3-tk` preinstalled | Verified on our unit |
| Touch Display 2, 5-inch | Active area 62.1 x 110.4 mm, 11.6 pixels per mm. Landscape through the compositor (`transform 90` in the kanshi configuration), touch follows | Dimensions verified from the product page; rotation and touch verified on our unit 2026-10-05 |
| Touchscreen spike | A throwaway Python script showing 8 OBD-II tiles and two buttons ran full screen on the Pi from the desktop by touch, in fake-data mode | Verified on our unit; the car not yet connected |
| mDNS from Windows | `lexus-head-unit.local` did not resolve from the laptop; the router's address was used | Observed |
| Pi 5 audio | No analogue audio output on the Pi 5; the display has no speaker. The car's USB port is a storage host and cannot take an audio stream | Docs verified (Pi 5); car USB behaviour from the owner's experience, unverified by test |
| 2013 GS350 OBD | CAN-based; protocol and PIDs not confirmed; Bluetooth audio streaming and an AUX jack in the car unverified (OQ-30) | Unverified until the first parked session |
| Protected audio in the system browser | Raspberry Pi OS's browser reportedly ships a DRM module on arm64; Qt's web view does not | Unverified on our unit (OQ-27); checked by LHU-023 |

## Desk environment (LHU-002, D-021)

Debian 13 in WSL2. Source stays in `C:\Lexus Car Device` (`/mnt/c/Lexus Car Device` from WSL). The build directory is `~/build/lexus-car-device` inside WSL. Git runs from Windows only.

Packages installed, by approved set:

| Set | Packages named | Versions | apt result |
|---|---|---|---|
| A, build | build-essential, cmake, ninja-build, gdb, pkg-config | 12.12, 3.31.6-2, 1.12.1-1, 16.3-1, 1.8.1-4 | 111 new, 6 upgraded, 136 MB download, 495 MB on disk |
| B, quality | clang, clang-tidy, clang-format, libgtest-dev, libgmock-dev | 1:19.0-63 (19.1.7), 1.16.0-1 | 42 new, 143 MB download, 874 MB on disk |
| C, Qt | qt6-base-dev, qt6-declarative-dev, qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-layouts, qml6-module-qttest | 6.8.2 | 223 new, 1 upgraded, 96.3 MB download, 499 MB on disk |

Package count went from 157 to 533. Disk use went from 261 MB to 2.7 GB. The D-Bus boundary (LHU-022) will need `qt6-base-dev`'s D-Bus module, which is part of set C; this is checked when LHU-022 starts.

Environment check (a throwaway project outside the repository; source on `/mnt/c` in a folder with a space in its name; build inside WSL):

| Check | Result |
|---|---|
| CMake configure with Ninja | Exit 0, 3.7 s; found Qt 6.8.2 and GTest |
| Build of 2 targets | Exit 0, 2.3 s |
| GoogleTest through ctest, with AddressSanitizer and UndefinedBehaviorSanitizer, `-Wall -Wextra -Werror` | 1 of 1 passed |
| clang-tidy with the compile database | Exit 0 |
| clang-format | Exit 0 |
| QML window, wayland plugin (default under WSLg) | 60 frames drawn, exit 0 |
| QML window, xcb plugin | 60 frames drawn, exit 0 |
| QML window, offscreen plugin (what CI will use) | 60 frames drawn, exit 0 |

Differences from what was planned:
- Every package name in sets A, B and C existed under the planned name. None had to change.
- apt upgraded 7 packages that were already in the base image, as dependencies of the sets.
- CMake reports the GoogleTest version as 1.15.0 while the Debian package is 1.16.0-1. Not yet explained; it does not affect the build.
- Python 3.13.5 arrived as a dependency. git is not installed in Debian, which matches the Windows-git-only rule.
- The Qt wayland platform plugin arrived as a dependency, so the default plugin under WSLg is wayland, not xcb.
- The packages were installed as root because the distro had no normal user yet (OQ-16, since closed).

## Approved assessments

### 1. Building for the 2GB Pi (D-022)

The dev laptop is x86-64 and the Pi is arm64, so a desk build cannot simply be copied over.

| Route | How | For | Against |
|---|---|---|---|
| A. Build on the Pi | `cmake --build` with 2 parallel jobs | Simplest; exact target environment; debug where it runs | Uncooled Pi will run hot while compiling; slower |
| B. arm64 container on the laptop | `debian:trixie` arm64 image under Docker emulation, copy the binary over SSH | No load on the Pi | Emulated compile is slow (factor unverified); Raspberry Pi OS Qt packages may differ from Debian's; about 2 hours of setup |
| C. arm64 build in CI | `ubuntu-24.04-arm` runner, `debian:trixie` container, download the artifact | Native-speed arm64; reproducible | Minutes per iteration; same package-mismatch risk as B |

**Decision:** route A day to day with 2 jobs and the default Raspberry Pi OS zram swap (verified 2 GiB). Route C as the release build and the fallback (LHU-033). Route B is not planned. True cross-compilation with a sysroot is rejected.

Do not use a plain swap file on the SD card as working memory: it is slow and wears the card.

**Switch trigger, measured by LHU-018:** record peak memory, swap activity (`vmstat`), temperature and build time for a clean build. If the build is killed for memory, swaps continuously, or reaches 80°C, drop to 1 job; if that still fails, use route C. (`/usr/bin/time -v` is not installed in the desk Debian; check on the Pi.)

### 2. vLinker MC+ pairing on Raspberry Pi OS (OQ-5, LHU-017)

The adapter is dual mode. Linux uses the classic side. In order:

1. In the car, confirm the adapter works with a phone app. This separates adapter or car problems from Pi problems.
2. On the Pi: pair and trust the device whose name ends in "-Android" (desktop Bluetooth menu or `bluetoothctl`). Ignore the "-IOS" name (BLE).
3. `bluetoothctl info` should list the Serial Port service (UUID 0x1101).
4. Open RFCOMM channel 1 by **name**, looked up through `bluetoothctl devices Paired` at run time, from a Python `socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM)`; the same socket call in C++ is the planned `ByteTransport` for the car. The address is never typed into a file.
5. Send `ATZ`, `ATI`, `ATRV`, `ATDPN`, `0100`, `0120`, `0140` and record every reply. Do not commit the adapter's Bluetooth address.

Fallbacks, in order: the BLE serial service; a USB OBD adapter (would be a purchase, so only with a new decision).

Consequences for the design:
- The code uses only base ELM327 `AT` commands and no `ST` commands, which keeps the adapter swappable.
- The adapter pairs with a fixed PIN and stays powered in the port, so it is unplugged when not testing.
- Its request rate is unknown; measured in LHU-017. From v0.2.0 the same radio also carries audio to the car (OQ-28).

### 3. HMI on the 5-inch display (D-024)

The pixel grid is the same as the 7-inch (720x1280), but every pixel is 71% as large: 11.6 pixels per mm against 8.3. The landscape screen is 110.4 mm wide and 62.1 mm tall.

- **Rule:** sizes are defined in millimetres and converted through one pixels-per-millimetre constant.
- **Touch targets:** at least 10 mm (116 px). This applies to the hub's app icons too.
- **Primary values:** character height at least 4 mm (46 px), about 20 arcminutes at 700 mm viewing distance. The 20-arcminute figure is attributed to ISO 15008 from memory and is **unverified**.
- **Content per screen:** the hub shows the app grid and a status strip. Home shows 2 primary values and a status strip. Vehicle data shows the 8 signals of REQ-004 as a 4 x 2 grid of tiles about 27 x 27 mm (LHU-039). Diagnostics scrolls.
- These are design rules in `ARCHITECTURE.md`, checked in the HMI tests. REQ-012 is unchanged.

### 4. Display stack (D-045, closes OQ-6)

The Raspberry Pi OS desktop image with its labwc Wayland compositor is the display stack. The hub must run beside the system browser, which needs a compositor, so Qt eglfs on the bare framebuffer is rejected. The panel is rotated in the compositor configuration and touch follows (verified 2026-10-05). The open point is how the user gets back to the hub from a fullscreen browser window, since Wayland does not let one client raise itself over another; the LHU-020 spike settles it (OQ-29).

## Architecture

```
hub (QML launcher,        vehicle-data app (QML:      system browser
 app registry,             home, vehicle data,         (web apps, one process
 process manager)          diagnostics)                 per URL entry)
        \                        |
         +------- D-Bus ---------+        apps started and tracked by the hub
                     |
   vehicle-data service (one process)
     Qt D-Bus adapter <- the only Qt code in the service process
     Service layer (plain C++17): SignalStore, StalenessMonitor, ConnectionStateMachine,
                                  DerivedSignalEngine, PowerStatusProvider
       | VehicleDataSource interface (emits SignalSample, ConnectionEvent)
       +- SocketCanDbcSource   = CanFrameReader (real socket | fake) + DbcDecoder
       +- Elm327ObdSource      = ByteTransport (serial device | pseudo-terminal | fake) + Elm327Protocol + ObdPidDecoder + CommandAllowlist
       +- ReplaySource         = recorded session file
       +- GpsSource (roadmap)  = position stream from the phone
```

In v0.1.0 the service layer and the view models share one process; LHU-022 moves the service layer into its own process behind D-Bus without changing it. A signal carries value, unit, timestamp and status, shaped like a property value in a vehicle hardware abstraction layer; docs state this as an influence, not compatibility. Details: `docs/architecture/ARCHITECTURE.md`.

## Repository layout (`Lexus-Car-Device`)

```
README.md  CMakeLists.txt  CMakePresets.json  .clang-format  .clang-tidy
tests/.clang-tidy (relaxes two checks for test code)
.gitattributes  .gitignore
.github/   workflows/ci.yml  pull_request_template.md  ISSUE_TEMPLATE/{bug,ticket}.md
docs/      planning/KICKOFF_PLAN.md       requirements/REQUIREMENTS.md
           traceability/TRACEABILITY.md   architecture/ARCHITECTURE.md   adr/
           design/README.md  design/DESIGN_NOTE_TEMPLATE.md  design/DN-nnn-<short-name>.md
           safety/SAFETY_STATEMENT.md     test/TEST_STRATEGY.md
           test/MANUAL_ON_CAR_PROCEDURE.md  test/MANUAL_WEB_APPS_PROCEDURE.md  test/results/
           review/CODE_REVIEW_CHECKLIST.md  release/RELEASE_CHECKLIST.md
           measurements/ (raw CSV plus method per measurement)
src/       hardware/{can,obd,transport,gps}/  service/  service_dbus/  hmi/{viewmodels,qml}/  hub/  app/
tests/     unit/  integration/  scenarios/  vcan/  hmi/
tools/     elm327_emulator/  can_traffic_generator/  measure/  analysis/
           check_traceability.py  check_private_data.py  test_check_private_data.py
           private_data_allowlist.txt
dbc/       simulated_vehicle.dbc (invented data, labelled as such)
deploy/    PI_SETUP.md (first-boot checklist), systemd units, browser launch configuration, app registry
```

Ignored, never committed: `local_recordings/`, build output.

## Branch, review and release model (D-014 to D-020, D-043)

- `main` production, `dev` integration, both protected by rulesets. `feature/LHU-nnn-description` and `bugfix/LHU-nnn-description` off `dev`.
- PR into `dev`: required CI checks green; the review checklist filled in as a PR comment; requirement IDs and the design note (DN-nnn) in the PR body, with deviations from the design listed; squash merge by the repository owner. Required approvals are off.
- `dev` to `main`: release PR, `RELEASE_CHECKLIST.md` passed, merge commit, annotated tag. Releases are the milestones v0.1.0 Core, v0.2.0 Platform, v1.0.0 Head unit (D-043).
- The bootstrap exception is the "Initial commit" 414ff66 made by GitHub.
- All pushes are made by the repository owner.
- Commits use the GitHub no-reply identity, set in the repository's local git config.

## Requirements

REQ-001 to REQ-015 were approved with the kickoff plan; REQ-016 to REQ-022 were added by this revision. The full text with acceptance criteria is `docs/requirements/REQUIREMENTS.md`; the summary:

| ID | Requirement (summary) | Milestone |
|---|---|---|
| REQ-001 | Nothing sent to the vehicle except allowlisted OBD-II reads (Mode 01, 03, 09) and ELM327 setup commands; no Mode 04 anywhere; no CAN send path | v0.1.0 |
| REQ-002 | Service layer gets data only through `VehicleDataSource`; source chosen by configuration | v0.1.0 |
| REQ-003 | Every signal has value, unit, monotonic timestamp, status NeverReceived / Valid / Stale | v0.1.0 |
| REQ-004 | OBD source discovers supported PIDs and polls only those, from the list of 8 | v0.1.0 |
| REQ-005 | CAN source decodes every signal of the simulated DBC, checked against `cantools` | v1.0.0 |
| REQ-006 | Stale after the per-signal timeout (default 1000 ms); never drawn live | v0.1.0 |
| REQ-007 | Four-state connection state machine with a written transition table | v0.1.0 |
| REQ-008 | Link loss detected within 2 s; reconnect with backoff 1, 2, 4 s, cap 10 s | v0.1.0 |
| REQ-009 | Sample-to-screen latency p95 200 ms or less, including the D-Bus hop | v1.0.0 (measured) |
| REQ-010 | Malformed input never crashes and never yields Valid | v0.1.0 |
| REQ-011 | QML binds only to view models; view models reach vehicle data only through the client | v0.1.0, reworded v0.2.0 |
| REQ-012 | Home and vehicle-data screens show value, unit and status per signal | v0.1.0 |
| REQ-013 | Boot to first frame, provisional 15 s | v1.0.0 (measured) |
| REQ-014 | Resident memory, provisional 150 MB, re-set per process | v1.0.0 (measured) |
| REQ-015 | Record and replay raw bytes; scrub the VIN including its Mode 09 encoding | v1.0.0 |
| REQ-016 | Hub shows configured apps, starts each as a process, is visible again within 1 s of exit, never exits | v0.2.0 |
| REQ-017 | Vehicle-data service publishes signals and connection state over D-Bus; late joiners get the current state | v0.2.0 |
| REQ-018 | URL entries open the system browser full screen; video, protected audio and a game recorded as facts | v0.2.0 |
| REQ-019 | Audio from the Pi through the car stereo over Bluetooth while the OBD link stays connected | v0.2.0 |
| REQ-020 | Under-voltage and throttle flags on screen within 5 s; clean shutdown control | v0.2.0 |
| REQ-021 | Diagnostics screen: Mode 03 codes with text, Mode 09 vehicle information, connection and power status | v1.0.0 |
| REQ-022 | Derived signals: fuel economy from mass air flow and speed, trip distance, RPM bands, warm-up time | v1.0.0 |

## Test strategy

| Tier | What | Runs on |
|---|---|---|
| T1 Unit | GoogleTest for every non-UI class; sanitizer build | WSL, CI |
| T2 Integration | Service layer plus real source code against fake transport, fake CAN reader, and the ELM327 emulator over a pseudo-terminal; from v0.2.0 also two D-Bus clients against the service | WSL, CI |
| T3 vcan | Python traffic generator to `vcan0` to app; ctest label `vcan` | Pi only (CI if proven later) |
| T4 Scenarios | Scripted fault injection, list below | WSL, CI; some Pi only |
| T5 HMI | Qt Quick Test, offscreen platform; hub grid, screens, power flags | WSL, CI |
| T6 Manual | On-car procedure (parked first, results per run) and the web-apps procedure on the Pi | Car, Pi |
| Static | clang-format check, clang-tidy, warnings as errors | CI |
| Traceability | `check_traceability.py`: every REQ maps to a design element and at least one test tagged with its ID | CI |
| Privacy | `check_private_data.py`: no VIN-shaped or Bluetooth-address-shaped string in tracked files | CI |
| Thermal and power | LHU-015 logger running during every Pi bring-up session and every on-car session (LHU-016) | Pi |

Scenario coverage: adapter unplugged (emulator closes the terminal); Bluetooth drop mid-drive (emulator goes silent, then end-of-file); stale data (link up, one PID unanswered); corrupt frames (emulator garbles bytes, wrong CAN length); ignition off and on (`UNABLE TO CONNECT` / `NO DATA`, then recovery); cold boot (Pi, measured); low power (Pi `get_throttled` flags surfaced; fake provider at desk); app exit and restart (hub, fake app process); audio link active while polling (car, logged).

Measurements: one script per metric in `tools/measure/`, raw CSV committed, sample size and method stated, before and after shown, bad runs kept. Memory is measured per process from v0.2.0.

## Privacy check (D-023, delivered with LHU-005)

`tools/check_private_data.py`, Python standard library only, run by CI on every PR.

- Scans every tracked text file (`git ls-files`) and file names.
- **VIN-shaped:** a standalone 17-character run of the VIN alphabet (digits and capital letters except I, O and Q) that contains at least one letter and one digit.
- **Bluetooth-address-shaped:** six two-digit hexadecimal groups separated by colons or hyphens.
- A short allowlist file holds documented false positives, each with a reason.
- Has its own unit test, using a published sample VIN that is not a real vehicle's, held in the allowlist.
- Exit code 1 and the file, line and match on any hit; matches are masked in the output.
- Recordings: the scrub step of LHU-029 decodes Mode 09 replies before a recording leaves `local_recordings/`, because the VIN is hexadecimal there and a text search does not see it.

## Thermal and power logging (LHU-015, LHU-016, D-013, thresholds approved by D-025)

**Script:** `tools/measure/log_thermal_power.py`, Python standard library only, runs on the Pi.

**Each row:** wall-clock timestamp; seconds since boot; `vcgencmd measure_temp` in °C; `vcgencmd get_throttled` raw hex; eight decoded columns for bits 0, 1, 2, 3, 16, 17, 18, 19; Arm clock from `vcgencmd measure_clock arm`; input voltage from `vcgencmd pmic_read_adc EXT5V_V` (verified to work on this unit). Session label, power source and ambient temperature are given on the command line.

**Interval: 5 seconds.** SoC temperature changes over tens of seconds, so 5 s resolves it. Under-voltage and throttle events shorter than 5 s are still caught, because bits 16 to 19 stay set until reboot. Each session therefore starts from a fresh boot. One hour is 720 rows.

**Output:** `docs/measurements/thermal_power/<date>_<label>.csv`, plus a printed summary: sample count, minimum, median and maximum temperature, count of samples at or above each threshold, and every flag seen.

**Sessions:** desk idle 30 min; desk clean build (LHU-018); app running at desk 30 min; car parked 30 min (LHU-017); each drive (LHU-034); audio plus polling (LHU-024). Desk sessions run on the phone charger and car sessions on the power bank, so the two supplies are measured separately.

**Thresholds:**

| Measure | Pass | Warn | Fail | Why |
|---|---|---|---|---|
| Under-voltage (bit 0 or 16) | Never set | — | Set at any time | It is the firmware's own detector; any event risks a reset and SD card corruption |
| Throttling (bits 1, 2, 17, 18) | Never set | — | Set at any time | Latency and boot-time numbers measured while throttled are not valid |
| Maximum SoC temperature | Below 75°C | 75°C to below 80°C | 80°C or above | Firmware starts throttling at 80°C. 5°C of margin covers the sampling interval and a hotter car cabin |
| Soft temperature limit (bits 3, 19) | Never set | Set | — | Informational on this board |
| Input voltage, if readable | 4.75 V or above | Below 4.75 V | — | 5 V minus the 5% USB tolerance; the under-voltage flag is the hard fail |

First data, 2026-10-05, desk charger, basement room: idle 51°C, 59.3°C peak during the package upgrade, flags 0x0, 5.245 V. Pass. The car cabin and the browser under load remain the open questions.

## Design-first gate (D-033)

Before any ticket that adds or changes a component moves to In progress, its design is written down, reviewed and approved. The process and the template are in `docs/design/`.

1. **Draft.** The author writes the design note alone, time-boxed to 30 to 45 minutes, as `docs/design/DN-nnn-<short-name>.md`, where `nnn` is the ticket number. Sections, in order: problem in two sentences; clarifying questions with the assumption made for each; nouns to classes with each class's responsibility; what each class stores (constructor and fields); verbs to methods (public interface with inputs, outputs and units); interaction sequence for the main scenario; failure cases and how the design handles each; test plan mapped to requirement IDs; at least one alternative considered and why it was rejected.
2. **Design review.** A senior reviewer questions the design, one or two questions at a time, and points at gaps by asking, not by telling. The reviewer's own design is not shown until the author has defended or revised it.
3. **Comparison and revision.** The reviewer shows how they would have designed it and the key differences. The author revises the note.
4. **Approval.** The author marks the note Approved. Coding does not start before this.
5. **Commit.** The note is committed with the code PR or before it. The PR body states: "Implements DN-nnn; deviations from the design are listed with reasons."
6. **After merge.** The author adds a short "Design vs. implementation" section to the note.

**Applies to** every ticket that adds or changes a component. **Exempt:** tickets that only change documentation, the build or CI, spikes, measurement sessions and releases.

**Build-out form (D-049, 2026-10-08).** For the build-out from v0.1.0 to v1.0.0 the note is written by the implementer of the ticket, in the full or light form of the table below, marked Approved and committed in the same pull request as the code; the repository owner reviews the note and the code together after merge, and the "Design vs. implementation" section is added then. The sections, the forms and the PR sentence are unchanged; what changes is that the review happens after merge instead of before coding. Every assumption made in place of a hardware fact is listed in the note's clarifying questions and in the assumptions table of `docs/release/PI_BRINGUP_CHECKLIST.md`, and is implemented as configuration rather than a constant.

**Two forms (D-037).** The gate is tiered by how much design a ticket needs:

| Form | Tickets | Note contents | Estimated cost |
|---|---|---|---|
| Full | LHU-006, 007, 008, 010, 012, 013, 021, 022, 027, 029, 030, 031, 036 | All nine sections of the template | 1.75 h (draft 0.75, review 0.5, comparison and revision 0.25, section after merge 0.25) |
| Light | LHU-009, 011, 015, 025, 028, 039 | One page: problem, public interface, failure cases, test plan | 0.75 h (draft 15 to 20 min, review 15 min, the rest as above) |
| Exempt | LHU-001, 004, 005, 014, 016 to 020, 023, 024, 026, 032 to 035, 037, 038 | — | — |

The costs are estimates and unverified until the first two notes are done. Draft time and review time are recorded separately on each note, and the figures are re-set from the measured values.

## Tickets by milestone

Hours are estimates, unverified until the first code tickets give a velocity. "Build" is the work itself; "Gate" is the design-first gate in its full (1.75 h) or light (0.75 h) form. "Actual" is the hours entered on the board when the ticket closed. Issue numbers are on the GitHub board.

### v0.1.0 Core

| Ticket | Work | Build | Gate | Total | Actual | Status on 2026-10-08 | REQ |
|---|---|---|---|---|---|---|---|
| LHU-001 | Hardware purchased; power bank model and rated output recorded (above) | 0.25 | exempt | 0.25 | | **Done with this revision** | — |
| LHU-002 | Dev environment: Debian 13 in WSL, toolchain, build directory inside WSL, QML window via WSLg | 2.5 | exempt | 2.5 | 1.5 | **Done 2026-10-01** | — |
| LHU-003 | Repo bootstrap: folder connected to the public remote, `dev` created, first PR, rulesets, labels, Projects board | 2 | exempt | 2 | 1.0 | **Done 2026-10-02** | — |
| LHU-005 | CMake skeleton, GoogleTest, CI, design note template; check names required in both rulesets | 2.75 | exempt | 2.75 | 2.0 | **Done 2026-10-02** (PR #17) | — |
| LHU-004 | Docs baseline: requirements, traceability, test strategy, release checklist, architecture, safety statement, ADR-001, README | 4 | exempt | 4 | 1.0 | **Done 2026-10-04** (PR #18) | all |
| LHU-006 | Signal model and SignalStore with staleness; `tools/check_traceability.py` | 4 | 1.75 full | 5.75 | 2.0 (estimated) | **Done 2026-10-08** (PR #48) | 003, 006 |
| LHU-007 | Connection state machine | 3 | 1.75 full | 4.75 | 1.5 (estimated) | **Done 2026-10-08** (PR #49) | 007 |
| LHU-008 | `VehicleDataSource` interface, `SignalStoreFeeder`, FakeSource, integration suite, link-graph CI check | 2 | 1.75 full | 3.75 | 2.0 (estimated) | **Done 2026-10-08** (PR #50) | 002 |
| LHU-009 | OBD PID decoder and supported-PID bitmaps | 3 | 0.75 light | 3.75 | 1.5 (estimated) | **Done 2026-10-08** (PR #51) | 004, 010 |
| LHU-010 | ELM327 response parser, command allowlist, byte transport interface and fake | 5 | 1.75 full | 6.75 | 2.5 (estimated) | **Done 2026-10-08** (PR #52) | 001, 010 |
| LHU-011 | ELM327 emulator with fault injection (Python) | 1 | 0.75 light | 1.75 | | **Done** (DN-011, this PR) | fixture |
| LHU-012 | `Elm327ObdSource`: transport, polling loop, reconnect with backoff, integration tests against the emulator | 4 | 1.75 full | 5.75 | | Backlog | 002, 004, 008 |
| LHU-013 | QML home screen bound to a view model, live from the emulator, sized in millimetres | 3 | 1.75 full | 4.75 | | Backlog | 006, 007, 011, 012 |
| LHU-039 | Vehicle-data screen: 4 x 2 signal grid | 4 | 0.75 light | 4.75 | | Backlog (new in revision 6) | 006, 012 |
| LHU-015 | Thermal and power logger script, with a unit test of the flag decoding | 0.75 | 0.75 light | 1.5 | | Backlog | — |
| LHU-016 | Thermal and power logging in every bring-up and on-car session; CSVs committed | 1.5 | exempt | 1.5 | | Backlog | — |
| LHU-017 | vLinker MC+ pairing and first parked car session; PID support and request rate recorded | 2 | exempt | 2 | | Backlog | feeds 004 |
| LHU-018 | Build-strategy measurement on the 2GB Pi | 1 | exempt | 1 | | Backlog | — |
| LHU-019 | Plan revision 6, hardware record, Pi first-boot checklist | 1.5 | exempt | 1.5 | | **Done 2026-10-06** (PR #43) | 016 to 022 (docs) |
| LHU-040 | README front page and repository cleanup | 1 | exempt | 1 | 1.0 (estimated after the fact) | **Done 2026-10-06** (PR #45) | — |
| LHU-041 | Plan status fixes, build-out design-note process (D-049), Pi bring-up checklist created | 0.5 | exempt | 0.5 | | **Done with this revision** | — |
| LHU-014 | Release v0.1.0 Core: review, measurements so far | 1.5 | exempt | 1.5 | | Backlog | — |
| **Remaining** | | | | **49.25** | 6.5 actual on 12.25 estimated for the five closed tickets with hours recorded; LHU-001 and LHU-019 have none | | |

### v0.2.0 Platform

| Ticket | Work | Build | Gate | Total | REQ |
|---|---|---|---|---|---|
| LHU-020 | Spike: return to the hub from a fullscreen app under labwc (OQ-29) | 2 | exempt | 2 | — |
| LHU-021 | App hub launcher: registry, process manager, grid, status strip | 8 | 1.75 full | 9.75 | 016 |
| LHU-022 | Vehicle-data service over D-Bus; client library; systemd units | 6 | 1.75 full | 7.75 | 017, 002, 011 |
| LHU-023 | Web apps in the system browser; protected-audio and memory facts recorded (OQ-27) | 3 | exempt | 3 | 018 |
| LHU-024 | Bluetooth audio from the Pi to the car stereo; coexistence measured (OQ-28) | 4 | exempt | 4 | 019 |
| LHU-025 | Power status and clean shutdown | 4 | 0.75 light | 4.75 | 020 |
| LHU-026 | Release v0.2.0 Platform | 1.5 | exempt | 1.5 | — |
| **Total** | | | | **32.75** | |

### v1.0.0 Head unit

| Ticket | Work | Build | Gate | Total | REQ |
|---|---|---|---|---|---|
| LHU-027 | DBC decoder with an independent oracle; `simulated_vehicle.dbc` | 5 | 1.75 full | 6.75 | 005, 010 |
| LHU-028 | SocketCAN source and vcan tests on the Pi | 5 | 0.75 light | 5.75 | 001, 002 |
| LHU-029 | Record and replay with vehicle-identification scrub | 4 | 1.75 full | 5.75 | 015 |
| LHU-030 | Diagnostics screen: trouble codes and vehicle information | 5 | 1.75 full | 6.75 | 021 |
| LHU-031 | Trip analytics as derived signals | 7 | 1.75 full | 8.75 | 022 |
| LHU-032 | Whole-system measurements: boot, latency, memory per process; one optimisation pass | 6 | exempt | 6 | 009, 013, 014 |
| LHU-033 | arm64 release build in CI | 2 | exempt | 2 | — |
| LHU-034 | Manual on-car test procedure and first drives; mounting decided (OQ-14) | 3 | exempt | 3 | — |
| LHU-035 | Release v1.0.0: documentation, demo video, license (OQ-25) | 4 | exempt | 4 | — |
| **Total** | | | | **48.75** | |

### Roadmap (not scheduled)

| Ticket | Work | Build | Gate | Total |
|---|---|---|---|---|
| LHU-036 | GPS source from phone position and map tiles; iPhone position streaming and Qt map packaging verified first (OQ-31) | 14 | 1.75 full | 15.75 |
| LHU-037 | Offline statistics on recorded trips (`tools/analysis/`) | 10 | exempt | 10 |
| LHU-038 | Call and notification status (optional; depends on what the phone exposes) | 7 | exempt | 7 |

## Risks most likely to blow the timeline

| # | Risk | Mitigation | Change in revision 6 |
|---|---|---|---|
| 1 | ~~Hardware not arrived~~ | Arrived and brought up 2026-10-05 | **Closed** |
| 2 | Learning C++17 and Qt while writing the core | Core is plain C++ without Qt; any ticket past 2x estimate is re-planned at once: split it or cut its scope | Unchanged |
| 3 | Solo process overhead (PRs, reviews, traceability, docs, the design-first gate) | Templates, the review checklist and the traceability script; small PRs; the gate is tiered (D-037) and exempts spikes, sessions and releases | Unchanged |
| 4 | vLinker MC+ pairing on Raspberry Pi OS: dual-mode adapter, clone-grade ELM327 behaviour | Ordered pairing steps (LHU-017); RFCOMM socket by name behind `ByteTransport`; base `AT` commands only | BlueZ 5.82 and `rfcomm` verified present on the image |
| 5 | Portrait-native 5-inch display: rotation and touch, small physical size | Rotation and touch verified under labwc (D-045); sizes in millimetres (D-024) | **Reduced**: verified on the unit |
| 6 | GS350 unknowns: supported PIDs (including mass air flow for REQ-022), achievable poll rate | Supported-PID discovery is a requirement; measured in LHU-017 | Unchanged |
| 7 | Power. Desk charger below the 5 A the Pi 5 prefers; the power bank may sag, cut out at low current, or lose power abruptly and corrupt the SD card | Both supplies measured separately (LHU-016); power bank recorded (A1383); clean shutdown and on-screen flags (REQ-020) | Desk charger passed first boot; bank unverified |
| 8 | Heat: no cooler, Pi on the back of a warm display, car cabin, browser under load | Measured against the approved thresholds (D-025); a fail becomes a bug with evidence | First data inside the pass band; cabin and browser load open |
| 9 | 2GB RAM: builds on the Pi run out of memory or swap heavily | 2 jobs, zram 2 GiB verified, measured by LHU-018; CI arm64 build as fallback (D-022) | Unchanged |
| 10 | Qt version skew between desk and Pi | Both verified at 6.8.2 | **Closed** |
| 11 | Boot-time and memory optimisation becoming open-ended | Baseline first; fixed list of levers; one pass in LHU-032 | Unchanged |
| 12 | vcan unavailable in CI | vcan tier on Pi only; decode logic covered by fakes | Unchanged |
| 13 | Repository in a Windows folder with a space in its path, built from WSL | Build directory inside WSL; LF forced; Windows git only | Unchanged |
| 14 | Public from day one: private details, the VIN or Bluetooth addresses become permanently visible | No-reply commit identity; privacy CI check; ignored `local_recordings/`; adapter addressed by name at run time | Unchanged |
| 15 | Bare board with no case in a car | OQ-14, decided in LHU-034 before the first drive | Unchanged |
| 16 | **Returning to the hub from a fullscreen browser under Wayland** may need compositor cooperation that Qt does not expose directly | LHU-020 spike before DN-021; fallbacks: compositor overlay bar, the system panel, browser app mode without kiosk | **New** |
| 17 | **Browser memory on 2 GB**: the system browser with video may take 400 to 800 MB and squeeze the service and the hub | Per-process budgets (REQ-014); browser cache in RAM; measured in LHU-023 and LHU-032; a fail becomes a bug | **New** |
| 18 | **One Bluetooth radio for OBD polling and audio**: dropouts or link loss | Measured with logged causes (REQ-019, LHU-024); if it fails, audio is dropped before the vehicle path is touched | **New** |
| 19 | **Protected audio may not play** in the system browser on arm64 | Recorded as a fact (OQ-27); video and games do not depend on it; no purchase follows | **New** |
| 20 | **No date** (D-043): an open-ended schedule removes the forcing function | Milestones with release checklists; one-week sprints on the board; the "resume-ready" point named (v0.2.0 plus LHU-030) | **New** |

## Standing technical positions

1. One frame-level interface over vcan/DBC and ELM327 does not work; the interface is at the signal level (D-001).
2. Simulated CAN alone would leave the car's code path untested until the car; hence the ELM327 emulator (D-002).
3. The scope is larger than three weeks; it is organised as milestones and worked until done (D-005, D-043).
4. "Read-only" is defined by an allowlist, because OBD requests are frames transmitted on the bus (D-009). The allowlist did not grow with the scope: Mode 02 (freeze frame) was considered for the diagnostics screen and left out.
5. "Speed within 200 ms of the source update" cannot be measured end to end; REQ-009 measures from source receipt to rendered frame, including the D-Bus hop, and adapter round-trip time is reported separately.
6. This is ASPICE-inspired practice with ISO 26262 awareness, not compliance, and the docs say so.
7. The DBC describes an invented vehicle and is labelled so. Decoding real broadcast frames is not planned before v1.0.0.
8. Keeping the repository on the Windows drive costs some build speed and needs line-ending care (risk 13).
9. No cooler and building on the Pi pull against each other (risks 8 and 9); the first clean build on the Pi is the likeliest first threshold failure.
10. The platform shape (hub, central vehicle-data service, apps) is described as the shape of an automotive infotainment platform in Linux terms; it is an influence, not a claim of compatibility with any product (D-042).
11. Content that runs in a browser is content, not product code; the product is the hub, the service, the vehicle-data path and the measurements (D-042).
12. The vehicle-data service is the only process that talks to the vehicle, so the read-only guarantee lives in one process (REQ-001, REQ-017).
