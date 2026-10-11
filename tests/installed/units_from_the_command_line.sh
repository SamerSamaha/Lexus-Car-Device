#!/bin/sh
# Verifies: REQ-027
# The vehicle-data app chooses its display units from --units, then display.units in the car
# file, then US when a car file exists and metric when none does (DN-050). Each case starts the
# program offscreen for 3 s and reads the units it reports on its error output.
set -u
program="$1"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
status=0
printf '[display]\nunits = metric\n' > "$work/metric.conf"
printf '[mode]\nkind = car\n' > "$work/car_without_units.conf"
check() {
    expected="$1"
    shift
    (cd "$work" && QT_QPA_PLATFORM=offscreen timeout 3 "$program" --source fake "$@") \
        > "$work/out.log" 2>&1
    if grep -q "lexus-head-unit: units $expected" "$work/out.log"; then
        echo "PASS  $* -> $expected"
    else
        echo "FAIL  $* -> expected $expected:"
        cat "$work/out.log"
        status=1
    fi
}
check metric --car-config "$work/none.conf"
check us --car-config "$work/car_without_units.conf"
check metric --car-config "$work/metric.conf"
check us --car-config "$work/metric.conf" --units us
check metric --car-config "$work/car_without_units.conf" --units metric
exit "$status"
