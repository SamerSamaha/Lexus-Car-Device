# DN-028: SocketCAN source (light note)

| | |
|---|---|
| Ticket | LHU-028 |
| Requirements | REQ-001, REQ-002 |
| Author | implementer (build-out form, D-049) |
| Status | Implemented |
| Draft written | 2026-10-09, about 20 minutes |
| Design review | after merge, by the repository owner (D-049) |
| Approved | 2026-10-09 |

## Problem

The CAN path needs a `VehicleDataSource` that reads raw frames from a SocketCAN interface (`vcan0` on the Pi, a real interface later), decodes them with `DbcDecoder` (DN-027), and publishes the eight project signals through the same interface as the OBD source (REQ-002). It must have no way to transmit (REQ-001).

## Public interface

| Class | Interface | Notes |
|---|---|---|
| `CanFrameReader` (interface) | `open() -> bool`, `read(timeoutMilliseconds) -> CanReadResult {status: Frame, Timeout, Closed, Error; frame}`, `close()`, `isOpen()` | **No write, send or transmit member**: a compile-time check in the tests fails if one is ever added, and `tools/check_can_read_only.py` fails CI if a send call appears under `src/hardware/can/` |
| `SocketCanFrameReader` | constructor takes the interface name | `socket(PF_CAN, SOCK_RAW, CAN_RAW)`, non-blocking, bound to the interface; reads `struct can_frame` through `poll`; `ENETDOWN` and other errors are Error, a vanished interface is Closed |
| `FakeCanFrameReader` | `queueFrame(frame)`, `failOpen(bool)`, `breakLink()`, `restoreLink()` | Tests only |
| `SocketCanDbcSource` | `VehicleDataSource` over a reader and a `DbcDecoder`; `CanSourceConfiguration {interfaceName, dbcPath, readTimeoutMilliseconds = 50, linkLossTimeoutMilliseconds = 2000, maximumFramesPerRun = 256, backoff}` | Maps DBC signal names to `SignalId` (VehicleSpeed, EngineSpeed to EngineRpm, CoolantTemperature, EngineLoad, ThrottlePosition, IntakeAirTemperature, ControlModuleVoltage, FuelLevel); other DBC signals are decoded and ignored |
| `ReconnectBackoff` | moved from the ELM327 library to the service library | A generic policy; both sources use it |
| `source.kind = can` | `can.interface`, `can.dbc` in `deploy/head_unit.conf` | Chosen at start-up by configuration (REQ-002) |

Sequence: Disconnected, StartRequested, Connecting; `open()` and a check that every mapped DBC signal's unit is the project unit (a mismatch is HandshakeFailed: the configuration is wrong, not the bus); Connected. Each `runOnce()` reads up to 256 frames (waiting at most 50 ms for the first), decodes them, and emits one sample per mapped signal with the read time from the source's clock.

## Failure cases

| Failure | Handling |
|---|---|
| Interface missing or down at start | `open()` false: HandshakeFailed, backoff 1, 2, 4, 8, 10 s (REQ-008 schedule) |
| Reader Error or Closed while connected | LinkLost, backoff, reopen |
| No frame for 2 s while connected | LinkLost (a silent bus means the ECUs or the link are gone) |
| DBC file has errors | HandshakeFailed every attempt, with the first error kept for the log |
| Unit of a mapped signal differs from the project unit | HandshakeFailed |
| Unknown identifier, wrong length, invalid frame | Counted by the decoder and in `malformedInputs`; no sample (REQ-010) |

## Test plan

| # | Test | Requirement |
|---|---|---|
| 1 | Compile-time: `CanFrameReader`, `SocketCanFrameReader` and `FakeCanFrameReader` have no member named write, send or transmit | REQ-001 |
| 2 | `tools/check_can_read_only.py` and its unit tests: no `write(`, `send(`, `sendto(`, `sendmsg(` under `src/hardware/can/` | REQ-001 |
| 3 | Source over the fake reader: connect, samples with project units, malformed frames counted, silence and reader errors give LinkLost and reconnect with backoff, unit mismatch fails the handshake | REQ-002 |
| 4 | The REQ-002 integration suite runs a CAN harness (fake reader) beside the fake and ELM327 harnesses | REQ-002 |
| 5 | On the Pi only, label `vcan`: the real reader receives frames written to `vcan0` by the test, and the source publishes them; skipped elsewhere with the reason "LHU-028: vcan0 not present" | REQ-002 |
| 6 | `tools/can_traffic_generator.py` (standard library only) writes the project signals to `vcan0` for manual runs; its encoding is unit-tested at the desk | REQ-002 |

## Changes after the design review

| # | Change | Reason |
|---|---|---|
| 1 | None yet; the review happens after merge (D-049) | |

## Design vs. implementation

Merged as PR #63. No deviation from the interface. Details the note did not spell out:

- `ReconnectBackoff` moved with its test to the service library, so the CAN source does not depend on the ELM327 library.
- The frame encoder that the tests use (`tests/integration/can_frame_encoder.h`) lives only under `tests/`; the product has no encoder.
- The CAN harness joined the REQ-002 suite: start and stop, a produced sample with its unit, link loss and recovery, counters, each passing for the fake, CAN and ELM327 sources.
- At the desk the two `vcan` tests skip with "LHU-028: vcan0 is not present"; their first run is checklist step 3.7. Status: Implemented.
