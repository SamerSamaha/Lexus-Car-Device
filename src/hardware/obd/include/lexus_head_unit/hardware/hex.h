#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lexus_head_unit {

[[nodiscard]] bool isHexText(std::string_view text);
[[nodiscard]] std::optional<std::uint8_t> hexByte(std::string_view twoCharacters);
[[nodiscard]] std::optional<std::vector<std::uint8_t>> hexToBytes(std::string_view text);
[[nodiscard]] std::string hexText(std::uint8_t byte);
[[nodiscard]] std::string hexText(const std::vector<std::uint8_t>& bytes);

} // namespace lexus_head_unit
