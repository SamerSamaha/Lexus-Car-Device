# Car day guide

For a day in the driveway with only the Raspberry Pi and its display, the power bank, the vLinker adapter and a phone. No laptop goes into the car.

Part A is done at home, the evening before, with the laptop. Part B is in the car. Part C is filming. Part D is back at home.

Rules that do not change in the car:

- The unit only reads from the car. The only requests are the allowlisted Mode 01, 03 and 09 reads; nothing can clear codes or write anything.
- Nobody looks at or touches the screen while the car moves. Everything that needs a tap is done parked.
- The adapter is unplugged from the port at the end of the day. It is powered by the car, and it pairs with a fixed PIN.
- Until the mounting is decided (OQ-14), the unit rests on the passenger seat and the car stays parked. A drive happens only with the unit held as decided and a passenger filming.

Numbers in this guide come from a measurement or say "not yet measured". Statements about the car and the adapter that have not been checked are assumptions; each one is listed in `docs/release/PI_BRINGUP_CHECKLIST.md`, section 2.

## Part A: at home, the evening before

### A1. Install car mode (about 10 minutes plus downloads; not yet measured)

1. The Pi is on the desk charger and on the home network, and the laptop reaches it with `ssh lexus-pi` (`deploy/PI_SETUP.md`).
2. Get the latest build. On the laptop, in PowerShell, in the repository folder:

   ```sh
   gh run list --branch dev --workflow CI --limit 1
   gh run download <run id> --name lexus-head-unit-arm64 --dir lexus-head-unit-arm64
   scp -r lexus-head-unit-arm64 lexus-pi:~/
   ```

   GitHub requires a login to download artifacts, even on a public repository, so this happens on the laptop. The alternative is to build on the Pi with `--build` instead of `--artifact` (several minutes with 2 jobs; not yet measured, LHU-018).
3. On the Pi, over SSH:

   ```sh
   cd ~/Lexus-Car-Device && git pull
   deploy/car/install_car_mode.sh --artifact ~/lexus-head-unit-arm64 --with-demo
   ```

4. Read the self-check at the end, one line per item. At home, before pairing, expect FAIL on nothing. Expect SKIP on:
   - "adapter address";
   - "/dev/rfcomm0 bound";
   - "phone hotspot saved", until A3 is done.

   Any FAIL is fixed before the car day. The line says what is wrong.
5. `sudo reboot`. With no keyboard input, the Pi should show the hub full screen. The large text at the left of the strip says "Searching for adapter" in amber.

The install is safe to run again after every `git pull` or new artifact.

### A2. Check demo mode on the Pi

1. On the hub, tap **Demo mode**. The strip turns green with "Live" within a few seconds.
2. Tap **Vehicle data**. Values move on Home, the 4 x 2 grid, Trip and Diagnostics (two demo trouble codes, an identification that is plainly not a VIN).
3. Tap **Hub** on the vehicle-data Home screen to get back.
4. Tap **Car mode** before putting the unit in the car. `lexus-mode show` over SSH confirms it.

Also from SSH:
- `lexus-mode emulator` runs the ELM327 emulator on the Pi.
- `lexus-mode replay last` plays back the newest car session, once one exists.

### A3. Make the Pi join the phone hotspot by itself

On the iPhone, open Settings and then Personal Hotspot:
- Turn on "Allow Others to Join".
- Turn on "Maximize Compatibility". This makes the hotspot use 2.4 GHz, which carries further. Whether the Pi needs it is not yet measured.

The network name is the phone's name, shown in Settings, General, About, Name.

On the Pi, over SSH from the laptop, enter the name and password exactly as the phone shows them:

```sh
sudo nmcli connection add type wifi con-name hotspot ifname wlan0 ssid "<phone name>" \
  wifi-sec.key-mgmt wpa-psk wifi-sec.psk "<hotspot password>" connection.autoconnect-priority 20
```

The priority 20 makes the Pi prefer the hotspot over the home network whenever both are in range. Test it at home: turn the hotspot on, then `sudo nmcli connection up hotspot`. The hub's address line under the grid changes to an address on the phone's network. iPhone hotspots usually hand out `172.20.10.x`; this has not been measured on this phone.

If the connection was already made on hardware day (`deploy/PI_SETUP.md` section 7), check it with `nmcli connection show hotspot`.

### A4. SSH from the phone

The Pi accepts only keys, no passwords (`deploy/PI_SETUP.md`). Do this at home:

1. Install an SSH app on the iPhone. Any app that can create an ed25519 key works. Which app does not matter, and none has been tried here.
2. In the app, create a key and copy its public line, which starts with `ssh-ed25519`.
3. On the Pi, over SSH from the laptop, add that line on a new line of `~/.ssh/authorized_keys`.
4. Phone on the hotspot, Pi on the hotspot: in the app, connect to `lexus@<the address on the hub's line>`. `lexus-mode show` answers.

The address of the Pi is shown at the bottom of the hub, as `SSH lexus@<address>`. The iPhone itself does not list the addresses of connected devices. Whether `lexus-head-unit.local` resolves from the phone is not yet measured. From Windows it did not (hardware day).

### A5. Pack

- The Pi with the display.
- The charged power bank, with its USB-C cable.
- The vLinker MC+.
- The phone, charged.
- A clip or holder for the phone if a passenger films a drive.

## Part B: in the car

### B1. Before starting

| Step | Do | The screen should show |
|---|---|---|
| 1 | Car parked, engine off. Unit on the passenger seat, display up | — |
| 2 | Find the diagnostic port: on most Lexus models it is under the dashboard on the driver's side, below and left of the steering column (assumption A23, not yet checked on this car). It is a 16-pin trapezoid socket | — |
| 3 | Plug the vLinker in fully. Its light comes on: the port powers it even with the ignition off | — |
| 4 | Ignition to ON: with the brake pedal released, press the start button twice (once is accessory, twice is ON on Lexus push-button start), or start the engine | — |
| 5 | Plug the power bank's USB-C cable into the Pi | Rainbow screen, then the boot text, then the hub. Time to the hub: not yet measured (REQ-013 target 15 s, provisional) |
| 6 | Wait | "Searching for adapter" in amber until paired (B2), then "Live" in green |

### B2. First time only: pair the adapter

Pairing is needed once. BlueZ keeps the pairing and trust across reboots, and car mode reconnects by itself every time after that.

**Touchscreen only (nothing to type):** with the adapter in the port and the ignition on:
1. Tap **Pair adapter** on the hub. A terminal opens and scans for up to 30 s for a device named like "vLinker". It prefers the name ending in "-Android", which is the classic Bluetooth side the serial port uses. "-IOS" is the BLE side.
2. It pairs with PIN 1234. The vLinker's PIN is assumed to be 1234 (assumption A20); a different PIN goes into `car.conf`.
3. It trusts the adapter, saves its address in `~/.config/lexus-head-unit/car.conf` and restarts the binding and the service.
4. The terminal shows the result for 60 s and closes. Within about a minute the strip says "Live". The time is not yet measured: it is Bluetooth connection time plus the protocol search, A18.

**From the phone over SSH** (phone on the hotspot, A3 and A4): `lexus-pair-adapter`. It does the same and prints each step; `--pin 0000` if the PIN is different.

**Through the Raspberry Pi OS Bluetooth menu:** the menu is on the desktop panel, which car mode keeps behind the full-screen hub. Use it before car mode is installed, or when the hub is not showing:
1. Tap the Bluetooth icon in the panel, then "Add Device".
2. Choose "vLinker MC-Android" and "Pair". If a PIN is asked, type 1234 with the on-screen keyboard. Whether the image shows an on-screen keyboard is not yet checked.
3. Then tap **Pair adapter** on the hub once. It finds the adapter already paired and only saves its address.

**If pairing fails:**
- If it reports "no device found", check that the adapter's light is on and the ignition is ON. Make sure the adapter is not connected to a phone app at the same time.
- If it reports a pairing failure, the PIN may be wrong, or the adapter is paired to another device. Remove it there, then retry.

### B3. What the strip says, and what to do

| Large text on the strip | Meaning | What to do |
|---|---|---|
| Searching for adapter (amber) | No adapter answered since the unit started | Is the adapter plugged in, light on? Paired (B2)? Self-check over SSH: `lexus-car-selfcheck` |
| Adapter found, no vehicle (amber) | The adapter answers but the car does not: the ignition is off (assumption A19) | Ignition to ON. It goes Live by itself within one retry (up to 10 s) plus the protocol search |
| Live (green) | Data is flowing | Nothing |
| Link lost, retrying (red) | Data was live and the adapter stopped answering: unplugged, out of range, or asleep after the car was switched off | Plug it back in or switch the ignition on; it reconnects by itself. Nothing needs restarting |
| Not started (grey) | The hub has heard nothing from the vehicle-data service yet (just after boot). If the service stops later, the strip says "Link lost, retrying" instead | Wait a few seconds; if it stays, over SSH: `systemctl --user status lexus-vehicle-data-service` (it restarts by itself after 2 s) |
| "Vehicle off: shutting down in N s", with Cancel | The vehicle has been quiet for 5 minutes after being live | Tap Cancel to keep the unit on, or let it shut down cleanly |

The power text on the right of the strip shows "Power OK", or the firmware flag in red, for example under-voltage on a weak power bank. An amber dot means a flag was set since boot.

### B4. Ending the day

1. Switch the car off. After 5 minutes of no vehicle data the hub counts down 60 s and shuts the Pi down cleanly. To leave sooner, tap the power button on the strip twice.
2. Wait until the screen is dark and the Pi's green light has stopped blinking. Only then unplug the power bank.
3. Unplug the adapter from the port.

If the power bank is unplugged while running, the session data up to about 5 s before the cut is kept (writeback limit, DN-044). The file system is checked at the next boot.

## Part C: recording the demo videos

Film with the phone. Never touch the screen while the car moves; the driver never looks at it.

| # | Shot | How | Car state |
|---|---|---|---|
| 1 | Cold start to live data | Phone on a stand, one take: plug in the power bank, the boot, the hub, "Searching", then "Live" | Parked, engine idling |
| 2 | The four link states | Unplug and replug the adapter ("Link lost, retrying", then "Live"); ignition off ("Adapter found, no vehicle"), ignition on | Parked |
| 3 | Vehicle data grid | Tap Vehicle data, hold 10 s, blip the throttle once to about 2,000 rpm so speed and RPM visibly react | Parked, engine idling, foot on the brake, in P |
| 4 | Trip screen | Tap Trip after a short drive (shot 6) to show distance, economy, warm-up and RPM bands | Parked, after the drive |
| 5 | Diagnostics | Tap Diagnostics and Read codes. **Do not film the identification line**: it shows the VIN. Frame the code list only, or cover the line | Parked |
| 6 | Short drive | A passenger films the screen and, if they like, the tachometer, for under 10 minutes on quiet roads. Nobody touches the screen. Only after OQ-14 is decided | Driving |
| 7 | Demo mode at the desk | Optional, for a clean screen recording without glare: demo mode on the Pi at home, filmed or recorded | At home |

Before anything goes on GitHub, watch every clip for the VIN and the licence plate, and blur them. Bluetooth addresses do not appear on any screen of the unit.

## Part D: at home, copying everything off

1. Pi on the home network, on the desk charger. Over SSH from the laptop:

   ```sh
   lexus-export-sessions
   ```

   It prepares `~/lexus-data/export/<session>/` for every session. That folder holds:
   - the session log and its summary;
   - the thermal log and its summary;
   - the configuration used;
   - the recording with the VIN scrubbed.

   The raw recording never enters the export folder. A recording that still contains anything VIN-shaped is not exported, and the command reports it.
2. On the laptop, in PowerShell, in the repository folder:

   ```sh
   scp -r lexus-pi:lexus-data/export local_recordings/car-sessions
   ```

   `local_recordings/` is ignored by git.
3. What goes into the repository later, in a pull request, and only after reading it:
   - Thermal CSVs and summaries go under `docs/measurements/thermal_power/` (LHU-016).
   - Results go under `docs/test/results/`, following the procedure and checklist steps.
   - Scrubbed recordings only if a test needs them.
   - The car's raw recordings and the `car.conf` file never go in.
4. The videos go to the README's Demo section once they exist (LHU-045, LHU-035).

## What this guide does not cover

- How the unit is held for a drive (OQ-14). It must be decided before shot 6.
- Audio to the car stereo (LHU-024, `docs/test/MANUAL_ON_CAR_PROCEDURE.md`).
- The measurement runs (boot time, latency, memory): checklist step 3.9.
