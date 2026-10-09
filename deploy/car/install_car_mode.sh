#!/bin/bash
# Installs car mode on the Raspberry Pi (LHU-044, REQ-025): power on, and the hub is on the
# screen with the vehicle-data service and the adapter binding running, no keyboard needed.
#
# Run on the Pi at home, as the desktop user (not root; sudo is used where needed), from the
# clone at ~/Lexus-Car-Device, with the Pi on the home network:
#
#   deploy/car/install_car_mode.sh --artifact ~/lexus-head-unit-arm64   the CI arm64 build
#   deploy/car/install_car_mode.sh --build                              build here (2 jobs)
#   deploy/car/install_car_mode.sh                                      keep the executables
#
# Options: --no-autologin (leave the login as it is), --with-demo (the self-check also runs
# demo mode for up to 20 s), --dry-run (print every change, make none).
# Safe to run again after a git pull or a new artifact: every step checks before it changes.
# It ends with the self-check, which prints PASS, FAIL or SKIP per item.

set -euo pipefail

repository="$(cd "$(dirname "$(readlink -f "$0")")/../.." && pwd)"
artifact=""
build=0
autologin=1
with_demo=0
dry_run=0
user_name="$(id -un)"

while [ $# -gt 0 ]; do
    case "$1" in
        --artifact) artifact="${2:?--artifact needs a folder}"; shift 2 ;;
        --build) build=1; shift ;;
        --no-autologin) autologin=0; shift ;;
        --with-demo) with_demo=1; shift ;;
        --dry-run) dry_run=1; shift ;;
        -h|--help) sed -n '2,17p' "$0"; exit 0 ;;
        *) echo "install_car_mode: unknown option $1" >&2; exit 2 ;;
    esac
done

step() { printf '\n== %s\n' "$*"; }
note() { printf '   %s\n' "$*"; }
run() {
    if [ "$dry_run" -eq 1 ]; then
        printf '   would run: %s\n' "$*"
    else
        "$@"
    fi
}
# Writes stdin to a root-owned file only when the content differs; prints whether it changed.
install_root_file() {
    local target="$1" mode="$2" content
    content="$(cat)"
    if [ -f "$target" ] && [ "$(cat "$target")" = "$content" ]; then
        note "unchanged: $target"
        return 1
    fi
    if [ "$dry_run" -eq 1 ]; then
        note "would write: $target"
    else
        printf '%s\n' "$content" | sudo tee "$target" > /dev/null
        sudo chmod "$mode" "$target"
        note "written: $target"
    fi
    return 0
}

if [ "$(id -u)" -eq 0 ]; then
    if [ "$dry_run" -eq 0 ]; then
        echo "install_car_mode: run as the desktop user, not as root (sudo is used where needed)" >&2
        exit 2
    fi
    note "running as root: a dry run only (the tests in CI run as root)"
fi
if [ -n "$artifact" ] && [ "$build" -eq 1 ]; then
    echo "install_car_mode: give --artifact or --build, not both" >&2
    exit 2
fi
if [ "$repository" != "$HOME/Lexus-Car-Device" ]; then
    if [ "$dry_run" -eq 1 ]; then
        note "the clone is at $repository; the units expect $HOME/Lexus-Car-Device (dry run carries on)"
    else
        echo "install_car_mode: clone the repository to $HOME/Lexus-Car-Device; the units use that path" >&2
        exit 2
    fi
fi

step "1. Packages (installed only if missing)"
# Runtime for the three executables: Qt base, D-Bus, network, QML and Qt Quick with the two
# QML modules the screens import. bluez provides bluetoothctl and rfcomm (the adapter link);
# python3 runs the car-mode tools, the thermal logger and the emulator.
packages="libqt6core6t64 libqt6gui6 libqt6dbus6 libqt6network6 libqt6qml6 libqt6quick6
          qml6-module-qtquick qml6-module-qtquick-window bluez python3 lxterminal"
if [ "$build" -eq 1 ]; then
    # Building here: compiler, CMake and Ninja, the Qt development files, GoogleTest for the
    # test targets the build includes, and dbus-daemon for the D-Bus tests.
    packages="$packages build-essential cmake ninja-build qt6-base-dev qt6-declarative-dev
              libgtest-dev libgmock-dev dbus-daemon"
fi
missing=""
for package in $packages; do
    if ! dpkg-query -W -f='${Status}' "$package" 2>/dev/null | grep -q "install ok installed"; then
        missing="$missing $package"
    fi
done
if [ -n "$missing" ]; then
    note "installing:$missing"
    run sudo apt-get update
    # shellcheck disable=SC2086
    run sudo apt-get install --yes --no-install-recommends $missing
else
    note "all present"
fi

step "2. Executables in /usr/local/bin"
executables="lexus-hub lexus-head-unit lexus-vehicle-data-service"
source_folder=""
if [ -n "$artifact" ]; then
    source_folder="$artifact"
    for name in $executables; do
        [ -f "$artifact/$name" ] || { echo "install_car_mode: $artifact/$name missing" >&2; exit 1; }
        # Bytes 18 and 19 of an ELF file name the CPU: b7 00 is AArch64 (LHU-033).
        machine="$(od -An -tx1 -j18 -N2 "$artifact/$name" | tr -d ' ')"
        [ "$machine" = "b700" ] || { echo "install_car_mode: $name is not built for AArch64 ($machine)" >&2; exit 1; }
    done
    origin="CI artifact, commit $(cat "$artifact/commit.txt" 2>/dev/null || echo unknown)"
elif [ "$build" -eq 1 ]; then
    run cmake --preset release -S "$repository"
    run cmake --build --preset release --parallel 2
    build_folder="$HOME/build/lexus-car-device/release"
    source_folder="$build_folder/collected"
    run mkdir -p "$source_folder"
    run cp "$build_folder/src/hub/app/lexus-hub" "$build_folder/src/app/lexus-head-unit" \
        "$build_folder/src/service_dbus/lexus-vehicle-data-service" "$source_folder/"
    origin="built on the Pi"
fi
if [ -n "$source_folder" ]; then
    for name in $executables; do
        run sudo install -m 755 "$source_folder/$name" "/usr/local/bin/$name"
    done
    run sudo mkdir -p /usr/local/share/lexus-head-unit
    printf 'installed %s from %s\n' "$(date -Iseconds)" "$origin" \
        | install_root_file /usr/local/share/lexus-head-unit/installed.txt 644 || true
else
    note "kept the executables already installed (no --artifact or --build)"
fi

step "3. Car-mode tools as commands"
for pair in "car_session.py:lexus-car-session" "car_mode.py:lexus-mode" \
            "pair_adapter.py:lexus-pair-adapter" "export_sessions.py:lexus-export-sessions" \
            "self_check.py:lexus-car-selfcheck"; do
    run sudo ln -sfn "$repository/tools/car/${pair%%:*}" "/usr/local/bin/${pair##*:}"
done
run sudo ln -sfn "$repository/deploy/bind_obd_adapter.sh" /usr/local/bin/lexus-bind-adapter
run sudo ln -sfn "$repository/deploy/car/lexus-hub-session" /usr/local/bin/lexus-hub-session

step "4. The car-mode file and the session folder"
config="$HOME/.config/lexus-head-unit/car.conf"
if [ -f "$config" ]; then
    note "kept: $config"
else
    run install -D -m 600 "$repository/deploy/car/car.conf.example" "$config"
fi
run chmod 600 "$config"
run mkdir -p "$HOME/lexus-data/sessions" "$HOME/lexus-data/export"
if id -nG "$user_name" | tr ' ' '\n' | grep -qx dialout; then
    note "$user_name may open /dev/rfcomm0 (dialout)"
else
    run sudo usermod -aG dialout "$user_name"
    note "added to dialout; takes effect after the reboot"
fi

step "5. System: adapter binding, pairing permission, writeback limits, boot repair"
system_changed=0
sed -e "s|@HOME@|$HOME|g" -e "s|@USER@|$user_name|g" \
    "$repository/deploy/car/systemd/system/lexus-obd-bind.service.in" \
    | install_root_file /etc/systemd/system/lexus-obd-bind.service 644 && system_changed=1
sed -e "s|@USER@|$user_name|g" "$repository/deploy/car/50-lexus-obd-bind.rules.in" \
    | install_root_file /etc/polkit-1/rules.d/50-lexus-obd-bind.rules 644 || true
if install_root_file /etc/sysctl.d/90-lexus-writeback.conf 644 \
    < "$repository/deploy/car/90-lexus-writeback.conf"; then
    run sudo sysctl -p /etc/sysctl.d/90-lexus-writeback.conf
fi
cmdline=/boot/firmware/cmdline.txt
if [ -f "$cmdline" ] && ! tr ' ' '\n' < "$cmdline" | grep -qx "fsck.repair=yes"; then
    run sudo cp "$cmdline" "$cmdline.before-lexus"
    run sudo sed -i '1 s/$/ fsck.repair=yes/' "$cmdline"
    note "added fsck.repair=yes; the old file is $cmdline.before-lexus"
else
    note "fsck.repair=yes present (or no $cmdline)"
fi
if [ "$system_changed" -eq 1 ]; then
    run sudo systemctl daemon-reload
fi
run sudo systemctl enable lexus-obd-bind.service
run sudo systemctl restart lexus-obd-bind.service

step "6. User units: the vehicle-data service and the emulator"
run install -D -m 644 "$repository/deploy/car/systemd/user/lexus-vehicle-data-service.service" \
    "$HOME/.config/systemd/user/lexus-vehicle-data-service.service"
run install -D -m 644 "$repository/deploy/car/systemd/user/lexus-elm327-emulator.service" \
    "$HOME/.config/systemd/user/lexus-elm327-emulator.service"
run systemctl --user daemon-reload
# The desk hub unit of checklist step 3.6 would start a second hub; car mode starts it from labwc.
if systemctl --user is-enabled lexus-hub.service > /dev/null 2>&1; then
    run systemctl --user disable --now lexus-hub.service
fi
run systemctl --user enable lexus-vehicle-data-service.service
run systemctl --user restart lexus-vehicle-data-service.service

step "7. Desktop: log in by itself and start the hub"
if [ "$autologin" -eq 1 ]; then
    # B4: desktop with automatic login of the user, the documented raspi-config setting.
    run sudo raspi-config nonint do_boot_behaviour B4
else
    note "--no-autologin: login left as it is"
fi
autostart="$HOME/.config/labwc/autostart"
marker="# lexus head unit car mode (deploy/car/install_car_mode.sh)"
if [ -f "$autostart" ] && grep -qF "$marker" "$autostart"; then
    note "unchanged: $autostart"
elif [ "$dry_run" -eq 1 ]; then
    note "would add lexus-hub-session to $autostart"
else
    mkdir -p "$(dirname "$autostart")"
    printf '%s\n/usr/local/bin/lexus-hub-session &\n' "$marker" >> "$autostart"
    note "added lexus-hub-session to $autostart"
fi

step "8. Self-check"
if [ "$dry_run" -eq 1 ]; then
    note "would run: lexus-car-selfcheck$([ "$with_demo" -eq 1 ] && echo " --with-demo")"
    exit 0
fi
if [ "$with_demo" -eq 1 ]; then
    lexus-car-selfcheck --with-demo
else
    lexus-car-selfcheck
fi
