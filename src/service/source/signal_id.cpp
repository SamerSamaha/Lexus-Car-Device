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
    case SignalId::MassAirFlow:
        return "MassAirFlow";
    case SignalId::InstantFuelEconomy:
        return "InstantFuelEconomy";
    case SignalId::TripFuelEconomy:
        return "TripFuelEconomy";
    case SignalId::TripDistance:
        return "TripDistance";
    case SignalId::TimeBelow1000Rpm:
        return "TimeBelow1000Rpm";
    case SignalId::Time1000To2499Rpm:
        return "Time1000To2499Rpm";
    case SignalId::Time2500To3999Rpm:
        return "Time2500To3999Rpm";
    case SignalId::TimeFrom4000Rpm:
        return "TimeFrom4000Rpm";
    case SignalId::CoolantWarmUpTime:
        return "CoolantWarmUpTime";
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
    case Unit::GramsPerSecond:
        return "g/s";
    case Unit::LitresPer100Kilometres:
        return "L/100 km";
    case Unit::Kilometres:
        return "km";
    case Unit::Minutes:
        return "min";
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
