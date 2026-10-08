# DN-009: OBD PID decoder

| | |
|---|---|
| Ticket | LHU-009 |
| Requirements | REQ-004, REQ-010 |
| Author | implementer (build-out form, D-049) |
| Status | Approved |
| Form | Light (one page: problem, public interface, failure cases, test plan) |
| Draft written | 2026-10-08, about 10 minutes |
| Design review | after merge, by the repository owner (D-049) |
| Approved | 2026-10-08 |

## 1. Problem

A Mode 01 reply carries the raw data bytes of one parameter (PID); the service layer needs the value in its unit. This ticket is the pure function from (PID, data bytes) to (signal, value, unit) by the SAE J1979 formulas for the 8 PIDs of REQ-004, plus the decoding of the supported-PID bitmaps (PID 0x00, 0x20, 0x40) that the source uses to discover what the vehicle answers. No I/O, no text parsing: the ELM327 text layer (LHU-010) hands these functions bytes.

Assumption for the car (checklist A1): the 2013 GS350 answers the standard PIDs; the formulas are the published ones and the set actually supported is discovered at run time, never assumed.

## 2. Public interface

| Function or type | Inputs | Output | Notes |
|---|---|---|---|
| `ObdPid` (enum class, `std::uint8_t`) | — | — | `SupportedPids00` 0x00, `EngineLoad` 0x04, `CoolantTemperature` 0x05, `EngineRpm` 0x0C, `VehicleSpeed` 0x0D, `IntakeAirTemperature` 0x0F, `ThrottlePosition` 0x11, `SupportedPids20` 0x20, `FuelLevel` 0x2F, `SupportedPids40` 0x40, `ControlModuleVoltage` 0x42 |
| `DecodedPid` (struct) | — | — | `SignalId signalId`, `double value`, `Unit unit` |
| `pidForSignal(SignalId)` | signal | `ObdPid` | The 8 signals of REQ-004 map one to one |
| `signalForPid(std::uint8_t)` | PID byte | `std::optional<SignalId>` | Empty for a PID that is not one of the 8 |
| `expectedDataByteCount(std::uint8_t pid)` | PID byte | `std::optional<std::size_t>` | 1 or 2 for the 8 PIDs, 4 for the bitmaps, empty otherwise |
| `decodePid(std::uint8_t pid, const std::vector<std::uint8_t>& dataBytes)` | PID byte, data bytes A, B, ... | `std::optional<DecodedPid>` | Empty unless the PID is known and the byte count is exactly the expected one. Formulas: load, throttle, fuel level `A * 100 / 255` %; coolant and intake air `A - 40` °C; RPM `(256 A + B) / 4` rpm; speed `A` km/h; voltage `(256 A + B) / 1000` V |
| `SupportedPidSet` (class) | — | — | 256 flags; `contains(pid)`, `insert(pid)`, `addBitmap(basePid, 4 bytes)` where `basePid` is 0x00, 0x20 or 0x40 and bit 31 of the bitmap is `basePid + 1`; `nextBitmapPid(basePid)` returns `basePid + 0x20` when the last bit says a further bitmap exists; `count()` |
| `pollablePids(const SupportedPidSet&)` | set | `std::vector<ObdPid>` | The subset of the 8 PIDs of REQ-004 the vehicle reports as supported, in a fixed order |

All functions are `noexcept` in behaviour: no throw, no undefined behaviour on any input. Every value is finite.

## 3. Failure cases

| # | Failure | How the design handles it |
|---|---|---|
| 1 | Byte count shorter or longer than the formula needs (a truncated reply, or extra bytes) | `decodePid` returns empty. The caller counts it as malformed input (REQ-010) and emits no sample |
| 2 | PID outside the 8 | Empty; the source never requests such a PID, so this only happens on a corrupt reply whose echoed PID does not match |
| 3 | Bitmap reply with the wrong length | `addBitmap` returns false and changes nothing |
| 4 | Bitmap says a further bitmap exists but the vehicle then answers `NO DATA` | Handled in the source (LHU-012): discovery stops with what was found |
| 5 | Values at the raw extremes | Every formula is linear, so min and max raw give the min and max value; the test pins them |

## 4. Test plan

| # | Test | Requirement |
|---|---|---|
| 1 | For each of the 8 PIDs: minimum raw, maximum raw and three mid-range vectors equal the SAE J1979 formula within 1e-9, in the stated unit (40 vectors, listed in the test) | REQ-004 |
| 2 | `signalForPid` and `pidForSignal` are inverse on the 8 signals; unknown PIDs give empty | REQ-004 |
| 3 | Wrong byte count (too few, too many, empty) and unknown PID give empty; a randomised test over 10,000 (pid, bytes) pairs never crashes and only returns a value when the pair is well formed | REQ-010 |
| 4 | `SupportedPidSet`: the bitmap of the ELM327 touch demo's fake (`BE 3F A8 13` for 0x00) decodes to the documented PIDs; bit 31 is PID 0x01; the last bit chains to the next bitmap; `pollablePids` returns only supported PIDs of the 8 in order; wrong bitmap length is refused | REQ-004 |

Discovery over the real adapter (an unsupported PID is never requested over 100 cycles) is the LHU-012 integration test against the emulator.

## Changes after the design review

| # | Change | Reason |
|---|---|---|
| 1 | None yet; the review happens after merge (D-049) | |

## Design vs. implementation

Added after the code pull request is merged.
