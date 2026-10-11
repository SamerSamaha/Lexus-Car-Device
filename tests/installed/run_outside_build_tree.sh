#!/bin/sh
# Verifies: REQ-025, REQ-030
# Each program given is copied alone into an empty folder and started there, offscreen, the way
# the installer puts it in /usr/local/bin on the Pi. It must still be running after 5 s: a QML
# module that is only found through the build folder makes it exit at once (LHU-046).
set -u
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
status=0
for program in "$@"; do
    name="$(basename "$program")"
    cp "$program" "$work/$name"
    arguments=""
    [ "$name" = "lexus-head-unit" ] && arguments="--source fake"
    launcher=""
    command -v dbus-run-session >/dev/null 2>&1 && launcher="dbus-run-session --"
    # shellcheck disable=SC2086
    (cd "$work" && QT_QPA_PLATFORM=offscreen timeout 5 $launcher "./$name" $arguments) \
        >"$work/$name.log" 2>&1
    code=$?
    if [ "$code" -eq 124 ]; then
        echo "PASS  $name ran 5 s outside the build folder"
    else
        echo "FAIL  $name exited with $code outside the build folder:"
        cat "$work/$name.log"
        status=1
    fi
done
exit "$status"
