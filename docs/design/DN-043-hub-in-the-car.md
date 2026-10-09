# DN-043: Hub in the car: shutdown after the ignition goes off, address line, way back (light note)

| | |
|---|---|
| Ticket | LHU-043 |
| Requirements | REQ-024 (new); touches REQ-016, REQ-020 |
| Author | implementer (build-out form, D-049) |
| Status | Implemented |
| Draft written | 2026-10-09, about 25 minutes |
| Design review | after merge, by the repository owner (D-049) |
| Approved | 2026-10-09 |

## Problem

In the car the unit runs from a power bank with no laptop, keyboard or desktop panel. Three things are missing for that: the Pi must shut itself down cleanly once the car has been switched off, because the next thing that happens is someone pulling the power bank cable (the SD card risk of REQ-020); the owner needs the Pi's network address to reach it over SSH from a phone on the hotspot; and the vehicle-data app needs its own way back to the hub, because the panel launcher that DN-021 relies on is not used in car mode.

## Public interface

| Piece | Interface | Notes |
|---|---|---|
| `IgnitionOffShutdownPolicy` (hub core, plain C++) | `IgnitionOffShutdownSettings{quietMilliseconds = 300000, countdownMilliseconds = 60000}`; `onLinkDetail(detail, nowMs)`; `update(nowMs)` returns the phase; `cancel(nowMs)`; `phase()`; `millisecondsRemaining(nowMs)` | Phases: Disabled (quiet time 0), WaitingForLive, Live, Quiet, CountingDown, ShutdownDue (returned once), Cancelled. It arms only after the vehicle has been Live in this run, so a desk session, a demo or a car that was never switched on never shuts the unit down |
| `IgnitionOffShutdownModel` (hub view models, Qt) | properties `countdownActive`, `secondsRemaining`, `Q_INVOKABLE cancel()`; slot `applyLinkDetail(detail)`; `tick()` | A timer calls `tick()` every 250 ms; on ShutdownDue it calls `ShutdownController::shutdownNow()`. Takes a function returning monotonic milliseconds (the hub passes its steady clock, tests a manual clock), so the HMI layer includes no service header beyond the value types (REQ-011) |
| `ShutdownController::shutdownNow(reason)` | runs the configured command once, as the second tap does | The two-tap path now calls the same function |
| `NetworkAddressModel` (hub view models) | property `addressText` ("SSH lexus@172.20.10.2", or "No network"); `refresh()`; constructed with an address provider | The product provider reads `QNetworkInterface` (IPv4, up, not loopback); refreshed every 10 s |
| Hub screen | a countdown banner over the grid ("Vehicle off: shutting down in 42 s") with a Cancel button of at least 10 mm; the address line under the grid | |
| `lexus-head-unit --hub-button` | a "Hub" button on Home that quits the app with exit code 0 | The hub registry's restart policy is `on-failure`, so a clean exit is not restarted and the hub is in front again (REQ-016) |
| `deploy/hub.conf` | `hub.ignition_off_shutdown_ms` (300000; 0 disables), `hub.shutdown_countdown_ms` (60000) | |

Which details count as "not live": AdapterWithoutVehicle (the car is off), LinkLostRetrying (the adapter went to sleep or lost power after the car was switched off) and Idle (the service stopped). SearchingForAdapter never follows Live from the ELM327 source.

## Failure cases

| Failure | Handling |
|---|---|
| The engine is off only briefly (a fuel stop) | Five minutes of quiet before the countdown, then 60 s with Cancel. A cancelled countdown does not come back until the vehicle has been live again |
| The Bluetooth link drops while driving for more than 5 minutes | The countdown starts; nobody has to touch the screen, and the shutdown is clean, so the session data is kept. The driver never has to react while driving; the documented consequence is a lost session tail |
| The shutdown command fails to start | `lastResultText` says so, as for the button; the policy stays in ShutdownDue and does not retry in a loop |
| No network | "No network"; the address is re-read every 10 s, so joining the hotspot later shows up |
| The vehicle-data app crashes rather than quitting | The hub's existing `on-failure` restart applies; the Hub button exits with 0 and is never restarted |

## Test plan

| # | Test | Requirement |
|---|---|---|
| 1 | Policy with a manual clock: no countdown before the first Live; quiet for 299,999 ms gives no countdown, 300,000 ms starts it; Live during the quiet or the countdown resets; ShutdownDue exactly at the end of the countdown and only once; cancel holds until Live again; quiet time 0 disables; Idle and LinkLostRetrying count as quiet | REQ-024 |
| 2 | Model: the countdown text and seconds, cancel, one call of the executor at the end | REQ-024, REQ-020 |
| 3 | Address model with a fake provider: one address, several, none | REQ-024 |
| 4 | HMI: the banner appears with a Cancel target of at least 10 mm and hides on Cancel; the address line shows the model's text; the app's Hub button is at least 10 mm and requests the exit | REQ-024 |
| 5 | Pi: checklist step 3.14 (switch the ignition off with the unit running, watch the countdown, let it shut down, check the next boot's log) | REQ-024, REQ-020 |

## Changes after the design review

| # | Change | Reason |
|---|---|---|
| 1 | None yet; the review happens after merge (D-049) | |

## Design vs. implementation

To be written after merge.
