# Manual procedure in the car

Every on-car run follows the operating rules of `docs/safety/SAFETY_STATEMENT.md`: parked first, nothing touched on the screen while driving, the adapter unplugged when not testing. Each run is recorded in `docs/test/results/<date>_<section>.md`; a failed or partial run is recorded too. Recordings that may contain the vehicle identification number stay in `local_recordings/` until scrubbed (`tools/scrub_recording.py`). The thermal and power log runs in every session (`docs/release/PI_BRINGUP_CHECKLIST.md` step 3.1).

This file grows by section as the tickets that need the car land; the drive procedure itself is LHU-034.

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
