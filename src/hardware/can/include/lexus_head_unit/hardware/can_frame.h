#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace lexus_head_unit {

constexpr std::size_t canMaximumDataLength = 8;

// One classic CAN frame as a reader delivers it. length above 8 is not a valid classic frame
// and is rejected by the decoder.
struct CanFrame {
    std::uint32_t identifier = 0;
    bool extended = false;
    std::uint8_t length = 0;
    std::array<std::uint8_t, canMaximumDataLength> data{};
    std::int64_t timestampMilliseconds = 0;
};

} // namespace lexus_head_unit
