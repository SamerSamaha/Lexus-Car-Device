# Pi and car checklist

Every step that must be run on the Raspberry Pi or in the car, in one place. The software is built and tested at the desk against fakes and the ELM327 emulator; this file lists what the desk cannot prove. Each ticket that adds such a step adds it here, with the exact commands, the expected output and where the measured result is recorded. First-boot setup of a fresh card is in `deploy/PI_SETUP.md`; this file starts where that one ends.

Rules:

- A number in this file is either **measured** (copied from a run, with the date and the raw file named) or **not yet measured**. Nothing in between: no estimates, no placeholders.
- An assumption made at the desk in place of a hardware fact is listed in the table of section 2 with the step that verifies it. Until that step has run, the assumption stays an assumption, however plausible.
- Raw outputs go under `docs/measurements/` or `docs/test/results/`; recordings that may contain the vehicle identification number stay in the ignored `local_recordings/` folder until scrubbed (D-023).

## 1. Status

| Area | Ticket | State |
|---|---|---|
| Thermal and power log in every Pi session | LHU-015, LHU-016 | Script written and unit-tested (LHU-015); no session logged with it yet; two raw CSVs from 2026-10-05 exist on the Pi under `~/measurements/` |
| Adapter pairing and first parked session, adapter bound to `/dev/rfcomm0` and polled by the real source | LHU-017 | Not yet done |
| Clean-build measurement on the Pi | LHU-018 | Not yet done |
| Home screen on the panel: fills the rotated screen, 4 mm digits, touch | LHU-013 | Not yet done |
| Return to the hub from an app, hub visible after an app exits, URL app tracked | LHU-020, LHU-021 | Hub built and tested at the desk (LHU-021); step 3.5 not yet done |
| Vehicle-data service and hub as systemd user units, the app reading the service over D-Bus | LHU-022 | Built and tested at the desk on a private bus (LHU-022); step 3.6 not yet done |
| Web apps, protected audio, browser memory | LHU-023 | Not yet done |
| Bluetooth audio to the car stereo | LHU-024 | Not yet done |
| Power flags on screen, clean shutdown cycles | LHU-025 | Not yet done |
| vcan tests | LHU-028 | Built and skipped at the desk (no vcan module); step 3.7 not yet done |
| Boot time, latency, memory per process | LHU-032 | Not yet done |
| On-car procedure and drives | LHU-034 | Not yet done |

## 2. Assumptions to verify on hardware

Each row is an assumption made at the desk. The design note of the ticket names it too. The "Verified by" column is the step in section 3 that turns it into a fact; the "Result" column stays "not yet verified" until that step has run.

| # | Assumption | Made in | Verified by | Result |
|---|---|---|---|---|
| A1 | The 2013 GS350 answers the standard Mode 01 PIDs of REQ-004 (0x04, 0x05, 0x0C, 0x0D, 0x0F, 0x11, 0x2F, 0x42); which of them it supports is discovered at run time from the PID 0x00, 0x20 and 0x40 bitmaps | Plan, REQ-004 | Step 3.2 | Not yet verified |
| A2 | The vLinker MC+ exposes the Serial Port service on its classic-Bluetooth side and an RFCOMM socket on channel 1 carries the ELM327 text protocol | Plan, OQ-5 | Step 3.2 | Not yet verified |
| A3 | A clean build of the project on the Pi with 2 parallel jobs completes without being killed for memory and below 80 °C | D-022 | Step 3.3 | Not yet verified |
| A4 | One worker loop calling `runOnce()` on the source and `check()` on the staleness monitor keeps up with the adapter's reply rate on the Pi (no source owns a thread) | DN-008 | Step 3.2 (request rate) and the LHU-032 latency measurement | Not yet verified |
| A5 | `rfcomm bind` on the Pi gives a `/dev/rfcomm0` that the serial transport can open, write and `poll`-read like the emulator's pseudo-terminal, and that reads end-of-file when the adapter disappears | DN-012 | Step 3.2, items 6 and 7 | Not yet verified |
| A6 | The adapter answers fast enough that polling with `poll_interval_ms = 0` is acceptable to the car and the adapter; the actual request rate is unknown | DN-012 | Step 3.2, item 5; `poll_interval_ms` in `deploy/head_unit.conf` is the brake if not | Not yet verified |
| A7 | The adapter's reply to one request arrives within 1 s (`reply_timeout_ms`), so that link loss is declared within 2 s only when the link is really gone | DN-012 | Step 3.2, item 5 (worst reply time over 100 requests) | Not yet verified |
| A8 | A primary value drawn with a font pixel size of `mm(5.7)` has a cap height of about 4 mm on the panel (the font's cap height is assumed to be 0.7 of the em size) | DN-013 | Step 3.4 | Not yet verified |
| A9 | The 1280 x 720 window fills the rotated panel under labwc and touch lands where the button is drawn | D-045, DN-013 | Step 3.4 | Not yet verified |
| A10 | Chromium started with its own `--user-data-dir` stays as the process the hub launched, rather than handing the URL to a running instance and exiting, so the hub can track a URL app | DN-021 | Step 3.5, item 5 | Not yet verified |
| A11 | The Raspberry Pi OS panel stays visible above a maximised app window under labwc, and a launcher on it runs `lexus-hub --send return` from a tap | DN-021 | Step 3.5, items 3 and 4 | Not yet verified |
| A12 | When an app's process group ends, labwc shows the hub's window (it is the window underneath) within 1 s, without the hub raising itself | DN-021 | Step 3.5, item 4 | Not yet verified |
| A13 | Under the Raspberry Pi OS desktop, labwc activates `graphical-session.target` for the user and user units see `WAYLAND_DISPLAY`, so `lexus-hub.service` can show a window; the session bus at `/run/user/<uid>/bus` is the one the desktop apps use | DN-022 | Step 3.6 | Not yet verified |

## 3. Steps

### 3.1 Thermal and power log (every session)

From a fresh boot, before the workload, in a second terminal:

```sh
cd ~/Lexus-Car-Device
python3 tools/measure/log_thermal_power.py --label <session> --power-source "5V3A charger" \
    --ambient-celsius <room C> --duration-seconds 1800
# expected first line after the run: session <session>: 360 samples, 0 command errors
# then temperature min/median/max, flags seen: none, input volts min, verdict: pass
```

Labels: `desk_idle_30min`, `desk_clean_build` (with 3.3), `desk_app_30min` (with 3.4), `car_parked_30min` (with 3.2, power source `power bank A1383`), one per drive (LHU-034). Copy the CSV and its `.summary.txt` into `docs/measurements/thermal_power/` unchanged and add the row to that folder's README (LHU-016). A `warn` or `fail` verdict becomes a bug issue with the file attached (D-013).

The two raw CSVs of 2026-10-05 (earlier ad-hoc logger: idle 51 °C, 59.3 °C peak during the package upgrade, `get_throttled` 0x0, input 5.245 V; measured, on the Pi under `~/measurements/`) are copied into the folder with the first LHU-016 session.

### 3.2 Adapter pairing and first parked session (LHU-017)

Adapter in the port, engine off, ignition on, vehicle parked. Thermal log running (3.1).

1. Confirm the adapter answers a phone OBD app in the car. If it does not, the problem is the adapter or the car, not the Pi.
2. On the Pi desktop, pair and trust the device whose name ends in "-Android" through the Bluetooth menu (or `bluetoothctl`: `scan on`, `pair <address>`, `trust <address>`). The "-IOS" name is the BLE side and is not used. The address is never written into this repository.
3. `bluetoothctl info <address>` lists "Serial Port" (UUID 0x1101). Record: present or absent.
4. Send `ATZ`, `ATI`, `ATRV`, `ATDPN`, `0100`, `0120`, `0140` and record every reply in `docs/test/results/<date>_first_parked_session.md`. The reply to `ATDPN` is the protocol number; `0100`, `0120`, `0140` are the supported-PID bitmaps (assumption A1).
5. Record the request rate: 100 consecutive `010C` requests, wall-clock time for all 100 and the longest single reply time, in the same results file (assumptions A6, A7).
6. Bind the adapter to a device by name and open it with the real transport:

```sh
sudo deploy/bind_obd_adapter.sh vLinker        # prints: bound /dev/rfcomm0 to the device named like 'vLinker' on channel 1
ls -l /dev/rfcomm0
```

7. Run the head unit against it with `elm327.device = /dev/rfcomm0` in `deploy/head_unit.conf` (the application arrives with LHU-013) and record: Connected reached (yes or no), the number of PIDs in the poll list, samples per second over one minute, and whether unplugging the adapter gives Error within 2 s and replugging gives Connected again (assumption A5).

Results: not yet measured.

### 3.4 Home screen on the panel (LHU-013)

After a build on the Pi (3.3) and with the emulator or the adapter:

```sh
# Against the emulator, no car needed:
python3 tools/elm327_emulator/elm327_emulator.py --link /tmp/obd --control /tmp/obd.control &
~/build/lexus-car-device/release/src/app/lexus-head-unit --source elm327 --config deploy/head_unit.conf --fullscreen
# (edit deploy/head_unit.conf so elm327.device = /tmp/obd for this run)
```

Record in `docs/test/results/<date>_home_screen_on_pi.md`: whether the window fills the panel in landscape (A9); the measured height in millimetres of the digit "8" in the speed tile, with a ruler against the glass (A8; target at least 4 mm); whether a tap on "Vehicle data" opens the 4 x 2 grid and "Home" returns (A9); the status strip going Connected; a value greying out with the `STALE` badge after `printf 'stale 0D\n'` on the control socket; the measured width and height in millimetres of one grid tile (expected about 25 x 23 mm).

Results: not yet measured.

### 3.5 Hub and the way back (LHU-020, LHU-021)

After a build on the Pi (3.3), with the thermal log running:

1. Put both executables on the path, then start the hub:

```sh
sudo ln -sf ~/build/lexus-car-device/release/src/hub/app/lexus-hub /usr/local/bin/lexus-hub
sudo ln -sf ~/build/lexus-car-device/release/src/app/lexus-head-unit /usr/local/bin/lexus-head-unit
cd ~/Lexus-Car-Device && lexus-hub --registry deploy/hub.conf --fullscreen &
lexus-hub --send status    # expected: state idle app - hub_pid <n> app_pid - window visible ...
```

2. Tap "Vehicle data". Expected: the vehicle-data app covers the hub; `lexus-hub --send status` says `state running app vehicle_data`.
3. Copy `deploy/lexus-hub-return.desktop` to `~/.local/share/applications/` and add it to the panel as a launcher (right-click the panel, add or remove launchers). Record whether the panel stays visible over the app (A11). Fallback if it does not: a labwc key binding in `~/.config/labwc/rc.xml` running `lexus-hub --send return`, then `labwc --reconfigure`.
4. Tap the Hub launcher. Record whether the hub is in front within 1 s, timed by video or stopwatch, 10 times (A11, A12).
5. Add a URL app to `deploy/hub.conf` (any public page), restart the hub, launch it, and record whether `lexus-hub --send status` stays `running` while the browser is open (A10), and the browser's memory with `ps -o rss= -p <app_pid>` (the LHU-020 memory question).
6. Close the vehicle-data app from inside (if it has no close control, `kill <app_pid>`): the hub must be in front within 1 s.

Record everything in `docs/test/results/<date>_hub_on_pi.md`.

Results: not yet measured.

### 3.6 Service and hub as user units (LHU-022)

After 3.5, with the executables linked into `/usr/local/bin` (also `lexus-vehicle-data-service` from `src/service_dbus/`):

```sh
install -D -m 644 deploy/systemd/*.service ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable --now lexus-vehicle-data-service
busctl --user call io.github.samersamaha.LexusHeadUnit /io/github/samersamaha/LexusHeadUnit/VehicleData io.github.samersamaha.LexusHeadUnit.VehicleData1 GetConnection
systemctl --user enable lexus-hub && systemctl --user is-active graphical-session.target
```

Record: the `GetConnection` reply; whether `graphical-session.target` is active and `systemctl --user show-environment` lists `WAYLAND_DISPLAY` (A13); after a reboot, whether the hub appears by itself. Fallback if not: remove the hub unit and add `lexus-hub --registry /home/<user>/Lexus-Car-Device/deploy/hub.conf --fullscreen &` to `~/.config/labwc/autostart`. Then tap "Vehicle data" and confirm the values move (the app now reads the service), `systemctl --user restart lexus-vehicle-data-service` turns them Stale and back within a few seconds, and the D-Bus hop (part of LHU-032) is measured with the adapter connected.

Results: not yet measured.

### 3.7 CAN source on vcan0 (LHU-028)

After a build on the Pi (3.3):

```sh
sudo deploy/setup_vcan.sh                       # expected: vcan0 listed, state UNKNOWN or UP
ctest --preset release -L vcan --output-on-failure   # expected: 2 tests passed, none skipped
python3 tools/can_traffic_generator.py --interface vcan0 --rate-hz 50 --duration-seconds 120 &
~/build/lexus-car-device/release/src/app/lexus-head-unit --source can --config deploy/head_unit.conf --fullscreen
```

Record in `docs/test/results/<date>_vcan_on_pi.md`: the ctest summary; whether the vehicle-data screen shows the generator's moving values and Connected; whether stopping the generator turns the strip to Error within about 2 s and restarting it reconnects (backoff 1, 2, 4, 8, 10 s); `ip -s link show vcan0` packet counts before and after.

Results: not yet measured.

### 3.3 Clean-build measurement on the Pi (LHU-018)

With the thermal log running: clone, `cmake --preset release`, `cmake --build --preset release --parallel 2`, with `vmstat 5` in a second terminal. Record wall-clock time, peak memory, swap activity and maximum temperature in `docs/measurements/build/<date>_pi_clean_build.md`. Switch trigger (D-022): killed for memory, continuous swapping, or 80 °C.

Results: not yet measured.
