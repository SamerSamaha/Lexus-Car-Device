#!/bin/sh
# Makes the paired car stereo the default audio output (LHU-024, REQ-019). Run as the desktop
# user, after the stereo is paired and connected over Bluetooth (A2DP):
#   deploy/set_audio_output_to_car.sh
# PipeWire names a Bluetooth output "bluez_output.<address>.<profile>"; the script finds it at
# run time, so no Bluetooth address is ever written into this repository (D-023).
set -eu
sink=$(pactl list short sinks | awk '$2 ~ /^bluez_output\./ { print $2; exit }')
if [ -z "$sink" ]; then
    echo "set_audio_output_to_car: no Bluetooth audio output connected" >&2
    exit 1
fi
pactl set-default-sink "$sink"
echo "set_audio_output_to_car: default output is now the Bluetooth device"
pactl get-default-sink | sed 's/\.[0-9A-F_]\{17\}\./.<address>./'
