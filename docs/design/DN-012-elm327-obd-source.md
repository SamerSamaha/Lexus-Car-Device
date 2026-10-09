# DN-012: Elm327ObdSource

| | |
|---|---|
| Ticket | LHU-012 |
| Requirements | REQ-002, REQ-004, REQ-008 |
| Author | implementer (build-out form, D-049) |
| Status | Implemented |
| Draft written | 2026-10-08, about 30 minutes |
| Design review | after merge, by the repository owner (D-049) |
| Approved | 2026-10-08 |

## 1. Problem

The pieces exist (store, state machine, interface, decoder, protocol, allowlist, emulator) but nothing yet talks to an adapter: `Elm327ObdSource` opens the serial device, configures the adapter, discovers the supported PIDs, polls them in turn, turns replies into samples, detects a lost link within 2 s and reconnects with exponential backoff, all from a worker loop that calls `runOnce()`. With it, the integration suite of DN-008 runs against the emulator over a pseudo-terminal and the source selection by configuration (REQ-002) has its format.

## 2. Clarifying questions and assumptions

| # | Question | Assumption made |
|---|---|---|
| 1 | What is the real transport? | `FileDescriptorByteTransport`: a POSIX file descriptor opened from a path, put into raw mode when it is a terminal, read through `poll()` with a timeout. The same class opens the emulator's pseudo-terminal at the desk and `/dev/rfcomm0` on the Pi, which `deploy/bind_obd_adapter.sh` creates by binding the paired adapter by name (checklist step; assumption A2 and new A5) |
| 2 | How is the adapter configured? | The setup sequence of DN-010 in order: `ATZ` (any reply but a timeout or link error is accepted; the adapter prints its version), `ATE0`, `ATL0`, `ATS0`, `ATH0`, `ATSP0` (each must answer `OK`), then `ATI`, `ATRV`, `ATDPN` (any text or data reply, kept for logs). A timeout or link error anywhere is a handshake failure |
| 3 | How are PIDs discovered? | `0100`, then `0120` and `0140` while the chain bit says more exist; the bitmaps go into a `SupportedPidSet`; `pollablePids` gives the poll list of REQ-004. No bitmap at all (`NO DATA`, `UNABLE TO CONNECT`, timeout) is a handshake failure: the vehicle is not answering (ignition off at start), so the source retries with backoff rather than polling nothing |
| 4 | How is the link judged lost (REQ-008, 2 s)? | A `LinkError` from the protocol (end-of-file, write failure) is immediate loss. Otherwise loss is declared when a reply timeout occurs and more than `linkLossTimeoutMilliseconds` (default 2000) have passed since the last reply of any kind from the adapter. With the default reply timeout of 1000 ms, two consecutive silent requests declare loss at about 2 s |
| 5 | What about `NO DATA`, `UNABLE TO CONNECT`, `CAN ERROR`, `BUS INIT: ...ERROR`, `?`, negative responses and malformed replies? | The adapter answered, so the link is up: they count as malformed inputs (REQ-010 counter), produce no sample, and the signals go Stale by themselves (REQ-006). Ignition off therefore shows as Stale values with the connection still Connected, which is what the driver should see: the adapter is fine, the car is off |
| 6 | What is the backoff? | `ReconnectBackoff` with the schedule 1, 2, 4, 8 s then 10 s for every further attempt (REQ-008); reset on a successful handshake. The source records `nextAttemptAtMilliseconds`; `runOnce()` in Error raises BackoffElapsed when the clock reaches it and attempts a connection in the same call |
| 7 | What does the worker loop sleep on? | `idleHintMilliseconds()`, added to the interface with a default of 0: the time until the next useful `runOnce()` (until the retry time in Error; the poll interval between requests; 1 s when nothing is pollable). The loop (LHU-013) sleeps at most that long |
| 8 | What is the configuration file? | A plain `key = value` text file with `[section]` headers and `#` comments, parsed by `KeyValueConfiguration` in the service library (no third-party parser). Keys: `source.kind` (`elm327`, `fake`; later `can`, `replay`), `elm327.device`, `elm327.reply_timeout_ms`, `elm327.link_loss_timeout_ms`, `elm327.poll_interval_ms`, `staleness.default_ms`, `staleness.<signal>_ms`. The application (LHU-013) reads it and builds the source; `Elm327SourceConfiguration::fromConfiguration` turns the map into typed values with defaults |
| 9 | Request rate? | Unknown until LHU-017 measures it (assumption A6): the source polls as fast as the adapter answers, with `poll_interval_ms` as the brake if the car or the adapter needs one |
| 10 | Which thread? | The worker thread, through `runOnce()`. The source owns the transport, the protocol, the state machine, the backoff and the PID set |

## 3. Nouns to classes

| Class | Responsibility |
|---|---|
| `FileDescriptorByteTransport` | `ByteTransport` over a path: open (raw terminal mode), close, write, `poll`-timed read honouring a 0 ms timeout |
| `ReconnectBackoff` | The 1, 2, 4, 8, 10, 10 s schedule: `nextDelayMilliseconds()`, `reset()`, `attemptCount()` |
| `Elm327SourceConfiguration` | Typed settings with defaults; `fromConfiguration(KeyValueConfiguration)` |
| `Elm327ObdSource` | The `VehicleDataSource` for the adapter: handshake, discovery, poll loop, loss detection, backoff |
| `KeyValueConfiguration` | Reads the configuration file into `section.key` strings; typed getters with defaults |
| `EmulatorProcess` (tests) | Spawns the emulator, reads its device path, sends control commands, kills and restarts it |
| `Elm327SourceHarness` (tests) | The DN-008 harness for this source over the emulator |

## 4. What each class stores

- `FileDescriptorByteTransport`: `std::string m_path`, `int m_fileDescriptor` (-1 when closed), `bool m_isTerminal`. Owned by the wiring, passed by reference to the source.
- `ReconnectBackoff`: `std::vector<std::int64_t> m_scheduleMilliseconds` (default 1000, 2000, 4000, 8000), `std::int64_t m_capMilliseconds` (10000), `std::size_t m_attemptCount`.
- `Elm327SourceConfiguration`: `devicePath`, `replyTimeoutMilliseconds` 1000, `linkLossTimeoutMilliseconds` 2000, `pollIntervalMilliseconds` 0, `backoffScheduleMilliseconds`, `backoffCapMilliseconds`.
- `Elm327ObdSource`: constructor takes `ByteTransport&`, `const Clock&`, `Elm327SourceConfiguration`; fields: `Elm327Protocol m_protocol`, `ConnectionStateMachine m_machine`, `ReconnectBackoff m_backoff`, `SupportedPidSet m_supportedPids`, `std::vector<ObdPid> m_pollList`, `std::size_t m_pollIndex`, `std::int64_t m_lastAdapterReplyAtMilliseconds`, `m_nextAttemptAtMilliseconds`, `std::string m_adapterIdentity`, `m_adapterVoltageText`, `m_protocolNumberText`, `VehicleDataSourceListener* m_listener`, `SourceCounters m_counters`, `std::uint64_t m_connectionAttempts`.

## 5. Verbs to methods

| Class | Method | Inputs (with units) | Output (with units) | Notes |
|---|---|---|---|---|
| `Elm327ObdSource` | `start(listener)` | listener | — | StartRequested, then one connection attempt at once (handshake and discovery); Connected or Error |
| `Elm327ObdSource` | `runOnce()` | — | — | Connected: one request for the next PID in the poll list, decode, emit. Error: if `now >= nextAttemptAt`, BackoffElapsed and a connection attempt. Disconnected: nothing |
| `Elm327ObdSource` | `stop()` | — | — | StopRequested if not already Disconnected; transport closed; listener detached |
| `Elm327ObdSource` | `idleHintMilliseconds()` | — | ms | See question 7 |
| `Elm327ObdSource` | `supportedPids()`, `pollList()`, `adapterIdentity()`, `adapterVoltageText()`, `protocolNumberText()`, `connectionAttempts()` | — | — | For tests, logs and the diagnostics screen |
| `ReconnectBackoff` | `nextDelayMilliseconds()` | — | ms | Attempt n (from 0) gives schedule[n], or the cap beyond the schedule; increments the attempt count |
| `KeyValueConfiguration` | `loadFromFile(path)`, `loadFromText(text)` | path or text | `bool` | Lines `key = value`, `[section]`, `#` comments; later keys override earlier ones |
| `KeyValueConfiguration` | `stringValue(key, default)`, `integerValue(key, default)` | `section.key` | value | A non-numeric value for an integer key gives the default |

## 6. Interaction sequence, main scenario

1. The application reads the configuration, finds `source.kind = elm327`, builds `FileDescriptorByteTransport(elm327.device)` and `Elm327ObdSource(transport, clock, configuration)`.
2. `start(feeder)`: StartRequested; the transport opens the device; `ATZ` and the setup commands succeed; `0100`, `0120`, `0140` fill the supported set; the poll list has, say, 8 PIDs; HandshakeSucceeded; the backoff resets.
3. The worker loop calls `runOnce()`: `010D` is sent; `410D3C` comes back; `decodePid` gives 60 km/h; the sample is stamped with the clock and handed to the feeder; the poll index moves to `010C`.
4. The adapter goes silent. The next request times out after 1 s; the one after it times out at 2 s since the last reply: LinkLost, the transport is closed, the next attempt is scheduled 1 s ahead; `idleHintMilliseconds()` tells the loop to sleep.
5. The clock reaches the attempt time: BackoffElapsed, the device is reopened, the handshake runs; if it fails, HandshakeFailed and the next delay is 2 s, then 4, 8, 10, 10 ... s; if it succeeds, Connected and polling resumes without the application restarting (REQ-008).

## 7. Failure cases

| # | Failure | How the design handles it |
|---|---|---|
| 1 | Device path does not exist (adapter not bound, emulator not started) | `open()` fails: HandshakeFailed, Error, backoff; attempts continue forever at 10 s |
| 2 | Adapter answers but the vehicle does not (ignition off at start) | No bitmap: HandshakeFailed, backoff; once the car is on, the next attempt succeeds |
| 3 | Vehicle stops answering while polling (ignition off while connected) | Named errors are counted, no samples; signals go Stale; the connection stays Connected; polling continues and recovers by itself when the car is back on |
| 4 | Adapter dies (end-of-file, write error) | LinkError: immediate LinkLost, backoff |
| 5 | Adapter silent but the device stays open (Bluetooth drop) | Timeouts: LinkLost when the last reply is older than the link-loss timeout, about 2 s with the defaults |
| 6 | Corrupt reply | Malformed, counted, no sample; the next request proceeds |
| 7 | A PID answers `NO DATA` while the others work (stale data scenario) | Counted; only that signal goes Stale |
| 8 | `stop()` during Error or Connecting | StopRequested from either state; transport closed; nothing scheduled |
| 9 | Poll list empty (vehicle supports none of the 8) | Connected, no requests, idle hint 1 s; the screens show NeverReceived |
| 10 | A request outside the allowlist | Impossible by construction: requests are built with `CommandAllowlist::obdRequest(0x01, pid)`; the emulator's forbidden counter is asserted 0 in the integration suite |
| 11 | Zero-millisecond read on a real device | `poll(fd, 0)` returns at once; the stale-byte drain of DN-010 cannot block (review finding) |

## 8. Test plan

| # | Test | Requirement |
|---|---|---|
| 1 | `ReconnectBackoffTest`: delays 1000, 2000, 4000, 8000, 10000, 10000, 10000; reset starts over; custom schedule | REQ-008 |
| 2 | `Elm327ObdSourceTest` (fake transport, manual clock): handshake sequence as sent; setup failure modes; discovery over 1, 2 and 3 bitmaps; poll order; a sample per reply with the clock stamp; named errors counted without samples; link lost after two silent requests at 2 s; link lost at once on end-of-file; attempts at exactly 1, 2, 4, 8, 10, 10 s on the manual clock; backoff reset after success; stop from each state; idle hints; counters | REQ-004, REQ-008, REQ-010 |
| 3 | `FileDescriptorByteTransportTest`: a pseudo-terminal created in the test: write and read, 0 ms read returns within 50 ms, end-of-file when the far end closes, nonexistent path fails to open | fixture |
| 4 | `KeyValueConfigurationTest` and `Elm327SourceConfigurationTest`: parsing, sections, comments, defaults, bad integers | REQ-002 |
| 5 | Integration: `Elm327SourceHarness` added to `service_against_each_source_test.cpp`: the same four tests now pass against the real source over the emulator; the emulator's forbidden-request count is 0 | REQ-002, REQ-001 |
| 6 | Integration `pid_discovery_test.cpp`: profile `no-fuel-level`; 100 polling cycles; the emulator's per-command statistics show 0 requests for `012F` and at least 100 for `010D` | REQ-004 |
| 7 | Scenario `reconnect_after_emulator_restart_test.cpp` (real clock, labelled `scenario`): the emulator is killed and restarted 20 times; each time Error within 2 s of the kill and Connected within 15 s of the restart; then the emulator stays dead for 36 s and the attempt times are 1, 2, 4, 8, 10, 10 s apart within 10% | REQ-008 |
| 8 | Scenario `fault_injection_scenarios_test.cpp`: stale PID (only that signal Stale), corrupt replies (counted, no Valid sample from them), ignition off and on (Stale then Valid, still Connected), silence then exit (Error, then recovery after restart) | REQ-006, REQ-007, REQ-010 |

## 9. Alternatives considered

1. **An RFCOMM socket transport in C++ (`AF_BLUETOOTH`).** Rejected for v0.1.0: it needs the BlueZ headers or a hand-written `sockaddr_rc`, cannot be tested at the desk, and `rfcomm bind` gives a device path that the file-descriptor transport handles like the emulator's pseudo-terminal. It stays an option if `rfcomm bind` misbehaves on the Pi (checklist A5).
2. **Declaring link loss on the first timeout.** Simpler, but one slow reply (the adapter searching for the protocol) would drop the link needlessly; the time-since-last-reply rule keeps the 2 s bound and tolerates one hiccup.
3. **Treating `UNABLE TO CONNECT` as link loss.** Rejected: the adapter is reachable; reconnecting would reset it for nothing and hide the real state (car off) behind "Error". Staleness already tells the user.
4. **A JSON or TOML configuration.** Rejected: no parser in the standard library; `key = value` with sections is enough and is read by eye on the Pi.
5. **Measuring REQ-008 with the manual clock only.** Rejected for the 20-trial scenario because the requirement is about real detection and recovery time against a real process; the manual clock proves the schedule exactly, the scenario proves it in real time.

## Changes after the design review

| # | Change | Reason |
|---|---|---|
| 1 | None yet; the review happens after merge (D-049) | |

## Design vs. implementation

Merged as PR #54 (250d38f). No deviations. Noted: two samples of the same signal in one millisecond are rejected by the store (DN-006), which the emulator can produce and the adapter cannot; the discovery test counts them. Review finding carried: reply timeouts are not yet visible in `SourceCounters` (add for the diagnostics screen, LHU-030). Status: Implemented.
