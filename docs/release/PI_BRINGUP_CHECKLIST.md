# Pi and car checklist

Every step that must be run on the Raspberry Pi or in the car, in one place. The software is built and tested at the desk against fakes and the ELM327 emulator; this file lists what the desk cannot prove. Each ticket that adds such a step adds it here, with the exact commands, the expected output and where the measured result is recorded. First-boot setup of a fresh card is in `deploy/PI_SETUP.md`; this file starts where that one ends.

Rules:

- A number in this file is either **measured** (copied from a run, with the date and the raw file named) or **not yet measured**. Nothing in between: no estimates, no placeholders.
- An assumption made at the desk in place of a hardware fact is listed in the table of section 2 with the step that verifies it. Until that step has run, the assumption stays an assumption, however plausible.
- Raw outputs go under `docs/measurements/` or `docs/test/results/`; recordings that may contain the vehicle identification number stay in the ignored `local_recordings/` folder until scrubbed (D-023).

## 1. Status

| Area | Ticket | State |
|---|---|---|
| Thermal and power log in every Pi session | LHU-015, LHU-016 | Script not yet written; two raw CSVs from 2026-10-05 exist on the Pi under `~/measurements/` |
| Adapter pairing and first parked session, adapter bound to `/dev/rfcomm0` and polled by the real source | LHU-017 | Not yet done |
| Clean-build measurement on the Pi | LHU-018 | Not yet done |
| Return to the hub from a fullscreen app | LHU-020, LHU-021 | Not yet done |
| Web apps, protected audio, browser memory | LHU-023 | Not yet done |
| Bluetooth audio to the car stereo | LHU-024 | Not yet done |
| Power flags on screen, clean shutdown cycles | LHU-025 | Not yet done |
| vcan tests | LHU-028 | Not yet done |
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

## 3. Steps

### 3.1 Thermal and power log (every session)

Added by LHU-015. Until the script exists, the two raw CSVs of 2026-10-05 are the only data (idle 51 °C, 59.3 °C peak during the package upgrade, `get_throttled` 0x0, input 5.245 V; measured, from `~/measurements/` on the Pi).

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

### 3.3 Clean-build measurement on the Pi (LHU-018)

With the thermal log running: clone, `cmake --preset release`, `cmake --build --preset release --parallel 2`, with `vmstat 5` in a second terminal. Record wall-clock time, peak memory, swap activity and maximum temperature in `docs/measurements/build/<date>_pi_clean_build.md`. Switch trigger (D-022): killed for memory, continuous swapping, or 80 °C.

Results: not yet measured.
