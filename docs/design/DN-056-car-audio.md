# DN-056: Bluetooth audio to the car stereo (light note)

| | |
|---|---|
| Ticket | LHU-056 |
| Requirements | REQ-032 (new); REQ-019 (coexistence, measured in the car) |
| Author | implementer (build-out form, D-049) |
| Status | Implemented |
| Draft written | 2026-10-11, about 15 minutes |
| Design review | after merge, by the repository owner (D-049) |
| Approved | 2026-10-11 |

## Problem

The demo should sound through the car: YouTube, Spotify and the mirrored phone play on the Pi, and the Pi has no speaker. The GS350 accepts Bluetooth audio from a device (the owner, OQ-30). LHU-024 left a desk script that makes an already connected stereo the output; pairing and connecting still needed a keyboard.

## Public interface

| Piece | Interface | Notes |
|---|---|---|
| `lexus-car-audio` (`tools/car/car_audio.py`) | `--config`, `--pair` (a new stereo even if one is saved), `--scan-seconds` (45), `--bluetoothctl`, `--wait-at-end` | Uses the pseudo-terminal session and the pairing answers of `pair_adapter.py` (a passkey confirmation is answered yes; the owner confirms on the car) |
| Saved stereo | `[audio] address` in car.conf; `[audio] name_fragment` (default `LEXUS`, A34) | The address only in the local file; printed masked to its last two bytes |
| Output | waits up to 15 s for PipeWire's `bluez_output.*` sink, then `pactl set-default-sink` | Every program then plays to the car; the screen recorder records the monitor of the same output (DN-049) |
| Hub tile "Car audio" | `lxterminal -e "lexus-car-audio --wait-at-end 20"` | Like the Pair adapter tile: nothing to type |
| Installer | `libspa-0.2-bluetooth` (PipeWire's Bluetooth support), `pulseaudio-utils` (`pactl`); the command link | Both Debian 13 packages |

## Failure cases

| Case | Handling |
|---|---|
| No device with the name found | Exit 2, nothing saved; the message names the setting to change |
| Pairing refused or timed out on the car | Exit 3, nothing saved |
| The car is off or Bluetooth audio not selected | Connecting fails: exit 3 with the hint |
| PipeWire makes no Bluetooth output within 15 s | Exit 4; the stereo stays connected; the sound stays on the previous output |
| The OBD adapter and the stereo on one radio | Measured in the car, REQ-019 (OQ-28): if the OBD link drops with audio playing, audio is the one dropped (plan, risk list) |
| The car radio is broken (the owner, 2026-10-11) | Nothing at the desk depends on it; step 3.21 waits for the repair |

## Test plan

| Requirement | Test |
|---|---|
| REQ-032 | `tools/test_car_audio.py`: the sink list, the stereo chosen by name, the wait for the output and its limit; against a fake `bluetoothctl` on a pseudo-terminal: a new stereo paired (passkey confirmed), trusted, connected, saved and made the output, the address printed masked; a saved stereo only connected; none found, refused, car off, each without saving; the tile and the installer |
| REQ-032, REQ-019 | In the car: checklist step 3.21 |

## Alternative considered

Letting the owner pair the stereo once from the desktop's Bluetooth menu was rejected: car mode shows no desktop, and the connection must come back after every boot without a keyboard, which the tile does.
