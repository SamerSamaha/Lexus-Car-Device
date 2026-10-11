#include "lexus_head_unit/hmi/display_units.h"

#include "lexus_head_unit/service/signal_id.h"

#include <QString>

#include <optional>
#include <string_view>

// Qt declares its macros and basic types in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace lexus_head_unit {

namespace {

constexpr double fahrenheitPerCelsius = 1.8;
constexpr double fahrenheitAtZeroCelsius = 32.0;

QString textOf(std::string_view text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

} // namespace

std::optional<DisplayUnits> parseDisplayUnits(const QString& text) {
    const QString normalised = text.trimmed().toLower();
    if (normalised == QStringLiteral("metric")) {
        return DisplayUnits::Metric;
    }
    if (normalised == QStringLiteral("us")) {
        return DisplayUnits::UsCustomary;
    }
    return std::nullopt;
}

DisplayUnits chooseDisplayUnits(const QString& commandLineValue,
                                const QString& carConfigurationValue,
                                DisplayUnits fallback,
                                bool& valid) {
    valid = true;
    for (const QString& candidate : {commandLineValue, carConfigurationValue}) {
        if (candidate.trimmed().isEmpty()) {
            continue;
        }
        const std::optional<DisplayUnits> parsed = parseDisplayUnits(candidate);
        if (parsed.has_value()) {
            return *parsed;
        }
        valid = false;
    }
    return fallback;
}

QString displayUnitText(Unit unit, DisplayUnits units) {
    if (units == DisplayUnits::UsCustomary) {
        switch (unit) {
        case Unit::KilometresPerHour:
            return QStringLiteral("mph");
        case Unit::DegreesCelsius:
            return QStringLiteral("°F");
        case Unit::LitresPer100Kilometres:
            return QStringLiteral("mpg");
        case Unit::Kilometres:
            return QStringLiteral("mi");
        default:
            break;
        }
    }
    return textOf(toString(unit));
}

std::optional<double> displayValue(double metricValue, Unit unit, DisplayUnits units) {
    if (units == DisplayUnits::Metric) {
        return metricValue;
    }
    switch (unit) {
    case Unit::KilometresPerHour:
    case Unit::Kilometres:
        return metricValue / kilometresPerMile;
    case Unit::DegreesCelsius:
        return (metricValue * fahrenheitPerCelsius) + fahrenheitAtZeroCelsius;
    case Unit::LitresPer100Kilometres:
        if (metricValue <= 0.0) {
            return std::nullopt;
        }
        return usMpgTimesLitresPer100Kilometres / metricValue;
    default:
        return metricValue;
    }
}

int displayDecimals(Unit unit) {
    constexpr int degreesDecimals = 5;
    if (unit == Unit::Degrees) {
        return degreesDecimals;
    }
    const bool oneDecimal = unit == Unit::Volts || unit == Unit::GramsPerSecond ||
                            unit == Unit::LitresPer100Kilometres || unit == Unit::Kilometres ||
                            unit == Unit::Minutes;
    return oneDecimal ? 1 : 0;
}

} // namespace lexus_head_unit
// NOLINTEND(misc-include-cleaner)
