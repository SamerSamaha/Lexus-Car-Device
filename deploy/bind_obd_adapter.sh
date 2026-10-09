#!/bin/sh
# Binds the paired Bluetooth OBD-II adapter to /dev/rfcomm0 on the Raspberry Pi, by name.
#
# The adapter's Bluetooth address is looked up at run time through bluetoothctl and never
# written into a file (privacy rule). Pair and trust the adapter once through the desktop
# Bluetooth menu or bluetoothctl first; its classic-Bluetooth name ends in "-Android".
#
# Usage: sudo deploy/bind_obd_adapter.sh [NAME_FRAGMENT] [CHANNEL]
#   NAME_FRAGMENT  part of the paired device name (default: vLinker)
#   CHANNEL        RFCOMM channel (default: 1)
# Afterwards the head unit configuration key elm327.device = /dev/rfcomm0 applies.

set -eu

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
if [ -e /dev/rfcomm0 ]; then
    rfcomm release 0 > /dev/null 2>&1 || true
fi
rfcomm bind 0 "$address" "$channel"
echo "bound /dev/rfcomm0 to the device named like '$name_fragment' on channel $channel"
