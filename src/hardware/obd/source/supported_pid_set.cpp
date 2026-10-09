#include "lexus_head_unit/hardware/supported_pid_set.h"

#include "lexus_head_unit/hardware/obd_pid_decoder.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace lexus_head_unit {

namespace {

constexpr std::size_t bitsPerByte = 8;
constexpr std::size_t bitsPerBitmap = supportedPidBitmapByteCount * bitsPerByte;
constexpr std::uint8_t highestBitmapBase = 0xE0;

constexpr std::array<ObdPid, 9> pollOrder = {
    ObdPid::VehicleSpeed,
    ObdPid::EngineRpm,
    ObdPid::CoolantTemperature,
    ObdPid::EngineLoad,
    ObdPid::ThrottlePosition,
    ObdPid::IntakeAirTemperature,
    ObdPid::ControlModuleVoltage,
    ObdPid::FuelLevel,
    ObdPid::MassAirFlow,
};

bool isBitmapBase(std::uint8_t basePid) {
    return basePid % supportedPidBitmapSpan == 0 && basePid <= highestBitmapBase;
}

} // namespace

bool SupportedPidSet::contains(std::uint8_t pid) const {
    return m_supported.at(pid);
}

void SupportedPidSet::insert(std::uint8_t pid) {
    m_supported.at(pid) = true;
}

std::size_t SupportedPidSet::count() const {
    std::size_t total = 0;
    for (const bool flag : m_supported) {
        if (flag) {
            ++total;
        }
    }
    return total;
}

bool SupportedPidSet::addBitmap(std::uint8_t basePid,
                                const std::vector<std::uint8_t>& bitmapBytes) {
    if (!isBitmapBase(basePid) || bitmapBytes.size() != supportedPidBitmapByteCount) {
        return false;
    }
    for (std::size_t bitIndex = 0; bitIndex < bitsPerBitmap; ++bitIndex) {
        const std::uint8_t byteValue = bitmapBytes.at(bitIndex / bitsPerByte);
        const std::size_t shift = (bitsPerByte - 1) - (bitIndex % bitsPerByte);
        const bool isSet = ((byteValue >> shift) & 1U) != 0U;
        if (isSet) {
            const std::size_t pid = static_cast<std::size_t>(basePid) + bitIndex + 1;
            m_supported.at(pid) = true;
        }
    }
    return true;
}

std::optional<std::uint8_t> SupportedPidSet::nextBitmapPid(std::uint8_t basePid) const {
    if (!isBitmapBase(basePid) || basePid >= highestBitmapBase) {
        return std::nullopt;
    }
    const auto nextBase = static_cast<std::uint8_t>(basePid + supportedPidBitmapSpan);
    if (contains(nextBase)) {
        return nextBase;
    }
    return std::nullopt;
}

std::vector<ObdPid> pollablePids(const SupportedPidSet& supported) {
    std::vector<ObdPid> pollable;
    for (const ObdPid pid : pollOrder) {
        if (supported.contains(pidByte(pid))) {
            pollable.push_back(pid);
        }
    }
    return pollable;
}

} // namespace lexus_head_unit
