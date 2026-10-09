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
};

constexpr std::size_t signalCount = 8;

constexpr std::array<SignalId, signalCount> allSignalIds = {
    SignalId::VehicleSpeed,
    SignalId::EngineRpm,
    SignalId::CoolantTemperature,
    SignalId::EngineLoad,
    SignalId::ThrottlePosition,
    SignalId::IntakeAirTemperature,
    SignalId::ControlModuleVoltage,
    SignalId::FuelLevel,
};

enum class Unit {
    KilometresPerHour,
    RevolutionsPerMinute,
    DegreesCelsius,
    Percent,
    Volts,
};

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

std::string_view toString(SignalId signalId);
std::string_view toString(Unit unit);
std::string_view toString(SignalStatus status);

} // namespace lexus_head_unit
