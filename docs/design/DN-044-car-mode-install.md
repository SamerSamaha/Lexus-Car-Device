# DN-044: Car mode install: boot to the hub, adapter binding, pairing, sessions, demo modes (light note)

| | |
|---|---|
| Ticket | LHU-044 |
| Requirements | REQ-025 (new); touches REQ-015, REQ-020, REQ-016 |
| Author | implementer (build-out form, D-049) |
| Status | Implemented |
| Draft written | 2026-10-09, about 40 minutes |
| Design review | after merge, by the repository owner (D-049) |
| Approved | 2026-10-09 |

## Problem

In the car there is only the Pi, the power bank, the adapter and a phone. Power on must lead to the hub on the screen with live data and nothing to type. The adapter must be found from one local setting. Every session must leave its data behind for the measurement tickets, safely across a pulled power cable. Demonstrations must run on the Pi without the car. And one script at home must set all of this up, repeatably, and say what works.

## Public interface

| Piece | What it is | Notes |
|---|---|---|
| `~/.config/lexus-head-unit/car.conf` | The only file with the adapter address; also the mode, the PIN, the data folder, the power-source label, the hotspot connection name | Created from `deploy/car/car.conf.example` once, mode 600, never overwritten, never committed. `tools/car/car_config.py` reads it and edits single keys, keeping comments |
| `lexus-obd-bind.service` (system) | Runs `lexus-bind-adapter --config <car.conf>` at boot: `rfcomm bind 0 <address> <channel>` | `deploy/bind_obd_adapter.sh` gained `--config`. With no address yet it exits 0, so the boot continues and the strip says "Searching for adapter". A bound device connects only when opened, so binding works with the adapter away; the service's backoff (1, 2, 4, 8, then every 10 s, forever) does the rest (REQ-008) |
| `lexus-vehicle-data-service.service` (user, car variant) | Runs `lexus-car-session`, which opens `<data_dir>/sessions/NNNN-<mode>/`, writes `head_unit.conf` (the repository file plus the mode's overrides; the later line wins), `session.txt`, starts the thermal logger there, then `exec`s the service with `--session-log` and, in the adapter modes, `--record` | Folders are numbered, not dated: the Pi has no clock battery |
| `lexus-hub-session` | Started from `~/.config/labwc/autostart`: starts the service unit, keeps `lexus-hub --registry deploy/car/hub_car.conf --fullscreen` running (restarts it 1 s after any exit), starts kanshi if it is not running | The car registry holds only native apps; a web app would have no way back without the panel (DN-043) |
| `lexus-mode car\|demo\|replay [FILE\|last]\|emulator\|show` | Writes the mode, starts or stops the emulator unit, restarts the service | Also the hub tiles "Demo mode", "Car mode", "Replay last drive" |
| `lexus-pair-adapter` | Finds the adapter (already paired, or scan for the name fragment, preferring "-Android"), pairs with the PIN, trusts, writes the address, restarts the binding and the service | Drives `bluetoothctl` on a pseudo-terminal. A polkit rule lets the user restart that one system unit. The hub tile "Pair adapter" runs it in `lxterminal` with nothing to type |
| `lexus-export-sessions` | Writes `<data_dir>/export/<session>/`: the logs as they are, a session summary, the thermal summary (rebuilt from the rows if a power cut prevented it), and `obd.scrubbed.rec` | The raw `obd.rec` never goes to the export folder; a recording with any VIN-shaped string left is not exported (REQ-015, D-023) |
| `deploy/car/install_car_mode.sh` | Packages if missing; executables from `--artifact` (checked as AArch64) or `--build` (2 jobs, D-022); the commands; the car-mode file; the system unit, polkit rule, writeback limits and `fsck.repair=yes`; the user units; auto-login (`raspi-config nonint do_boot_behaviour B4`) and the autostart line; then the self-check | Every step checks before it changes; `--dry-run` prints the changes; the desk hub unit of step 3.6 is disabled so that only one hub runs |
| `lexus-car-selfcheck [--with-demo]` | 24 items, each PASS, FAIL or SKIP, plus one when an adapter address is set (paired and trusted in BlueZ) and one with `--with-demo`; exit 1 on any FAIL | `--with-demo` switches to demo mode, checks Live and Valid samples over D-Bus within 20 s, and switches back |

## Power safety: what was chosen and why

| Option | Protects against | Cost | Decision |
|---|---|---|---|
| Read-only root (overlay) with a separate writable data partition | Almost all corruption of the system from a pulled cable | The card's root partition already fills it (expanded at first boot); shrinking a mounted ext4 root from a script is not safe, so this needs a reflash with a data partition planned in. Every package update needs the overlay switched off and a reboot | **Not now.** Documented as the step up if the shutdown cycles of steps 3.8 and 3.14 or a real power cut ever show file-system errors |
| Overlay without a data partition | The system | Every session log, recording and thermal log is lost at the next boot, which defeats REQ-025 | Rejected |
| Clean shutdown before the cable is pulled: the two-tap button (LHU-025) and the shutdown after the ignition goes off (LHU-043) | Everything, when used | Needs the user to wait for the shutdown (about 5 minutes after switching off, or two taps) | **Chosen**, the main protection |
| Writeback limits: `vm.dirty_expire_centisecs = 500`, `vm.dirty_writeback_centisecs = 100` | Losing the last 30 s of logs at a pulled cable | More, smaller writes: a few kB/s in a session | **Chosen** |
| `fsck.repair=yes` on the kernel command line | Boot stopping at a file-system check after a cut | none | **Chosen** (the installer adds it if missing, with a backup) |
| ext4's journal (already there) | Metadata consistency after a cut | none | Kept |
| A card with power-loss protection, a UPS board, an RTC battery | Cuts during a write; clock at boot | A purchase | Reported only (D-044) |

Trade-off stated plainly: without the overlay, a cable pulled while the system writes can still corrupt a file, and in rare cases the file system needs the repair at the next boot. The journal, the repair flag, the 5 s writeback and the automatic shutdown make that unlikely and bound the data lost to a few seconds; the shutdown cycles measure it.

## Failure cases

| Failure | Handling |
|---|---|
| No adapter address yet | The binding exits 0 with a message; the strip says "Searching for adapter"; the self-check says SKIP |
| Adapter absent, car off | The service retries forever (REQ-008); the strip says which (REQ-023) |
| Replay chosen with no recording yet | The session falls back to the demo source and says so in `session.txt` and the journal |
| The hub exits or crashes | `lexus-hub-session` starts it again after 1 s |
| The service crashes | `Restart=always` after 2 s, a new session folder |
| Power cut in a session | Logs up to about 5 s before the cut are on the card; the thermal summary is rebuilt at export |
| `labwc` runs only the user's autostart file (assumption A21) | kanshi is started by `lexus-hub-session`; the panel and the desktop then do not start, which is what car mode wants |
| Auto-login under another display manager than LightDM (assumption A22) | The self-check reports FAIL for "desktop logs in by itself"; the guide gives the manual setting |

## Test plan

| # | Test | Requirement |
|---|---|---|
| 1 | `tools/test_car_mode.py`: config read and edit; session numbering and the command per mode, replay of the newest car recording and its fallback; mode switching; pairing against a fake `bluetoothctl` on a pseudo-terminal (new adapter with PIN, already paired, not found, PIN refused; only the last two bytes printed); export (no raw recording, VIN gone, summary rebuilt, unchanged sessions skipped); self-check parsers and report; installer dry run (every step, nothing written, a foreign CPU refused); the scripts parse; the binding script without an address | REQ-025 |
| 2 | On the Pi: `install_car_mode.sh --artifact ...` then the self-check, run twice; a reboot to the hub with no input; checklist step 3.15 | REQ-025 |

## Changes after the design review

| # | Change | Reason |
|---|---|---|
| 1 | None yet; the review happens after merge (D-049) | |

## Design vs. implementation

To be written after merge.
