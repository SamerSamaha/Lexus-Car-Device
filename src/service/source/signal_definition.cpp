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
    {SignalId::MassAirFlow,
     "Mass air flow",
     Unit::GramsPerSecond,
     defaultStalenessTimeoutMilliseconds},
    {SignalId::InstantFuelEconomy,
     "Fuel economy",
     Unit::LitresPer100Kilometres,
     defaultStalenessTimeoutMilliseconds},
    {SignalId::TripFuelEconomy,
     "Trip fuel economy",
     Unit::LitresPer100Kilometres,
     defaultStalenessTimeoutMilliseconds},
    {SignalId::TripDistance,
     "Trip distance",
     Unit::Kilometres,
     defaultStalenessTimeoutMilliseconds},
    {SignalId::TimeBelow1000Rpm,
     "Time below 1000 rpm",
     Unit::Minutes,
     defaultStalenessTimeoutMilliseconds},
    {SignalId::Time1000To2499Rpm,
     "Time 1000 to 2499 rpm",
     Unit::Minutes,
     defaultStalenessTimeoutMilliseconds},
    {SignalId::Time2500To3999Rpm,
     "Time 2500 to 3999 rpm",
     Unit::Minutes,
     defaultStalenessTimeoutMilliseconds},
    {SignalId::TimeFrom4000Rpm,
     "Time from 4000 rpm",
     Unit::Minutes,
     defaultStalenessTimeoutMilliseconds},
    {SignalId::CoolantWarmUpTime,
     "Coolant warm-up time",
     Unit::Minutes,
     defaultStalenessTimeoutMilliseconds},
}};

} // namespace

const SignalDefinitionTable& defaultSignalDefinitions() {
    return defaultTable;
}

const SignalDefinition& definitionOf(SignalId signalId) {
    return defaultTable.at(indexOf(signalId));
}

} // namespace lexus_head_unit
