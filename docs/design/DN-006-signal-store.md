# DN-006: Signal model and SignalStore with staleness

| | |
|---|---|
| Ticket | LHU-006 |
| Requirements | REQ-003, REQ-006 |
| Author | implementer (build-out form, D-049) |
| Status | Approved |
| Draft written | 2026-10-08, about 20 minutes |
| Design review | after merge, by the repository owner (D-049) |
| Approved | 2026-10-08 |

## 1. Problem

The service layer needs one record type that every data source produces and every screen consumes: a signal with a value, a unit, a monotonic timestamp and a status of NeverReceived, Valid or Stale. It needs a store that keeps the latest such record per signal, rejects samples that arrive out of order, tells a listener about every change, and marks a signal Stale when its own timeout has passed on an injected clock, so that the behaviour is testable without waiting.

## 2. Clarifying questions and assumptions

| # | Question | Assumption made |
|---|---|---|
| 1 | Which signals exist? | The 8 PIDs of REQ-004 (vehicle speed, engine RPM, coolant temperature, engine load, throttle position, intake air temperature, control module voltage, fuel level). Later tickets extend the enumeration (mass air flow and derived signals in LHU-031). The vehicle's actual support is discovered at run time (assumption A1 in `docs/release/PI_BRINGUP_CHECKLIST.md`) |
| 2 | Where does the timestamp come from? | The source stamps each sample with `Clock::nowMilliseconds()` from the same clock the staleness monitor reads, so "age" is a difference of two readings of one monotonic clock. Resolution 1 ms (`int64_t`). The real clock is `std::chrono::steady_clock`; tests inject `ManualClock` |
| 3 | Who sets the status? | Only the store. A source never emits Stale; whatever status a source writes into a sample is ignored, the store writes Valid on acceptance and Stale on timeout |
| 4 | Is "same timestamp as the stored sample" out of order? | Yes. A sample is accepted only if its timestamp is strictly newer than the stored one (architecture section 8); equal timestamps are rejected and counted, because two readings in the same millisecond cannot be ordered |
| 5 | Does the store check the unit? | Yes. The unit of a signal is fixed in its definition; a sample carrying another unit is rejected and counted separately from out-of-order samples. This catches a decoder wired to the wrong signal |
| 6 | Where do per-signal timeouts come from? | The definition table carries the default (1000 ms); a different timeout per signal is set through `SignalStore::setStalenessTimeout` by whoever reads the configuration (LHU-012 defines the file) |
| 7 | Which thread? | One worker thread owns the store and the monitor. No locking inside; the HMI gets copies through the listener and a queued hop (LHU-013), the D-Bus adapter the same (LHU-022) |
| 8 | What does a Stale signal keep? | Its last value, unit and timestamp. Only the status changes, so a screen can grey the old value out instead of blanking it (REQ-012) |
| 9 | Who calls the staleness check and how often? | The worker loop, at an interval it chooses (the sources of LHU-012 call it between polls). The check is idempotent and cheap: one subtraction per signal |

## 3. Nouns to classes

| Class | Responsibility |
|---|---|
| `SignalId` (enum class) | Names each signal; also the index into the store's arrays |
| `Unit` (enum class) | The unit a value is expressed in, with a short text for display |
| `SignalStatus` (enum class) | NeverReceived, Valid, Stale |
| `SignalSample` (struct) | One record: id, value, unit, timestamp in ms, status. Shaped like a property value in a vehicle hardware abstraction layer (an influence, not compatibility) |
| `SignalDefinition` (struct) | Static facts about a signal: id, name, unit, staleness timeout. `defaultSignalDefinitions()` is the table; `definitionOf(id)` looks one up |
| `Clock` (interface) | `nowMilliseconds()` on a monotonic clock. `SteadyClock` is the real one |
| `ManualClock` | Test clock: `advanceMilliseconds`, `setMilliseconds`. Lives in the library so integration tests and the emulator harness can use it |
| `SignalStore` | Latest sample per signal; accepts or rejects updates; counts rejections; notifies a listener; marks Stale on request; holds the per-signal timeouts |
| `StalenessMonitor` | Given a store and a clock, finds every Valid signal older than its timeout and asks the store to mark it Stale |

## 4. What each class stores

- `SignalSample`: `SignalId signalId`, `double value`, `Unit unit`, `std::int64_t timestampMilliseconds`, `SignalStatus status`. A plain value type, copied freely; 32 bytes.
- `SignalDefinition`: `SignalId id`, `std::string_view name`, `Unit unit`, `std::int64_t stalenessTimeoutMilliseconds`. The default table is a `constexpr std::array` with one row per `SignalId`, in enumeration order, so a lookup is an array index.
- `SteadyClock`: no fields. `ManualClock`: `std::int64_t m_nowMilliseconds`, constructed at a given time (default 0).
- `SignalStore`: constructor takes nothing (default definitions) or a `std::vector<SignalDefinition>` override table. Fields: `std::array<SignalSample, signalCount> m_latestSamples`, one per id, initialised to NeverReceived with value 0 and the definition's unit; `std::array<std::int64_t, signalCount> m_stalenessTimeoutsMilliseconds`; `std::uint64_t m_rejectedOutOfOrderCount`, `m_rejectedUnitMismatchCount`, `m_rejectedUnknownSignalCount`; `std::function<void(const SignalSample&)> m_changeListener`. Owned by the worker-thread object that creates it (the application's wiring in `src/app/`, from LHU-013); lives as long as the process.
- `StalenessMonitor`: constructor takes `SignalStore&` and `const Clock&`; keeps non-owning pointers to both. The caller guarantees both outlive the monitor (they are siblings in the same owner).

## 5. Verbs to methods

| Class | Method | Inputs (with units) | Output (with units) | Notes |
|---|---|---|---|---|
| `SignalStore` | `update(sample)` | `SignalSample`: id, value in the signal's unit, unit, timestamp ms | `UpdateResult`: Accepted, RejectedOutOfOrder, RejectedUnitMismatch, RejectedUnknownSignal | Accepted: stores value, unit, timestamp, sets Valid, calls the listener. Rejected: nothing stored, counter incremented, listener not called. Worker thread |
| `SignalStore` | `markStale(id)` | `SignalId` | `bool`: true if the status changed | Valid becomes Stale and the listener is called; NeverReceived or already Stale: no change, no call |
| `SignalStore` | `latest(id)` | `SignalId` | `const SignalSample&` | Always valid to read; NeverReceived until the first update |
| `SignalStore` | `snapshot()` | — | `std::array<SignalSample, signalCount>` copy | For late joiners (D-Bus, LHU-022) |
| `SignalStore` | `setStalenessTimeout(id, ms)` | id, timeout ms (must be > 0) | `bool`: false and unchanged if ms is not positive | Configuration hook (REQ-006) |
| `SignalStore` | `stalenessTimeoutMilliseconds(id)` | id | ms | |
| `SignalStore` | `setChangeListener(listener)` | `std::function<void(const SignalSample&)>` | — | One listener; replacing it is allowed; an empty function means no notifications |
| `SignalStore` | `rejectedOutOfOrderCount()`, `rejectedUnitMismatchCount()`, `rejectedUnknownSignalCount()` | — | counts | REQ-003: rejected samples are counted |
| `StalenessMonitor` | `check()` | — | number of signals newly marked Stale | For each id with status Valid: `now - timestamp > timeout` means Stale. Strictly greater, so a signal is still Valid exactly at timestamp + timeout (REQ-006 acceptance) |
| `Clock` | `nowMilliseconds()` | — | ms, monotonic | `SteadyClock` reads `std::chrono::steady_clock` |
| `ManualClock` | `advanceMilliseconds(ms)`, `setMilliseconds(ms)` | ms | — | Tests only |
| free functions | `toString(SignalId)`, `toString(Unit)`, `toString(SignalStatus)` | enum | `std::string_view` | Display and logs; `Unit` text is the symbol ("km/h") |
| free functions | `defaultSignalDefinitions()`, `definitionOf(id)` | — / id | the table / one row | `constexpr` |

Bad input: an id outside the enumeration cannot be constructed from C++ without a cast; the store still checks the index against `signalCount` and counts `RejectedUnknownSignal` rather than indexing out of bounds. A `NaN` value is a decoder bug and is rejected as out of order? No: it is accepted, because the store does not know the signal's domain; decoders (LHU-009) are responsible for never producing one, and their tests check it.

## 6. Interaction sequence, main scenario

1. The application constructs `SteadyClock`, `SignalStore` (default definitions, then per-signal timeouts from configuration) and `StalenessMonitor(store, clock)`, all on the worker thread.
2. The application sets the store's change listener to a function that copies the sample to the HMI through a queued hop.
3. A source decodes vehicle speed 63 km/h and calls `store.update({VehicleSpeed, 63.0, KilometresPerHour, clock.nowMilliseconds(), ignored})`.
4. The store finds the stored sample for VehicleSpeed is NeverReceived (timestamp 0), the unit matches, and the new timestamp is newer: it stores the value, sets Valid, returns Accepted, and calls the listener with the stored sample.
5. Between polls the worker loop calls `monitor.check()`. The clock says 1,100 ms have passed since the sample's timestamp, the timeout is 1,000 ms, so the monitor calls `store.markStale(VehicleSpeed)`; the status becomes Stale and the listener is called again with the same value and the new status.
6. The next accepted sample for VehicleSpeed sets Valid again (Stale to Valid, REQ-003).

```
source --update(sample)--> SignalStore --listener(sample)--> HMI hop
                               ^
StalenessMonitor --check()-----+  (reads Clock, calls markStale)
```

## 7. Failure cases

| # | Failure | How the design handles it |
|---|---|---|
| 1 | Sample older than or equal to the stored one (a late reply, a replayed file with repeated timestamps) | Rejected, `rejectedOutOfOrderCount` incremented, listener not called, stored sample unchanged |
| 2 | Sample carries the wrong unit for its id | Rejected, `rejectedUnitMismatchCount` incremented |
| 3 | Sample id outside the enumeration (only possible through a cast) | Rejected, `rejectedUnknownSignalCount` incremented; no out-of-bounds access |
| 4 | Clock goes backwards | It cannot: `steady_clock` is monotonic by definition; `ManualClock` can be set backwards by a test only. If `now < timestamp` the age is negative and the signal stays Valid |
| 5 | No listener set | Notifications are skipped; every other behaviour is identical |
| 6 | Listener throws | Not caught here; the worker thread's owner decides. Listeners in this project copy a 32-byte struct and do not throw |
| 7 | Timeout set to 0 or negative | `setStalenessTimeout` returns false and leaves the previous value |
| 8 | `markStale` on a NeverReceived signal | No change, returns false; a signal that never arrived is not "stale", it is absent (REQ-003 distinguishes the two) |
| 9 | Monitor called very often | Cost is `signalCount` subtractions; no allocation |

## 8. Test plan

| # | Test | Requirement |
|---|---|---|
| 1 | `SignalSampleTest`: a default sample is NeverReceived; every field round-trips; `toString` of every id, unit and status is non-empty and distinct | REQ-003 |
| 2 | `SignalDefinitionTest`: the table has one row per id in enumeration order; every default timeout is 1000 ms; `definitionOf` returns the matching row | REQ-006 |
| 3 | `SignalStoreTest`: first update turns NeverReceived into Valid with the exact value, unit and timestamp (resolution 1 ms: two samples 1 ms apart are both accepted) | REQ-003 |
| 4 | `SignalStoreTest`: an older timestamp and an equal timestamp are rejected, counted, and leave the stored sample untouched; the listener is not called | REQ-003 |
| 5 | `SignalStoreTest`: wrong unit rejected and counted; unknown id rejected and counted | REQ-003 |
| 6 | `SignalStoreTest`: `markStale` moves Valid to Stale keeping the value, notifies once; a second `markStale` is a no-op; `markStale` on NeverReceived is a no-op; an update after Stale gives Valid (every transition of REQ-003) | REQ-003, REQ-006 |
| 7 | `SignalStoreTest`: the listener receives each accepted sample and each staleness change, in order; replacing the listener with an empty one stops notifications | REQ-003 |
| 8 | `SignalStoreTest`: `setStalenessTimeout` rejects 0 and negative values and accepts positive ones; `snapshot` returns every signal | REQ-006 |
| 9 | `StalenessMonitorTest`: with `ManualClock`, a signal updated at T is Valid at T + 1000 and Stale at T + 1100 for the default timeout, and Valid at T + 250, Stale at T + 350 for a per-signal timeout of 250 ms; `check` returns the number newly marked; only the expired signal changes while the others stay Valid | REQ-006 |
| 10 | `StalenessMonitorTest`: a clock that reads earlier than the sample leaves the signal Valid; NeverReceived signals are never marked | REQ-006 |
| 11 | `tools/test_check_traceability.py`: the script finds tags, fails on an Approved requirement without a tag or a measurement file, passes on a Planned row, and fails on an unknown requirement in a tag | process |

## 9. Alternatives considered

1. **Staleness inside `SignalStore`, with the clock injected into the store.** One class fewer. Rejected because the store would then need the clock for every update too, or carry an unused dependency; keeping "what is the latest sample" and "is it old" apart means each has one reason to change and the store can be tested with no clock at all.
2. **`std::map<SignalId, SignalSample>`.** Flexible for ids that are not dense. Rejected: the enumeration is dense and small, the per-sample path is hot (every decoded reply), and a `std::array` gives constant-time access with no allocation; `snapshot()` is a 256-byte copy.
3. **Letting the source set the status, with the store trusting it.** Rejected because REQ-003 wants the status transitions owned and tested in one place; a source that could write Stale could also write Valid for a bad reply, which REQ-010 forbids.
4. **Rejecting equal timestamps as "duplicate" but accepting them silently.** Rejected: silent drops hide a replay or decoder fault; a count makes the fault visible in a test and in a log.
5. **A mutex inside the store so any thread may read.** Rejected for v0.1.0: the architecture (section 7) has one owning thread and copies across the boundary; a lock would invite the HMI to read the worker's data directly, which REQ-011 is meant to prevent.

## Changes after the design review

| # | Change | Reason |
|---|---|---|
| 1 | None yet; the review happens after merge (D-049) | |

## Design vs. implementation

Added after the code pull request is merged.
