# DN-008: VehicleDataSource interface and FakeSource

| | |
|---|---|
| Ticket | LHU-008 |
| Requirements | REQ-002 |
| Author | implementer (build-out form, D-049) |
| Status | Implemented |
| Draft written | 2026-10-08, about 20 minutes |
| Design review | after merge, by the repository owner (D-049) |
| Approved | 2026-10-08 |

## 1. Problem

The service layer must receive vehicle data through one interface, `VehicleDataSource`, so that the ELM327 path, the CAN path, a replayed recording and a fake can be swapped without the store, the monitor or the screens knowing which one runs. This ticket defines that interface, the listener it calls, the adapter that feeds the `SignalStore`, a scriptable `FakeSource`, the integration suite that every later source must also pass, and the CI check that no service or HMI target links to a concrete source.

## 2. Clarifying questions and assumptions

| # | Question | Assumption made |
|---|---|---|
| 1 | Where does the interface live? | In the service library (`lexus_head_unit/service/vehicle_data_source.h`), not under `src/hardware/` as the architecture document first said. The consumer owns the interface and the sources implement it (dependency inversion); otherwise the service library would depend on a hardware target, the wrong direction. `ARCHITECTURE.md` is corrected in this PR |
| 2 | Does a source own a thread? | No. The worker loop (the application in v0.1.0, the service process from v0.2.0) calls `runOnce()` repeatedly; each call does one bounded unit of work (one request and reply, one read with a timeout, one scripted step) and returns. Blocking is bounded by the source's own timeouts. This keeps every source deterministic in tests and keeps all service-layer objects on one thread (architecture section 7) |
| 3 | Who owns the connection state machine? | The source: it is the only thing that knows when a handshake succeeded or a link was lost. It reports each accepted transition to the listener as the "connection event" of the architecture. The service layer never calls `handle()` itself |
| 4 | How does the listener get attached? | `start(listener)` takes a reference; the listener must outlive the source until `stop()`. The wiring that creates both guarantees it |
| 5 | What is a "unit" of malformed input for the error counter (REQ-010)? | One reply or frame that the source's decoder refused. The source counts it in `SourceCounters::malformedInputs`; nothing reaches the store |
| 6 | How does one suite run against every source without recompiling? | The suite is parameterised by a `SourceHarness` that knows how to build the source and how to make it produce a sample, lose the link and recover (scripting for the fake; the emulator for ELM327 in LHU-012; a traffic generator for CAN in LHU-028; a recording for replay in LHU-029). The product code selects the source by configuration (DN-012); the suite selects the harness by parameter |
| 7 | What proves "no link dependency on a concrete source"? | `tools/check_link_graph.py` reads CMake's graphviz output and fails if `lexus_head_unit_service` or any HMI target reaches a target whose name ends in `_source`. Run in CI after the configure step |

## 3. Nouns to classes

| Class | Responsibility |
|---|---|
| `VehicleDataSource` (interface) | `name()`, `start(listener)`, `runOnce()`, `stop()`, `connectionState()`, `counters()` |
| `VehicleDataSourceListener` (interface) | `onSample(sample)`, `onConnectionChanged(transition)`; implemented by the service-layer side |
| `SourceCounters` (struct) | `samplesEmitted`, `malformedInputs`, `requestsSent`, `rejectedTransitions` |
| `SignalStoreFeeder` | The service layer's listener: writes each sample into a `SignalStore`, keeps the latest connection transition and a count, forwards transitions to an optional log hook |
| `FakeSource` | Scriptable source in `src/hardware/fake/`: owns a `ConnectionStateMachine`; each `runOnce()` executes one scripted step |
| `SourceHarness` (test interface) | Per-source adapter for the integration suite: make the source, produce a sample, break and restore the link |
| `check_link_graph.py` | CI check of REQ-002's link rule |

## 4. What each class stores

- `SignalStoreFeeder`: constructor takes `SignalStore&`; fields: non-owning `SignalStore*`, `std::optional<ConnectionTransition> m_latestTransition`, `std::uint64_t m_transitionCount`, `std::uint64_t m_acceptedSamples`, `m_rejectedSamples`, a `std::function` transition hook. Owned by the wiring; outlives the source's running period.
- `FakeSource`: constructor takes `const Clock&`; fields: `ConnectionStateMachine m_machine`, `std::deque<ScriptStep> m_script` (a step is Sample{id, value}, MalformedInput, LinkLoss, Reconnect, HandshakeFailure), `VehicleDataSourceListener* m_listener` (null when stopped), `SourceCounters m_counters`, `bool m_failNextHandshake`. The machine's transition listener forwards to `m_listener`.

## 5. Verbs to methods

| Class | Method | Inputs (with units) | Output (with units) | Notes |
|---|---|---|---|---|
| `VehicleDataSource` | `start(listener)` | listener reference | — | Raises StartRequested; a source that can connect at once also raises HandshakeSucceeded. Worker thread |
| `VehicleDataSource` | `runOnce()` | — | — | One bounded step. Worker thread. Does nothing when stopped |
| `VehicleDataSource` | `stop()` | — | — | Raises StopRequested if running; detaches the listener |
| `VehicleDataSource` | `connectionState()`, `counters()`, `name()` | — | state, counters, text | |
| `VehicleDataSourceListener` | `onSample(sample)` | `SignalSample` with the source's clock timestamp | — | Called on the worker thread, synchronously inside `runOnce()` |
| `VehicleDataSourceListener` | `onConnectionChanged(transition)` | `ConnectionTransition` | — | Same thread; one call per accepted transition |
| `SignalStoreFeeder` | `onSample` | sample | — | `store.update(sample)`; counts accepted and rejected |
| `SignalStoreFeeder` | `onConnectionChanged` | transition | — | Keeps latest, counts, forwards to the hook |
| `SignalStoreFeeder` | `connectionState()` | — | state or Disconnected before the first transition | |
| `FakeSource` | `scriptSample(id, value)`, `scriptMalformedInput()`, `scriptLinkLoss()`, `scriptReconnect()`, `scriptHandshakeFailure()` | — | — | Append one step; `runOnce()` consumes one step per call; `pendingSteps()` reports the queue length |

Bad input: a scripted sample for an unknown id is emitted as is; the store rejects and counts it, which is exactly what the feeder test checks. `runOnce()` while Disconnected consumes nothing and emits nothing.

## 6. Interaction sequence, main scenario

1. The wiring creates `SteadyClock`, `SignalStore`, `StalenessMonitor`, `SignalStoreFeeder(store)`, and the configured source, say `FakeSource(clock)`.
2. `source.start(feeder)`: the fake raises StartRequested then HandshakeSucceeded; the feeder records Connecting then Connected.
3. The worker loop repeats: `source.runOnce()`, then `monitor.check()`. Each `runOnce()` pops a scripted sample, stamps it with the clock, and calls `feeder.onSample`, which calls `store.update` and the store notifies its change listener (the HMI hop, LHU-013).
4. A scripted LinkLoss step makes the fake raise LinkLost: the feeder records Error. The monitor marks the signals Stale as their timeouts pass.
5. A scripted Reconnect step raises BackoffElapsed and HandshakeSucceeded: Connected again; the next sample sets Valid.
6. `source.stop()`: StopRequested, Disconnected, listener detached.

## 7. Failure cases

| # | Failure | How the design handles it |
|---|---|---|
| 1 | `runOnce()` before `start()` or after `stop()` | No listener: nothing is emitted, the script is not consumed |
| 2 | `start()` twice | The second StartRequested is rejected by the machine and counted in `rejectedTransitions`; the source stays as it was |
| 3 | `stop()` while Disconnected | Guarded: the source checks its state and does not raise the trigger (review finding on DN-007), so the rejected counter keeps its meaning |
| 4 | Scripted handshake failure | `start()` raises StartRequested then HandshakeFailed: state Error, as a real source with an unreachable adapter would |
| 5 | Malformed input | Counted, no sample, state unchanged (REQ-010 shape, proven for real decoders in LHU-009 and LHU-010) |
| 6 | Listener throws inside `runOnce()` | Propagates to the worker loop; not caught in the source |
| 7 | A service or HMI target gains a link to a concrete source | `check_link_graph.py` fails CI |

## 8. Test plan

| # | Test | Requirement |
|---|---|---|
| 1 | `FakeSourceTest`: starts Disconnected; `start` moves to Connected through Connecting and the listener sees both transitions; `stop` moves to Disconnected and detaches | REQ-002 |
| 2 | `FakeSourceTest`: each scripted step does what section 5 says; samples carry the clock time; counters count; `runOnce` when stopped does nothing | REQ-002 |
| 3 | `SignalStoreFeederTest`: samples land in the store as Valid; a rejected sample is counted by the feeder and the store; transitions are kept and forwarded | REQ-002 |
| 4 | `ServiceAgainstEachSourceTest` (integration, parameterised by harness): start reaches Connected; a produced sample is Valid in the store with the right unit; the link break reaches Error and the monitor marks Stale after the timeout; restore reaches Connected and a new sample is Valid; stop reaches Disconnected; counters are consistent. One harness today (fake); LHU-012, LHU-028 and LHU-029 add theirs | REQ-002 |
| 5 | `tools/test_check_link_graph.py`: a graph where the service reaches a `_source` target fails; one where only the test executables do passes; transitive edges are followed | REQ-002 |
| 6 | CI: configure with `--graphviz`, run the checker | REQ-002 |

## 9. Alternatives considered

1. **Sources own a thread and push to the listener asynchronously.** Closer to how a serial reader is often written. Rejected: the service layer would then need locks or a queue in every test, and the "one worker thread owns everything" rule of the architecture would be broken inside the sources. A bounded `runOnce()` driven by one loop gives the same throughput (the loop blocks in the source's own read timeout, not in a sleep) with no shared state.
2. **Interface in `src/hardware/` as the architecture document said.** Rejected for the dependency direction explained in question 1; the document is corrected rather than the code bent to it.
3. **Signals and slots (Qt) for the listener.** Rejected by D-008: no Qt in the service layer. A two-method abstract listener is enough and is mocked trivially.
4. **One listener method with a variant payload.** Rejected: two typed methods are clearer and let the feeder count samples and transitions separately.
5. **Reusing `FakeSource` as the only test double, without a harness.** Rejected because the suite must run unchanged against the ELM327 source over the emulator; the harness is the seam that makes "the same suite" literally true.

## Changes after the design review

| # | Change | Reason |
|---|---|---|
| 1 | None yet; the review happens after merge (D-049) | |

## Design vs. implementation

Merged as PR #50 (be74734). No deviations. Review findings carried: the placeholder unit for an unknown id in the fake is test behaviour, not product behaviour; the guarded target names in `check_link_graph.py` must be matched when LHU-013 names the HMI targets. Status: Implemented.
