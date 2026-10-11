# DN-054: The map app with the phone's position (light note)

| | |
|---|---|
| Ticket | LHU-054 |
| Requirements | REQ-030 (new); uses REQ-029; touches REQ-011, REQ-016, REQ-026 |
| Author | implementer (build-out form, D-049) |
| Status | Implemented |
| Draft written | 2026-10-11, about 20 minutes |
| Design review | after merge, by the repository owner (D-049) |
| Approved | 2026-10-11 |

## Problem

The demo video should show where the car is, on a map, from the hub, without a navigation app (D-042: no turn-by-turn) and without a purchase. The position comes from the phone link (DN-053). A recording made at home must never show where home is.

## Public interface

| Piece | Interface | Notes |
|---|---|---|
| `lexus-map` (`src/map_app/`) | `--fullscreen`, `--hide-pointer`, `--car-config` (default `~/.config/lexus-head-unit/car.conf`), `--recording-status` (default `$XDG_RUNTIME_DIR/lexus-screen-recording`), `--bus-address` | A client of the vehicle-data service (REQ-011: only the D-Bus client, never a source) |
| `MapViewModel` (`src/hmi/map/`) | properties `hasPosition`, `positionLive`, `latitude`, `longitude`, `accuracyMetres`, `masked`, `statusText`; slot `onSample`; `refresh()` | Takes a position over only when its accuracy arrives, the last of the three phone signals, so a new latitude never shows with the old longitude and the mask is decided on a whole position. While masked, `latitude` and `longitude` read 0 |
| `isPositionMasked(home, latitude, longitude, recording, maskAlways)` | true inside the home radius while recording (or always) | Haversine on a 6,371 km sphere |
| `[privacy]` in car.conf | `home_latitude`, `home_longitude`, `home_radius_m` (500), `mask_home_always` (off) | On the Pi only; the app prints only "home area set" or "not set" |
| `MapScreen.qml` | a strip with a live dot, the status and a Hub button (10 mm, quits: the hub restarts apps only on failure); the map area | The map is created with the first position and not drawn while masked: a cover takes the whole area, so no tile around home is ever on screen |
| `MapView.qml` | QtLocation `Map` with the osm plugin, tiles from `tile.openstreetmap.org` (custom host, provider repository off), a user agent naming the project, a tile cache, the attribution "© OpenStreetMap contributors"; a dot (grey when stale) and the accuracy circle; zoom 16, follows the dot | Loaded by a `Loader`, so the screen works and says so when the module is missing (the desk) |
| `deploy/car/hub_car.conf` | a "Map" tile, second in the grid | `restart = on-failure` |
| Installer | `qml6-module-qtlocation`, `qml6-module-qtpositioning`, `qt6-location-plugins` (Debian 13 packages, 6.8.2) | Not on the desk: the map view is not tested there (see the test plan) |

## Failure cases

| Case | Handling |
|---|---|
| No position yet | The cover asks for the phone link; no map is created, so no tile is fetched |
| The phone stops reporting | After 5 s the service marks the position Stale: the dot turns grey, the strip says so |
| Near home while recording | Cover with "Position hidden near home"; the coordinates read 0 |
| The recording starts while the map shows home | The recorder's status file is read every second; the model refreshes on each change, so at most about one second of the first file can show the map before the cover (A30); recording is normally switched on from the hub, before the map is opened |
| QtLocation module or plugin missing | The cover says which package is missing |
| No network for tiles | The map shows the cached tiles and blank squares; the dot still moves |
| No home area in car.conf | Nothing is hidden; the self-check could warn (not done: the owner may choose not to set it) |

## Test plan

| Requirement | Test |
|---|---|
| REQ-030 | `tests/unit/hmi/map_view_model_test.cpp`: haversine against known distances; masked at 445 m, not at 556 m with a 500 m radius, only while recording unless always, nothing without a home; a position taken over only with its accuracy, never half; stale; the coordinates read 0 while masked; the refresh follows the recorder |
| REQ-030 | `tests/hmi_map/tst_map_screen.qml`: shown away from home; covered near home while recording with the reason; amber when lost; the Hub button 10 mm; the loader ready or the missing-module message. `tests/hmi_map/tst_map_main_window.qml`: the model reaches the screen through `Main.qml` and the mask works in the window |
| REQ-030 | `tests/installed/run_outside_build_tree.sh` (lexus-map from an empty folder) and `tests/installed/command_line_options.sh` (`--hide-pointer`; the home area read and its coordinates never printed) |
| REQ-030 | On the Pi: checklist step 3.19 (the map with tiles, the dot moving while driving, the mask at home while recording) |

The QtLocation view itself is tested only on the Pi: the desk does not have the module, and installing packages on the desk needs the owner's approval. The test home is an invented point in Lake Michigan.

## Alternative considered

A static image of the map or a web map in the browser was rejected: the browser would need the position handed to it, a page per app, and the return-to-hub path of the browser (OQ-29); a native Qt map is one more QML client of the same D-Bus service, which is the platform story of D-042.
