# DN-030: Diagnostics screen: trouble codes and vehicle information

| | |
|---|---|
| Ticket | LHU-030 |
| Requirements | REQ-021; REQ-001 unchanged |
| Author | implementer (build-out form, D-049) |
| Status | Implemented |
| Draft written | 2026-10-09, about 45 minutes |
| Design review | after merge, by the repository owner (D-049) |
| Approved | 2026-10-09 |

## 1. Problem

The head unit shows live values but not what a mechanic asks first: which trouble codes are stored, and which vehicle this is. This ticket reads the stored codes (Mode 03) and the vehicle identification number (Mode 09 PID 02) on request, decodes them, carries them from the source through the service and D-Bus to a scrolling diagnostics screen with the standard text of each code, beside the connection state and the power flags, and adds no way to clear codes (REQ-021, REQ-001).

## 2. Clarifying questions and assumptions

| # | Question | Assumption made |
|---|---|---|
| 1 | When are codes read? | On request only: the screen's "Read codes" button (and once when the screen is first shown). Codes change rarely; polling Mode 03 would slow the live values. The request travels as a flag the source checks on its next run, so it is safe from any thread |
| 2 | Which reply formats? | CAN (ISO 15765), as the GS350 uses (A1): `43 <count> <code bytes>...`, single frame or multi-frame (`<length>` line, then `0:`, `1:`... lines), with or without spaces; several ECUs answering give several messages, each decoded and the codes joined. The older format without a count byte (`43` then three code pairs padded with `00 00`) is also decoded, so a K-line car would not be misread |
| 3 | How is a code decoded? | Two bytes: the top two bits pick the letter (P, C, B, U), the next two the first digit (0 to 3), then three hexadecimal digits (SAE J2012) |
| 4 | Where does the text of a code come from? | A table in the OBD library of generic codes the project can explain (about 60 common powertrain and network codes), written in the project's own words; any other code gets its category ("generic powertrain code", "manufacturer-specific body code", ...) and says it has no text. The text is attached by the source, because the HMI may not include hardware headers (REQ-011) |
| 5 | How is the vehicle identification decoded? | Mode 09 PID 02: `49 02 01` then 17 ASCII characters, normally multi-frame; padding is dropped; anything that is not 17 characters is "not read" |
| 6 | The VIN is private (D-023). Where may it go? | On screen only. It crosses the session D-Bus of the user's own desktop to the app; it is never written to the session log, the latency log, or any file of this project. A recording made with `--record` while diagnostics are read does contain it in Mode 09 form, which is what `tools/scrub_recording.py` removes (DN-029) |
| 7 | How does it reach the app? | `VehicleDataSourceListener::onDiagnostics(report)` (a new method with an empty default, so other listeners are unchanged) and `VehicleDataSource::requestDiagnostics()` (empty default: the CAN and fake-free sources ignore it). The feeder forwards to a hook; the service publishes on D-Bus: method `RequestDiagnostics()`, method `GetDiagnostics()`, signal `DiagnosticsChanged()`. Adding members keeps `VehicleData1` compatible; the introspection test checks the file and the object still agree |
| 8 | What does the screen show? | A status strip (connection state; "Home" navigation); the code count and a scrolling list of code and text; "No stored trouble codes" or "Not read yet"; the vehicle identification or "Not read"; when the codes were read; the power flags (the hub's `PowerStatusModel`); a 10 mm "Read codes" button |
| 9 | Navigation | The one navigation button cycles Home, Vehicle data, Diagnostics, Home; its text names the next screen |
| 10 | Clearing codes? | Not possible: Mode 04 is not on the allowlist, no code path builds it, and the 256-mode test of REQ-001 stays the proof |

## 3. Nouns to classes

| Class | Library | Responsibility |
|---|---|---|
| `TroubleCode`, `DiagnosticsReport` | service (value types) | A code and its text; the codes, whether they were read, the identification, the time |
| `obdMessages`, `decodeTroubleCodes`, `decodeVehicleIdentification`, `troubleCodeDescription` | OBD | Reply framing and decoding; the code table |
| `Elm327ObdSource` (changed) | ELM327 | `requestDiagnostics()`; on the next run while connected, Mode 03 then 0902, decode, report |
| `FakeSource` (changed) | fake | Answers a request with a scripted or demo report |
| `SignalStoreFeeder` (changed) | service | Forwards a report to its diagnostics hook |
| `VehicleDataService`, `VehicleDataClient` (changed) | D-Bus | The three new members; the client fetches on change and on connection |
| `DiagnosticsViewModel` | HMI | What the screen binds to; a refresh that calls a requester |
| `DiagnosticsScreen.qml` | HMI | The screen |

## 4. What each class stores

- `DiagnosticsReport`: `bool codesRead`, `std::vector<TroubleCode> troubleCodes`, `bool identificationRead`, `std::string vehicleIdentification`, `std::int64_t timestampMilliseconds`.
- `Elm327ObdSource`: an `std::atomic<bool>` request flag.
- `VehicleDataService`: the last report and a requester function; `VehicleDataClient`: the last report.
- `DiagnosticsViewModel`: the code lines, the identification text, the read-time text, a requester function.

## 5. Verbs to methods

| Class | Method | Notes |
|---|---|---|
| free | `obdMessages(reply) -> std::vector<std::vector<std::uint8_t>>` | One message per ECU; multi-frame joined and cut to its length |
| free | `decodeTroubleCodes(reply) -> std::optional<std::vector<std::string>>` | Empty optional when no message starts with 0x43; `NO DATA` is zero codes |
| free | `decodeVehicleIdentification(reply) -> std::optional<std::string>` | 17 characters or nothing |
| free | `troubleCodeDescription(code) -> std::string` | Table text or the category |
| `VehicleDataSource` | `requestDiagnostics()` | Default does nothing |
| `VehicleDataSourceListener` | `onDiagnostics(report)` | Default does nothing |
| `DiagnosticsViewModel` | `refresh()` (`Q_INVOKABLE`), slot `onDiagnostics(report)` | |

## 6. Interaction sequence

1. The user opens Diagnostics; the screen calls `refresh()`; the view model's requester calls `VehicleDataClient::requestDiagnostics()`, an asynchronous D-Bus call.
2. The service's `RequestDiagnostics()` calls `Elm327ObdSource::requestDiagnostics()` (sets the flag).
3. On its next run while connected, the source sends `03` and `0902`, decodes both, attaches the texts, and calls `onDiagnostics`.
4. The feeder's hook hands the report to the service (queued to its thread), which keeps it and emits `DiagnosticsChanged`.
5. The client calls `GetDiagnostics()`, emits `diagnosticsArrived(report)`, the view model updates, the list scrolls.

In-process modes (`elm327`, `fake`, `replay`) take the same path through `WorkerBridge` instead of D-Bus.

## 7. Failure cases

| Failure | Handling |
|---|---|
| Request while not connected | The flag waits for the next connection |
| `NO DATA` to `03` | Zero codes, read |
| Timeout or garbage to `03` or `0902` | That part "not read"; a link error is link loss as during polling |
| A count byte that disagrees with the length | Decoded as the older format; anything still malformed is "not read" |
| Unknown code | Its category text |
| Service gone | The client keeps the last report and the connection shows Error |

## 8. Test plan

| # | Test | Requirement |
|---|---|---|
| 1 | Decoder: 0, 1, 2 and 6 codes, one of them multi-frame, across P, C, B and U, with and without spaces, two ECUs, the older format, `NO DATA`, malformed | REQ-021 |
| 2 | Vehicle identification from a spaced and an unspaced multi-frame reply; wrong length is nothing | REQ-021 |
| 3 | Code texts: table entries and every category fallback | REQ-021 |
| 4 | Source over the fake transport: a request sends exactly `03` and `0902` once, the report reaches the listener; no request, no diagnostics traffic | REQ-021, REQ-001 |
| 5 | Integration against the emulator with six stored codes (multi-frame) and its VIN | REQ-021 |
| 6 | D-Bus: a client's request reaches the source requester; a published report reaches two clients; the introspection still equals the file | REQ-021 |
| 7 | HMI: the screen shows "Not read yet", then each code with its text, the identification and the power text; "No stored trouble codes" for zero; the Read codes button calls refresh; the navigation cycle | REQ-021 |

## 9. Alternatives considered

| Alternative | Why rejected |
|---|---|
| Poll Mode 03 every few seconds | Costs live-value rate for data that changes a few times a year |
| A separate diagnostics D-Bus interface | More moving parts; three additive members on `VehicleData1` are compatible |
| Code texts in QML | The HMI may not know OBD; the source owns the meaning of its codes |
| A full SAE J2012 table | Thousands of entries, not ours to reproduce; the category fallback keeps every code meaningful |

## Changes after the design review

| # | Change | Reason |
|---|---|---|
| 1 | None yet; the review happens after merge (D-049) | |

## Design vs. implementation

| # | Design | Implementation | Reason |
|---|---|---|---|
| 1 | Diagnostics joins the screen cycle (test 7, "the navigation cycle") | A Diagnostics button on the right half of Home's lower area; Home again from the strip, as on the vehicle-data screen | One tap from Home and one back; a cycle would put a third tap between Home and the codes. The button exists only when a diagnostics view model is given |
| 2 | Not foreseen | `classifyReplyLines` in `Elm327Protocol` now classifies a multi-frame reply (a length line, then `0:`, `1:` frames) as data | Before, the length line made the reply malformed, so a 6-code reply or the identification would have counted as a protocol error |
| 3 | The client fetches the report "on change and on connection" | Both: `DiagnosticsChanged` triggers `GetDiagnostics`, and `start()` fetches once after subscribing | A hub app opened after a read shows the last report without asking the adapter again (tested with a late client) |
| 4 | The screen shows the power text | `DiagnosticsScreen` binds to the hub's `PowerStatusModel`; the vehicle-data app now creates one and polls `vcgencmd` every 2 s | The same model and reader as the hub strip; no second decoder |
| 5 | Test names in the traceability row (`dtc_decoder_test`, `vehicle_info_decoder_test`, `diagnostics_screen_test.qml`) | `obd_diagnostics_test.cpp`, `elm327_diagnostics_test.cpp`, `diagnostics_against_emulator_test.cpp`, `diagnostics_view_model_test.cpp`, `tst_diagnostics_screen.qml`, one test in `vehicle_data_dbus_test.cpp` | The decoders share one file; the QML runner only loads `tst_*.qml` |

Not verified at the desk: the reply of the real car to `03` and `0902` (format, number of ECUs, whether the identification is sent at all). That is checklist step 3.10 under assumption A15.
