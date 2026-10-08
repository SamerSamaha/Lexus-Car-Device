#include "lexus_head_unit/service/signal_id.h"

#include <string_view>

namespace lexus_head_unit {

std::string_view toString(SignalId signalId) {
    switch (signalId) {
    case SignalId::VehicleSpeed:
        return "VehicleSpeed";
    case SignalId::EngineRpm:
        return "EngineRpm";
    case SignalId::CoolantTemperature:
        return "CoolantTemperature";
    case SignalId::EngineLoad:
        return "EngineLoad";
    case SignalId::ThrottlePosition:
        return "ThrottlePosition";
    case SignalId::IntakeAirTemperature:
        return "IntakeAirTemperature";
    case SignalId::ControlModuleVoltage:
        return "ControlModuleVoltage";
    case SignalId::FuelLevel:
        return "FuelLevel";
    }
    return "UnknownSignal";
}

std::string_view toString(Unit unit) {
    switch (unit) {
    case Unit::KilometresPerHour:
        return "km/h";
    case Unit::RevolutionsPerMinute:
        return "rpm";
    case Unit::DegreesCelsius:
        return "degC";
    case Unit::Percent:
        return "%";
    case Unit::Volts:
        return "V";
    }
    return "?";
}

std::string_view toString(SignalStatus status) {
    switch (status) {
    case SignalStatus::NeverReceived:
        return "NeverReceived";
    case SignalStatus::Valid:
        return "Valid";
    case SignalStatus::Stale:
        return "Stale";
    }
    return "UnknownStatus";
}

} // namespace lexus_head_unit
