# Lexus Head Unit — project plan

**Revision 4, 2026-10-02.**
- Revision 1: kickoff plan, approved by Samer.
- Revision 2: hardware as purchased, public repository, review and protection model (D-011 to D-018).
- Revision 3: approvals D-019 to D-027 applied; desk environment installed and verified.
- Revision 4: design-first gate added (D-033); sprint 1 estimates and statuses updated; LHU-005 before LHU-004 (D-032).

The canonical copy of this file is `docs/planning/KICKOFF_PLAN.md` in the repository. Ticket LHU-004 splits it into the other `docs/` files.

## Context

A plug-in, read-only infotainment head unit for a 2013 Lexus GS350, built as a portfolio project to demonstrate infotainment software skills: C++17, Qt/QML, Linux, CAN and OBD-II integration, testing on bench hardware, performance on constrained hardware, and an ASPICE-inspired process. It is not a safety-rated product.

Standing decisions: about 35 hours per week; MVP by 2026-10-22, then continue; both data paths behind a signal-level interface; vcan tests on the Pi with in-process fakes on WSL; public repository; power and heat measured, not assumed.

## Hardware as purchased (D-011, D-012, D-026)

| Part | Purchased |
|---|---|
| Computer | Raspberry Pi 5, 2GB |
| Display | Official Touch Display 2, 5-inch, 720x1280 native portrait |
| Storage | TOPESEL 32GB microSD |
| OBD adapter | Vgate vLinker MC+ (Bluetooth, ELM327/ELM329/STN compatible) |
| Case | None; the Pi mounts to the back of the display |
| Cooling | None |
| Power, car | Anker 20,000 mAh USB-C power bank; model to be confirmed (OQ-3) |
| Power, desk | 5 V / 3 A USB-C phone charger |

Nothing has arrived as of 2026-10-01.

## Evidence

| Item | Finding | Status |
|---|---|---|
| Dev machine | Windows 11 Home, Core Ultra 7 155U, 15.4 GB RAM | Verified |
| WSL2 | WSL 2.5.7, kernel 6.6.87.1, WSLg 1.0.66 | Verified |
| Debian distro | Debian GNU/Linux 13 (trixie), 13.5, amd64, systemd enabled | Verified |
| Desk toolchain | g++ 14.2.0, CMake 3.31.6, Ninja 1.12.1, gdb 16.3, clang / clang-tidy / clang-format 19.1.7, GoogleTest 1.16.0-1, Qt 6.8.2, Python 3.13.5 | Verified |
| Desk environment check | See "Desk environment" below | Verified, 1 run each |
| vcan in WSL2 | `modinfo vcan`: not found; no CONFIG_CAN_VCAN | Verified absent |
| Local repository | `origin/main` is one commit, 414ff66 "Initial commit"; `main`, `dev` and the first feature branch created locally | Verified |
| GitHub protection | Classic branch protection works on public Free repos. Rulesets on a personal Free public repo expected to work | Classic verified from docs; rulesets unverified until the settings page is opened |
| GitHub arm64 runners | `ubuntu-24.04-arm` free for public repos | Unverified by us |
| vcan on GitHub-hosted runners | Not shipped; fragile to add | Unverified by us |
| vLinker MC+ | Dual mode, Bluetooth 3.0 classic plus BLE 4.0; classic name "vLinker MC-Android"; BLE name "vLinker MC-IOS" | Unverified until paired |
| Pi 5 power input | Raspberry Pi docs: 5 V at 5 A, or 5 V at 3 A with a 600 mA USB peripheral limit | Docs verified; our supplies unverified |
| Pi 5 thermal | Docs: progressive throttling from 80°C, full at 85°C. Raspberry Pi's test: uncooled idle about 65°C on an air-conditioned bench; uncooled stress settles just above 85°C, throttled | Docs verified; our unit unverified |
| `vcgencmd get_throttled` | Bits 0 to 3: under-voltage, frequency capped, throttled, soft temperature limit (now). Bits 16 to 19: the same, "has occurred" since boot | Verified from docs |
| Raspberry Pi OS swap | Trixie reportedly ships zram swap backed by a file, RAM x 1 up to 2048 MiB | Unverified until booted |
| Touch Display 2, 5-inch | Active area 62.1 x 110.4 mm, 11.6 pixels per mm | Verified from the product page |
| 2013 GS350 OBD | CAN-based; protocol and PIDs not confirmed | Unverified until queried |

## Desk environment (LHU-002, D-021)

Debian 13 in WSL2. Source stays in `C:\Lexus Car Device` (`/mnt/c/Lexus Car Device` from WSL). The build directory is `~/build/lexus-car-device` inside WSL. Git runs from Windows only.

Packages installed, by approved set:

| Set | Packages named | Versions | apt result |
|---|---|---|---|
| A, build | build-essential, cmake, ninja-build, gdb, pkg-config | 12.12, 3.31.6-2, 1.12.1-1, 16.3-1, 1.8.1-4 | 111 new, 6 upgraded, 136 MB download, 495 MB on disk |
| B, quality | clang, clang-tidy, clang-format, libgtest-dev, libgmock-dev | 1:19.0-63 (19.1.7), 1.16.0-1 | 42 new, 143 MB download, 874 MB on disk |
| C, Qt | qt6-base-dev, qt6-declarative-dev, qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-layouts, qml6-module-qttest | 6.8.2 | 223 new, 1 upgraded, 96.3 MB download, 499 MB on disk |

Package count went from 157 to 533. Disk use went from 261 MB to 2.7 GB.

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
- The packages were installed as root because the distro has no normal user yet (OQ-16).

## Approved assessments

### 1. Building for the 2GB Pi (D-022)

The dev laptop is x86-64 and the Pi is arm64, so a desk build cannot simply be copied over.

| Route | How | For | Against |
|---|---|---|---|
| A. Build on the Pi | `cmake --build` with 2 parallel jobs | Simplest; exact target environment; debug where it runs | Uncooled Pi will run hot while compiling; slower |
| B. arm64 container on the laptop | `debian:trixie` arm64 image under Docker emulation, copy the binary over SSH | No load on the Pi | Emulated compile is slow (factor unverified); Raspberry Pi OS Qt packages may differ from Debian's; about 2 hours of setup |
| C. arm64 build in CI | `ubuntu-24.04-arm` runner, `debian:trixie` container, download the artifact | Native-speed arm64; reproducible | Minutes per iteration; same package-mismatch risk as B |

**Decision:** route A day to day with 2 jobs and the default Raspberry Pi OS zram swap. Route C as the release build and the fallback. Route B is not planned. True cross-compilation with a sysroot is rejected.

Do not use a plain swap file on the SD card as working memory: it is slow and wears the card. If the default swap turns out smaller than reported or absent, set zram to 2048 MiB.

**Switch trigger, measured by LHU-018:** record peak memory (`/usr/bin/time -v`), swap activity (`vmstat`), temperature and build time for a clean build. If the build is killed for memory, swaps continuously, or reaches 80°C, drop to 1 job; if that still fails, use route C.

### 2. vLinker MC+ pairing on Raspberry Pi OS (OQ-5, unverified until hardware arrives)

The adapter is dual mode. Linux should use the classic side. Try in this order:

1. In the car, confirm the adapter works with a phone app. This separates adapter or car problems from Pi problems.
2. On the Pi: `bluetoothctl`, then `power on`, `agent on`, `default-agent`, `scan on`. Look for "vLinker MC-Android". Ignore "vLinker MC-IOS" (BLE).
3. `pair <address>` with the adapter's default PIN, then `trust <address>`.
4. `bluetoothctl info <address>` should list the Serial Port service (UUID 0x1101).
5. Open RFCOMM channel 1: `sudo rfcomm bind 0 <address> 1` if the `rfcomm` tool is installed. If it is not, open a Python `socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM)`; the same socket call in C++ is the planned `ByteTransport` for the car.
6. Send `ATZ`, `ATI`, `ATRV`, `ATDPN`, `0100` and record every reply. Do not commit the adapter's Bluetooth address.

Fallbacks, in order: the BLE serial service; a USB OBD adapter.

Consequences for the design:
- The code uses only base ELM327 `AT` commands and no `ST` commands, which keeps the adapter swappable.
- The adapter pairs with a fixed PIN and stays powered in the port, so it is unplugged when not testing.
- Its request rate is unknown; measured in sprint 2.

### 3. HMI on the 5-inch display (D-024)

The pixel grid is the same as the 7-inch (720x1280), but every pixel is 71% as large: 11.6 pixels per mm against 8.3. The landscape screen is 110.4 mm wide and 62.1 mm tall.

- **Rule:** sizes are defined in millimetres and converted through one pixels-per-millimetre constant.
- **Touch targets:** at least 10 mm (116 px).
- **Primary values:** character height at least 4 mm (46 px), about 20 arcminutes at 700 mm viewing distance. The 20-arcminute figure is attributed to ISO 15008 from memory and is **unverified**.
- **Content per screen:** home shows 2 primary values and a status strip. Vehicle data shows the 8 signals of REQ-004 as a 4 x 2 grid of tiles about 27 x 27 mm. Diagnostics scrolls.
- These are design rules in `ARCHITECTURE.md`, checked in the HMI tests. REQ-012 is unchanged.

## Architecture

```
QML screens (home, vehicle data, diagnostics)
   | bindings only
View models (QObject, Q_PROPERTY)            <- Qt boundary, UI thread
   | queued signal, thread hop
Service layer (plain C++17): SignalStore, StalenessMonitor, ConnectionStateMachine
   | VehicleDataSource interface (emits SignalSample, ConnectionEvent)
   +- SocketCanDbcSource   = CanFrameReader (real socket | fake) + DbcDecoder
   +- Elm327ObdSource      = ByteTransport (serial device | pseudo-terminal | fake) + Elm327Protocol + ObdPidDecoder + CommandAllowlist
   +- ReplaySource         = recorded session file
```

A signal carries value, unit, timestamp and status, shaped like an Android VHAL `VehiclePropValue`; docs state this mapping.

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
           test/MANUAL_ON_CAR_PROCEDURE.md  test/results/
           review/CODE_REVIEW_CHECKLIST.md  release/RELEASE_CHECKLIST.md
           measurements/ (raw CSV plus method per measurement)
src/       hardware/{can,obd,transport}/  service/  hmi/{viewmodels,qml}/  app/
tests/     unit/  integration/  scenarios/  vcan/  hmi/
tools/     elm327_emulator/  can_traffic_generator/  measure/
           check_traceability.py  check_private_data.py  test_check_private_data.py
           private_data_allowlist.txt
dbc/       simulated_vehicle.dbc (invented data, labelled as such)
deploy/    systemd unit, Pi setup notes
```

Ignored, never committed: `local_recordings/`, build output.

## Branch, review and release model (D-014 to D-020)

- `main` production, `dev` integration, both protected by rulesets. `feature/LHU-nnn-description` and `bugfix/LHU-nnn-description` off `dev`.
- PR into `dev`: required CI checks green; the review checklist filled in as a PR comment; requirement IDs and the design note (DN-nnn) in the PR body, with deviations from the design listed; squash merge by Samer. Required approvals are off.
- `dev` to `main`: release PR, `RELEASE_CHECKLIST.md` passed, merge commit, annotated tag. Planned: v0.1.0 (sprint 1), v0.2.0 (sprint 2), v1.0.0 (sprint 3).
- The bootstrap exception is the "Initial commit" 414ff66 made by GitHub.
- All pushes are made by the repository owner.
- Commits use the GitHub no-reply identity, set in the repository's local git config.
- **First PR (LHU-003)** contains: `docs/planning/KICKOFF_PLAN.md`, `.gitattributes`, `.gitignore`, `.github/pull_request_template.md`, `docs/review/CODE_REVIEW_CHECKLIST.md`, `.github/ISSUE_TEMPLATE/bug.md`, `.github/ISSUE_TEMPLATE/ticket.md`.

## First 15 requirements

| ID | Requirement | Acceptance criterion |
|---|---|---|
| REQ-001 | The system sends nothing to the vehicle except allowlisted OBD-II read requests (Mode 01, 03, 09) and ELM327 setup commands | Unit test offers all 256 mode values; only allowlisted ones reach the transport, 0 bytes written otherwise. SocketCAN source never calls a send function (fake reader fails the test on write) |
| REQ-002 | Service layer gets data only through the `VehicleDataSource` interface; source chosen at startup by configuration | Same integration suite passes against each source with no recompile; service and HMI targets have no link dependency on concrete sources (checked in CI) |
| REQ-003 | Every signal exposes value, unit, monotonic timestamp (1 ms resolution) and status: NeverReceived, Valid, Stale | Unit tests cover each field and each status transition |
| REQ-004 | OBD source discovers supported PIDs (PID 0x00 bitmap) and polls only supported ones from: speed 0x0D, RPM 0x0C, coolant 0x05, engine load 0x04, throttle 0x11, intake air 0x0F, module voltage 0x42, fuel level 0x2F | Decoded values equal SAE J1979 formulas for minimum, maximum and 3 mid vectors per PID; unsupported PID is never requested |
| REQ-005 | CAN source decodes every signal in `simulated_vehicle.dbc` (both byte orders, signed, unsigned, scale, offset) | 10,000 or more random frames; 100% agree with Python `cantools` within 1e-6; mismatches listed in full |
| REQ-006 | A signal becomes Stale when no update arrives within its timeout (default 1000 ms, per-signal configurable) | Transition occurs between timeout and timeout + 100 ms; HMI never shows a stale value as live |
| REQ-007 | Connection state machine: Disconnected, Connecting, Connected, Error, with a defined transition table | Every legal transition tested; every illegal one rejected; state shown on HMI within 500 ms |
| REQ-008 | Link loss is detected within 2 s and reconnection retried with backoff (1, 2, 4 s, cap 10 s) without app restart | Emulator killed and restarted, 20 trials: reconnect within 15 s of emulator return in 20 of 20 |
| REQ-009 | Sample-to-screen latency, from source receipt to the rendered frame showing it | 95th percentile 200 ms or less over 1,000 or more samples; report min, median, p95, p99, max and raw data |
| REQ-010 | Malformed input never crashes and never yields a Valid sample: ELM327 `NO DATA`, `?`, `CAN ERROR`, `BUFFER FULL`, `STOPPED`, `UNABLE TO CONNECT`, truncated or non-hex text; CAN frames with wrong length | Named corpus tests plus 100,000 random byte strings under AddressSanitizer and UndefinedBehaviorSanitizer: 0 crashes, 0 Valid samples; error counter increments |
| REQ-011 | QML never touches hardware or service classes directly, only view models | HMI target links only to view-model library; CI check fails on a forbidden include |
| REQ-012 | Home and vehicle data screens show each signal's value, unit and status | Qt Quick Test with a fake view model verifies displayed text and stale styling for every signal |
| REQ-013 | Boot to first rendered home screen frame on the Pi | Provisional 15 s or less, 10 or more cold boots, all values reported; target re-set after baseline (OQ-8) |
| REQ-014 | Application memory on the Pi | Provisional resident memory 150 MB or less at 30 min; growth from minute 5 to minute 30 under 5%; 1 sample per 10 s, raw data kept |
| REQ-015 | Raw transport bytes can be recorded with timestamps and replayed through `ReplaySource` | Replaying a recording reproduces the identical signal sample sequence (values and order) |

Diagnostics screen, power status and clean shutdown become REQ-016 onward in sprint 2 planning.

## Test strategy

| Tier | What | Runs on |
|---|---|---|
| T1 Unit | GoogleTest for every non-UI class; sanitizer build | WSL, CI |
| T2 Integration | Service layer plus real source code against fake transport, fake CAN reader, and the ELM327 emulator over a pseudo-terminal | WSL, CI |
| T3 vcan | Python traffic generator to `vcan0` to app; ctest label `vcan` | Pi only (CI if proven later) |
| T4 Scenarios | Scripted fault injection, list below | WSL, CI; some Pi only |
| T5 HMI | Qt Quick Test, offscreen platform | WSL, CI |
| T6 On-car manual | Written procedure, parked first, results recorded per run in `docs/test/results/` | Car |
| Static | clang-format check, clang-tidy, warnings as errors | CI |
| Traceability | `check_traceability.py`: every REQ maps to a design element and at least one test tagged with its ID | CI |
| Privacy | `check_private_data.py`: no VIN-shaped or Bluetooth-address-shaped string in tracked files | CI |
| Thermal and power | LHU-015 logger running during every Pi bring-up session and every on-car session | Pi |

Scenario coverage: adapter unplugged (emulator closes the terminal); Bluetooth drop mid-drive (emulator goes silent, then end-of-file); stale data (link up, one PID unanswered); corrupt frames (emulator garbles bytes, wrong CAN length); ignition off and on (`UNABLE TO CONNECT` / `NO DATA`, then recovery); cold boot (Pi, measured); low power (Pi `get_throttled` flags surfaced; fake provider at desk).

Measurements: one script per metric in `tools/measure/`, raw CSV committed, sample size and method stated, before and after shown, bad runs kept.

## Privacy check (D-023, delivered with LHU-005)

`tools/check_private_data.py`, Python standard library only, run by CI on every PR.

- Scans every tracked text file (`git ls-files`).
- **VIN-shaped:** a standalone 17-character run of the VIN alphabet (digits and capital letters except I, O and Q) that contains at least one letter and one digit.
- **Bluetooth-address-shaped:** six two-digit hexadecimal groups separated by colons or hyphens.
- A short allowlist file holds documented false positives, each with a reason.
- Has its own unit test, using a published sample VIN that is not a real vehicle's, held in the allowlist.
- Exit code 1 and the file, line and match on any hit.

## Thermal and power logging (LHU-015, D-013, thresholds approved by D-025)

**Script:** `tools/measure/log_thermal_power.py`, Python standard library only, runs on the Pi.

**Each row:** wall-clock timestamp; seconds since boot; `vcgencmd measure_temp` in °C; `vcgencmd get_throttled` raw hex; eight decoded columns for bits 0, 1, 2, 3, 16, 17, 18, 19; Arm clock from `vcgencmd measure_clock arm`; input voltage from `vcgencmd pmic_read_adc EXT5V_V` if the command exists on the unit (unverified). Session label, power source and ambient temperature are given on the command line.

**Interval: 5 seconds.** SoC temperature changes over tens of seconds, so 5 s resolves it. Under-voltage and throttle events shorter than 5 s are still caught, because bits 16 to 19 stay set until reboot. Each session therefore starts from a fresh boot. One hour is 720 rows.

**Output:** `docs/measurements/thermal_power/<date>_<label>.csv`, plus a printed summary: sample count, minimum, median and maximum temperature, count of samples at or above each threshold, and every flag seen.

**Sessions:** desk idle 30 min; desk clean build; app running at desk 30 min; car parked 30 min; each drive. Desk sessions run on the phone charger and car sessions on the power bank, so the two supplies are measured separately.

**Thresholds:**

| Measure | Pass | Warn | Fail | Why |
|---|---|---|---|---|
| Under-voltage (bit 0 or 16) | Never set | — | Set at any time | It is the firmware's own detector; any event risks a reset and SD card corruption |
| Throttling (bits 1, 2, 17, 18) | Never set | — | Set at any time | Latency and boot-time numbers measured while throttled are not valid |
| Maximum SoC temperature | Below 75°C | 75°C to below 80°C | 80°C or above | Firmware starts throttling at 80°C. 5°C of margin covers the sampling interval and a hotter car cabin |
| Soft temperature limit (bits 3, 19) | Never set | Set | — | Informational on this board |
| Input voltage, if readable | 4.75 V or above | Below 4.75 V | — | 5 V minus the 5% USB tolerance; the under-voltage flag is the hard fail |

Expectation, stated before measuring: Raspberry Pi reports about 65°C idle for an uncooled Pi 5 in an air-conditioned room, and throttling under sustained full load. So idle should pass, a build on the Pi may warn or fail, and a hot car is the open question. A fail is logged as a bug with the CSV attached; the fix is decided then.

## Design-first gate (D-033)

Before any ticket that adds or changes a component moves to In progress, its design is written down, reviewed and approved. The process and the template are in `docs/design/`.

1. **Draft.** Samer writes the design note alone, time-boxed to 30 to 45 minutes, as `docs/design/DN-nnn-<short-name>.md`, where `nnn` is the ticket number. Sections, in order: problem in two sentences; clarifying questions with the assumption made for each; nouns to classes with each class's responsibility; what each class stores (constructor and fields); verbs to methods (public interface with inputs, outputs and units); interaction sequence for the main scenario; failure cases and how the design handles each; test plan mapped to requirement IDs; at least one alternative considered and why it was rejected.
2. **Design review.** A senior reviewer questions the design, one or two questions at a time, and points at gaps by asking, not by telling. The reviewer's own design is not shown until Samer has defended or revised his.
3. **Comparison and revision.** The reviewer shows how they would have designed it and the key differences. Samer revises the note.
4. **Approval.** Samer marks the note Approved. Coding does not start before this.
5. **Commit.** The note is committed with the code PR or before it. The PR body states: "Implements DN-nnn; deviations from the design are listed with reasons."
6. **After merge.** Samer adds a short "Design vs. implementation" section to the note.

**Applies to:** LHU-006 to LHU-013 and LHU-015 in sprint 1, and every later ticket that adds or changes a component. **Exempt:** tickets that only change documentation, the build or CI, including LHU-004 and LHU-005.

**Cost, estimated and unverified until the first two notes are done:** 1.75 hours per ticket (draft 0.75, review 0.5, comparison and revision 0.25, section after merge 0.25). Draft time and review time are recorded separately on each note, and the figure is re-set at the mid-sprint checkpoint from the measured values.

## Sprint 1 (Thu 2026-10-01 to Wed 2026-10-07, desk only, no hardware needed)

Hours are Samer's hours and are estimates, unverified until the first tickets give a velocity. "Build" is the estimate for the work itself; "Design gate" is the 1.75 hours of the design-first gate (D-033).

| Ticket | Work | Build | Design gate | Total | Status on 2026-10-02 | REQ |
|---|---|---|---|---|---|---|
| LHU-001 | Hardware is purchased. Remaining: record the power bank model and rated output (OQ-3) | 0.25 | exempt | 0.25 | Ready | — |
| LHU-002 | Dev environment: Debian 13 in WSL, toolchain, build directory inside WSL, QML window via WSLg | 2.5 | exempt | 2.5 | **Done 2026-10-01** | — |
| LHU-003 | Repo bootstrap: folder connected to the public remote, `dev` created, first PR, rulesets, labels, Projects board with all tickets | 2 | exempt | 2 | **Done 2026-10-02** | — |
| LHU-005 | CMake skeleton, GoogleTest, CI (build, test, format, tidy, sanitizers, privacy check), design note template; then the check names are added to the rulesets. Done before LHU-004 (D-032) | 2.75 (was 1.75) | exempt | 2.75 | In progress | — |
| LHU-004 | Docs baseline: requirements, traceability, test strategy, release checklist, architecture, safety statement, ADR-001 | 3 (was 2) | exempt | 3 | Ready | all |
| LHU-006 | Signal model and SignalStore with staleness | 4 | 1.75 | 5.75 | Backlog | 003, 006 |
| LHU-007 | Connection state machine | 3 | 1.75 | 4.75 | Backlog | 007 |
| LHU-008 | `VehicleDataSource` interface and FakeSource | 2 | 1.75 | 3.75 | Backlog | 002 |
| LHU-009 | OBD PID decoder | 3 | 1.75 | 4.75 | Backlog | 004 |
| LHU-010 | ELM327 response parser and command allowlist | 5 | 1.75 | 6.75 | Backlog | 001, 010 |
| LHU-011 | ELM327 emulator with fault injection (Python) | 1 | 1.75 | 2.75 | Backlog | — |
| LHU-012 | `Elm327ObdSource`: transport, polling loop, integration tests against emulator | 4 | 1.75 | 5.75 | Backlog | 002, 008 |
| LHU-014 | Sprint review, measurements so far, release v0.1.0 | 1.5 | exempt | 1.5 | Backlog | — |
| LHU-015 | Thermal and power logger script, with a unit test of the flag decoding against a fake command runner | 0.75 | 1.75 | 2.5 | Backlog | — |
| **Committed** | | **34.75** | **14** | **48.75** | 4.5 done, 44.25 remaining | |
| LHU-013 (stretch) | QML home screen bound to a view model, live from the emulator, sized in millimetres | 3 | 1.75 | 4.75 | Backlog | 011, 012 |

**Capacity is 35 hours. The committed total is 48.75 hours, 13.75 hours over.** Before the gate the committed total was 34.75 hours. Sprint 1 therefore does not fit as listed: which tickets move to sprint 2, or which tickets get a shorter form of the gate, is decided before LHU-006 starts and recorded in the next revision of this plan.

Mid-sprint checkpoint after LHU-007: compare actual to estimated hours and re-plan.

## Sprint 2 outline (Oct 8 to 14)

| Ticket | Work | Hours |
|---|---|---|
| LHU-016 | Run the thermal and power logger in every bring-up and on-car session; commit the CSVs; raise bugs on any fail | 1.5 |
| LHU-017 | vLinker MC+ pairing spike, following the ordered list above; record every reply | 2 |
| LHU-018 | Build-strategy measurement on the 2GB Pi: peak memory, swap activity, temperature, build time at 2 jobs | 1 |
| (to be numbered) | DBC decoder and SocketCAN source with `cantools` oracle; Pi bring-up and display spike; vcan tier; record and replay; arm64 CI build; parked on-car test; first short drives | — |

Sprint 3 outline (Oct 15 to 22): boot and memory baseline then fixed-list optimization, diagnostics screen, docs, demo video, v1.0.0.

## Risks most likely to blow the timeline

| # | Risk | Mitigation | Change in revision 3 |
|---|---|---|---|
| 1 | Hardware purchased but not arrived; week 2 depends on delivery | Sprint 1 needs none; the emulator path keeps desk work going if parts are late | Unchanged |
| 2 | Learning C++17 and Qt while writing the core | Core is plain C++ without Qt; any ticket past 2x estimate is re-planned at once: split it or cut its scope | Unchanged |
| 3 | Solo process overhead (PRs, reviews, traceability, docs, and from revision 4 the design-first gate) | Templates, the review checklist and the traceability script; small PRs; about 20% of hours budgeted for PRs, reviews and docs. The gate adds an estimated 1.75 hours per component ticket: 14 hours on the 8 committed sprint 1 tickets, which is 40% of one week's capacity. Drafts are time-boxed; the cost is measured on the first two notes and the plan re-set at the mid-sprint checkpoint | **Raised in revision 4** (D-033) |
| 4 | vLinker MC+ pairing on Raspberry Pi OS: dual-mode adapter, `rfcomm` tool deprecated, clone-grade ELM327 behaviour | Ordered pairing spike (LHU-017); direct RFCOMM socket behind `ByteTransport`; base `AT` commands only; USB adapter as last resort | Unchanged |
| 5 | Portrait-native 5-inch display: rotation and touch under Qt, and small physical size | Day-one spike on the Pi; kiosk compositor as fallback; sizes in millimetres (D-024) | Unchanged |
| 6 | GS350 unknowns: supported PIDs, achievable poll rate | Supported-PID discovery is a requirement; priority polling; latency measured from source receipt | Unchanged |
| 7 | Power. Desk: a 5 V / 3 A phone charger is below the 5 A the Pi 5 prefers, and its real output is unknown. Car: the power bank may sag, cut out, or lose power abruptly and corrupt the SD card | Both supplies measured separately by LHU-015 and LHU-016; a fail becomes a bug with evidence; clean shutdown control; power bank model and ratings recorded (OQ-3) | **Rewritten** for D-026: the desk no longer depends on the bank, so the "Pi dies when the bank needs charging" concern is gone |
| 8 | Heat: no cooler, Pi on the back of a warm display, car cabin. Raspberry Pi's own data shows an uncooled Pi 5 throttles under sustained load | Measured by LHU-015 and LHU-016 against the approved thresholds (D-025); a fail becomes a bug with evidence | Thresholds now approved |
| 9 | 2GB RAM: builds on the Pi run out of memory or swap heavily | 2 jobs, default zram, measured by LHU-018; CI arm64 build as fallback (D-022) | Strategy now approved |
| 10 | Qt version skew between desk and Pi | Desk is now Debian 13 with Qt 6.8.2 (verified); CI will use the same; the Pi's version is confirmed at bring-up | **Reduced**: desk side verified |
| 11 | Boot-time optimization becoming open-ended | Baseline first; fixed list of levers; one-day time box | Unchanged |
| 12 | vcan unavailable in CI | vcan tier on Pi only; decode logic covered by fakes | Unchanged |
| 13 | Repository in a Windows folder with a space in its path, built from WSL | Build directory inside WSL; `.gitattributes` forcing LF; Windows git only; quoted paths. A throwaway project in a folder with a space configured and built correctly | **Reduced**: verified once on a small project; build speed on the real project still unmeasured |
| 14 | Public from day one: private details, the VIN or Bluetooth addresses become permanently visible | No-reply commit identity; privacy rule and CI check (D-023); ignored `local_recordings/` folder | Mitigations now decided; the CI check does not exist until LHU-005 |
| 15 | Bare board with no case in a car: shorts against metal, connector strain, no fixed mounting | OQ-14, deferred by D-027 until before the first on-car test | Deferred, stays on this list |

## Standing technical positions

1. One frame-level interface over vcan/DBC and ELM327 does not work; the interface is at the signal level (D-001).
2. Simulated CAN alone would leave the car's code path untested until the car; hence the ELM327 emulator (D-002).
3. The full scope does not fit 3 weeks; the MVP is defined and work continues after (D-005).
4. "Read-only" is defined by an allowlist, because OBD requests are frames transmitted on the bus (D-009).
5. "Speed within 200 ms of the source update" cannot be measured end to end; REQ-009 measures from source receipt to rendered frame, and adapter round-trip time is reported separately.
6. This is ASPICE-inspired practice with ISO 26262 awareness, not compliance, and the docs say so.
7. The DBC describes an invented vehicle and is labelled so. Decoding real broadcast frames is after the MVP.
8. Keeping the repository on the Windows drive costs some build speed and needs line-ending care (risk 13).
9. No cooler and building on the Pi pull against each other (risks 8 and 9); the first clean build on the Pi is the likeliest first threshold failure.
10. Status checks cannot be required until CI exists (LHU-005), so the rulesets go in without them and are edited afterwards.
