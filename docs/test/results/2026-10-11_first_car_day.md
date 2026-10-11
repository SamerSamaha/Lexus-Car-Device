# 2026-10-11 First car day

The first day with the unit in the owner's 2013 Lexus GS350, in the driveway and on a short drive. Car mode was installed on the Pi from the CI arm64 artifact of commit 0f8cc5e. What follows is what the owner observed and wrote down that day, from the touchscreen and a phone; no measurement script ran in the car, so every figure here is a reading from the screen, not a measured series.

| Item | Value |
|---|---|
| Date | 2026-10-11 |
| Software | `dev` 0f8cc5e, CI arm64 artifact, installed with `deploy/car/install_car_mode.sh --artifact` |
| Install self-check | 23 PASS, 0 FAIL, 2 SKIP (adapter address and `/dev/rfcomm0`, both before pairing, as expected) |
| Power source | power bank A1383 |
| How the unit was held | Not fixed; mounting is not a requirement (OQ-14 closed by the owner, 2026-10-11) |
| Thermal log | Ran in every session (car mode starts it); the files are on the Pi and not yet copied to the laptop |
| Session folders and recordings | On the Pi only; not yet exported (`lexus-export-sessions`, then `python tools/fetch_from_pi.py`) |

## Steps

| Step | Result |
|---|---|
| Pairing from the hub's Pair adapter tile, by touch | Paired with PIN 1234 (A20 verified) |
| Parked, engine off, ignition on | Live. Coolant 18 °C, intake air 21 °C, throttle 18 %, control module voltage 11.9 V |
| Parked, engine running (cold) | Live. 988 rpm at idle, coolant 52 °C, engine load 30 %, throttle 17 %, 13.8 V |
| Driving (a passenger watched; nobody touched the screen) | Values changed with the drive |
| Diagnostics (Mode 03, Mode 09) | One stored code read: P0420 (catalyst efficiency below threshold, bank 1). The vehicle identification was read in full and shown on screen unmasked; it is masked to its last 4 characters since LHU-049 |
| Supported PIDs | Mass air flow (PID 0x10) supported, so fuel economy works; fuel level (PID 0x2F) not supported, so its tile always showed "--" (replaced by mass air flow in LHU-051) |
| Trip screen after a short drive | 0.8 km, 28.5 L/100 km, 5.7 min below 1000 rpm, 2.5 min from 1000 to 2499 rpm |
| Adapter pulled from the port while Live | The strip read "Link lost, retrying" |
| Adapter plugged in again | Reconnected (the strip went back to Live) |
| Shutdown from the hub, then power on with the ignition on | The hub and Live with no input in 10 to 15 s (the owner's estimate, not timed with a stopwatch) |
| Ignition-off automatic shutdown | Not run in the car (the owner's choice) |
| Phone hotspot | The Pi joined the hotspot and got an address on it |

## Defects seen on the touchscreen

| Defect | Fixed in |
|---|---|
| The VIN shown in full on the Diagnostics screen | LHU-049 (last 4 characters only) |
| The mouse pointer in the middle of the screen | LHU-051 (`--hide-pointer`) |
| The strip printed "HandshakeSucceeded" | LHU-051 (causes in words) |
| Tile titles cut ("Time 1000 to 2499 rp...") | LHU-051 (short titles that fit) |
| Hub tile "Replay last dr..." cut | LHU-051 (two lines) |
| Fuel level tile always "--" | LHU-051 (mass air flow in its place) |

## Bugs found on the Pi the same day

LHU-046 (QML modules found only from the build folder; self-check crash on the root-only polkit folder), LHU-047 (the Hub button never shown), LHU-048 (a switch from demo to car mode shut the Pi down 360 s later). All three merged on 2026-10-11.

## Not done on this day

- The thermal and session files were not copied to the laptop, so no figure from them is quoted here.
- No timed boot-to-Live, no request rate, no latency or memory measurement (checklist steps 3.9, 3.13).
- The trip values were not compared with the car's trip meter (step 3.11); 28.5 L/100 km over 0.8 km of a cold start is not a figure to judge the economy constants (A16) by.
