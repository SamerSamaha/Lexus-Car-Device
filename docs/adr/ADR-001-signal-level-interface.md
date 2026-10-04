# ADR-001: The data source interface is at the decoded-signal level

| | |
|---|---|
| Status | Accepted |
| Date | 2026-10-01 (decision D-001); written down 2026-10-03 |
| Decided by | The project lead, on a senior reviewer's recommendation |
| Requirements | REQ-002, REQ-003 |
| Related | `docs/architecture/ARCHITECTURE.md` sections 2 and 4 |

## Context

The head unit has two ways of getting data out of the vehicle, and they do not look alike:

- **The CAN path** (SocketCAN on `vcan0` at the desk, a real interface later). The bus *broadcasts* frames continuously; nobody asks for them. A frame is an identifier plus up to 8 data bytes, and one frame carries several signals packed into bit fields. A DBC file says which bits mean what. Data arrives at the bus rate, typically 10 to 100 frames per second per identifier.
- **The OBD-II path** (ELM327-compatible Bluetooth adapter). This is *request and response*: the software asks for one parameter (`010D`, vehicle speed), the adapter returns a text line (`41 0D 3C`), and nothing arrives unless asked. One reply carries one parameter, decoded by a published formula (SAE J1979). Throughput is a few requests per second.

The service layer (store, staleness, connection state) and the HMI must work with either path, and with a recorded session being replayed, without being recompiled (REQ-002). The question is *at which level* the common interface sits.

## Decision

The interface, `VehicleDataSource`, delivers **decoded signal samples**: one record per signal with a value, a unit, a monotonic timestamp and a status (`SignalSample`, REQ-003). Each source does its own decoding below the interface:

- `SocketCanDbcSource` = frame reader + DBC decoder, emits samples.
- `Elm327ObdSource` = byte transport + ELM327 protocol + PID decoder, emits samples.
- `ReplaySource` re-emits recorded samples.

Nothing above the interface knows about frames, PIDs, byte orders or adapter prompts.

## Alternatives considered

### A. A frame-level interface

One interface that delivers raw CAN frames, with a single decoder above it.

Rejected because the OBD path does not produce CAN frames. An ELM327 reply is a text line that already *is* the answer to one question; turning it back into a fake CAN frame so that a shared decoder can take it apart again is extra code with no information gain. It would also force the request and response logic (what to ask, when, what to do on `NO DATA`) into a layer that the CAN path does not need. The two paths would share a type but not a behaviour.

### B. Two separate stacks, no common interface

Let the service layer talk to each source through its own interface.

Rejected because the service layer would then contain two code paths to test and keep in step, and a third for replay. REQ-002 (same integration suite against every source, no recompile) would be unachievable, and the fake source used to test the service layer without hardware would need to impersonate two interfaces.

### C. An interface at the "raw bytes" level

One interface delivering bytes, with all parsing above it.

Rejected for the same reason as A, in a stronger form: SocketCAN does not expose a byte stream at all, it exposes framed messages. The lowest level the two paths share in nature is not bytes and not frames; it is the decoded value.

## Consequences

Positive:
- The service layer and the HMI are tested once, against a fake source, with no hardware code linked in (REQ-002). This is what makes the unit test suite fast and makes CI possible without a vehicle.
- A recorded drive replays through exactly the same path as live data (REQ-015), because replay is just another source.
- Each decoder is a pure function of input bytes to value, testable against an independent oracle: SAE J1979 formulas for PIDs, the Python `cantools` library for DBC decoding (REQ-004, REQ-005).
- The sample record (value, unit, timestamp, status) mirrors how a vehicle hardware abstraction layer in an automotive OS presents properties to applications. The shape is familiar to anyone who has worked with one; no compatibility with any specific platform is claimed.

Negative, accepted:
- Source-specific detail is hidden. A diagnostics screen that wants to show raw frames or adapter text needs a side channel, not the main interface. That is acceptable for the MVP; the diagnostics screen is sprint 2.
- Each source carries its own decoder, so a decoding bug is fixed per source. The oracles above are the mitigation.
- Staleness is judged above the interface from timestamps, so every source must stamp samples with the same monotonic clock. The `Clock` interface in the service layer exists for this reason.

## What would change this decision

If a third path appeared that produced CAN frames *and* needed request and response behaviour (for example reading UDS services over raw CAN), a frame-level layer might be worth adding **below** the signal interface, as an internal detail of that source. The signal-level interface itself would stay.
