# Raspberry Pi setup: first boot to a working desk unit

This is the checklist that was followed on 2026-10-05 to bring the Pi 5 and the 5-inch Touch Display 2 from the box to a landscape touch desktop with SSH, Bluetooth and thermal logging. It is written so that a fresh card can be prepared the same way. Facts marked **measured** were observed on this unit; everything else comes from the Raspberry Pi documentation. The systemd units for the vehicle-data service and the hub are added to this folder by LHU-022 and LHU-021.

## 1. Assembly (no tools beyond a small screwdriver)

1. Connect the display's "22-way to 15-way" ribbon cable to the Pi 5 connector marked CAM/DISP 1, contacts facing the USB ports, and the other end to the display board.
2. Connect the 3-pin power lead from the display board to the Pi's GPIO header: red to pin 2 (5 V), black to pin 6 (ground).
3. Mount the Pi on the back of the display with the four standoffs and M2.5 screws from the display box.
4. No case, no cooler (D-011). Heat and power are measured, not assumed (D-013).

## 2. Flashing the card

1. Raspberry Pi Imager 2.0.x on the laptop, with a USB microSD reader (the laptop has no card slot).
2. Choose the image **from Imager's own list**: Raspberry Pi OS (64-bit) desktop, Debian 13 "Trixie", image dated 2026-09-15. Do not use "Use custom" with a downloaded file: in Imager 2.0 that path skips the customisation step (known upstream issue).
3. Customisation: hostname `lexus-head-unit`, user `lexus`, SSH enabled with **public-key authentication only**, the dedicated public key pasted in, Wi-Fi credentials for the desk network, locale and keyboard.
4. Write, verify, eject.

The desktop image was chosen over Lite because the hub runs beside the system browser under a Wayland compositor (D-045). Qt eglfs on the bare framebuffer is rejected.

## 3. First boot

1. Power from the desk charger (5 V / 3 A USB-C). The Pi reports the 3 A supply and applies its 600 mA USB peripheral limit; this is expected.
2. The display is detected automatically on a Pi 5; no `config.txt` entry is needed. It comes up in portrait.
3. Wait for the desktop, then from the laptop:

```sh
ssh lexus-pi            # Host entry in ~/.ssh/config pointing at the Pi, user lexus, the dedicated key
```

If the `.local` name does not resolve from Windows, use the address shown by the router; on this unit mDNS resolution from Windows did **not** work (measured) and the address was used instead.

4. Update and reboot:

```sh
sudo apt update && sudo apt full-upgrade -y     # 93 packages on 2026-10-05 (measured)
sudo reboot
```

## 4. Landscape and touch

In the desktop: Preferences, Screen Configuration, select the DSI output, orientation **Left**, apply. This writes `transform 90` for the display output into `~/.config/kanshi/config` (measured: output named `DSI-2` on this unit). Touch follows the rotation without further steps (measured). Screen blanking was already off in the image; check Preferences, Raspberry Pi Configuration, Display if it is not.

Alternative for a console-only setup: `video=DSI-1:720x1280@60,rotate=90` in `/boot/firmware/cmdline.txt` (from the documentation; not used here).

## 5. Verify the software the project needs

```sh
apt policy qt6-base-dev | head -3          # expect 6.8.2 (measured: 6.8.2+dfsg-9+deb13u2)
bluetoothctl --version                     # expect BlueZ 5.8x (measured: 5.82)
which rfcomm                               # present on this image (measured)
python3 -c "import tkinter; print('tk ok')"  # preinstalled (measured)
swapon --show                              # expect zram, 2 GiB (measured)
vcgencmd measure_temp
vcgencmd get_throttled                     # expect throttled=0x0
vcgencmd pmic_read_adc EXT5V_V             # input voltage; worked on this unit (measured: 5.245 V idle)
```

## 6. Thermal and power logging

Every bring-up and on-car session runs the logger (LHU-015 script; LHU-016 sessions) from a fresh boot, writing one row every 5 s to a CSV under `~/measurements/`, later committed under `docs/measurements/thermal_power/` with the printed summary. Thresholds are in the plan (D-025): fail on any under-voltage or throttle flag or at 80 °C; warn from 75 °C.

First-boot results on this unit, desk charger, basement room (measured, raw CSVs kept): idle 51 °C on the desktop; 59.3 °C peak during the package upgrade; `get_throttled` 0x0 throughout; input 5.245 V. All inside the pass band.

## 7. Network in the car

The desk Wi-Fi does not reach the car. A phone hotspot is added as a second connection with a higher autoconnect priority than the desk network, so the Pi joins it when it is present:

```sh
sudo nmcli connection add type wifi con-name hotspot ifname wlan0 ssid "<hotspot name>" \
  wifi-sec.key-mgmt wpa-psk wifi-sec.psk "<password>" connection.autoconnect-priority 20
```

Web apps (REQ-018) need this connection; the vehicle-data path does not.

## 8. Bluetooth

The OBD adapter pairs on its classic-Bluetooth side (name ending in "-Android"; the "-IOS" name is the BLE side and is not used). Pairing is done once, through the desktop Bluetooth menu or `bluetoothctl` (`pair`, `trust`), then the software opens an RFCOMM socket by the device's **name**, looked up at run time through `bluetoothctl devices Paired`. No Bluetooth address is written into any file in this repository (D-023).

The car stereo is paired the same way as an audio sink when LHU-024 is done; the two links share one radio and their coexistence is measured, not assumed (OQ-28).

## 9. Ready-built executables from CI

Every CI run builds the release preset for arm64 and uploads it as the artifact `lexus-head-unit-arm64`. Instead of building on the Pi, it can be downloaded and copied over: `docs/release/PI_BRINGUP_CHECKLIST.md` step 3.12 has the commands and the checks. Building on the Pi stays the day-to-day route (D-022).

## 10. What is not done here

- No swap file on the SD card (D-022): the zram swap of the image is kept.
- No packages beyond the image and the project's dependencies.
- No change to the adapter's PIN or to the vehicle. The adapter is unplugged from the car when not testing.
