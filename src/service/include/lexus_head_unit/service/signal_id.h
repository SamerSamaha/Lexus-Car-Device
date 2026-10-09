#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace lexus_head_unit {

enum class SignalId {
    VehicleSpeed,
    EngineRpm,
    CoolantTemperature,
    EngineLoad,
    ThrottlePosition,
    IntakeAirTemperature,
    ControlModuleVoltage,
    FuelLevel,
    // Measured from LHU-031 (PID 0x10), the input of fuel economy.
    MassAirFlow,
    // Derived by the service layer (DN-031, REQ-022). Appended, so the numbers of the signals
    // above stay the same on D-Bus.
    InstantFuelEconomy,
    TripFuelEconomy,
    TripDistance,
    TimeBelow1000Rpm,
    Time1000To2499Rpm,
    Time2500To3999Rpm,
    TimeFrom4000Rpm,
    CoolantWarmUpTime,
};

constexpr std::size_t signalCount = 17;
static_assert(static_cast<std::size_t>(SignalId::CoolantWarmUpTime) + 1 == signalCount,
              "signalCount must follow the last SignalId");

constexpr std::array<SignalId, signalCount> allSignalIds = {
    SignalId::VehicleSpeed,
    SignalId::EngineRpm,
    SignalId::CoolantTemperature,
    SignalId::EngineLoad,
    SignalId::ThrottlePosition,
    SignalId::IntakeAirTemperature,
    SignalId::ControlModuleVoltage,
    SignalId::FuelLevel,
    SignalId::MassAirFlow,
    SignalId::InstantFuelEconomy,
    SignalId::TripFuelEconomy,
    SignalId::TripDistance,
    SignalId::TimeBelow1000Rpm,
    SignalId::Time1000To2499Rpm,
    SignalId::Time2500To3999Rpm,
    SignalId::TimeFrom4000Rpm,
    SignalId::CoolantWarmUpTime,
};

// What a source can produce.
constexpr std::size_t measuredSignalCount = 9;
constexpr std::array<SignalId, measuredSignalCount> measuredSignalIds = {
    SignalId::VehicleSpeed,
    SignalId::EngineRpm,
    SignalId::CoolantTemperature,
    SignalId::EngineLoad,
    SignalId::ThrottlePosition,
    SignalId::IntakeAirTemperature,
    SignalId::ControlModuleVoltage,
    SignalId::FuelLevel,
    SignalId::MassAirFlow,
};

// The eight tiles of the vehicle-data grid (REQ-004, DN-039).
constexpr std::size_t gridSignalCount = 8;
constexpr std::array<SignalId, gridSignalCount> gridSignalIds = {
    SignalId::VehicleSpeed,
    SignalId::EngineRpm,
    SignalId::CoolantTemperature,
    SignalId::EngineLoad,
    SignalId::ThrottlePosition,
    SignalId::IntakeAirTemperature,
    SignalId::ControlModuleVoltage,
    SignalId::FuelLevel,
};

// Computed by the DerivedSignalEngine (DN-031); the eight tiles of the trip screen.
constexpr std::size_t derivedSignalCount = 8;
constexpr std::array<SignalId, derivedSignalCount> derivedSignalIds = {
    SignalId::InstantFuelEconomy,
    SignalId::TripFuelEconomy,
    SignalId::TripDistance,
    SignalId::CoolantWarmUpTime,
    SignalId::TimeBelow1000Rpm,
    SignalId::Time1000To2499Rpm,
    SignalId::Time2500To3999Rpm,
    SignalId::TimeFrom4000Rpm,
};

static_assert(measuredSignalCount + derivedSignalCount == signalCount,
              "every signal is either measured or derived");

enum class Unit {
    KilometresPerHour,
    RevolutionsPerMinute,
    DegreesCelsius,
    Percent,
    Volts,
    GramsPerSecond,
    LitresPer100Kilometres,
    Kilometres,
    Minutes,
};

constexpr std::size_t unitCount = 9;
static_assert(static_cast<std::size_t>(Unit::Minutes) + 1 == unitCount,
              "unitCount must follow the last Unit");

enum class SignalStatus {
    NeverReceived,
    Valid,
    Stale,
};

constexpr std::size_t indexOf(SignalId signalId) {
    return static_cast<std::size_t>(signalId);
}

constexpr bool isKnownSignal(SignalId signalId) {
    return indexOf(signalId) < signalCount;
}

constexpr bool isDerivedSignal(SignalId signalId) {
    return indexOf(signalId) > indexOf(SignalId::MassAirFlow) && isKnownSignal(signalId);
}

std::string_view toString(SignalId signalId);
std::string_view toString(Unit unit);
std::string_view toString(SignalStatus status);

} // namespace lexus_head_unit
