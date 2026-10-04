# Architecture

This document describes the parts of the Lexus Head Unit, what each one owns, how data moves between them, and the rules that keep them apart. It is the reference that design notes (`docs/design/`) and code reviews are checked against. Decisions with alternatives are recorded in `docs/adr/`; this file states the result.

Status on 2026-10-03: the layering, interfaces and rules below are approved (D-001, D-002, D-008, D-009, D-024). Only the service library skeleton exists in code. Each component is built by the ticket named in `docs/traceability/TRACEABILITY.md`, after its design note is approved.

## 1. What the system is

A plug-in, read-only infotainment head unit: a Raspberry Pi 5 (2GB) with the official 5-inch Touch Display 2, powered over USB-C, reading vehicle data from a 2013 Lexus GS350 through a Bluetooth OBD-II adapter (Vgate vLinker MC+). It shows live vehicle signals and connection status. It never writes to the vehicle. At the desk, the same software runs against an ELM327 emulator or a simulated CAN bus.

## 2. Layers

```
+--------------------------------------------------------------------------+
| HMI (Qt Quick / QML)                                        UI thread    |
|   screens: home, vehicle data, diagnostics                               |
|   QML binds only to view-model properties           <- REQ-011          |
+--------------------------------------------------------------------------+
| View models (QObject, Q_PROPERTY, signals)                   UI thread   |
|   SignalTileModel, ConnectionStatusModel, DiagnosticsModel              |
+------------------------------- queued signals ---------------------------+
| Service layer (plain C++17, no Qt)                        worker thread  |
|   SignalStore  StalenessMonitor  ConnectionStateMachine                 |
|   SignalSample  SignalDefinition  Clock (interface)                     |
+--------------------------------------------------------------------------+
| VehicleDataSource (interface: emits SignalSample, ConnectionEvent)       |
+------------------------+------------------------+------------------------+
| SocketCanDbcSource     | Elm327ObdSource        | ReplaySource           |
|  CanFrameReader        |  ByteTransport         |  recorded session file |
|   (socket | fake)      |   (serial | pty | fake)|                        |
|  DbcDecoder            |  Elm327Protocol        |  FakeSource (tests)    |
|                        |  ObdPidDecoder         |                        |
|                        |  CommandAllowlist      |                        |
+------------------------+------------------------+------------------------+
| Operating system: SocketCAN (vcan0), RFCOMM socket, serial device        |
+--------------------------------------------------------------------------+
```

Data flows upward only. Nothing above the `VehicleDataSource` line knows which source is running (REQ-002). Nothing below the view models knows that Qt exists (D-008).

## 3. Components

### 3.1 Hardware layer, `src/hardware/`

| Component | Responsibility | Notes |
|---|---|---|
| `VehicleDataSource` | Interface. Starts, stops, and delivers `SignalSample` and `ConnectionEvent` to one listener | The only way vehicle data enters the service layer (REQ-002). Defined in `src/hardware/`, implemented by each source below |
| `ByteTransport` | Interface. Writes bytes to and reads bytes from one link | Implementations: a serial device or RFCOMM socket for the car; a pseudo-terminal for the emulator; a fake with scripted replies for unit tests |
| `CommandAllowlist` | Decides whether a command may be sent | Holds the fixed list: OBD Mode 01, 03, 09 and the ELM327 setup commands. Anything else is refused and counted. There is no Mode 04 anywhere (REQ-001, D-009) |
| `Elm327Protocol` | Sends one allowlisted command, reads the reply up to the `>` prompt, classifies it as a data reply or one of the named error replies, with a timeout | Base `AT` commands only, no adapter-specific `ST` commands, so the adapter can be swapped. Malformed text never escapes as data (REQ-010) |
| `ObdPidDecoder` | Turns a Mode 01 reply into a value with a unit by the SAE J1979 formula for that PID | The 8 PIDs of REQ-004. Pure function of bytes to value; no I/O |
| `Elm327ObdSource` | Owns a `ByteTransport`, an `Elm327Protocol`, the polling loop and PID discovery; drives the `ConnectionStateMachine`; reconnects with backoff | REQ-004, REQ-008 |
| `CanFrameReader` | Interface. Delivers raw CAN frames (id, length, 8 data bytes, timestamp) | Implementations: a SocketCAN socket on `vcan0` or a real interface; a fake for tests. **Has no send method** (REQ-001) |
| `DbcDecoder` | Decodes a raw frame into signal values using a DBC file | Both byte orders, signed and unsigned, scale and offset (REQ-005). Wrong frame length is a counted error, not a sample (REQ-010) |
| `SocketCanDbcSource` | Owns a `CanFrameReader` and a `DbcDecoder`; converts decoded values into `SignalSample` | Sprint 2 |
| `ReplaySource` | Reads a recorded session file and re-emits its bytes or samples with the original timing | REQ-015, sprint 2 |
| `FakeSource` | Test double that emits whatever a test scripts | Lets the service layer be tested without any hardware code |

### 3.2 Service layer, `src/service/`

| Component | Responsibility | Notes |
|---|---|---|
| `SignalSample` | Value type: signal id, value, unit, monotonic timestamp (ms), status | Shaped like a property value in a vehicle hardware abstraction layer: one record per signal carrying value, timestamp and status. The mapping is a design influence, not a claim of compatibility |
| `SignalDefinition` | Static description of a signal: id, name, unit, staleness timeout | Table of the signals the system knows; the default timeout is 1000 ms (REQ-006) |
| `SignalStore` | Holds the latest `SignalSample` per signal; rejects out-of-order timestamps; notifies the listener of each change | REQ-003. Single-writer: only the worker thread writes |
| `StalenessMonitor` | Marks a signal Stale when its timeout has passed since its last sample | Driven by an injected `Clock` so tests control time (REQ-006) |
| `ConnectionStateMachine` | Four states, Disconnected, Connecting, Connected, Error, and a written transition table; rejects illegal transitions | REQ-007. The table is in the design note DN-007 and copied here when approved |
| `Clock` | Interface returning monotonic milliseconds | Real implementation uses `std::chrono::steady_clock`; tests inject a manual clock |

The service layer depends on the C++17 standard library only. No Qt header is included anywhere under `src/service/` (D-008); this is checked by the fact that the library target links to nothing but the build-settings target.

### 3.3 HMI, `src/hmi/`

| Component | Responsibility | Notes |
|---|---|---|
| View models (`src/hmi/viewmodels/`) | `QObject` classes exposing `Q_PROPERTY` values, units, status text and connection state; receive service-layer updates through queued signals and own the thread hop | The only classes QML may bind to (REQ-011) |
| QML screens (`src/hmi/qml/`) | Home (2 primary values and a status strip), vehicle data (4 x 2 grid of signal tiles), diagnostics (scrolling list) | Bindings only; no logic beyond formatting |
| `app` (`src/app/`) | `main()`: reads configuration, constructs the chosen source, the service layer, the view models and the QML engine; starts the worker thread | The one place that knows every concrete type |

## 4. The signal model

Every piece of vehicle data is a `SignalSample`:

| Field | Type | Meaning |
|---|---|---|
| `signalId` | enumeration | Which signal (vehicle speed, engine RPM, coolant temperature, ...) |
| `value` | `double` | In the unit below, already scaled |
| `unit` | enumeration | km/h, rpm, degrees Celsius, percent, volts, ... |
| `timestampMilliseconds` | `int64_t` | Monotonic time at which the source produced the sample, 1 ms resolution |
| `status` | `NeverReceived`, `Valid`, `Stale` | Set by the store and the staleness monitor, never by the source |

Status rules (REQ-003, REQ-006):

```
NeverReceived --first sample--> Valid --timeout passes--> Stale --new sample--> Valid
```

A source never emits a Stale sample; staleness is the service layer's judgement about time. A malformed input produces no sample at all and increments the source's error counter (REQ-010).

## 5. Connection state

```
Disconnected --start()--> Connecting --handshake ok--> Connected
Connecting   --timeout or refused--> Error
Connected    --link lost (2 s)--> Error
Error        --backoff elapsed--> Connecting        (1, 2, 4, 8, 10, 10, ... s)
any state    --stop()--> Disconnected
```

The full transition table, including which transitions are illegal, is fixed in the design note of LHU-007 and verified by its unit tests (REQ-007). The backoff schedule is REQ-008.

## 6. Threads

| Thread | Runs | Owns |
|---|---|---|
| UI thread | Qt event loop, QML rendering, view models | Every `QObject` |
| Worker thread | The active `VehicleDataSource`, `SignalStore`, `StalenessMonitor`, `ConnectionStateMachine` | All service-layer objects |

Rules:
- The worker thread never touches a `QObject`. It hands results to the view models through a queued signal connection (Qt copies the value and delivers it on the UI thread).
- The UI thread never calls into the service layer directly. Commands (start, stop) go through the same queued mechanism in the other direction.
- Blocking I/O (socket reads, adapter timeouts) happens only on the worker thread, so the screen never stalls on the adapter.
- A code review states which thread runs each new function (checklist item 6).

## 7. Main data flow, ELM327 path

1. `Elm327ObdSource` writes an allowlisted request (for example `010D`, vehicle speed) through `ByteTransport`.
2. `Elm327Protocol` reads bytes until the `>` prompt or a timeout, and classifies the reply.
3. A data reply goes to `ObdPidDecoder`, which produces a value and unit; a named error reply increments the error counter and produces nothing.
4. The source stamps the monotonic time and emits a `SignalSample` to the service layer.
5. `SignalStore` rejects it if its timestamp is not newer than the stored sample, else stores it as Valid and notifies.
6. The notification crosses to the UI thread by a queued signal; the view model updates its properties.
7. QML bindings re-render the tile. The time from step 4 to the rendered frame is what REQ-009 measures.
8. Meanwhile `StalenessMonitor` checks every stored signal against its timeout and flips it to Stale when exceeded (REQ-006).

## 8. Dependency rules, checked

| Rule | Checked by |
|---|---|
| Service layer includes no Qt | Library links only to the build-settings target; a reviewer checks includes |
| Service and HMI targets have no link dependency on a concrete source | CI step inspecting the CMake link graph (REQ-002, with LHU-012) |
| QML and view models include nothing from `src/hardware/` or `src/service/` except through the view-model library | CI include check (REQ-011, with LHU-013) |
| No send path on the CAN side; only allowlisted commands on the OBD side | Unit tests of REQ-001; fake reader fails on write |
| No adapter-specific `ST` commands | Code review; the allowlist holds only `AT` setup commands |

## 9. HMI sizing rules (D-024)

The 5-inch Touch Display 2 is 720 x 1280 pixels on an active area of 62.1 mm x 110.4 mm, which is 11.6 pixels per millimetre. The panel is portrait-native and is used in landscape, so the software rotates (OQ-6).

- All sizes are written in millimetres and converted through one constant, `pixelsPerMillimetre = 11.6`, defined once in the HMI.
- Touch targets are at least 10 mm (116 px) on each side.
- Primary values have a character height of at least 4 mm (46 px). The 4 mm figure is about 20 arcminutes at 700 mm viewing distance; the 20-arcminute recommendation is attributed to ISO 15008 from memory and is **unverified**.
- Home shows 2 primary values and a status strip. Vehicle data shows the 8 signals of REQ-004 as a 4 x 2 grid of tiles about 27 x 27 mm. Diagnostics scrolls.
- A Stale signal is drawn in a visibly different style (REQ-006, REQ-012); the exact style is decided with LHU-013.

## 10. Deployment

- Target: Raspberry Pi OS 64-bit (Debian 13 based) on the Pi 5, Qt 6.8.2 from the distribution packages, the application started by a systemd unit (`deploy/`).
- Desk: Debian 13 in WSL2 with the same Qt version; the application runs against the ELM327 emulator over a pseudo-terminal or against `vcan0` on the Pi only (WSL2 has no vcan module).
- CI: Debian 13 container; unit, integration, scenario and HMI tests on the offscreen platform; arm64 release build later.
- Display stack on the Pi (Qt eglfs directly, or a kiosk Wayland compositor) is undecided until the bring-up spike (OQ-6).

## 11. Not decided yet

| Item | Decided by |
|---|---|
| Exact `ConnectionStateMachine` transition table | DN-007 |
| Signal id and unit enumerations, the signal table | DN-006 |
| Configuration file format and the source-selection key | DN-012 |
| Display stack and rotation on the Pi | Bring-up spike, sprint 2 |
| Diagnostics screen content (trouble codes, power status) | Sprint 2 requirements, REQ-016 onward |
