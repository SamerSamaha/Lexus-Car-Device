#!/bin/sh
# Verifies: REQ-027, REQ-028, REQ-030
# Options passed on the command line reach the program (lesson of LHU-047). The vehicle-data
# app chooses its display units from --units, then display.units in the car file, then US when
# a car file exists and metric when none does (DN-050); --hide-pointer hides the pointer in the
# app and the hub (DN-051). Each case starts the program offscreen for 3 s and reads what it
# reports on its error output, or that it is still running.
set -u
app="$1"
hub="$2"
map="$3"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
status=0
launcher=""
command -v dbus-run-session >/dev/null 2>&1 && launcher="dbus-run-session --"
printf '[display]\nunits = metric\n' > "$work/metric.conf"
printf '[mode]\nkind = car\n' > "$work/car_without_units.conf"
printf '[hub]\napps = one\n[app.one]\nname = One\nkind = native\ncommand = /bin/true\n' > "$work/hub.conf"
check_app() {
    expected="$1"
    shift
    (cd "$work" && QT_QPA_PLATFORM=offscreen timeout 3 "$app" --source fake "$@") \
        > "$work/out.log" 2>&1
    if grep -q "lexus-head-unit: $expected" "$work/out.log"; then
        echo "PASS  app $* -> $expected"
    else
        echo "FAIL  app $* -> expected '$expected':"
        cat "$work/out.log"
        status=1
    fi
}
check_app "units metric" --car-config "$work/none.conf"
check_app "units us" --car-config "$work/car_without_units.conf"
check_app "units metric" --car-config "$work/metric.conf"
check_app "units us" --car-config "$work/metric.conf" --units us
check_app "units metric" --car-config "$work/car_without_units.conf" --units metric
check_app "pointer hidden" --car-config "$work/none.conf" --hide-pointer
# shellcheck disable=SC2086
(cd "$work" && QT_QPA_PLATFORM=offscreen timeout 3 $launcher "$hub" --hide-pointer \
    --registry "$work/hub.conf" --control-socket "lexus-hub-test-$$") > "$work/hub.log" 2>&1
code=$?
if [ "$code" -eq 124 ]; then
    echo "PASS  hub --hide-pointer runs"
else
    echo "FAIL  hub --hide-pointer exited with $code:"
    cat "$work/hub.log"
    status=1
fi
# The map (DN-054): --hide-pointer reaches it, and it never prints the home coordinates, only
# whether a home area is set.
printf '[privacy]\nhome_latitude = 43.5\nhome_longitude = -87.0\n' > "$work/home.conf"
# shellcheck disable=SC2086
(cd "$work" && QT_QPA_PLATFORM=offscreen timeout 3 $launcher "$map" --hide-pointer \
    --car-config "$work/home.conf") > "$work/map.log" 2>&1
if grep -q "lexus-map: pointer hidden" "$work/map.log" \
    && grep -q "lexus-map: home area set, hidden while recording" "$work/map.log" \
    && ! grep -q "43.5" "$work/map.log"; then
    echo "PASS  map --hide-pointer, home area read and not printed"
else
    echo "FAIL  map output:"
    cat "$work/map.log"
    status=1
fi
exit "$status"
