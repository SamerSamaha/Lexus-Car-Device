# Architecture

This document describes the parts of the Lexus Head Unit, what each one owns, how data moves between them, and the rules that keep them apart. It is the reference that design notes (`docs/design/`) and code reviews are checked against. Decisions with alternatives are recorded in `docs/adr/`; this file states the result.

Status on 2026-10-05: the layering, interfaces and rules below are approved (D-001, D-002, D-008, D-009, D-024), and the scope decision of 2026-10-05 (plan revision 6) added the process view of section 2: an app hub, a vehicle-data service that other processes consume over D-Bus, and apps. The signal model, `SignalStore`, `StalenessMonitor`, the clocks (LHU-006, DN-006) `ConnectionStateMachine` (LHU-007, DN-007), the `VehicleDataSource` interface, `SignalStoreFeeder` and `FakeSource` (LHU-008, DN-008) exist in code. Each further component is built by the ticket named in `docs/traceability/TRACEABILITY.md`, with its design note.

## 1. What the system is

A plug-in, read-only infotainment platform: a Raspberry Pi 5 (2GB) with the official 5-inch Touch Display 2, powered from a USB-C power bank, reading vehicle data from a 2013 Lexus GS350 through a Bluetooth OBD-II adapter (Vgate vLinker MC+). A full-screen hub launches apps. The vehicle-data app shows live signals, derived trip values and connection status; the diagnostics screen shows trouble codes and power status; web apps run in the system browser; audio goes to the car stereo over Bluetooth. It never writes to the vehicle. At the desk, the same software runs against an ELM327 emulator or a simulated CAN bus.

The vehicle-data path is the core of the project and is built first (milestone v0.1.0). The hub, the service boundary and the apps follow (v0.2.0), then the CAN path, diagnostics, analytics and measurements (v1.0.0).

## 2. Process view

```
+-------------------+   +----------------------+   +--------------------------+
| hub               |   | vehicle-data app     |   | system browser           |
| QML launcher,     |   | QML screens: home,   |   | web apps: video, audio,  |
| app registry,     |   | vehicle data,        |   | games (one process per   |
| process manager,  |   | diagnostics          |   | URL entry)               |
| status strip      |   |                      |   |                          |
+---------+---------+   +----------+-----------+   +--------------------------+
          | D-Bus (VehicleDataClient)           |      started and tracked by the hub
          +-----------------+-------------------+
                            |
+---------------------------v----------------------------------------------+
| vehicle-data service (one process)                                       |
|   D-Bus adapter (Qt D-Bus) <- the only Qt code in this process           |
|   service layer (plain C++17): SignalStore, StalenessMonitor,            |
|     ConnectionStateMachine, DerivedSignalEngine, PowerStatusProvider     |
|   VehicleDataSource: Elm327ObdSource | SocketCanDbcSource | ReplaySource |
+--------------------------------------------------------------------------+
| operating system: Raspberry Pi OS desktop, labwc Wayland compositor,     |
| BlueZ (OBD link and audio to the car), PipeWire, systemd units           |
+--------------------------------------------------------------------------+
```

Rules of the process view:
- The vehicle-data service is the only process that talks to the vehicle. Every other process reads signals through `VehicleDataClient` over D-Bus (REQ-017). The read-only guarantee (REQ-001) therefore lives in one process.
- The hub starts, tracks and stops app processes and is visible again when one exits (REQ-016). It never exits while the system runs. How the user returns to the hub from a fullscreen browser is settled by the LHU-020 spike (OQ-29).
- Web apps are URL entries in the hub's registry, opened in the system browser full screen (REQ-018). They are content, not product code; the hub, the service and the measurements are the product.
- Memory is budgeted per process (REQ-014). The browser dominates and is measured separately.

This is the shape of an automotive platform in Linux terms: a launcher, apps, and one central service that owns vehicle data and hands it to any app that asks. The analogy is stated as a design influence, not as compatibility with any product.

## 3. Layers inside the vehicle-data path

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

In milestone v0.1.0 the view models and the service layer live in one process and the queued-signal line above is the boundary. From v0.2.0 (LHU-022) the service layer moves into the vehicle-data service process and the queued-signal line becomes D-Bus: the view models talk to `VehicleDataClient`, which receives D-Bus signals on the UI thread's event loop. The service layer itself does not change; the D-Bus adapter is the only new code in the service process, and it is the only Qt code there.

## 4. Components

### 4.1 Hardware layer, `src/hardware/`

| Component | Responsibility | Notes |
|---|---|---|
| `VehicleDataSource` | Interface: `start(listener)`, `runOnce()`, `stop()`, `connectionState()`, `counters()`. A source owns its `ConnectionStateMachine` and reports each `SignalSample` and each `ConnectionTransition` to one `VehicleDataSourceListener` | The only way vehicle data enters the service layer (REQ-002). Defined in the service library (the consumer owns the interface, DN-008), implemented by each source below. No source owns a thread: the worker loop calls `runOnce()`, which does one bounded unit of work |
| `ByteTransport` | Interface. Writes bytes to and reads bytes from one link | Implementations: a serial device or RFCOMM socket for the car; a pseudo-terminal for the emulator; a fake with scripted replies for unit tests |
| `CommandAllowlist` | Decides whether a command may be sent | Holds the fixed list: OBD Mode 01, 03, 09 and the ELM327 setup commands. Anything else is refused and counted. There is no Mode 04 anywhere (REQ-001, D-009) |
| `Elm327Protocol` | Sends one allowlisted command, reads the reply up to the `>` prompt, classifies it as a data reply or one of the named error replies, with a timeout | Base `AT` commands only, no adapter-specific `ST` commands, so the adapter can be swapped. Malformed text never escapes as data (REQ-010) |
| `ObdPidDecoder` | Turns a Mode 01 reply into a value with a unit by the SAE J1979 formula for that PID | The 8 PIDs of REQ-004. Pure function of bytes to value; no I/O |
| `Elm327ObdSource` | Owns a `ByteTransport`, an `Elm327Protocol`, the polling loop and PID discovery; drives the `ConnectionStateMachine`; reconnects with backoff | REQ-004, REQ-008 |
| `CanFrameReader` | Interface. Delivers raw CAN frames (id, length, 8 data bytes, timestamp) | Implementations: a SocketCAN socket on `vcan0` or a real interface; a fake for tests. **Has no send method** (REQ-001) |
| `DbcDecoder` | Decodes a raw frame into signal values using a DBC file | Both byte orders, signed and unsigned, scale and offset (REQ-005). Wrong frame length is a counted error, not a sample (REQ-010) |
| `SocketCanDbcSource` | Owns a `CanFrameReader` and a `DbcDecoder`; converts decoded values into `SignalSample` | LHU-028, v1.0.0 |
| `ReplaySource` | Reads a recorded session file and re-emits its bytes or samples with the original timing | REQ-015, LHU-029, v1.0.0 |
| `FakeSource` | Scriptable source (`src/hardware/fake/`): samples, malformed inputs, link loss, reconnect, handshake failure, one step per `runOnce()` | Lets the service layer be tested without any hardware code; also the "fake" choice of the source configuration |
| `DtcDecoder`, `VehicleInfoDecoder` | Turn a Mode 03 reply into trouble codes with their standard text, and a Mode 09 reply into vehicle information | REQ-021, LHU-030. Pure functions of bytes to values. There is still no Mode 04 and no Mode 02 (freeze frame); the allowlist of D-009 is unchanged |
| `GpsSource` | Roadmap: delivers latitude, longitude, speed and heading as signals from a position stream sent by the phone over the hotspot | LHU-036; proves the interface a third time, with a non-vehicle source |

### 4.2 Service layer, `src/service/`

| Component | Responsibility | Notes |
|---|---|---|
| `SignalSample` | Value type: signal id, value, unit, monotonic timestamp (ms), status | Shaped like a property value in a vehicle hardware abstraction layer: one record per signal carrying value, timestamp and status. The mapping is a design influence, not a claim of compatibility |
| `SignalDefinition` | Static description of a signal: id, name, unit, staleness timeout | Table of the signals the system knows; the default timeout is 1000 ms (REQ-006) |
| `SignalStore` | Holds the latest `SignalSample` per signal in an array indexed by `SignalId`; rejects a sample whose timestamp is not newer than the stored one, a sample with the wrong unit, or an unknown id, and counts each kind; notifies one listener of each change; holds the per-signal staleness timeouts | REQ-003. Single-writer: only the worker thread writes. The source's status field is ignored: the store writes Valid, the monitor writes Stale |
| `StalenessMonitor` | Marks a signal Stale when its timeout has passed since its last sample | Driven by an injected `Clock` so tests control time (REQ-006) |
| `ConnectionStateMachine` | Four states, Disconnected, Connecting, Connected, Error; six triggers; an 8-row transition table applied by `handle(trigger)`; illegal pairs rejected and counted; each accepted transition recorded with trigger and time and handed to one listener | REQ-007. The table is in section 6 |
| `SignalStoreFeeder` | The service layer's `VehicleDataSourceListener`: writes each sample into the `SignalStore`, keeps the latest connection transition and counts, forwards transitions to a log hook | LHU-008. The wiring passes it to `source.start()` |
| `Clock` | Interface returning monotonic milliseconds | `SteadyClock` uses `std::chrono::steady_clock`; `ManualClock` (in the library, for tests and tooling) is advanced by hand |
| `DerivedSignalEngine` | Computes derived signals from stored samples: fuel economy from mass air flow and speed, trip distance, time in RPM bands, warm-up time; writes them into the `SignalStore` like any source | REQ-022, LHU-031. Constants (air-fuel ratio, fuel density) are stated in DN-031. No model training on the device; offline statistics are tooling (LHU-037) |
| `PowerStatusProvider` | Interface returning the firmware's under-voltage and throttling flags; real implementation reads them on the Pi, a fake sets them in tests | REQ-020, LHU-025 |

The service layer depends on the C++17 standard library only. No Qt header is included anywhere under `src/service/` (D-008); this is checked by the fact that the library target links to nothing but the build-settings target.

### 4.2a Vehicle-data service process and client, `src/service_dbus/`

| Component | Responsibility | Notes |
|---|---|---|
| `VehicleDataService` | Hosts the service layer and the active source in its own process; publishes every signal change and connection state change over D-Bus; answers a current-state query so a late-joining client starts complete | REQ-017, LHU-022. Qt D-Bus adapter only; the service layer underneath is unchanged |
| `VehicleDataClient` | Client-side mirror: subscribes, holds the latest sample per signal, exposes them to view models | The only path from a view model to vehicle data (REQ-011 from v0.2.0). Used by the vehicle-data app and the hub's status strip |
| D-Bus interface definition | Introspection XML in `src/service_dbus/` naming the signals and the state query | Versioned; the test of REQ-017 runs two clients against it |

### 4.3 HMI, `src/hmi/`

| Component | Responsibility | Notes |
|---|---|---|
| View models (`src/hmi/viewmodels/`) | `QObject` classes exposing `Q_PROPERTY` values, units, status text and connection state; receive service-layer updates through queued signals and own the thread hop | The only classes QML may bind to (REQ-011) |
| QML screens (`src/hmi/qml/`) | Home (2 primary values and a status strip), vehicle data (4 x 2 grid of signal tiles, LHU-039), diagnostics (scrolling list, LHU-030) | Bindings only; no logic beyond formatting |
| `app` (`src/app/`) | `main()` of the vehicle-data app: reads configuration, constructs the view models and the QML engine. In v0.1.0 it also constructs the source and the service layer in-process and starts the worker thread; from v0.2.0 that moves to the service process and `app` constructs a `VehicleDataClient` | The one place that knows every concrete type |

### 4.4 Hub, `src/hub/`

| Component | Responsibility | Notes |
|---|---|---|
| `AppRegistry` | Reads the app configuration file: name, icon, kind (native command or URL), command line | REQ-016, REQ-018, LHU-021. Web apps are URL entries; the browser command and its flags live in `deploy/` |
| `ProcessManager` | Starts an app as a child process, tracks it, stops it, reports its exit; restart policy per entry | REQ-016. Plain C++17 over POSIX process calls, unit-tested with a fake process |
| `AppHub` and QML (`src/hub/qml/`) | Full-screen icon grid sized in millimetres (D-024); status strip with connection state and power flags from `VehicleDataClient` and `PowerStatusProvider`; shutdown control (REQ-020) | Return-to-hub mechanism decided by the LHU-020 spike (OQ-29) |

## 5. The signal model

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

## 6. Connection state

```
Disconnected --start()--> Connecting --handshake ok--> Connected
Connecting   --timeout or refused--> Error
Connected    --link lost (2 s)--> Error
Error        --backoff elapsed--> Connecting        (1, 2, 4, 8, 10, 10, ... s)
any state    --stop()--> Disconnected
```

The full transition table (DN-007), verified by a unit test over every one of the 24 state and trigger pairs (REQ-007):

| From | Trigger | To |
|---|---|---|
| Disconnected | StartRequested | Connecting |
| Connecting | HandshakeSucceeded | Connected |
| Connecting | HandshakeFailed | Error |
| Connecting | StopRequested | Disconnected |
| Connected | LinkLost | Error |
| Connected | StopRequested | Disconnected |
| Error | BackoffElapsed | Connecting |
| Error | StopRequested | Disconnected |

Every other pair is rejected, counted, and leaves the state unchanged; no listener is called. The machine records each accepted transition with its trigger (the cause) and the clock time, which is the connection log of REQ-019. The backoff timer that raises BackoffElapsed, and its schedule, are REQ-008 and live in the source's reconnect loop (LHU-012).

## 7. Threads

| Thread | Runs | Owns |
|---|---|---|
| UI thread | Qt event loop, QML rendering, view models | Every `QObject` |
| Worker thread | The active `VehicleDataSource`, `SignalStore`, `StalenessMonitor`, `ConnectionStateMachine` | All service-layer objects |

Rules:
- The worker thread never touches a `QObject`. It hands results to the view models through a queued signal connection (Qt copies the value and delivers it on the UI thread).
- The UI thread never calls into the service layer directly. Commands (start, stop) go through the same queued mechanism in the other direction.
- Blocking I/O (socket reads, adapter timeouts) happens only on the worker thread, so the screen never stalls on the adapter.
- A code review states which thread runs each new function (checklist item 6).

## 8. Main data flow, ELM327 path

1. `Elm327ObdSource` writes an allowlisted request (for example `010D`, vehicle speed) through `ByteTransport`.
2. `Elm327Protocol` reads bytes until the `>` prompt or a timeout, and classifies the reply.
3. A data reply goes to `ObdPidDecoder`, which produces a value and unit; a named error reply increments the error counter and produces nothing.
4. The source stamps the monotonic time and emits a `SignalSample` to the service layer.
5. `SignalStore` rejects it if its timestamp is not newer than the stored sample, else stores it as Valid and notifies.
6. The notification crosses to the UI thread by a queued signal; the view model updates its properties.
7. QML bindings re-render the tile. The time from step 4 to the rendered frame is what REQ-009 measures.
8. Meanwhile `StalenessMonitor` checks every stored signal against its timeout and flips it to Stale when exceeded (REQ-006).

## 9. Dependency rules, checked

| Rule | Checked by |
|---|---|
| Service layer includes no Qt | Library links only to the build-settings target; a reviewer checks includes |
| Service and HMI targets have no link dependency on a concrete source | CI step: `tools/check_link_graph.py` on `cmake --graphviz` output; any target ending in `_source` reachable from the service library or an HMI target fails (REQ-002, since LHU-008) |
| QML and view models include nothing from `src/hardware/` or `src/service/` except through the view-model library | CI include check (REQ-011, with LHU-013) |
| No send path on the CAN side; only allowlisted commands on the OBD side | Unit tests of REQ-001; fake reader fails on write |
| No adapter-specific `ST` commands | Code review; the allowlist holds only `AT` setup commands |

## 10. HMI sizing rules (D-024)

The 5-inch Touch Display 2 is 720 x 1280 pixels on an active area of 62.1 mm x 110.4 mm, which is 11.6 pixels per millimetre. The panel is portrait-native and is used in landscape; the compositor rotates it and touch follows (D-045, verified on the unit 2026-10-05).

- All sizes are written in millimetres and converted through one constant, `pixelsPerMillimetre = 11.6`, defined once in the HMI.
- Touch targets are at least 10 mm (116 px) on each side.
- Primary values have a character height of at least 4 mm (46 px). The 4 mm figure is about 20 arcminutes at 700 mm viewing distance; the 20-arcminute recommendation is attributed to ISO 15008 from memory and is **unverified**.
- Home shows 2 primary values and a status strip. Vehicle data shows the 8 signals of REQ-004 as a 4 x 2 grid of tiles about 27 x 27 mm. Diagnostics scrolls.
- A Stale signal is drawn in a visibly different style (REQ-006, REQ-012); the exact style is decided with LHU-013.

## 11. Deployment

- Target: Raspberry Pi OS 64-bit desktop (Debian 13 based, Trixie) on the Pi 5, Qt 6.8.2 from the distribution packages. The desktop image and its labwc Wayland compositor are the display stack (D-045): the hub needs a compositor to run beside the system browser, so Qt eglfs on the bare framebuffer is rejected. The panel is rotated to landscape in the compositor configuration; touch follows. First-boot steps are in `deploy/PI_SETUP.md`.
- Processes are started by systemd units in `deploy/`: the vehicle-data service first, then the hub; apps are started by the hub.
- Bluetooth: BlueZ carries both the RFCOMM link to the OBD adapter and, from v0.2.0, the audio link to the car stereo (PipeWire, REQ-019). Their coexistence on one radio is measured, not assumed (OQ-28).
- Desk: Debian 13 in WSL2 with the same Qt version; the application runs against the ELM327 emulator over a pseudo-terminal or against `vcan0` on the Pi only (WSL2 has no vcan module).
- CI: Debian 13 container; unit, integration, scenario and HMI tests on the offscreen platform; arm64 release build (LHU-033).

## 12. Not decided yet

| Item | Decided by |
|---|---|
| Configuration file format and the source-selection key | DN-012 |
| D-Bus interface names, signal payload layout, current-state query | DN-022 |
| App registry file format; how the user returns to the hub from a fullscreen browser (OQ-29) | LHU-020 spike, then DN-021 |
| Whether the system browser plays protected audio on this unit (OQ-27) | LHU-023, recorded as a fact either way |
| Audio and OBD links sharing one Bluetooth radio (OQ-28) | LHU-024, measured |
| Per-process memory budgets replacing the single 150 MB figure of REQ-014 | LHU-032 baseline (OQ-8) |
| Derived-signal constants and formulas | DN-031 |
