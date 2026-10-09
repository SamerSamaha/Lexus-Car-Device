#pragma once

#include "lexus_head_unit/hardware/obd_pid_decoder.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace lexus_head_unit {

constexpr std::size_t supportedPidBitmapByteCount = 4;
constexpr std::uint8_t supportedPidBitmapSpan = 0x20;

class SupportedPidSet {
public:
    [[nodiscard]] bool contains(std::uint8_t pid) const;
    void insert(std::uint8_t pid);
    [[nodiscard]] std::size_t count() const;

    bool addBitmap(std::uint8_t basePid, const std::vector<std::uint8_t>& bitmapBytes);
    [[nodiscard]] std::optional<std::uint8_t> nextBitmapPid(std::uint8_t basePid) const;

private:
    std::array<bool, 256> m_supported{};
};

std::vector<ObdPid> pollablePids(const SupportedPidSet& supported);

} // namespace lexus_head_unit
