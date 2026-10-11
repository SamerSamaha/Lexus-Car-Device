#pragma once

#include "lexus_head_unit/service/signal_id.h"

#include <QString>

#include <optional>

namespace lexus_head_unit {

// The units the screen shows values in (DN-050, REQ-027). The service layer, D-Bus and every
// recording stay metric; only the text on screen is converted.
enum class DisplayUnits {
    Metric,
    // mph, miles, degrees Fahrenheit, US miles per gallon.
    UsCustomary,
};

// "metric" or "us" (case-insensitive, surrounding spaces ignored); nothing for any other text.
std::optional<DisplayUnits> parseDisplayUnits(const QString& text);

// The command line value if given, else the car.conf value, else the fallback (US when a car
// file exists, so car mode defaults to US units; metric at the desk). An unknown text in either
// is reported through `valid` = false and the next source is used.
DisplayUnits chooseDisplayUnits(const QString& commandLineValue,
                                const QString& carConfigurationValue,
                                DisplayUnits fallback,
                                bool& valid);

// The unit text shown for a signal's unit.
QString displayUnitText(Unit unit, DisplayUnits units);

// The value in the shown unit, or nothing when it cannot be shown (fuel economy of 0 L/100 km
// or less has no miles-per-gallon value).
std::optional<double> displayValue(double metricValue, Unit unit, DisplayUnits units);

// Decimal places for the shown unit.
int displayDecimals(Unit unit);

// Exact conversion factors: 1 mile = 1.609344 km; 1 US gallon = 3.785411784 L.
constexpr double kilometresPerMile = 1.609344;
constexpr double litresPerUsGallon = 3.785411784;
// mpg = this / (L/100 km).
constexpr double usMpgTimesLitresPer100Kilometres = 100.0 * litresPerUsGallon / kilometresPerMile;
// Above this the miles-per-gallon value is shown as the cap with a ">" (coasting).
constexpr double maximumShownMilesPerGallon = 999.0;

} // namespace lexus_head_unit
