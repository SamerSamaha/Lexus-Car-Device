# DN-013: QML home screen, view models and the application

| | |
|---|---|
| Ticket | LHU-013 |
| Requirements | REQ-006, REQ-007, REQ-011, REQ-012 |
| Author | implementer (build-out form, D-049) |
| Status | Implemented |
| Draft written | 2026-10-08, about 30 minutes |
| Design review | after merge, by the repository owner (D-049) |
| Approved | 2026-10-08 |

## 1. Problem

Everything below the Qt boundary exists; nothing is on a screen. This ticket adds the view models that QML binds to, the thread hop from the worker thread to the UI thread, the worker loop that drives a source, the application that wires a configured source to a window, and the home screen (two primary values and a status strip), sized in millimetres, with Stale and NeverReceived drawn in their own styles. It also adds the HMI test tier on the offscreen platform and the REQ-011 include check in CI.

## 2. Clarifying questions and assumptions

| # | Question | Assumption made |
|---|---|---|
| 1 | What does QML bind to? | A `VehicleDataViewModel` context property named `vehicleData` with one `SignalTileModel` per signal (`name`, `valueText`, `unitText`, `statusText`, `isValid`, `isStale`, `isNeverReceived`), the list of all tiles, named accessors for the two primary values, and a `ConnectionStatusModel` (`stateText`, `isConnected`, `isError`, `lastCauseText`). No service or hardware type is visible from QML (REQ-011) |
| 2 | How does data cross threads? | `WorkerBridge`, a `QObject` whose two signals carry a `SignalSample` and a `ConnectionTransition` by value. The worker thread emits them from the store's change listener and the feeder's transition hook; the view model's slots are connected with `Qt::QueuedConnection`, so Qt copies the values and delivers them on the UI thread. The worker thread never touches a `QObject` (architecture section 7) |
| 3 | Who owns the worker thread? | `WorkerLoop` in the service library (plain C++17: `std::thread`, `std::atomic<bool>`): `start()` starts the source and the loop (`runOnce`, `check`, an optional per-cycle callback, sleep for the idle hint capped at 50 ms with a configurable minimum cycle); `stop()` joins. LHU-022 reuses it in the service process |
| 4 | How is the source chosen? | `source.kind` in the configuration file (DN-012): `elm327` builds `FileDescriptorByteTransport(elm327.device)` and `Elm327ObdSource`; `fake` builds `FakeSource` and a demo script that moves the eight values each cycle so the screen can be seen without the emulator. The factory lives in `src/app/`, the only place that knows every concrete type (architecture section 4.3) |
| 5 | Sizes? | `Sizes` QML singleton: `pixelsPerMillimetre = 11.6`, `mm(x)`; touch targets `mm(10)`; a primary value's font pixel size is `mm(5.7)`, which gives a cap height of about 4 mm if the font's cap height is 0.7 of the em size (assumption A8, checked by eye on the panel). The window is 1280 x 720: the compositor rotates the portrait panel (D-045) |
| 6 | How is Stale drawn? | The value in a muted colour and a `STALE` badge beside the unit; Valid is the live colour with no badge; NeverReceived shows `--` and no badge. The HMI test asserts the live colour is never used for Stale (REQ-006) |
| 7 | How is REQ-007's 500 ms checked? | The QML test sets a state on the fake view model and `tryCompare`s the strip text with a 500 ms limit |
| 8 | What is the REQ-011 CI check? | `tools/check_hmi_includes.py`: no file under `src/hmi/` includes a `lexus_head_unit/hardware/` header; the only `lexus_head_unit/service/` headers allowed there are the value types `signal_id.h`, `signal_sample.h`, `connection_state_machine.h`; QML files import only Qt modules and `LexusHeadUnit`. The link-graph check already forbids `lexus_head_unit_hmi*` targets from reaching a concrete source; the application target is the wiring and is not guarded (DN-008 question 7 corrected) |
| 9 | Value formatting? | Integers for km/h, rpm, °C and %, one decimal for volts; `--` when NeverReceived |
| 10 | Which Qt pieces? | Qt 6.8 Core, Gui, Qml, Quick, QuickControls2 for the window; QuickTest for the HMI tier, Test for nothing else. CI installs the Debian packages in both jobs; tests run with `QT_QPA_PLATFORM=offscreen` |

## 3. Nouns to classes

| Class | Responsibility |
|---|---|
| `SignalTileModel` (QObject) | One signal's displayable state: name, formatted value, unit text, status; `applySample(sample)` on the UI thread |
| `ConnectionStatusModel` (QObject) | The connection state as text and flags; `applyTransition(transition)` |
| `VehicleDataViewModel` (QObject) | Owns the eight tiles and the status model; slots `onSample`, `onConnectionChanged`; `tiles`, `vehicleSpeed`, `engineRpm`, `connection` properties |
| `WorkerBridge` (QObject) | Two signals emitted from the worker thread, consumed on the UI thread through queued connections |
| `WorkerLoop` (service library) | Owns the worker thread; drives `runOnce`, `check`, the per-cycle callback and the sleep |
| `SourceFactory` (app) | Builds the configured source, its transport and the demo script |
| `FakeVehicleDemo` (app) | Scripts eight moving values into a `FakeSource` each cycle |
| QML: `Sizes`, `SignalTile`, `StatusStrip`, `HomeScreen`, `Main` | The screen |
| `check_hmi_includes.py` | REQ-011 CI check |

## 4. What each class stores

- `SignalTileModel`: `QString m_name`, `m_unitText`, `m_valueText`, `SignalStatus m_status`, `Unit m_unit`; parent `VehicleDataViewModel`.
- `ConnectionStatusModel`: `ConnectionState m_state`, `QString m_lastCauseText`, `quint64 m_transitionCount`.
- `VehicleDataViewModel`: `std::array<SignalTileModel*, signalCount>` (children), `ConnectionStatusModel* m_connection`.
- `WorkerLoop`: references to the source, listener and monitor; `std::function<void()> m_perCycle`; `std::int64_t m_minimumCycleMilliseconds`; `std::thread`; `std::atomic<bool> m_stopRequested`, `m_running`. Owned by `main`; stopped before the Qt objects are destroyed.

## 5. Verbs to methods

| Class | Method | Inputs | Output | Notes |
|---|---|---|---|---|
| `SignalTileModel` | `applySample(sample)` | `SignalSample` | — | Formats by unit; emits `changed`. UI thread |
| `ConnectionStatusModel` | `applyTransition(transition)` | `ConnectionTransition` | — | `stateText` is `toString(state)`; `lastCauseText` is `toString(trigger)` |
| `VehicleDataViewModel` | `onSample(sample)`, `onConnectionChanged(transition)` | by value | — | Queued targets; unknown ids ignored |
| `WorkerBridge` | signals `sampleArrived(SignalSample)`, `connectionChanged(ConnectionTransition)` | — | — | Emitted on the worker thread |
| `WorkerLoop` | `start()`, `stop()`, `isRunning()` | — | — | `start` is idempotent; `stop` joins; the destructor stops |
| `Sizes` | `mm(millimetres)` | real | int pixels | `Math.round(x * 11.6)` |
| `main` | `--config PATH`, `--source KIND`, `--fullscreen` | | exit code | Defaults: `deploy/head_unit.conf`, the file's kind, windowed |

## 6. Interaction sequence, main scenario

1. `main` reads the configuration, builds clock, store, monitor, feeder, source (factory), bridge, view model, QML engine with `vehicleData` as a context property, loads `Main.qml`, connects the bridge to the view model with queued connections, sets the store's change listener and the feeder's transition hook to emit the bridge signals, and starts the `WorkerLoop`.
2. On the worker thread the loop starts the source (Connecting, Connected: two transitions through the hook, two queued signals), then polls; each accepted sample makes the store notify, the listener emits `sampleArrived`.
3. On the UI thread the queued slot runs `tiles[id]->applySample`, the tile's properties change, the QML bindings re-render the value, unit and status.
4. The monitor marks a signal Stale after its timeout; the store notifies with status Stale; the same path greys the value and shows the badge.
5. On quit, `main` stops the loop (joins the thread) before the view model and engine are destroyed.

## 7. Failure cases

| # | Failure | How the design handles it |
|---|---|---|
| 1 | Configuration file missing | The defaults apply (fake source is not the default: `elm327` with `/dev/rfcomm0`); a message is printed; the screen shows Error state and NeverReceived tiles rather than exiting |
| 2 | Unknown `source.kind` | Printed and the fake source is used so the screen can still be checked |
| 3 | Sample for an id outside the tiles | Ignored by the view model (cannot happen through the store) |
| 4 | Worker thread blocked in a read when quitting | `stop()` sets the flag and joins; the read is bounded by the reply timeout (1 s), so quit takes at most that long |
| 5 | Queued signals after the view model is gone | `main` stops the loop before destroying the view model; the remaining queued events are dropped with the application |
| 6 | Offscreen platform without fonts or GL in CI | Debian's `fonts-dejavu-core` and the offscreen plugin come with the Qt packages; if the HMI job fails for a platform reason the failure is visible, not silent |

## 8. Test plan

| # | Test | Requirement |
|---|---|---|
| 1 | `SignalTileModelTest`: formatting per unit, `--` when NeverReceived, status flags and text, change signal emitted | REQ-012 |
| 2 | `ConnectionStatusModelTest`: state text and flags for every state, last cause text | REQ-007, REQ-012 |
| 3 | `VehicleDataViewModelTest`: eight tiles in id order with the definition names and units; a sample emitted on another thread through the bridge reaches the tile on the main thread after `processEvents` | REQ-011 (thread hop), REQ-012 |
| 4 | `WorkerLoopTest` (fake source, manual clock): start runs the source and the per-cycle callback; stop joins; samples flow to the feeder | REQ-002 |
| 5 | QML `tst_home_screen.qml` on the offscreen platform with a fake view model: for both primary signals, Valid shows the value, the unit and the live colour; Stale shows the badge and never the live colour; NeverReceived shows `--`; the status strip shows the new state within 500 ms; the value font is at least 4 mm; the touch target is at least 10 mm | REQ-012, REQ-006, REQ-007 |
| 6 | `tools/test_check_hmi_includes.py`: a hardware include, a store include and a stray QML import fail; the allowed value types pass | REQ-011 |
| 7 | CI: Qt installed in both jobs; `check_hmi_includes.py` step; HMI tests under `QT_QPA_PLATFORM=offscreen` | REQ-011 |

## 9. Alternatives considered

1. **One `QAbstractListModel` for the tiles.** Idiomatic for lists. Rejected for v0.1.0: eight fixed tiles with named roles bind more readably as objects, the home screen needs two of them by name, and a list model can be added for the 4 x 2 grid (LHU-039) if the Repeater over `tiles` proves awkward.
2. **View models reading the `SignalStore` directly with a mutex.** Rejected: it would put service-layer objects on two threads and break the rule that the worker thread owns them; the queued copy costs 32 bytes per sample.
3. **A `QThread` running the worker.** Rejected: the service layer stays Qt-free (D-008); `std::thread` with an atomic stop flag is enough, and the same `WorkerLoop` serves the D-Bus process later.
4. **Sizes in device-independent pixels.** Rejected by D-024: millimetres through one constant keep the 10 mm and 4 mm rules checkable.
5. **Test hooks (`Q_INVOKABLE` simulate methods) on the product view model.** Rejected: the QML test subclasses the view model in the test executable instead, so the product class has no test-only surface.

## Changes after the design review

| # | Change | Reason |
|---|---|---|
| 1 | None yet; the review happens after merge (D-049) | |

## Design vs. implementation

Merged as PR #55 (86ecce3). Two implementation details the note did not spell out: the meta-type declarations live in `value_types.h`, included before any `Q_OBJECT` class that uses the types, because the moc of the view model instantiates them first; and the QML module is compiled without `qmlcachegen` because its generated loader needs a C++20 extension under clang. The QML `TestCase` is made visible so that visibility and mouse events are real. Review findings carried: the systemd unit must pass an absolute `--config` (LHU-022); the `Row` anchors in `SignalTile.qml` are replaced when LHU-039 touches the tile. Status: Implemented.
