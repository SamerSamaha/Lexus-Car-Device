#pragma once

#include "lexus_head_unit/service/signal_id.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace lexus_head_unit {

enum class ObdPid : std::uint8_t {
    SupportedPids00 = 0x00,
    EngineLoad = 0x04,
    CoolantTemperature = 0x05,
    EngineRpm = 0x0C,
    VehicleSpeed = 0x0D,
    IntakeAirTemperature = 0x0F,
    ThrottlePosition = 0x11,
    SupportedPids20 = 0x20,
    FuelLevel = 0x2F,
    SupportedPids40 = 0x40,
    ControlModuleVoltage = 0x42,
};

constexpr std::uint8_t pidByte(ObdPid pid) {
    return static_cast<std::uint8_t>(pid);
}

struct DecodedPid {
    SignalId signalId = SignalId::VehicleSpeed;
    double value = 0.0;
    Unit unit = Unit::KilometresPerHour;
};

ObdPid pidForSignal(SignalId signalId);
std::optional<SignalId> signalForPid(std::uint8_t pid);
std::optional<std::size_t> expectedDataByteCount(std::uint8_t pid);
std::optional<DecodedPid> decodePid(std::uint8_t pid, const std::vector<std::uint8_t>& dataBytes);

} // namespace lexus_head_unit
