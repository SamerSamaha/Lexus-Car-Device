# DN-053: The phone's position over the phone link

| | |
|---|---|
| Ticket | LHU-053 |
| Requirements | REQ-029 (new); touches REQ-003, REQ-006, REQ-017 |
| Author | implementer (build-out form, D-049) |
| Status | Implemented |
| Draft written | 2026-10-11, about 40 minutes |
| Design review | after merge, by the repository owner (D-049) |
| Approved | 2026-10-11 |

## 1. Problem

The map (LHU-054) needs the car's position, and the unit has no GPS receiver; no purchase is allowed (D-044). The owner's iPhone is in the car as the hotspot and knows its position, so the position must come from the phone to the vehicle-data service and be published like the vehicle signals, without a paid app.

## 2. Clarifying questions, with the assumption made for each

| # | Question | Assumption |
|---|---|---|
| 1 | Does Safari on iOS give a page on the Pi its position? | Only on a secure page (Apple's release notes since iOS 10). A certificate that chains to a private authority the owner installed and fully trusted makes the Pi's page secure (assumption A27, checklist step 3.18 item 3). Evidence found: one developer reports self-signed certificates working for geolocation on iOS after clicking through the warning; the profile with full trust is Apple's documented way to trust a private authority. Not verified on this phone |
| 2 | What address does the phone use? | The iPhone hotspot gives addresses in 172.20.10.0/28 (assumption A28; the Pi got one on the first car day); the certificate names all 14 and `lexus-head-unit.local` |
| 3 | What does Apple require of the certificate? | Server certificate at most 398 days, `serverAuth` usage, names in subjectAltName, RSA 2048 and SHA-256 (Apple's published requirements); 397 days chosen; the authority 10 years |
| 4 | Does Safari keep reporting with the page in the background? | No: only while the page is in front and the phone unlocked (A29). The page asks for a screen wake lock where Safari offers it and says so on screen |
| 5 | How often does the phone report? | About once a second with high accuracy; the page sends at most one report per 900 ms |
| 6 | Which process receives the reports? | The vehicle-data service, which owns every signal on D-Bus (REQ-017) |
| 7 | Is the position private? | Yes: it reveals where the owner lives. It is held in memory only, never written to the session log, a recording or any file; it is not on the vehicle-data grid; the map masks it near home while recording (LHU-054) |

## 3. Nouns to classes

| Class | Responsibility |
|---|---|
| `PhonePosition` | One report: latitude and longitude in degrees, accuracy in metres |
| `parsePhonePosition` | Turns the JSON body into a `PhonePosition` or a reason for refusing it |
| `PhonePositionTracker` | Turns accepted reports into the three phone signals and makes them Stale after 5 s of silence |
| `HttpRequestParser` | Collects one HTTP request from a connection, with size limits |
| `PhoneLinkServer` | The TLS server: routes, one request per connection, timeouts, counters; emits each accepted position |
| `phone_link_certificates.sh` | Makes the private authority and the server certificate, keeps the authority, renews the server certificate |
| `phone_page.html` | The page Safari runs: watches the position and posts it |

## 4. What each class stores

| Class | Fields |
|---|---|
| `PhonePositionTracker` | the last position and its time; whether there has been a report; whether Stale was published; reports accepted |
| `HttpRequestParser` | the bytes so far, the request, the result, where the body starts, the content length |
| `PhoneLinkServer` | the `QSslServer`, the page, the authority's certificate, open connections, positions accepted, requests refused |

## 5. Verbs to methods

| Method | Inputs | Output |
|---|---|---|
| `parsePhonePosition(body, error)` | JSON bytes | `optional<PhonePosition>`; latitude -90 to 90, longitude -180 to 180, accuracy 0 to 100,000 m |
| `PhonePositionTracker::onPosition(position, nowMs)` | a position, monotonic ms | three Valid samples (Latitude deg, Longitude deg, PositionAccuracy m) at nowMs; none if nowMs is not later than the last report |
| `PhonePositionTracker::checkStaleness(nowMs)` | monotonic ms | the three samples with status Stale, once, when the last report is older than 5,000 ms |
| `HttpRequestParser::feed(bytes)` | bytes as they arrive | Incomplete, Complete, Invalid or TooLarge (head over 8 KiB, body over 4 KiB) |
| `PhoneLinkServer::start(settings, error)` | address, port, certificate, key, authority | listening, or false with the reason |
| `PhoneLinkServer::respond(request, page, authority)` | a request | status and bytes, and the position if one was accepted |
| signal `positionReceived(position)` | | one per accepted report |
| Service configuration | `phone_link.port` (0 = off), `phone_link.certificate`, `phone_link.key`, `phone_link.ca_certificate` | written into each session's `head_unit.conf` by `car_session.py` when the certificates exist and `phone_link.enabled` is not off |

## 6. Interaction sequence (main scenario)

1. The install script makes `~/.config/lexus-head-unit/phone-link/` (authority, server certificate, keys 600).
2. Once: the owner opens `https://<Pi address>:8443/` in Safari, accepts the warning, taps the certificate link, installs the profile, turns on full trust. From then on the page opens without a warning.
3. Every boot: `car_session.py` adds `[phone_link]` to the session configuration; the service starts the server on its main thread.
4. The owner taps Share position; Safari asks for location; `watchPosition` reports; the page posts `{"latitude", "longitude", "accuracy"}`.
5. The server parses the request, answers 204, emits `positionReceived`; the tracker makes three Valid samples at the service's monotonic time; `VehicleDataService::publishSample` updates the mirror and sends `SampleChanged`, exactly as for a vehicle signal.
6. A timer every 500 ms asks the tracker; 5 s after the last report the three signals go Stale on D-Bus.
7. The hub's address line shows `Phone https://<address>:8443` next to the SSH address.

## 7. Failure cases

| Case | Handling |
|---|---|
| Certificates missing or unreadable | The server does not start; the service runs on and says why on its error output; the self-check fails "phone link certificates" |
| The phone does not trust the authority yet | Safari shows its warning; the TLS handshake fails or is accepted by the owner; nothing reaches the service until it succeeds |
| Malformed or out-of-range report | 400 with the reason; counted; nothing published |
| A stray client: slow, huge or many connections | 10 s per connection, 8 KiB head, 4 KiB body, 8 connections at once (503 beyond) |
| Two reports in one millisecond | The second is dropped (the store's rule for one signal) |
| The phone locks or Safari goes to the background | Reports stop; the signals go Stale after 5 s; the map shows it |
| Port taken | Logged; the rest of the service runs |
| Certificate ends | The install script renews it 30 days before; the self-check fails within 30 days; the authority stays, so the phone needs nothing |
| Somebody else on the hotspot posts positions | Possible: only the owner's devices are on his hotspot, and the position is shown, never acted on. A shared secret in the page was rejected as theatre (anyone who can load the page has it) |

## 8. Test plan

| Requirement | Test |
|---|---|
| REQ-029 | `tests/unit/phone_link/phone_link_test.cpp`: valid report and boundaries; 10 malformed bodies refused with a reason; three Valid samples with units; same-millisecond drop; Stale after more than 5,000 ms, once, and Valid again; the request parser byte by byte, 7 malformed requests, the size limits; every route; the response headers; the page watches and posts and loads nothing over plain HTTP |
| REQ-029 | `tests/unit/phone_link/phone_link_tls_test.cpp`: the server with certificates made by `phone_link_certificates.sh`, a client that trusts only that authority and checks the name 127.0.0.1: the page, a posted position received, a bad one refused, the authority served; the script run twice keeps the authority; keys not readable by others |
| REQ-029, REQ-017 | `tests/integration/vehicle_data_dbus_test.cpp`: the service executable on a private bus with `[phone_link]` in its configuration: a position posted over TLS reaches a D-Bus client as the three Valid signals and goes Stale within 7 s |
| REQ-029 | `tools/test_phone_link.py`: the session configuration per mode, off, missing certificates, the self-check item, the scripts, and the certificate (serverAuth, the names, verified against the authority, at most 397 days, keys 600) |
| REQ-029 | On the Pi and the iPhone: checklist step 3.18 |

## 9. Alternatives considered

- **A Python HTTPS bridge that calls `busctl` for each report.** Less C++, but one process and one D-Bus call per second more, `busctl` parsing a negative longitude as an option, and the position would pass through a shell command line. Rejected for the server inside the service.
- **A position app on the phone (NMEA over TCP).** Needs an app; the free ones found could not be verified (OQ-31), and the owner asked for no paid app.
- **Plain HTTP.** Safari refuses the position to an insecure page.
- **Feeding the phone signals through the `SignalStore` on the worker thread.** The store and its staleness monitor belong to the worker thread and the vehicle source; a second producer would need a queue into that thread. The tracker publishes through the same `publishSample` path with its own 5 s staleness, so a client sees no difference (value, unit, timestamp, status).
