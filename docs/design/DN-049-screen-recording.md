# DN-049: Screen recording with sound, and masking of private data on screen (light note)

| | |
|---|---|
| Ticket | LHU-049 |
| Requirements | REQ-026 (new); touches REQ-021, REQ-024 |
| Author | implementer (build-out form, D-049) |
| Status | Implemented |
| Draft written | 2026-10-11, about 20 minutes |
| Design review | after merge, by the repository owner (D-049) |
| Approved | 2026-10-11 |

## Problem

The demo video of the unit in the car needs a recording of what the touchscreen shows and of the sound the Pi plays, started from the hub without a laptop, so the owner can drive and collect the files later. A recording, like a photo of the screen, must never carry the vehicle identification number or the Pi's network address.

## Public interface

| Piece | Interface | Notes |
|---|---|---|
| `lexus-screen-record` (`tools/car/screen_record.py`) | `status`, `on`, `off`, `toggle`, `run`; `--config` (car.conf), `--status-file` | `on`/`off`/`toggle` write `[recording] enabled` into car.conf and start or stop the user unit; `run` records only while the setting is on, so the setting survives a reboot |
| `lexus-screen-recorder.service` (user unit) | `ExecStart=lexus-screen-record run`, `Restart=on-failure` | Started by `lexus-hub-session` at every login after `systemctl --user import-environment WAYLAND_DISPLAY XDG_RUNTIME_DIR`; SIGTERM ends the file cleanly |
| The recorder | `wf-recorder --file <file> --codec libx264 --codec-param preset=ultrafast --codec-param crf=26 --framerate 30 --overwrite --output DSI-2 --audio=@DEFAULT_MONITOR@` | Screen of the touchscreen output and the monitor of the default sound output, so the file has the sound the Pi plays whatever the car radio does (A24, A25). `recording.command` in car.conf replaces the whole line if the Pi needs other options |
| Files | `~/lexus-data/screen/screen-NNNN-<date>.mkv`, one per session (each boot, each switch on) | NNNN orders them even when the clock was wrong at boot. Matroska stays playable after a power cut |
| Status file | `$XDG_RUNTIME_DIR/lexus-screen-recording`: the path being written, present only while recording | Written by `run`, removed when it ends, also after a crash of the recorder |
| `RecordingStatusModel` (hub view models) | properties `recording`, `fileName`; `refresh()`; `startPolling(ms)` | `lexus-hub --recording-status <path>`; read every 1 s. An empty path never reports recording (the desk) |
| Hub screen | a red REC indicator on the strip; the address line hidden while recording | Context property `recordingContext`, passed through `Main.qml` (tested there, lesson of LHU-047) |
| `DiagnosticsViewModel::identificationText` | every character but the last four replaced by a bullet | Always, not only while recording: the VIN reaches only the screen, and the screen shows 4 characters |
| "Screen recording" hub tile | `lexus-screen-record toggle` | In `deploy/car/hub_car.conf` |
| `tools/fetch_from_pi.py` (laptop) | `--host` (default `lexus-pi`), `--destination` (default `local_recordings`), `--dry-run` | One SSH call runs `lexus-export-sessions` and lists the files, then scp copies what is missing or has another size: export to `car-sessions/`, screen to `screen/`; the file still being written is skipped |

## Failure cases

| Case | Handling |
|---|---|
| Less than `min_free_mb` (2048) free at start | No file is started; logged |
| Free space falls below it while recording | The recorder is stopped with SIGINT (file finished), the loop ends cleanly, the unit does not restart |
| wf-recorder exits by itself (no Wayland display, wrong output name, audio device missing) | `run` exits non-zero, the status file is removed, systemd restarts it after 5 s with a new file; the journal says why |
| Power cut while recording | The .mkv lacks its index; players read it, `ffmpeg -i in.mkv -c copy out.mkv` rewrites it |
| Recording on, hub restarted | The hub reads the status file again within 1 s |
| A VIN shorter than 4 characters (a decode error) | Masked completely |
| The address line during the recording | Hidden; after the recording stops it comes back within 1 s |

## Not done here, and why

- A position near home on the map is masked when the map exists (LHU-054).
- Notifications while the phone is mirrored are an iPhone setting (LHU-057).
- A software H.264 encode costs CPU on a Pi 5, which has no hardware encoder. Whether 30 frames per second keep up beside the hub, the app and the OBD polling is assumption A26, measured on the Pi (checklist step 3.16).

## Test plan

| Requirement | Test |
|---|---|
| REQ-026 | `tools/test_screen_recording.py` (toggle remembered in car.conf and the unit started or stopped; off records nothing; numbering; the command with and without audio and the override; status file while recording, SIGINT at stop; no start and a clean stop on low space; a dead recorder exits 1; status text; the unit, the session script, the registry tile and the example config; the script runs as a program outside the repository; the laptop copy: one SSH call, only missing files, never the raw recording or the file being written, unsafe paths dropped, dry run, unreachable Pi) |
| REQ-026 | `tests/unit/hmi/car_status_models_test.cpp` (the status file read, the file name, two changes; an empty path) |
| REQ-026 | `tests/hmi_hub/tst_hub_car.qml` (REC shown and the address hidden, then the reverse); `tests/hmi_hub/tst_hub_main_window.qml` (the same through `Main.qml` with the executable's context properties) |
| REQ-026, REQ-021 | `tests/unit/hmi/diagnostics_view_model_test.cpp` (17, 5, 4, 2 and 0 characters); `tests/hmi/tst_diagnostics_screen.qml` (the screen shows the masked text) |
| REQ-026 | On the Pi: checklist step 3.16 |

## Alternative considered

Recording from inside the hub with Qt (grabbing the window into frames and encoding them) was rejected: it would record only the hub's own window, not the vehicle-data app, the browser or the mirrored phone, and it would put an encoder into the process that must stay responsive. wf-recorder records the whole output from the compositor and is a Debian package.
