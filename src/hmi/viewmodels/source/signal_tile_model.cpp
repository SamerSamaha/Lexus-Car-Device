#include "lexus_head_unit/hmi/signal_tile_model.h"

#include "lexus_head_unit/hmi/display_units.h"
#include "lexus_head_unit/service/signal_definition.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"

#include <QObject>
#include <QString>

#include <optional>
#include <string_view>

// Qt declares its macros and basic types in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace lexus_head_unit {

namespace {

QString textOf(std::string_view text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

} // namespace

SignalTileModel::SignalTileModel(SignalId signalId, QObject* parent)
    : SignalTileModel(signalId, DisplayUnits::Metric, parent) {}

SignalTileModel::SignalTileModel(SignalId signalId, DisplayUnits units, QObject* parent)
    : QObject(parent),
      m_signalId(signalId),
      m_unit(definitionOf(signalId).unit),
      m_units(units),
      m_name(tileTitle(signalId)),
      m_unitText(displayUnitText(m_unit, units)),
      m_valueText(QStringLiteral("--")) {}

SignalId SignalTileModel::signalId() const {
    return m_signalId;
}

QString SignalTileModel::name() const {
    return m_name;
}

QString SignalTileModel::unitText() const {
    return m_unitText;
}

QString SignalTileModel::valueText() const {
    return m_valueText;
}

QString SignalTileModel::statusText() const {
    return textOf(toString(m_status));
}

bool SignalTileModel::isValid() const {
    return m_status == SignalStatus::Valid;
}

bool SignalTileModel::isStale() const {
    return m_status == SignalStatus::Stale;
}

bool SignalTileModel::isNeverReceived() const {
    return m_status == SignalStatus::NeverReceived;
}

void SignalTileModel::applySample(const SignalSample& sample) {
    if (sample.signalId != m_signalId) {
        return;
    }
    m_status = sample.status;
    if (m_status == SignalStatus::NeverReceived) {
        m_valueText = QStringLiteral("--");
    } else {
        m_valueText = formatValue(sample.value, m_unit, m_units);
    }
    emit changed();
}

QString SignalTileModel::tileTitle(SignalId signalId) {
    switch (signalId) {
    case SignalId::VehicleSpeed:
        return QStringLiteral("Speed");
    case SignalId::EngineRpm:
        return QStringLiteral("Engine speed");
    case SignalId::CoolantTemperature:
        return QStringLiteral("Coolant");
    case SignalId::EngineLoad:
        return QStringLiteral("Engine load");
    case SignalId::ThrottlePosition:
        return QStringLiteral("Throttle");
    case SignalId::IntakeAirTemperature:
        return QStringLiteral("Intake air");
    case SignalId::ControlModuleVoltage:
        return QStringLiteral("Voltage");
    case SignalId::FuelLevel:
        return QStringLiteral("Fuel level");
    case SignalId::MassAirFlow:
        return QStringLiteral("Air flow");
    case SignalId::InstantFuelEconomy:
        return QStringLiteral("Economy now");
    case SignalId::TripFuelEconomy:
        return QStringLiteral("Trip economy");
    case SignalId::TripDistance:
        return QStringLiteral("Trip distance");
    case SignalId::TimeBelow1000Rpm:
        return QStringLiteral("Below 1000 rpm");
    case SignalId::Time1000To2499Rpm:
        return QStringLiteral("1000-2499 rpm");
    case SignalId::Time2500To3999Rpm:
        return QStringLiteral("2500-3999 rpm");
    case SignalId::TimeFrom4000Rpm:
        return QStringLiteral("4000 rpm and up");
    case SignalId::CoolantWarmUpTime:
        return QStringLiteral("Warm-up time");
    case SignalId::Latitude:
        return QStringLiteral("Latitude");
    case SignalId::Longitude:
        return QStringLiteral("Longitude");
    case SignalId::PositionAccuracy:
        return QStringLiteral("Accuracy");
    }
    return textOf(definitionOf(signalId).name);
}

QString SignalTileModel::formatValue(double value, Unit unit) {
    return formatValue(value, unit, DisplayUnits::Metric);
}

QString SignalTileModel::formatValue(double value, Unit unit, DisplayUnits units) {
    const std::optional<double> shown = displayValue(value, unit, units);
    if (!shown.has_value()) {
        return QStringLiteral("--");
    }
    if (units == DisplayUnits::UsCustomary && unit == Unit::LitresPer100Kilometres &&
        *shown > maximumShownMilesPerGallon) {
        return QStringLiteral(">") + QString::number(maximumShownMilesPerGallon, 'f', 0);
    }
    return QString::number(*shown, 'f', displayDecimals(unit));
}

} // namespace lexus_head_unit
// NOLINTEND(misc-include-cleaner)
