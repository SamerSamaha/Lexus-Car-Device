# DN-042: Link detail for the car: searching, no vehicle, live, link lost

| | |
|---|---|
| Ticket | LHU-042 |
| Requirements | REQ-023 (new); touches REQ-007, REQ-008, REQ-017 |
| Author | implementer (build-out form, D-049) |
| Status | Implemented |
| Draft written | 2026-10-09, about 35 minutes |
| Design review | after merge, by the repository owner (D-049) |
| Approved | 2026-10-09 |

## 1. Problem

In the car, with no laptop, the status strip is the only way to tell why there is no data: the adapter is not found, the adapter is found but the car is switched off, the data is live, or a live link was lost and is being retried. Today all of these except "live" show the same connection state, Error, and one situation is not detected at all: when the ignition goes off after data was live, the adapter keeps answering `UNABLE TO CONNECT` or `NO DATA`, so the source stays Connected for as long as the car is off, with every value Stale.

## 2. Clarifying questions and assumptions

| # | Question | Assumption made |
|---|---|---|
| 1 | Can the four situations be four new connection states? | No. REQ-007 requires exactly four states and a written transition table, and its tests enumerate every pair. The situation is a separate value, the **link detail**, reported beside the state |
| 2 | How is "adapter but no vehicle" told apart from "no adapter"? | The handshake has two halves. The adapter half (open the device, `ATZ` and the setup commands) fails when the adapter is missing. The vehicle half (the supported-PID bitmaps `0100`, `0120`, `0140`) fails when the adapter answers but no control unit does. With the ignition off an ELM327 answers `UNABLE TO CONNECT` or `NO DATA` after its protocol search (assumption A19) |
| 3 | How is "ignition off after live" detected? | While connected, the adapter answers every request, but no request has produced a data reply for `elm327.vehicle_silence_timeout_ms` (default 5,000 ms). The link to the adapter is then dropped with the trigger LinkLost and the detail AdapterWithoutVehicle, and the normal retry finds the vehicle again when the ignition comes back. 5 s is more than two passes over the nine-PID poll list at any plausible rate, so one PID that answers `NO DATA` never fires it |
| 4 | Why not a new trigger for the vehicle going quiet? | A new trigger adds rows to the REQ-007 table. LinkLost is already the transition from Connected to Error; the detail carries the reason |
| 5 | Searching or retrying after the first live data? | "Searching for adapter" until the first live data of this run; afterwards a missing adapter reads "Link lost, retrying", because that is what the driver needs to know: it worked and stopped |
| 6 | The first `0100` after `ATSP0` | The ELM327 searches the protocols on the first request; the data sheet allows several seconds. With the 1 s reply timeout that request can time out, and every retry starts again with `ATZ`, which restarts the search: a possible endless loop on the car. The supported-PID requests get their own timeout, `elm327.discovery_timeout_ms`, default 10,000 ms (assumption A18) |
| 7 | Sources other than the ELM327 | `VehicleDataSource::linkDetail()` has a default that maps the state (Disconnected to Idle, Connecting to Searching, Connected to Live, Error to Link lost). The replay source forwards the ELM327 source it drives |
| 8 | How does the detail reach the screens? | The service reads `linkDetail()` after every worker cycle and publishes a change over D-Bus (`LinkDetailChanged`, `GetLinkDetail`), added to `VehicleData1` without changing any earlier member. A client that joins late fetches it. The session log gets one row per change |
| 9 | Size on screen | The detail is the largest text on both strips: `mm(4.5)` em size, against `mm(2.5)` for the state text it sits beside. The longest text, "Adapter found, no vehicle", fits on the 1280-pixel strip with the state, the cause and the navigation button (checked by the HMI test) |

## 3. Nouns to classes

| Class or type | Responsibility |
|---|---|
| `LinkDetail` (service library) | The five values: Idle, SearchingForAdapter, AdapterWithoutVehicle, Live, LinkLostRetrying; their names, strip texts and wire numbers |
| `Elm327ObdSource` | Sets the detail at each handshake result, lost link and quiet vehicle |
| `VehicleDataService`, `VehicleDataClient` | Carry the detail over D-Bus |
| `ConnectionStatusModel` | Holds the detail for QML: from each transition (`linkDetailAfter`) until the source reports its own |
| `StatusStrip.qml`, `HubStatusStrip.qml` | Show it as the headline of the strip |

## 4. What each class stores

| Class | Fields |
|---|---|
| `Elm327SourceConfiguration` | adds `discoveryTimeoutMilliseconds` (10,000) and `vehicleSilenceTimeoutMilliseconds` (5,000) |
| `Elm327ObdSource` | adds `m_linkDetail`, `m_hasBeenLive`, `m_lastDataReplyAtMilliseconds` |
| `VehicleDataService` | adds `m_linkDetail` (the mirror a late client reads) |
| `VehicleDataClient` | adds `m_linkDetail` |
| `ConnectionStatusModel` | adds `m_linkDetail` |

## 5. Verbs to methods

| Method | Input | Output |
|---|---|---|
| `linkDetailForState(state)` | a connection state | the default detail |
| `linkDetailAfter(transition, previous)` | a transition and the detail before it | the detail to show until the source reports one: HandshakeFailed gives Searching, LinkLost gives Link lost, Connecting keeps the previous one |
| `linkDetailFromNumber(n)` | a wire number | the detail, or empty when out of range |
| `displayText(detail)`, `toString(detail)` | a detail | the strip text, the name |
| `VehicleDataSource::linkDetail()` | — | the source's current detail |
| `Elm327Protocol::execute(command, timeoutMs)` | a command and its reply timeout | the reply, as `execute(command)` with the configured timeout |
| `VehicleDataService::publishLinkDetail(detail)` | any thread | queued to the service thread; emits `LinkDetailChanged(u)` when it changed |
| `VehicleDataService::GetLinkDetail()` | — | the number of the current detail |
| `VehicleDataClient::linkDetailChanged(detail)` | signal | each change, and the fetched value on start |
| `ConnectionStatusModel::applyLinkDetail(detail)` | a detail | properties `detailText` and `detailName` |
| `SessionLog::recordLinkDetail(ms, detail)` | time and detail | one `detail` row |

## 6. Interaction sequence (main scenario: car switched off, then on)

1. The service starts with the adapter in the port and the ignition off. `attemptConnection()` opens `/dev/rfcomm0`, the setup commands answer, `0100` answers `UNABLE TO CONNECT`: HandshakeFailed, detail AdapterWithoutVehicle, retry in 1 s, then 2, 4, 8, 10 s.
2. After the cycle, the service sees the detail changed and publishes it; the hub and the app show "Adapter found, no vehicle" in amber.
3. The ignition goes on. The next retry finds the bitmaps: HandshakeSucceeded, detail Live, green.
4. The driver switches off. Every request now gets `UNABLE TO CONNECT`; 5 s after the last data reply the source drops the link (LinkLost, detail AdapterWithoutVehicle) and the retries begin again.

## 7. Failure cases

| Failure | Handling |
|---|---|
| A wire number out of range on the client | Counted as a malformed message, ignored |
| The service leaves the bus | The client sets LinkLostRetrying with the synthetic LinkLost transition it already made |
| A source that never reports a detail | The model derives it from each transition |
| The protocol search takes longer than 10 s | The handshake fails as AdapterWithoutVehicle and is retried; `discovery_timeout_ms` is configurable; A18 is checked on the car |
| A car whose control unit answers `NO DATA` to one PID permanently | Other PIDs answer with data, so the 5 s timer never fires (unit test) |
| A car that answers nothing at all with the ignition off (silence, not an error text) | The existing 2 s link-loss rule fires with LinkLost; the retry then finds the adapter answering without bitmaps: AdapterWithoutVehicle |
| Retries every 10 s with the car off keep the adapter awake | Accepted; limited by the shutdown after the ignition goes off (LHU-043) and by unplugging the adapter after use |

## 8. Test plan

| # | Test | Requirement |
|---|---|---|
| 1 | `link_detail_test.cpp`: the state mapping, `linkDetailAfter` for each case, wire numbers, distinct names and texts of at most 26 characters | REQ-023, REQ-007 |
| 2 | `elm327_obd_source_test.cpp`: device not openable, setup not answered, ignition off, live then lost then still missing then back, ignition off while live at 4,999 and 5,000 ms then on again, one PID with `NO DATA` for 20 s stays live, the discovery timeout reaches the transport and the poll timeout stays 1 s, defaults | REQ-023, REQ-008 |
| 3 | `key_value_configuration_test.cpp`: the two keys read and defaulted | REQ-023 |
| 4 | `session_log_test.cpp`: detail rows | REQ-023 |
| 5 | `view_models_test.cpp`: the model follows transitions until the source reports, notifies once per change, Connecting keeps the detail | REQ-023 |
| 6 | `vehicle_data_dbus_test.cpp`: two changes arrive in order, a repeat is not published, a late client gets the current detail, the service leaving reads as LinkLostRetrying; the introspection test counts 10 members | REQ-023, REQ-017 |
| 7 | `link_detail_scenarios_test.cpp` against the emulator in real time: no adapter keeps searching over 3.5 s; ignition off at start then on reaches Live within 15 s; ignition off while live is reported between 4.5 and 7 s; emulator killed after live reads Link lost within 2 s and stays so while it is missing | REQ-023, REQ-008 |
| 8 | HMI: each of the four details on the app strip within 500 ms with its colour, at least `mm(4.5)`, larger than the state text, the longest text fits; the hub strip headline equals the model's text at the same size | REQ-023 |
| 9 | Car: checklist step 3.13 (each situation produced on purpose and read from the driver's seat) | REQ-023 |

## 9. Alternatives considered

| Alternative | Why rejected |
|---|---|
| Four new connection states | Breaks REQ-007 (exactly four states) and its exhaustive transition tests; the states describe the link to the adapter and stay correct |
| Put the detail inside `ConnectionTransition` and the `ConnectionChanged` signal | Changes an existing D-Bus signal (an incompatible change, `VehicleData2`) and the transition no longer describes only the state machine; the detail can also change without a transition (a retry that fails differently) |
| Keep Connected while the ignition is off and show "no vehicle" from stale values | The source would keep sending requests that cannot succeed at full rate and never re-run discovery, so a car that answers different PIDs after a restart would be polled with the old list |
| A listener callback for each detail change instead of reading after each cycle | One more virtual on every listener for a value that changes a few times per drive; reading after each cycle is one comparison and needs no thread hop beyond the existing one |

## Changes after the design review

| # | Change | Reason |
|---|---|---|
| 1 | None yet; the review happens after merge (D-049) | |

## Design vs. implementation

To be written after merge.
