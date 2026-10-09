# DN-022: Vehicle-data service over D-Bus

| | |
|---|---|
| Ticket | LHU-022 |
| Requirements | REQ-017; REQ-002 and REQ-011 at the process boundary |
| Author | implementer (build-out form, D-049) |
| Status | Implemented |
| Draft written | 2026-10-08, about 40 minutes |
| Design review | after merge, by the repository owner (D-049) |
| Approved | 2026-10-08 |

## 1. Problem

Until now the source, the service layer and the screens run in one process, so only that process can see vehicle data. This ticket moves the source and the service layer into one vehicle-data service process that publishes every signal (value, unit, timestamp, status) and the connection state on D-Bus, with a client library that gives any app the same samples and the full current state on connection, even when it connects late (REQ-017).

## 2. Clarifying questions and assumptions

| # | Question | Assumption made |
|---|---|---|
| 1 | Which bus? | The user's session bus by default (the hub, the apps and the service all run in the desktop user's session); `--bus system` is accepted but needs a policy file and is not used. Tests start a private `dbus-daemon --session` and pass its address with `--bus-address`, so they never touch the desk's own bus |
| 2 | Names | Service `io.github.samersamaha.LexusHeadUnit`, object `/io/github/samersamaha/LexusHeadUnit/VehicleData`, interface `io.github.samersamaha.LexusHeadUnit.VehicleData1`. The `1` is the interface version: an incompatible change makes `VehicleData2` beside it |
| 3 | What is on the wire? | Signal `SampleChanged(u signalId, d value, u unit, x timestampMilliseconds, u status)`; signal `ConnectionChanged(u from, u trigger, u to, x timestampMilliseconds)`; method `GetSamples() -> a(uduxu)`, the five sample fields in the same order as the signal, one entry per signal; method `GetConnection() -> (u state, b hasTransition, u from, u trigger, u to, x timestampMilliseconds)`; method `GetInterfaceVersion() -> u`. The enumerations travel as their numeric values; the client rejects a number out of range and counts it |
| 4 | How does a late client become complete without a race? | It subscribes to both signals first, then calls `GetSamples` and `GetConnection`. D-Bus delivers the messages of one sender connection in the order they were sent, so every signal that arrives before the reply describes a state no newer than the reply, and every signal after it is newer. The client therefore applies everything in arrival order, the reply overwriting all signals; no timestamp comparison is needed (and one would be wrong: marking a signal Stale keeps its timestamp) |
| 5 | Which thread publishes? | The worker thread produces samples (as today); the store's change listener hands each one to the service object with a queued call (`QMetaObject::invokeMethod` with a lambda, so the sample is copied and no meta-type is needed); the main thread updates a mirror of the latest samples and emits the D-Bus signal. The mirror answers `GetSamples` without touching the store across threads |
| 6 | What does a client do when the service disappears or comes back? | A `QDBusServiceWatcher` reports it. On loss: every Valid sample it holds becomes Stale (emitted), and the connection goes to Error with the trigger LinkLost, because the link to the data is gone. On return: it fetches the state again |
| 7 | How does the vehicle-data app use it? | `lexus-head-unit --source dbus` builds a `VehicleDataClient` instead of a source, and connects its signals to the same view-model slots. The view models do not change (REQ-011). The in-process modes (`elm327`, `fake`) stay for desk work and the existing tests |
| 8 | Where do the Qt meta-type declarations live? | In a header-only library (`src/qt_value_types/`) included by both the view models and the client, so the service process never links HMI code |
| 9 | How is the D-Bus hop measured (REQ-009 input)? | The integration test publishes 1,000 samples and records, at the client, the steady-clock time of arrival minus the time of publication (the same monotonic clock across processes on Linux). Minimum, median, 95th and 99th percentile and maximum are printed by the test. On the Pi this is part of LHU-032 |
| 10 | systemd? | User units in `deploy/systemd/`: the service as `Type=dbus` with `BusName=` and `Restart=on-failure`; the hub `Wants=` and `After=` the service, `Restart=always`. Both use absolute `--config` and `--registry` paths (the LHU-013 review finding). Whether labwc starts `graphical-session.target` and passes `WAYLAND_DISPLAY` to user units is assumption A13, with labwc's autostart file as the fallback |

## 3. Nouns to classes

| Class | Library | Responsibility |
|---|---|---|
| `DBusSample`, `DBusConnectionState` | `lexus_head_unit_service_dbus` | Wire structs, their D-Bus marshalling, conversion to and from `SignalSample` and `ConnectionTransition` with range checks |
| `VehicleDataService` | same | The exported object: a thread-safe `publishSample` and `publishTransition`, the mirror, the three methods, the two signals |
| `VehicleDataClient` | same | Subscribe, fetch, mirror, watch the service; emits `sampleArrived` and `connectionChanged` as Qt signals |
| `applyStalenessConfiguration`, `buildSource` | `lexus_head_unit_source_wiring` (moved from `src/app/`) | Shared by the app and the service executable |
| `lexus-vehicle-data-service` | executable | `main()`: configuration, source, service layer, worker loop, `VehicleDataService`, bus name |

## 4. What each class stores

- `VehicleDataService`: `std::array<SignalSample, signalCount> m_samples` (initialised from an empty store's snapshot), `ConnectionState m_state`, `std::optional<ConnectionTransition> m_lastTransition`, a published-sample counter.
- `VehicleDataClient`: `QDBusConnection`, `QDBusServiceWatcher`, `std::array<SignalSample, signalCount>`, `ConnectionState`, flags for initial state received and service available, counters for samples received and malformed messages.

## 5. Verbs to methods

| Class | Method | Notes |
|---|---|---|
| `VehicleDataService` | `publishSample(sample)`, `publishTransition(transition)` | Any thread; queued to the object's thread |
| | `registerOn(connection, error)` | Registers the object and the service name; false if the name is taken |
| | `GetSamples()`, `GetConnection()`, `GetInterfaceVersion()` | D-Bus methods (scriptable slots) |
| | `SampleChanged(...)`, `ConnectionChanged(...)` | D-Bus signals |
| `VehicleDataClient` | `start()` | Subscribes, starts the watcher, fetches the state |
| | `latest(id)`, `connectionState()`, `hasInitialState()`, `isServiceAvailable()`, `samplesReceived()`, `malformedMessages()` | |
| | signals `sampleArrived(SignalSample)`, `connectionChanged(ConnectionTransition)`, `initialStateReceived()`, `serviceAvailabilityChanged(bool)` | |
| free | `toDBus(sample)`, `fromDBus(dbusSample) -> optional<SignalSample>` and the same for the connection | Range checks on every enumeration |

## 6. Interaction sequence (main scenario)

1. systemd starts `lexus-vehicle-data-service`; it builds the source and the service layer as the app did, registers the object, then the bus name, then starts the worker loop.
2. The worker produces a sample; the store notifies; the listener calls `publishSample`; the main thread updates the mirror and emits `SampleChanged`.
3. The hub starts `lexus-head-unit --source dbus`; its client subscribes, calls `GetSamples` and `GetConnection`, applies the replies, and from then on applies each signal; the view models update as before.
4. If the service restarts, the client shows Stale values and Error, then becomes complete again when the name reappears.

## 7. Failure cases

| Failure | Handling |
|---|---|
| Bus not reachable at start | Service prints the error and exits 1 (systemd restarts it); client stays unavailable and keeps watching |
| Bus name already taken (a second service) | Exits 2 before starting the source, so two processes never talk to the adapter |
| Out-of-range enumeration or malformed reply | Dropped and counted (`malformedMessages`) |
| Service gone | Client marks Valid samples Stale and goes to Error (LinkLost) |
| Service back | Client fetches the whole state again |
| A slow client | D-Bus queues per connection; the service never waits for a client |

## 8. Test plan

| # | Test | Requirement |
|---|---|---|
| 1 | Wire conversion round trips every signal, unit, status, state and trigger; every out-of-range number is rejected | REQ-017 |
| 2 | Two clients on a private bus receive identical sequences equal to the 1,000 published samples; the hop is printed (min, median, p95, p99, max) | REQ-017, REQ-009 input |
| 3 | A third client connecting after 100 samples holds the current value of all 8 signals within 500 ms | REQ-017 |
| 4 | Connection transitions reach clients; a late client gets the current state and last transition | REQ-017 |
| 5 | Service gone: the client shows Stale and Error; service back: complete again | REQ-017 |
| 6 | End to end: the `lexus-vehicle-data-service` executable with the fake source on a private bus; a client sees all 8 signals Valid and Connected within 5 s; a second service on the same bus exits 2 | REQ-017, REQ-002 |
| 7 | Introspection of the live object matches the committed interface XML (method and signal names and signatures) | REQ-017 |

## 9. Alternatives considered

| Alternative | Why rejected |
|---|---|
| Properties with `PropertiesChanged` per signal | Eight properties of a struct type would carry the same data in a more verbose form, and a sample would need two messages (value and status); one signal per sample is simpler to order |
| A sample batch signal at a fixed rate | Adds latency up to the batch period to REQ-009 for no benefit at OBD rates (tens of samples per second) |
| A Unix socket with a custom protocol | Would work, but D-Bus gives discovery, name ownership, introspection and service-gone notification for free, and is what a Linux infotainment platform uses |
| Moving `WorkerBridge` into the service | The bridge carries HMI meta-types; the service only needs a queued lambda, so it stays free of the HMI library |

## Changes after the design review

| # | Change | Reason |
|---|---|---|
| 1 | None yet; the review happens after merge (D-049) | |

## Design vs. implementation

Merged as PR #61. No deviation from the interface or the sequence. Details the note did not spell out:

- `SIGTERM` handling moved from the hub into `lexus_head_unit_process_support`, so the service also stops its worker loop and source before exiting (the end-to-end test checks exit code 0 on `SIGTERM`).
- The vehicle-data app's `main()` now holds the in-process pipeline in one object, so that `--source dbus` and the in-process modes share the window code.
- The client also re-applies the last transition from the `GetConnection` reply, so a late client's connection strip shows the cause, not only the state.
- The hub's status strip does not use the client yet; that comes with the power flags (LHU-025).

- The hop from the worker thread uses a separate, unexported `VehicleDataServiceInbox` object. A first version emitted a queued signal from the exported object itself; Qt D-Bus watches every signal of an exported object, refused the ones emitted from the worker thread, and crashed converting the sample (found by the two-client test under AddressSanitizer). A version with `QMetaObject::invokeMethod` and a lambda worked but tripped the static analyser's leak check inside Qt.

**Desk measurement of the hop (REQ-009 input), private bus, 1,000 samples published one per millisecond from a worker thread, merged version:** debug min 0.133, median 0.180, p95 0.309, p99 0.357, max 0.385 ms; sanitizers min 0.151, median 0.263, p95 0.444, p99 0.518, max 0.709 ms. Earlier runs, kept as recorded: the `invokeMethod` version, debug, min 0.122, median 0.174, p95 0.316, p99 0.389, max 0.565 ms; and a first test that published all 1,000 samples while the sending thread was blocked waiting for the publisher, so it measured the queue rather than the hop (median 33.988, p95 43.935, max 45.044 ms), corrected by running the event loop while publishing. The late client was complete 2 to 7 ms after connecting across runs (budget 500 ms). On the Pi: not yet measured (LHU-032, checklist step 3.6). Status: Implemented.
