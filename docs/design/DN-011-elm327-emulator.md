# DN-011: ELM327 emulator with fault injection

| | |
|---|---|
| Ticket | LHU-011 |
| Requirements | none directly; the fixture for REQ-004, REQ-008 and REQ-010 |
| Author | implementer (build-out form, D-049) |
| Status | Approved |
| Form | Light (one page: problem, public interface, failure cases, test plan) |
| Draft written | 2026-10-08, about 15 minutes |
| Design review | after merge, by the repository owner (D-049) |
| Approved | 2026-10-08 |

## 1. Problem

The ELM327 path must be tested end to end before the car exists, and the faults the car and the adapter will produce (link drops, a silent adapter, a parameter that stops answering, garbled bytes, slow replies, ignition off) must be reproducible at the desk and in CI. `tools/elm327_emulator/elm327_emulator.py` behaves like an ELM327 adapter on a running engine, serves the text protocol on a pseudo-terminal that the real serial transport opens like any device, and takes fault commands on a control socket. It also counts every request outside the allowlist it ever receives, which is independent evidence for REQ-001.

Standard library only; Python 3.10 or later; Linux (pseudo-terminals). The command handler is a pure function so it is unit-tested on any platform.

## 2. Public interface

Command line:

| Option | Meaning |
|---|---|
| `--link PATH` | Create the pseudo-terminal and point the symlink `PATH` at its device, replacing an old link, so a restarted emulator is found at the same path (the Pi uses a fixed `/dev/rfcomm0` the same way) |
| `--control PATH` | Unix domain socket for fault injection and statistics |
| `--profile standard \| no-fuel-level \| minimal` | Which PIDs the vehicle reports as supported (assumption A1: the real set is discovered on the car) |
| `--vin TEXT` | The vehicle identification number returned by Mode 09 PID 02; default is the documented hypothetical value from `tools/private_data_allowlist.txt` |
| `--dtc CODE ...` | Stored trouble codes returned by Mode 03 (`P0133 P0420`); default none |
| `--log PATH` | CSV of every request and reply with a timestamp |
| `--verbose` | Print every exchange |

The emulator prints the device path on its first line of standard output, then serves until `exit` on the control socket or a signal.

Control socket, one line per command, reply `OK` or `ERR reason` (and JSON for `stats`):

| Command | Fault |
|---|---|
| `silence SECONDS` | No reply to anything for that long (Bluetooth drop mid-drive, before the link dies) |
| `stale PID` / `unstale PID` | That Mode 01 PID answers `NO DATA` (stale data scenario) |
| `corrupt N` | The next N data replies have one byte garbled (corrupt frames scenario) |
| `delay MILLISECONDS` | Every reply is delayed (slow adapter) |
| `ignition off` / `ignition on` | Every OBD request answers `UNABLE TO CONNECT` until on again |
| `stats` | JSON: requests, replies, per-command counts, forbidden requests (anything outside Mode 01, 03, 09 and the base AT list), unknown commands |
| `exit` | Close the device and quit (the serial side sees end-of-file, like a dead adapter) |

Adapter behaviour: `ATZ` resets (echo on, spaces on, linefeeds on), `ATE0`, `ATL0`, `ATS0`, `ATH0`, `ATSP0` answer `OK`, `ATI` the version text, `ATRV` the voltage, `ATDPN` `A6`; unknown `AT` commands answer `?`; Mode 01 PIDs from the vehicle model (values move with time: RPM, speed, coolant warming up, load, throttle, intake air, voltage, fuel level), `0100`, `0120`, `0140` bitmaps from the profile with the chain bit; Mode 03 codes and Mode 09 02 VIN in the ISO 15765 multi-frame text form; everything else `NO DATA`. Every reply ends with `\r\r>`; echo and spaces follow the AT settings.

## 3. Failure cases

| # | Failure | How the design handles it |
|---|---|---|
| 1 | The serial side closes and reopens (the source reconnecting) | The master end stays open; a reopen on the same symlink works without restarting the emulator |
| 2 | A request outside the allowlist arrives (would mean a bug in the C++ side) | Counted in `forbidden_requests`, answered `NO DATA`; the integration suite asserts the count is 0 |
| 3 | Garbage bytes | Lines that are not a known command answer `?`, as the adapter does; counted as unknown |
| 4 | Control command malformed | `ERR` with the reason; emulator keeps serving |
| 5 | The pseudo-terminal cannot be created (no `/dev/ptmx` in a container) | The emulator exits 2 with the error; the integration test reports the exact message as a skipped fixture, never as a pass |

## 4. Test plan

| # | Test | Requirement |
|---|---|---|
| 1 | Python unit tests of the handler: every AT command, echo and spaces toggles, bitmaps per profile with the chain bit, each PID encoded by the inverse of the DN-009 formula, Mode 03 with 0, 2 and 6 codes, Mode 09 02 multi-frame VIN, `NO DATA` for other modes, forbidden counter on Mode 04, each fault command | fixture |
| 2 | The C++ integration suite of LHU-012 against the emulator over the pseudo-terminal | REQ-002, REQ-004, REQ-008, REQ-010 |

## Changes after the design review

| # | Change | Reason |
|---|---|---|
| 1 | None yet; the review happens after merge (D-049) | |

## Design vs. implementation

Added after the code pull request is merged.
