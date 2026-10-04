# Safety statement

This document says what the Lexus Head Unit can and cannot do to the vehicle, how that is enforced, and what this project does and does not claim. It is written for someone deciding whether to plug the device into their own car, and for a reviewer asking how "read-only" is more than a promise.

## 1. What this product is not

**This is not a safety-rated product.** It is a portfolio project built by one person to demonstrate infotainment software skills. It has not been developed under, assessed against, or certified to ISO 26262, ASPICE, or any other automotive or functional-safety standard, and this repository makes no such claim anywhere.

What the project does take from those standards is practice, chosen because it is good engineering and because it shows awareness of how automotive software is actually built: numbered, testable requirements; a traceability matrix from requirement to design element to test; written design before code; written reviews with a checklist; measurements with stated sample sizes and raw data kept; a hazard list with mitigations. Where a document uses a term from a standard, it describes what this project did, not a level it reached. A sentence like "ASPICE-inspired" means inspired by, and nothing more.

The head unit has no safety function. It does not control, and cannot influence, braking, steering, propulsion, lighting, airbags, driver assistance or any other vehicle system. If it fails, freezes, shows a wrong value or loses power, the vehicle is unaffected: the factory instrument cluster and head unit remain the authority for every reading, and the driver must treat this device's values as informational.

## 2. Read-only toward the vehicle

The device connects to the vehicle through the OBD-II diagnostic port only. On that port it does one kind of thing: it sends standard diagnostic **read** requests and listens to the answers.

### 2.1 What is sent

Exactly these, and nothing else (REQ-001, decision D-009):

| Sent | What it is | Effect on the vehicle |
|---|---|---|
| OBD-II Mode 01 requests | "Report current data for parameter X" (speed, RPM, coolant temperature and the other PIDs of REQ-004, plus the supported-PID bitmap) | None. The engine control module answers with a value |
| OBD-II Mode 03 request | "Report stored diagnostic trouble codes" | None. Reading codes does not change them |
| OBD-II Mode 09 requests | "Report vehicle information" (calibration identifiers; the vehicle identification number is available here and is treated as private data, see section 4) | None |
| A fixed list of ELM327 `AT` setup commands | Configure the adapter itself: reset, echo off, protocol selection, headers | None on the vehicle; they configure the adapter |

### 2.2 What is never sent

- **Mode 04, clear diagnostic trouble codes.** This is the one standard OBD-II service that changes state in the vehicle. There is no code path for it: the string does not appear in the codebase, the allowlist unit test names it as a rejected value, and a reviewer checks for it in every pull request that touches the OBD source.
- Any other OBD-II mode (02, 05, 06, 07, 08, 0A) or manufacturer-specific service.
- Any raw CAN frame. The CAN path of this software (SocketCAN with a DBC file) is used with a *simulated* bus at the desk; its frame reader interface has no send method at all, and the fake reader used in tests fails the test if a write is attempted.
- Adapter-specific `ST` commands (the vLinker MC+ supports them). Only the base `AT` set is used, which also keeps the adapter swappable.

### 2.3 How "read-only" is enforced, not just promised

| Layer | Mechanism | Verified by |
|---|---|---|
| Code structure | Every command passes through `CommandAllowlist` before it reaches the transport. There is one send path and it is guarded | Code review, architecture rule (section 8 of `ARCHITECTURE.md`) |
| Unit test | All 256 possible mode values are offered to the command layer; only Mode 01, 03 and 09 reach the transport, 0 bytes are written for every other value, Mode 04 is named explicitly | REQ-001 test, run in CI on every pull request |
| CAN side | `CanFrameReader` has no send function; the fake reader fails a test on write | REQ-001 test |
| Review | The pull request template and the review checklist both ask "does anything send data toward the vehicle outside the allowlist?" | Every pull request |
| Policy | A feature that writes to the vehicle is a standing prohibition of the project, recorded in the kickoff plan, and would be refused in review regardless of its merit | Project rules |

### 2.4 What read-only does not protect against

Honesty about the limits:

- **Bus load.** Even read requests are frames transmitted on the diagnostic CAN bus. The request rate is kept low (a few per second; the achievable rate is measured in sprint 2). Standard OBD-II polling at this rate is what a handheld scan tool does. Whether the 2013 GS350 places a gateway between the diagnostic port and its other buses has not been verified by this project, so the request rate is treated as a measured parameter, not an assumption.
- **The adapter itself.** The Bluetooth adapter is a third-party device with its own firmware. This software controls what it is asked to do, not what its firmware is capable of. The adapter pairs with a fixed PIN and stays powered whenever it is in the port, so the operating rule is: **unplug it when not testing.**
- **A fault in the vehicle's own diagnostic implementation.** Nothing in this project can rule that out; it is why on-car testing starts parked.

## 3. Operating rules for on-car testing

1. On-car tests start with the vehicle parked, following the written procedure in `docs/test/MANUAL_ON_CAR_PROCEDURE.md`. Results of every run are recorded.
2. The screen is not touched while the vehicle is moving. The driver does not operate the device; a passenger does, or it is observed only.
3. The adapter is unplugged from the OBD-II port when not testing.
4. The Pi has no case (it mounts to the back of the display). Before the first on-car test, how the board is held so that it cannot short against metal or strain a connector is decided and recorded (open question OQ-14).
5. Power comes from a USB-C power bank, not from the vehicle. The device cannot drain the vehicle's battery and the vehicle cannot power-cycle the device.
6. Temperature, throttling and under-voltage of the Pi are logged every 5 seconds during every on-car session (LHU-015, LHU-016). An under-voltage or throttling event, or 80°C, is a failed run and becomes a bug with the log attached (D-013, D-025). An uncooled Pi 5 in a car cabin is a known risk; it is measured, not assumed safe.

## 4. Privacy of vehicle data

The repository is public. The following never enter it (decision D-023):

- The vehicle identification number (VIN). Mode 09 returns it, and the ELM327 returns it as hexadecimal bytes, so a raw recording contains it in a form a plain text search does not see. Raw recordings stay in the ignored `local_recordings/` folder until a scrub step has removed Mode 09 responses.
- Bluetooth device addresses of the adapter or of any phone.
- Any raw on-car recording that has not been scrubbed.

A CI check (`tools/check_private_data.py`) fails any pull request containing a VIN-shaped or Bluetooth-address-shaped string in tracked files or file names.

## 5. Hazards considered

This is not a hazard analysis in the sense of any standard. It is the list of ways this device could do harm that were thought about, and what was done about each.

| # | Hazard | Mitigation |
|---|---|---|
| 1 | Software sends a command that changes vehicle state | Allowlist with a unit test over all modes; no Mode 04 code path; no CAN send method (section 2) |
| 2 | Driver distraction from a screen in the cabin | Device is informational; not operated while driving; on-car tests parked first (section 3) |
| 3 | Driver trusts a wrong or stale value | Every value carries a status; Stale is shown distinctly (REQ-006, REQ-012); the factory cluster remains the authority (section 1) |
| 4 | Bare board shorts against vehicle metal or a connector is strained | Mounting decided and recorded before the first on-car test (OQ-14) |
| 5 | Overheating of an uncooled Pi 5 in a cabin | Temperature logged every 5 s; 80°C is a fail; firmware throttles at 80°C to 85°C (D-025) |
| 6 | Power bank sags or cuts out, corrupting the SD card | Under-voltage flag logged and treated as a fail; clean-shutdown handling planned (sprint 2 requirements) |
| 7 | Adapter left in the port, paired with a fixed PIN | Operating rule: unplug when not testing |
| 8 | Private vehicle data published | Privacy rule and CI check (section 4) |

## 6. Who is responsible

The person plugging the device into a vehicle is responsible for that vehicle. This repository provides software and the evidence of how it was tested; it provides no warranty, no certification and no assurance beyond what the tests and measurements in this repository show.
