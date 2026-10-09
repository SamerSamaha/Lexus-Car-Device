# DN-025: Power status and clean shutdown (light note)

| | |
|---|---|
| Ticket | LHU-025 |
| Requirements | REQ-020 |
| Author | implementer (build-out form, D-049) |
| Status | Implemented |
| Draft written | 2026-10-09, about 20 minutes |
| Design review | after merge, by the repository owner (D-049) |
| Approved | 2026-10-09 |

## Problem

The Pi runs from a power bank in the car with no cooling (D-011, D-013); the driver must see under-voltage and throttling as the firmware reports them, within 5 s, and must be able to shut the unit down cleanly instead of pulling power and risking the SD card (REQ-020).

## Public interface

| Piece | Interface | Notes |
|---|---|---|
| `PowerFlags`, `decodeGetThrottled(text)` (service library, plain C++) | `std::optional<PowerFlags>` from the output of `vcgencmd get_throttled` ("throttled=0x50005") | Bits 0 to 3 current (under-voltage, frequency capped, throttled, soft temperature limit), 16 to 19 the same since boot; the same bits as the thermal logger (LHU-015) |
| `currentFlagNames(flags)` | `std::vector<std::string>` | The short texts shown on the strip |
| `PowerStatusReader` (hub view-model library, Qt) | `requestReading()`; signal `readingReady(QString output, bool succeeded)` | `ProcessPowerStatusReader` runs the configured command with `QProcess`, asynchronously, so the UI thread never waits for `vcgencmd`; a fake for tests |
| `PowerStatusModel` | properties `available`, `underVoltage`, `frequencyCapped`, `throttled`, `softTemperatureLimit`, `occurredSinceBoot`, `flagsText`; `startPolling(ms)` | Polls every 2 s by default, so a flag is on screen at most about 2 s plus one command after the firmware sets it (REQ-020: 5 s) |
| `ShutdownController` | property `armed`; `Q_INVOKABLE press()`; constructed with the command and an executor | Two taps within 5 s: the first arms ("Tap again to shut down"), the second runs `systemctl poweroff` (configurable `hub.shutdown_command`); an unarmed press after the window re-arms. Desk and tests use a fake executor |
| Hub status strip | power chips (red for current flags, amber dot for "occurred since boot"), the connection state from `VehicleDataClient` (D-Bus), a 10 mm shutdown button | `hub.power_command` and `hub.shutdown_command` in `deploy/hub.conf` |

Placement: the reader lives in the hub, the process that shows the flags and owns shutdown, not in the vehicle-data service; the D-Bus interface stays about the vehicle. The diagnostics screen shows the same flags with LHU-030.

## Failure cases

| Failure | Handling |
|---|---|
| `vcgencmd` missing (the desk) or failing | `available` false, the strip shows "Power status unavailable"; no flag is invented |
| Output not in the expected form | Counted as unavailable for that reading |
| A reading still running when the next poll is due | The poll is skipped, never queued |
| Shutdown command fails to start | `lastResultText` says so; the controller disarms |
| Accidental tap | One tap only arms; it disarms by itself after 5 s |

## Test plan

| # | Test | Requirement |
|---|---|---|
| 1 | `decodeGetThrottled`: 0x0, each current bit, each since-boot bit, 0x50005, malformed outputs; flag names | REQ-020 |
| 2 | `PowerStatusModel` with a fake reader: each of the four flags set and cleared, unavailable on failure, a poll skipped while a reading runs | REQ-020 |
| 3 | `ShutdownController`: first press arms, second runs the command once, expiry disarms, failure reported | REQ-020 |
| 4 | HMI: each current flag shown within 5 s of being set and cleared when reset; the shutdown button at least 10 mm and two taps run the fake executor | REQ-020 |
| 5 | On the Pi (checklist step 3.8): 10 shutdown cycles through the button, 0 file-system errors in the next boot's log, every cycle recorded | REQ-020 |

## Changes after the design review

| # | Change | Reason |
|---|---|---|
| 1 | None yet; the review happens after merge (D-049) | |

## Design vs. implementation

Merged as PR #65. No deviation from the interface. Details:

- The strip gives the shutdown prompt ("Tap again to shut down") priority over the hub's own messages while the control is armed.
- `hub.power_command`, `hub.power_poll_ms` and `hub.shutdown_command` are read from the registry file and split without a shell, like app commands.
- The hub now also starts a `VehicleDataClient` and shows the connection state on the strip (the remaining item of DN-022).
- At the desk `vcgencmd` does not exist, so the hub shows "Power status unavailable"; the QML tests drive a fake reader polled every 100 ms. Status: Implemented.
