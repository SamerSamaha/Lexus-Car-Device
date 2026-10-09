#!/bin/sh
# Binds the paired Bluetooth OBD-II adapter to /dev/rfcomm0 on the Raspberry Pi.
#
# Two ways to name the adapter:
#   sudo deploy/bind_obd_adapter.sh [NAME_FRAGMENT] [CHANNEL]
#       Desk procedure (checklist step 3.2): the address is looked up at run time through
#       bluetoothctl by part of the device name (default: vLinker) and never written anywhere.
#   lexus-bind-adapter --config ~/.config/lexus-head-unit/car.conf
#       Car mode (LHU-044): run as root by lexus-obd-bind.service at boot. The address and the
#       channel come from the [adapter] section of the car-mode file on the Pi, which
#       lexus-pair-adapter writes and which is never committed. With no address there yet the
#       script says so and exits 0, so the boot carries on and the service keeps searching.
#
# A bound device connects only when it is opened, so binding succeeds with the adapter away.
# Pair and trust the adapter first; its classic-Bluetooth name ends in "-Android".
# Afterwards the head unit configuration key elm327.device = /dev/rfcomm0 applies.

set -eu

# Prints the value of key in [section] of an ini-style file, or nothing.
ini_value() {
    awk -v wanted_section="$2" -v wanted_key="$3" '
        /^[ \t]*[#;]/ { next }
        /^[ \t]*\[/ { section = $0; gsub(/^[ \t]*\[|\][ \t]*$/, "", section); next }
        section == wanted_section {
            split($0, parts, "=")
            key = parts[1]; gsub(/^[ \t]+|[ \t]+$/, "", key)
            if (key == wanted_key) {
                value = substr($0, index($0, "=") + 1); gsub(/^[ \t]+|[ \t]+$/, "", value)
                print value; exit
            }
        }' "$1"
}

if [ "${1:-}" = "--config" ]; then
    config="${2:?usage: bind_obd_adapter.sh --config FILE}"
    if [ ! -r "$config" ]; then
        echo "bind_obd_adapter: $config not found; run deploy/car/install_car_mode.sh" >&2
        exit 0
    fi
    address="$(ini_value "$config" adapter address | tr 'a-f' 'A-F')"
    channel="$(ini_value "$config" adapter channel)"
    channel="${channel:-1}"
    if [ -z "$address" ]; then
        echo "bind_obd_adapter: no adapter address in $config yet; run lexus-pair-adapter in the car"
        exit 0
    fi
    if ! echo "$address" | grep -Eq '^([0-9A-F]{2}:){5}[0-9A-F]{2}$'; then
        echo "bind_obd_adapter: the address in $config is not a Bluetooth address" >&2
        exit 1
    fi
    label="the configured adapter (..:$(echo "$address" | cut -d: -f5-6))"
else
    name_fragment="${1:-vLinker}"
    channel="${2:-1}"
    address="$(bluetoothctl devices Paired 2>/dev/null | grep -i -- "$name_fragment" | head -n 1 | awk '{print $2}')"
    if [ -z "$address" ]; then
        address="$(bluetoothctl devices 2>/dev/null | grep -i -- "$name_fragment" | head -n 1 | awk '{print $2}')"
    fi
    if [ -z "$address" ]; then
        echo "bind_obd_adapter: no paired device whose name contains '$name_fragment'; pair it first" >&2
        exit 1
    fi
    bluetoothctl trust "$address" > /dev/null 2>&1 || true
    label="the device named like '$name_fragment'"
fi

if [ -e /dev/rfcomm0 ]; then
    rfcomm release 0 > /dev/null 2>&1 || true
fi
rfcomm bind 0 "$address" "$channel"
echo "bound /dev/rfcomm0 to $label on channel $channel"
