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
    // The phone's position (DN-053, REQ-029), reported by the phone over the phone link and
    // published like the vehicle signals. Appended, so the numbers above stay the same on D-Bus.
    Latitude,
    Longitude,
    PositionAccuracy,
};

constexpr std::size_t signalCount = 20;
static_assert(static_cast<std::size_t>(SignalId::PositionAccuracy) + 1 == signalCount,
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
    SignalId::Latitude,
    SignalId::Longitude,
    SignalId::PositionAccuracy,
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

// The eight tiles of the vehicle-data grid (REQ-004, DN-039). The last tile was fuel level
// until the first car day: the 2013 GS350 does not support PID 0x2F, so it showed "--"; mass
// air flow (PID 0x10), which the car reports, took its place (LHU-051). Fuel level stays a
// signal and is still polled on a vehicle that supports it.
constexpr std::size_t gridSignalCount = 8;
constexpr std::array<SignalId, gridSignalCount> gridSignalIds = {
    SignalId::VehicleSpeed,
    SignalId::EngineRpm,
    SignalId::CoolantTemperature,
    SignalId::EngineLoad,
    SignalId::ThrottlePosition,
    SignalId::IntakeAirTemperature,
    SignalId::ControlModuleVoltage,
    SignalId::MassAirFlow,
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

// Reported by the phone, not by a vehicle source (DN-053).
constexpr std::size_t phoneSignalCount = 3;
constexpr std::array<SignalId, phoneSignalCount> phoneSignalIds = {
    SignalId::Latitude,
    SignalId::Longitude,
    SignalId::PositionAccuracy,
};

static_assert(measuredSignalCount + derivedSignalCount + phoneSignalCount == signalCount,
              "every signal is measured, derived or from the phone");

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
    // Appended for the phone's position (DN-053).
    Degrees,
    Metres,
};

constexpr std::size_t unitCount = 11;
static_assert(static_cast<std::size_t>(Unit::Metres) + 1 == unitCount,
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
    return indexOf(signalId) > indexOf(SignalId::MassAirFlow) &&
           indexOf(signalId) <= indexOf(SignalId::CoolantWarmUpTime);
}

constexpr bool isPhoneSignal(SignalId signalId) {
    return indexOf(signalId) >= indexOf(SignalId::Latitude) && isKnownSignal(signalId);
}

std::string_view toString(SignalId signalId);
std::string_view toString(Unit unit);
std::string_view toString(SignalStatus status);

} // namespace lexus_head_unit
