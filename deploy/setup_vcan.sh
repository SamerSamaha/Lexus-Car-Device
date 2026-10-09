#!/bin/sh
# Creates the virtual CAN interface vcan0 for the CAN source and its tests (LHU-028).
# Pi only: the stock WSL2 kernel has no vcan module (D-003). Run with sudo.
#   sudo deploy/setup_vcan.sh
set -eu
modprobe vcan
if ! ip link show vcan0 > /dev/null 2>&1; then
    ip link add dev vcan0 type vcan
fi
ip link set up vcan0
ip -details link show vcan0
