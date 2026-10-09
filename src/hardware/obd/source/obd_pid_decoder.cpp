#include "lexus_head_unit/hardware/obd_pid_decoder.h"

#include "lexus_head_unit/service/signal_id.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace lexus_head_unit {

namespace {

constexpr double percentPerRawUnit = 100.0 / 255.0;
constexpr double temperatureOffsetDegreesCelsius = 40.0;
constexpr double rpmDivisor = 4.0;
constexpr double millivoltsPerVolt = 1000.0;
constexpr double highByteWeight = 256.0;
constexpr double massAirFlowDivisor = 100.0;

double twoByteValue(const std::vector<std::uint8_t>& dataBytes) {
    return (highByteWeight * dataBytes.at(0)) + dataBytes.at(1);
}

} // namespace

std::optional<ObdPid> pidForSignal(SignalId signalId) {
    switch (signalId) {
    case SignalId::VehicleSpeed:
        return ObdPid::VehicleSpeed;
    case SignalId::EngineRpm:
        return ObdPid::EngineRpm;
    case SignalId::CoolantTemperature:
        return ObdPid::CoolantTemperature;
    case SignalId::EngineLoad:
        return ObdPid::EngineLoad;
    case SignalId::ThrottlePosition:
        return ObdPid::ThrottlePosition;
    case SignalId::IntakeAirTemperature:
        return ObdPid::IntakeAirTemperature;
    case SignalId::ControlModuleVoltage:
        return ObdPid::ControlModuleVoltage;
    case SignalId::FuelLevel:
        return ObdPid::FuelLevel;
    case SignalId::MassAirFlow:
        return ObdPid::MassAirFlow;
    default:
        return std::nullopt;
    }
}

std::optional<SignalId> signalForPid(std::uint8_t pid) {
    switch (pid) {
    case pidByte(ObdPid::VehicleSpeed):
        return SignalId::VehicleSpeed;
    case pidByte(ObdPid::EngineRpm):
        return SignalId::EngineRpm;
    case pidByte(ObdPid::CoolantTemperature):
        return SignalId::CoolantTemperature;
    case pidByte(ObdPid::EngineLoad):
        return SignalId::EngineLoad;
    case pidByte(ObdPid::ThrottlePosition):
        return SignalId::ThrottlePosition;
    case pidByte(ObdPid::IntakeAirTemperature):
        return SignalId::IntakeAirTemperature;
    case pidByte(ObdPid::ControlModuleVoltage):
        return SignalId::ControlModuleVoltage;
    case pidByte(ObdPid::FuelLevel):
        return SignalId::FuelLevel;
    case pidByte(ObdPid::MassAirFlow):
        return SignalId::MassAirFlow;
    default:
        return std::nullopt;
    }
}

std::optional<std::size_t> expectedDataByteCount(std::uint8_t pid) {
    switch (pid) {
    case pidByte(ObdPid::SupportedPids00):
    case pidByte(ObdPid::SupportedPids20):
    case pidByte(ObdPid::SupportedPids40):
        return 4;
    case pidByte(ObdPid::EngineRpm):
    case pidByte(ObdPid::ControlModuleVoltage):
    case pidByte(ObdPid::MassAirFlow):
        return 2;
    case pidByte(ObdPid::VehicleSpeed):
    case pidByte(ObdPid::CoolantTemperature):
    case pidByte(ObdPid::EngineLoad):
    case pidByte(ObdPid::ThrottlePosition):
    case pidByte(ObdPid::IntakeAirTemperature):
    case pidByte(ObdPid::FuelLevel):
        return 1;
    default:
        return std::nullopt;
    }
}

std::optional<DecodedPid> decodePid(std::uint8_t pid, const std::vector<std::uint8_t>& dataBytes) {
    const std::optional<SignalId> signalId = signalForPid(pid);
    const std::optional<std::size_t> expectedCount = expectedDataByteCount(pid);
    if (!signalId.has_value() || !expectedCount.has_value() || dataBytes.size() != *expectedCount) {
        return std::nullopt;
    }
    DecodedPid decoded;
    decoded.signalId = *signalId;
    const double firstByte = dataBytes.at(0);
    switch (*signalId) {
    case SignalId::VehicleSpeed:
        decoded.value = firstByte;
        decoded.unit = Unit::KilometresPerHour;
        break;
    case SignalId::EngineRpm:
        decoded.value = twoByteValue(dataBytes) / rpmDivisor;
        decoded.unit = Unit::RevolutionsPerMinute;
        break;
    case SignalId::CoolantTemperature:
    case SignalId::IntakeAirTemperature:
        decoded.value = firstByte - temperatureOffsetDegreesCelsius;
        decoded.unit = Unit::DegreesCelsius;
        break;
    case SignalId::EngineLoad:
    case SignalId::ThrottlePosition:
    case SignalId::FuelLevel:
        decoded.value = firstByte * percentPerRawUnit;
        decoded.unit = Unit::Percent;
        break;
    case SignalId::ControlModuleVoltage:
        decoded.value = twoByteValue(dataBytes) / millivoltsPerVolt;
        decoded.unit = Unit::Volts;
        break;
    case SignalId::MassAirFlow:
        decoded.value = twoByteValue(dataBytes) / massAirFlowDivisor;
        decoded.unit = Unit::GramsPerSecond;
        break;
    default:
        return std::nullopt;
    }
    return decoded;
}

} // namespace lexus_head_unit
