#include "lexus_head_unit/service/signal_definition.h"

#include "lexus_head_unit/service/signal_id.h"

namespace lexus_head_unit {

namespace {

constexpr SignalDefinitionTable defaultTable = {{
    {SignalId::VehicleSpeed,
     "Vehicle speed",
     Unit::KilometresPerHour,
     defaultStalenessTimeoutMilliseconds},
    {SignalId::EngineRpm,
     "Engine speed",
     Unit::RevolutionsPerMinute,
     defaultStalenessTimeoutMilliseconds},
    {SignalId::CoolantTemperature,
     "Coolant temperature",
     Unit::DegreesCelsius,
     defaultStalenessTimeoutMilliseconds},
    {SignalId::EngineLoad, "Engine load", Unit::Percent, defaultStalenessTimeoutMilliseconds},
    {SignalId::ThrottlePosition,
     "Throttle position",
     Unit::Percent,
     defaultStalenessTimeoutMilliseconds},
    {SignalId::IntakeAirTemperature,
     "Intake air temperature",
     Unit::DegreesCelsius,
     defaultStalenessTimeoutMilliseconds},
    {SignalId::ControlModuleVoltage,
     "Control module voltage",
     Unit::Volts,
     defaultStalenessTimeoutMilliseconds},
    {SignalId::FuelLevel, "Fuel level", Unit::Percent, defaultStalenessTimeoutMilliseconds},
}};

} // namespace

const SignalDefinitionTable& defaultSignalDefinitions() {
    return defaultTable;
}

const SignalDefinition& definitionOf(SignalId signalId) {
    return defaultTable.at(indexOf(signalId));
}

} // namespace lexus_head_unit
