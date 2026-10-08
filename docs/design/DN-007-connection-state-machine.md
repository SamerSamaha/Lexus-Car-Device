# DN-007: Connection state machine

| | |
|---|---|
| Ticket | LHU-007 |
| Requirements | REQ-007 |
| Author | implementer (build-out form, D-049) |
| Status | Approved |
| Draft written | 2026-10-08, about 15 minutes |
| Design review | after merge, by the repository owner (D-049) |
| Approved | 2026-10-08 |

## 1. Problem

The service layer needs one place that knows whether the vehicle link is Disconnected, Connecting, Connected or in Error, moves between those states only along a written table, and tells a listener about every move with its cause and time. Sources (LHU-012, LHU-028) drive it; the HMI and the hub's status strip show it; the on-car logs of REQ-019 are its transition records.

## 2. Clarifying questions and assumptions

| # | Question | Assumption made |
|---|---|---|
| 1 | What moves the machine: method calls per transition, or one `handle(trigger)`? | One `handle(trigger)` with an enumeration of triggers. The table is then data (from, trigger, to), the illegal pairs are the pairs missing from it, and the REQ-007 test can enumerate every pair mechanically |
| 2 | Which triggers exist? | StartRequested, HandshakeSucceeded, HandshakeFailed, LinkLost, BackoffElapsed, StopRequested. A handshake timeout and a refused connection are both HandshakeFailed; the 2 s link-loss detection of REQ-008 happens in the source, which then raises LinkLost |
| 3 | Is Stop from Disconnected legal? | No. The architecture's "any state, stop, Disconnected" means every state the machine can be running in; a stop while already Disconnected is rejected and counted, so a double stop in a caller is visible rather than silently absorbed |
| 4 | Does the machine own the backoff timer? | No. It only knows the trigger BackoffElapsed. The schedule (1, 2, 4, 8, 10, 10 s) is REQ-008 and lives with the reconnect loop in LHU-012, where the scenario test proves it |
| 5 | What is recorded per transition? | From, trigger, to, and the time from the injected `Clock` (DN-006), as a `ConnectionTransition` value handed to one listener and kept as `lastTransition()`. REQ-019 asks for "every transition logged with its cause"; the trigger is the cause |
| 6 | Which thread? | The worker thread that owns the source; same rule as the store (DN-006) |
| 7 | Does an illegal trigger throw? | No. It returns Rejected, increments a counter, leaves the state unchanged and calls no listener. A source in a race (a late LinkLost after Stop) must not bring the thread down |

## 3. Nouns to classes

| Class | Responsibility |
|---|---|
| `ConnectionState` (enum class) | Disconnected, Connecting, Connected, Error |
| `ConnectionTrigger` (enum class) | The six causes of a transition |
| `ConnectionTransition` (struct) | One recorded move: from, trigger, to, timestamp ms |
| `TransitionResult` (enum class) | Accepted or Rejected |
| `ConnectionStateMachine` | Holds the current state, applies the table, counts, notifies |
| `connectionTransitionTable()` | The written table as data, exposed so tests and documentation read the same source |

## 4. What each class stores

- `ConnectionStateMachine`: constructor takes `const Clock&`; fields: `const Clock* m_clock`, `ConnectionState m_state` (starts Disconnected), `std::optional<ConnectionTransition> m_lastTransition`, `std::uint64_t m_acceptedCount`, `std::uint64_t m_rejectedCount`, `std::function<void(const ConnectionTransition&)> m_listener`. Owned by the source that drives it (LHU-012) or by the application wiring; outlives nothing it points at (the clock is a sibling).
- The table: a `constexpr std::array` of `{from, trigger, to}` rows, 8 rows.

## 5. Verbs to methods

| Class | Method | Inputs (with units) | Output (with units) | Notes |
|---|---|---|---|---|
| `ConnectionStateMachine` | `handle(trigger)` | `ConnectionTrigger` | `TransitionResult` | Looks the pair up in the table; Accepted: state changes, transition recorded with `clock.nowMilliseconds()`, listener called; Rejected: nothing changes, counter incremented |
| `ConnectionStateMachine` | `state()` | — | `ConnectionState` | |
| `ConnectionStateMachine` | `lastTransition()` | — | `std::optional<ConnectionTransition>` | Empty until the first accepted transition |
| `ConnectionStateMachine` | `acceptedTransitionCount()`, `rejectedTransitionCount()` | — | counts | |
| `ConnectionStateMachine` | `setTransitionListener(listener)` | `std::function` | — | One listener, replaceable, empty means none |
| free functions | `connectionTransitionTable()` | — | the 8 rows | |
| free functions | `toString(ConnectionState)`, `toString(ConnectionTrigger)`, `toString(TransitionResult)` | enum | `std::string_view` | |

The transition table:

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

Every other (state, trigger) pair, 16 of the 24, is rejected.

## 6. Interaction sequence, main scenario

1. The source is told to start. It calls `machine.handle(StartRequested)`: Disconnected to Connecting; the listener logs it and the HMI shows "Connecting".
2. The source opens the transport and runs the ELM327 setup commands. They succeed: `handle(HandshakeSucceeded)`, Connecting to Connected.
3. The source polls. A read times out twice within 2 s: `handle(LinkLost)`, Connected to Error. The source starts its backoff timer (LHU-012).
4. The timer fires: `handle(BackoffElapsed)`, Error to Connecting; the source reopens the transport and handshakes again.
5. The user quits: `handle(StopRequested)` from whatever running state, to Disconnected.

## 7. Failure cases

| # | Failure | How the design handles it |
|---|---|---|
| 1 | A trigger that is illegal in the current state (a late LinkLost after Stop; a second Start while Connecting) | Rejected, counted, state unchanged, no notification |
| 2 | Listener not set | Transition still recorded in `lastTransition()`; nothing else differs |
| 3 | Listener throws | Not caught; same rule as the store listener (DN-006) |
| 4 | Clock reads the same millisecond for two transitions | Both are recorded with equal timestamps; order is preserved by the listener call order, not by time |
| 5 | A new trigger or state added later without a table row | The REQ-007 test enumerates every pair against its own copy of the table and fails on the difference |

## 8. Test plan

| # | Test | Requirement |
|---|---|---|
| 1 | Starts Disconnected with no last transition and zero counts | REQ-007 |
| 2 | Every one of the 24 (state, trigger) pairs: the test holds its own copy of the 8 legal rows; for each pair the machine is driven into the state, the trigger is applied, and the result and resulting state are compared. 8 Accepted with the expected target, 16 Rejected with the state unchanged | REQ-007 |
| 3 | Rejected triggers increment the rejected count and do not call the listener | REQ-007 |
| 4 | Accepted triggers record from, trigger, to and the clock time in `lastTransition()` and in the listener, in order | REQ-007 |
| 5 | The main scenario of section 6 runs end to end: Disconnected, Connecting, Connected, Error, Connecting, Connected, Disconnected | REQ-007 |
| 6 | `connectionTransitionTable()` has exactly 8 rows and no duplicate (from, trigger) pair | REQ-007 |
| 7 | `toString` names every state and trigger | REQ-007 |

The HMI half of REQ-007 (new state shown within 500 ms) is LHU-013.

## 9. Alternatives considered

1. **One method per transition (`start()`, `connected()`, `fail()`, `stop()`).** Reads well at the call site. Rejected because the legality check would be spread over four methods and the "every pair not in the table is rejected" test would have to be written by hand four times; with a trigger enumeration the test is a loop over the product of two enumerations.
2. **Hierarchical or framework-based state machine (Boost.SML, a Qt state machine).** Rejected: four states and six triggers fit in an 8-row table; a dependency adds nothing and a Qt type would break the no-Qt rule of the service layer (D-008).
3. **Throwing on an illegal trigger.** Makes bugs loud in tests. Rejected because on the car a stray trigger from a racing timer would take the worker thread down; counting it keeps the fault visible in the counters and logs without a crash.
4. **Putting the backoff schedule in the machine.** Tempting because Error to Connecting is where it is used. Rejected so that the machine stays a pure table with no time arithmetic; the schedule belongs to the reconnect loop that owns the timer (LHU-012, REQ-008).

## Changes after the design review

| # | Change | Reason |
|---|---|---|
| 1 | None yet; the review happens after merge (D-049) | |

## Design vs. implementation

Added after the code pull request is merged.
