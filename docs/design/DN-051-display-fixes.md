# DN-051: Display fixes from the first car day (light note)

| | |
|---|---|
| Ticket | LHU-051 |
| Requirements | REQ-028 (new); touches REQ-004, REQ-007, REQ-012, REQ-016 |
| Author | implementer (build-out form, D-049) |
| Status | Implemented |
| Draft written | 2026-10-11, about 15 minutes |
| Design review | after merge, by the repository owner (D-049) |
| Approved | 2026-10-11 |

## Problem

On the touchscreen in the car (2026-10-11) the owner saw five things that make the screen look unfinished in a demo: the mouse pointer in the middle of the screen; the strip printing the trigger identifier "HandshakeSucceeded"; tile titles running past their tile ("Time 1000 to 2499 rp..."); the hub tile "Replay last dr..." cut; and a fuel level tile that always showed "--", because the 2013 GS350 does not support PID 0x2F. Every one is presentation; none changes a value.

## Public interface

| Piece | Change | Notes |
|---|---|---|
| `lexus-hub --hide-pointer`, `lexus-head-unit --hide-pointer` | `QGuiApplication::setOverrideCursor(Qt::BlankCursor)` | Passed by `lexus-hub-session` and by the car registry's vehicle-data command. Covers the two Qt windows; another program's window (a browser, the mirroring window) keeps its own pointer policy |
| `ConnectionStatusModel::causeText(trigger)` | "starting", "adapter answered", "adapter did not answer", "link lost", "trying again", "stopped" | `lastCauseText` uses it; the identifiers stay in the session log and on D-Bus |
| `SignalTileModel::tileTitle(signalId)` | 15 characters or fewer: "Speed", "Coolant", "Below 1000 rpm", "1000-2499 rpm", "4000 rpm and up", "Air flow", ... | The definition names stay as they are, because they are also configuration keys (`staleness.<name>_ms`) |
| `SignalTile.qml` title | bounded by the tile, `fontSizeMode: HorizontalFit` down to 2 mm | Shrinks instead of running under the next tile |
| `AppTile.qml` name | two lines with word wrap, then smaller down to the label size; the icon gives way down to 6 mm | `truncated` is false for every name of the car registry and for two longer ones |
| `gridSignalIds` | the eighth tile is mass air flow (PID 0x10, reported by the car) instead of fuel level | Fuel level stays a signal: polled when a vehicle supports it, still on D-Bus |

## Failure cases

| Case | Handling |
|---|---|
| A future app name longer than two lines at the label size | Qt shortens it with an ellipsis on the second line; the test lists the names it checks |
| A vehicle that supports fuel level | The value is still received and published; it is not on the grid. A configurable grid was rejected as scope without a user (see below) |
| A pointer device plugged in | Hidden over the hub and the app; it still works for taps |

## Test plan

| Requirement | Test |
|---|---|
| REQ-028 | `tests/unit/hmi/view_models_test.cpp` (every cause in words, lower case, never the identifier; every tile title 15 characters or fewer; the last grid tile is mass air flow) |
| REQ-028 | `tests/hmi/tst_vehicle_data_screen.qml` (eight titles, units with g/s last, each title on one line inside its tile; valid, stale and never-received on the mass air flow tile) and `tests/hmi/tst_trip_screen.qml` (the short titles) |
| REQ-028 | `tests/hmi/tst_home_screen.qml` (the cause in words on the strip) |
| REQ-028 | `tests/hmi_hub/tst_app_tile.qml` (six names on a tile of the car grid's size: never cut, two lines at most, the icon inside the tile) |
| REQ-028 | `tests/installed/command_line_options.sh` (`--hide-pointer` reaches the app, which reports it, and the hub accepts it and runs) |
| REQ-028 | On the Pi: checklist step 3.17 items 4 to 6 |

## Alternative considered

A grid chosen from the supported-PID bitmap at run time (show whatever the car supports) was considered for the fuel level tile. It was rejected for now: the tile order would change from car to car, the HMI tests could no longer name the tiles, and this project has one car, whose supported PIDs are known since the first car day.
