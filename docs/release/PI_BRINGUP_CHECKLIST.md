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
| The CI arm64 build runs on the Pi | LHU-033 | Built and tested on `aarch64` in CI, artifact uploaded (LHU-033); step 3.12 not yet done |
| Clean-build measurement on the Pi | LHU-018 | Not yet done |
| Home screen on the panel: fills the rotated screen, 4 mm digits, touch | LHU-013 | Not yet done |
| Return to the hub from an app, hub visible after an app exits, URL app tracked | LHU-020, LHU-021 | Hub built and tested at the desk (LHU-021); step 3.5 not yet done |
| Vehicle-data service and hub as systemd user units, the app reading the service over D-Bus | LHU-022 | Built and tested at the desk on a private bus (LHU-022); step 3.6 not yet done |
| Web apps, protected audio, browser memory | LHU-023 | Procedure, browser flags and memory sampler ready (LHU-023); `docs/test/MANUAL_WEB_APPS_PROCEDURE.md` not yet run |
| Bluetooth audio to the car stereo | LHU-024 | Output script, session log and summariser ready (LHU-024); the audio section of `docs/test/MANUAL_ON_CAR_PROCEDURE.md` not yet run |
| Power flags on screen, clean shutdown cycles | LHU-025 | Flags and the shutdown control built and tested at the desk with a fake reader (LHU-025); step 3.8 not yet done |
| vcan tests | LHU-028 | Built and skipped at the desk (no vcan module); step 3.7 not yet done |
| Trip values compared with the car's own trip meter; PID 0x10 supported or not | LHU-031 | Engine, trip screen and replay test built at the desk (LHU-031); step 3.11 not yet done |
| Trouble codes and vehicle identification read from the car | LHU-030 | Decoders, source, D-Bus members and screen built and tested at the desk against the emulator (LHU-030); step 3.10 not yet done |
| Boot time, latency, memory per process | LHU-032 | Probe, marker and three measurement scripts ready (LHU-032); step 3.9 not yet done |
| The four link situations read from the driver's seat | LHU-042 | Detail, D-Bus members and strip headline built and tested at the desk against the emulator (LHU-042); step 3.13 not yet done |
| Shutdown after the ignition goes off, address line, Hub button | LHU-043 | Policy, countdown, address and button built and tested at the desk (LHU-043); step 3.14 not yet done |
| Car mode: install, boot to the hub without input, pairing, session folders, export, demo modes | LHU-044 | Install script, tools, units and self-check built and tested at the desk, the install as a dry run (LHU-044); step 3.15 not yet done |
| Car day guide for the owner, README demo section | LHU-045 | Written (`docs/release/CAR_DAY_GUIDE.md`); the README lists every demo clip as pending until real media exists |
| On-car procedure and drives | LHU-034 | Procedure written: parked run P1 to P8, drive run D1 to D4, a results template (`docs/test/MANUAL_ON_CAR_PROCEDURE.md`, LHU-034); no run done; the drive run waits for the mounting decision (OQ-14) |

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
| A14 | `/tmp` on the Raspberry Pi OS desktop image is a `tmpfs`, so the browser profile and cache in `/tmp` live in RAM and do not wear the SD card | LHU-023 | `docs/test/MANUAL_WEB_APPS_PROCEDURE.md`, preparation step 3 | Not yet verified |
| A17 | Executables built in the `debian:trixie` container run on Raspberry Pi OS (same Debian 13 base): every shared library resolves, and the Qt QML and Quick packages on the Pi are the versions the container used (6.8.2+dfsg-7) | LHU-033 | Step 3.12 | Not yet verified |
| A16 | The GS350 supports PID 0x10 (mass air flow), and the stated constants (air-fuel ratio 14.7, petrol at 745 g/L) put the average economy within about 10 % of the car's own figure | DN-031 | Step 3.11 | Not yet verified |
| A18 | The vLinker finishes its protocol search and answers the first `0100` after `ATSP0` within 10 s (`elm327.discovery_timeout_ms`) | DN-042 | Step 3.13, item 2 (time from adapter connected to Live) | Not yet verified |
| A19 | With the ignition off and the adapter powered, the adapter answers `0100` and the Mode 01 requests with an error text (`UNABLE TO CONNECT`, `NO DATA`, `CAN ERROR`) rather than not at all, so the strip reads "Adapter found, no vehicle"; if it stays silent the strip reads "Link lost, retrying" instead and the unit still recovers | DN-042 | Step 3.13, item 3 | Not yet verified |
| A20 | The vLinker MC+ pairs on its classic side with the PIN 1234, or without a PIN (secure simple pairing); `lexus-pair-adapter` answers either | LHU-044 | Step 3.15, item 4 | Not yet verified |
| A21 | Under the Raspberry Pi OS desktop, labwc runs `~/.config/labwc/autostart` at login, so `lexus-hub-session` starts the hub; whether the system autostart (panel, desktop) also runs does not matter, because the hub is full screen and kanshi is started either way | LHU-044 | Step 3.15, item 3 | Not yet verified |
| A22 | `raspi-config nonint do_boot_behaviour B4` sets automatic login to the desktop on this image (LightDM, `autologin-user` in `/etc/lightdm/lightdm.conf`) | LHU-044 | Step 3.15, items 2 and 3 | Not yet verified |
| A23 | The diagnostic port of the 2013 GS350 is under the dashboard on the driver's side, below and left of the steering column, as on other Lexus models | LHU-045 | `docs/release/CAR_DAY_GUIDE.md` B1, step 2 (look) | Not yet verified |
| A15 | The GS350 answers `03` in the CAN format (a count byte, multi-frame when there are more than two codes) and `0902` with a 17-character identification, as the emulator does; replies from more than one ECU are joined | DN-030 | Step 3.10 | Not yet verified |

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

Record the first parked session too (LHU-029): add `--record local_recordings/<date>_first_parked.rec` to the command of step 7; afterwards, on the Pi, `--source replay` with `replay.file` pointing at it must show the same values. The recording stays in `local_recordings/`; only a copy scrubbed with `python3 tools/scrub_recording.py <file> --output <copy>` may leave it.

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
5. Run `docs/test/MANUAL_WEB_APPS_PROCEDURE.md` (the three URL apps in `deploy/hub.conf`): it records whether `lexus-hub --send status` stays `running` while the browser is open (A10) and the browser's memory with `tools/measure/sample_process_memory.py` (the LHU-020 memory question).
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

### 3.8 Power flags and clean shutdown (LHU-025)

With the hub running (3.5 or 3.6):

1. Run `vcgencmd get_throttled` in a terminal and compare with the strip: "Power OK" for `throttled=0x0`; any other value names the current flags in red, and an amber dot means a flag was set since boot.
2. On the power bank under load (or during the clean build of 3.3), record any flag that appears and the time between `vcgencmd` showing it and the strip showing it (REQ-020: 5 s or less).
3. Ten shutdown cycles: tap the power button twice; wait for the Pi to power off; power on; after boot run `journalctl -b -1 -p err --no-pager | grep -iE "ext4|fsck|mmc"` and `dmesg | grep -iE "ext4-fs error|fsck"`. Record each cycle (date, time to power-off, the two outputs) in `docs/test/results/<date>_shutdown_cycles.md`. Pass: 0 file-system errors over the 10 cycles. `systemctl poweroff` from the desktop user needs no password on the desktop image (polkit allows the active session); if it asks, record that and set `hub.shutdown_command` accordingly.

Results: not yet measured.

### 3.9 Whole-system measurements (LHU-032)

With the service and the hub as user units (3.6) and the thermal log running (3.1):

1. **Boot (REQ-013).** Add `--first-frame-mark /run/user/1000/lexus-first-frame.txt` to the hub unit's `ExecStart`. Then 10 cold boots (power removed for 10 s each time); after each, `python3 tools/measure/measure_boot_time.py record --mark /run/user/1000/lexus-first-frame.txt`. Separately, time power-on to the first kernel message once with a video of the screen and the power switch: the mark starts at the kernel, so firmware and bootloader time is reported beside it. Finally `measure_boot_time.py summarise`.
2. **Latency (REQ-009).** Start the vehicle-data app from a terminal with `--source dbus --latency-log docs/measurements/latency/<date>_pi.csv` (the service on the adapter or the emulator, recorded which), run until 1,000 or more rows, then `python3 tools/measure/measure_latency.py <file>`. The adapter's round trip is reported apart from the step 3.2 request-rate measurement.
3. **Memory (REQ-014).** With the hub, the vehicle-data app and a browser app running: `python3 tools/measure/measure_memory.py --process service=lexus-vehicle-da --process hub=lexus-hub --process app=lexus-head-unit --process browser=chromium --label system_30min`. The kernel cuts command names to 15 characters, hence `lexus-vehicle-da`.
4. From the three baselines, re-set the provisional targets of REQ-013 and REQ-014 per process (OQ-8), then make one optimisation pass from a fixed list (for example: the QML cache compiler, fewer QML imports at start, the browser's `--renderer-process-limit`), and measure again; both runs stay in the data.

Results: not yet measured.

### 3.10 Trouble codes and vehicle identification (LHU-030)

Parked, ignition on, adapter bound as in 3.2, the vehicle-data app running on the real source:

1. Open Diagnostics from Home. Within a few seconds the summary shows a code count or "No stored trouble codes", and "Read at" shows the time. Compare the codes with a phone OBD app or the dashboard warning lights; record both lists.
2. Check that the identification line matches the VIN on the door pillar label. **Do not photograph the screen or copy the VIN into any file**; record only "matches" or "does not match".
3. Tap Read codes three times; the live tiles must keep updating between reads, and the session log must show no transition out of Connected.
4. If the car has no stored codes, step 1 shows the zero case only; that is a valid result. Codes are never cleared by this unit (REQ-001); do not clear them with another tool for this test.

Record in `docs/test/results/<date>_diagnostics.md`: the code lists, "matches" or "does not match", whether the reply was multi-frame (from the raw request log, scrubbed as in LHU-029), and A15 confirmed or not.

Results: not yet done.

### 3.11 Trip values against the car (LHU-031)

The screen is not touched while driving. Before setting off, parked:

1. In the session log or the raw request log of 3.2, check whether `0110` is answered. If it is `NO DATA` or missing from the `0100` bitmap, PID 0x10 is unsupported: record that under OQ-32; the two economy tiles stay empty and the rest of this step still applies.
2. Reset the car's trip meter B. Start the vehicle-data app on the real source; the trip starts with the first sample.

After a drive of at least 10 km by someone else, or with the screen ignored, parked again:

3. Open Trip from Home. Record the trip distance against trip meter B, the trip average economy against the car's own average if it shows one, the warm-up time, and the four band times.
4. Pass for distance: within 2 % of the trip meter. The economy figure is a comparison, not a pass or fail: record the difference, and if it is over 10 % note whether E10 fuel or the density constant explains it (A16).

Record in `docs/test/results/<date>_trip_values.md`.

Results: not yet done.

### 3.12 The CI arm64 build on the Pi (LHU-033)

On the laptop, from the repository folder (PowerShell), download the artifact of the latest green run on `dev` and copy it over:

```sh
gh run list --branch dev --workflow CI --limit 1
gh run download <run id> --name lexus-head-unit-arm64 --dir lexus-head-unit-arm64
scp -r lexus-head-unit-arm64 lexus-pi:~/
```

On the Pi:

1. `cat ~/lexus-head-unit-arm64/build_environment.txt` and compare each line with `dpkg-query --show libc6 libstdc++6 qt6-base-dev qt6-declarative-dev 'libqt6core6*' 'libqt6dbus6*' 'libqt6quick6*' 'libqt6qml6*'` on the Pi. Record every difference.
2. `chmod +x ~/lexus-head-unit-arm64/lexus-*` (an artifact does not keep the executable bit), then `ldd ~/lexus-head-unit-arm64/lexus-head-unit | grep "not found"` prints nothing; the same for the other two.
3. `~/lexus-head-unit-arm64/lexus-head-unit --source fake --fullscreen` shows the home screen with moving values; Trip and Diagnostics open.
4. Record in `docs/test/results/<date>_ci_arm64_build_on_pi.md`: the run ID, the differences of item 1, the result of items 2 and 3. A17 is verified when items 2 and 3 pass.

Results: not yet done.

### 3.13 The four link situations in the car (LHU-042)

Parked, the unit running in car mode (the vehicle-data service as a user unit, the hub on the screen). Read the large text at the left of the hub strip from the driver's seat each time.

1. Adapter not in the port, Pi powered: "Searching for adapter" in amber. Record whether it is readable from the driver's seat without leaning (yes or no).
2. Plug the adapter in, ignition to ON (engine off): the text changes to "Live" in green. Record the seconds from plugging in to "Live" with a stopwatch, 3 times (A18: under 10 s of protocol search, plus Bluetooth connection time, which is not yet measured).
3. Ignition off, adapter left in: within about 7 s, "Adapter found, no vehicle" in amber (A19). If it reads "Link lost, retrying" instead, record that: the adapter is silent rather than answering with an error.
4. Ignition back to ON: "Live" again. Record the seconds.
5. Unplug the adapter while "Live": "Link lost, retrying" in red within about 2 s; plug it back: "Live".
6. Copy the session log of the run (it has a `detail` row for every change) next to the results.

Record in `docs/test/results/<date>_link_detail_in_car.md`: each step's text, colour, time and readability.

Results: not yet done.

### 3.14 Shutdown after the ignition goes off (LHU-043)

In the car, parked, car mode installed (LHU-044), the hub on the screen and the strip saying "Live".

1. Switch the ignition off and leave the adapter in. Expect "Adapter found, no vehicle" (or "Link lost, retrying" if the adapter sleeps), then after 5 minutes a banner "Vehicle off: shutting down in 60 s" counting down, with Cancel.
2. Tap Cancel once: the banner goes away and stays away while the car is off. Switch the ignition on until "Live", then off again: the countdown returns after 5 minutes.
3. This time let it run out. Record the time from the banner reaching 0 to the screen going dark and the green LED of the Pi stopping.
4. Unplug the power bank, plug it in again (or press the Pi's power button), and after the boot run `journalctl -b -1 -p err --no-pager | grep -iE "ext4|fsck|mmc"` and `dmesg | grep -iE "ext4-fs error|fsck"`: both print nothing.
5. On the hub, read the address line under the grid with the phone on the hotspot: it shows `SSH lexus@<address>`; `ssh lexus@<address>` from the phone works.
6. Open Vehicle data, tap Hub on its Home screen: the hub is in front again within 1 s.

Record in `docs/test/results/<date>_ignition_off_shutdown.md`: each step, the times, the two log outputs.

Results: not yet done.

### 3.15 Car mode on the Pi (LHU-044)

At home first, then in the car; `docs/release/CAR_DAY_GUIDE.md` is the same in the order a car day needs it.

1. Install: `deploy/car/install_car_mode.sh --artifact ~/lexus-head-unit-arm64 --with-demo` (or `--build`). Record the whole output and the self-check counts. Run it again: the second run changes nothing (every step says unchanged or kept) and the self-check gives the same counts.
2. `sudo reboot` with no keyboard or mouse attached. Record: whether the hub appears full screen without input (A21, A22), and the seconds from power-on to the hub by stopwatch. Repeat for 5 cold boots (power removed for 10 s).
3. Check that no desktop, panel, terminal or dialog is visible at any point after the boot splash. Record anything that appears.
4. In the car: pair with the hub's Pair adapter tile (A20); record what the terminal printed (the address is shown masked).
5. Reboot in the car with the adapter in and the ignition on: record the seconds from power-on to "Live".
6. End one session by pulling the power bank cable while Live. After the next boot: `ls ~/lexus-data/sessions/` shows that session's folder with `session_log.csv`, `obd.rec` and the thermal CSV, each ending within a few seconds of the cut; `journalctl -b -1` and `dmesg` show no ext4 errors.
7. `lexus-export-sessions`: record the summary line; then `grep -c` for the car's VIN in the export folder gives 0 (type the VIN only on the Pi's command line, never into a file).
8. Demo modes: tap Demo mode, Replay last drive, Car mode on the hub; each gives a new session folder with the mode in its name.

Record in `docs/test/results/<date>_car_mode_install.md`.

Results: not yet done.

### 3.3 Clean-build measurement on the Pi (LHU-018)

With the thermal log running: clone, `cmake --preset release`, `cmake --build --preset release --parallel 2`, with `vmstat 5` in a second terminal. Record wall-clock time, peak memory, swap activity and maximum temperature in `docs/measurements/build/<date>_pi_clean_build.md`. Switch trigger (D-022): killed for memory, continuous swapping, or 80 °C.

Results: not yet measured.
