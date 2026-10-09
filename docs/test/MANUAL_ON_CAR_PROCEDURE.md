# Manual procedure in the car

Every on-car run follows the operating rules of `docs/safety/SAFETY_STATEMENT.md`: parked first, nothing touched on the screen while driving, the adapter unplugged when not testing. Each run is recorded in `docs/test/results/<date>_<section>.md`; a failed or partial run is recorded too. Recordings that may contain the vehicle identification number stay in `local_recordings/` until scrubbed (`tools/scrub_recording.py`). The thermal and power log runs in every session (`docs/release/PI_BRINGUP_CHECKLIST.md` step 3.1).

The parked run and the drive run below are LHU-034. The audio session (LHU-024) and the checklist steps for diagnostics (3.10) and trip values (3.11) use the same preparation.

## Before every run

| # | Check | Done |
|---|---|---|
| 1 | The unit is held as decided under OQ-14. Until OQ-14 is decided, only the parked run is allowed, with the unit resting on the passenger seat; no drive run takes place with a loose board in the cabin | |
| 2 | The power bank (A1383) is charged; the Pi is powered from it; the thermal log is started (checklist step 3.1, label `car_parked_<n>` or `car_drive_<n>`) | |
| 3 | The adapter is plugged into the diagnostic port only for the run and unplugged afterwards | |
| 4 | The session log is on: the service runs with `--session-log %h/local_recordings/<date>_<run>.csv` | |
| 5 | For a drive run: the driver does not look at or touch the screen; a passenger may watch it but does not touch it while the car moves. Anything that needs a tap is done parked | |

## Parked run (LHU-034, REQ-004, REQ-006, REQ-007, REQ-008, REQ-020)

Parked, engine running, 15 minutes. Compare the screen with the car's own instruments; the car is the reference.

| # | Do | Expect | Requirement |
|---|---|---|---|
| P1 | Start the hub and open the vehicle-data app; wait for Connected | Connected within 15 s; the poll list of the session log names only PIDs in the `0100`, `0120`, `0140` bitmaps | REQ-004, REQ-007 |
| P2 | Read engine speed against the tachometer at idle, then hold about 2,000 rpm for 10 s | Within 100 rpm of the tachometer at both points (the needle is read by eye, so this is a sanity check, not a calibration) | REQ-004 |
| P3 | Read coolant temperature from cold until the gauge settles | Rises and settles; the warm-up time appears on the Trip screen once 80 °C is reached | REQ-004, REQ-022 |
| P4 | Read vehicle speed | 0 km/h | REQ-004 |
| P5 | Unplug the adapter from the port for 20 s, then plug it in | Error within 2 s of unplugging, values greyed with `STALE` after their timeout, Connected again within 15 s of plugging in, without restarting anything | REQ-006, REQ-007, REQ-008 |
| P6 | Switch the engine off, wait 10 s, start it again | The same as P5: Error, Stale, then Connected | REQ-008 |
| P7 | Read the power line on the hub strip and the Diagnostics screen | "Power OK", or the flag shown is the one `vcgencmd get_throttled` reports | REQ-020 |
| P8 | Open Diagnostics and read codes (checklist step 3.10) | The codes match a phone OBD app; the identification line matches (do not write it down) | REQ-021 |

## Drive run (LHU-034)

Only after OQ-14 is decided and the parked run has passed. 20 to 40 minutes, mixed town and main road, driven normally. Nobody touches the screen while the car moves.

| # | Do | Expect |
|---|---|---|
| D1 | Parked: reset trip meter B, start the session log and the thermal log, open the vehicle-data app | Connected |
| D2 | Drive | Nothing to do on the screen |
| D3 | Parked again: stop the session log; read the Trip screen (checklist step 3.11) | Trip distance within 2 % of trip meter B |
| D4 | Summarise the session log | `python3 tools/measure/summarise_session_log.py ~/local_recordings/<date>_<run>.csv`: transitions out of Connected with their causes, request rate |

Pass for the drive run: no transition out of Connected that the session log attributes to anything other than the engine being switched off; trip distance within 2 % of the trip meter; thermal verdict `pass` or `warn` (a `fail` becomes a bug issue, D-013).

## Results template

Copy into `docs/test/results/<date>_<run>.md` for each run, filled in; a failed or partial run is recorded too.

```markdown
# <date> <parked | drive> run <n>

| Item | Value |
|---|---|
| Date, start and end time | |
| Software commit (`git rev-parse --short HEAD` on the Pi) | |
| How the unit was held (OQ-14) | |
| Power source | power bank A1383 |
| Thermal verdict and file (step 3.1) | |
| Poll list (from the session log) | |
| Steps passed, failed, not run (P1 to P8 or D1 to D4), with the reading for each | |
| Transitions out of Connected: time and cause | |
| Request rate (per second) | |
| Anything unexpected | |
```

## Audio to the car stereo and the OBD link on one radio (LHU-024, REQ-019)

Parked, engine running or ignition on (the stereo must be on), the adapter in the port and paired (step 3.2 of the checklist), the vehicle-data service running as a user unit.

### Preparation

1. Find out whether the stereo accepts Bluetooth audio from a phone-like device (OQ-30): in the car's own menus, look for Bluetooth audio or "add device". Record the menu path. If it does not, record that and stop: REQ-019 cannot be met without a purchase, and D-044 rules purchases out.
2. Pair the Pi with the stereo from the Pi's desktop Bluetooth menu (the stereo in pairing mode). No address is written down anywhere in this repository.
3. Connect, then run `deploy/set_audio_output_to_car.sh`. Expected: "default output is now the Bluetooth device".
4. Restart the service with a session log: in `~/.config/systemd/user/lexus-vehicle-data-service.service` add `--session-log %h/local_recordings/<date>_audio_session.csv` to `ExecStart`, then `systemctl --user daemon-reload && systemctl --user restart lexus-vehicle-data-service`.

### The 30-minute session

| Minutes | Do |
|---|---|
| 0 to 10 | No audio. The vehicle-data app open, values moving |
| 10 to 30 | Play audio (a long track or a stream) through the car stereo; listen, and count audible dropouts with the time of each |
| 30 | Stop audio; stop the session; copy the session log |

Then:

```sh
python3 tools/measure/summarise_session_log.py ~/local_recordings/<date>_audio_session.csv \
    --audio-start-seconds 600 --audio-end-seconds 1800
```

### Record

| Item | Value |
|---|---|
| Stereo accepts Bluetooth audio (OQ-30), menu path | |
| Request rate without audio (per second) | |
| Request rate with audio (per second) and the change (REQ-019: within 20 %) | |
| Transitions out of Connected: time, cause, during audio or not | |
| For each one during audio: caused by the audio link? (the tester's judgement, with the reason) | |
| Audible dropouts: count and times | |
| Thermal and power verdict (step 3.1) | |

Pass (REQ-019): 0 transitions out of Connected caused by the audio link, the rate with audio within 20 % of the rate without, dropouts counted and recorded. If it fails, audio is dropped, not the vehicle path (OQ-28).

Results: not yet measured.
