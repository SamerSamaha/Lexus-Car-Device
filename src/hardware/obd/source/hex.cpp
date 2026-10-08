#include "lexus_head_unit/hardware/hex.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lexus_head_unit {

namespace {

constexpr std::string_view hexDigits = "0123456789ABCDEF";
constexpr std::uint8_t nibbleBits = 4;
constexpr std::uint8_t lowNibbleMask = 0x0F;
constexpr std::uint8_t letterDigitOffset = 10;

std::optional<std::uint8_t> nibbleValue(char character) {
    if (character >= '0' && character <= '9') {
        return static_cast<std::uint8_t>(character - '0');
    }
    if (character >= 'A' && character <= 'F') {
        return static_cast<std::uint8_t>(character - 'A' + letterDigitOffset);
    }
    if (character >= 'a' && character <= 'f') {
        return static_cast<std::uint8_t>(character - 'a' + letterDigitOffset);
    }
    return std::nullopt;
}

} // namespace

bool isHexText(std::string_view text) {
    if (text.empty()) {
        return false;
    }
    return std::all_of(text.begin(), text.end(), [](char character) {
        return nibbleValue(character).has_value();
    });
}

std::optional<std::uint8_t> hexByte(std::string_view twoCharacters) {
    if (twoCharacters.size() != 2) {
        return std::nullopt;
    }
    const std::optional<std::uint8_t> high = nibbleValue(twoCharacters[0]);
    const std::optional<std::uint8_t> low = nibbleValue(twoCharacters[1]);
    if (!high.has_value() || !low.has_value()) {
        return std::nullopt;
    }
    return static_cast<std::uint8_t>((*high << nibbleBits) | *low);
}

std::optional<std::vector<std::uint8_t>> hexToBytes(std::string_view text) {
    if (text.size() % 2 != 0) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> bytes;
    bytes.reserve(text.size() / 2);
    for (std::size_t index = 0; index + 1 < text.size(); index += 2) {
        const std::optional<std::uint8_t> byte = hexByte(text.substr(index, 2));
        if (!byte.has_value()) {
            return std::nullopt;
        }
        bytes.push_back(*byte);
    }
    return bytes;
}

std::string hexText(std::uint8_t byte) {
    std::string text;
    text.push_back(hexDigits[static_cast<std::size_t>(byte >> nibbleBits)]);
    text.push_back(hexDigits[static_cast<std::size_t>(byte & lowNibbleMask)]);
    return text;
}

std::string hexText(const std::vector<std::uint8_t>& bytes) {
    std::string text;
    for (const std::uint8_t byte : bytes) {
        text += hexText(byte);
    }
    return text;
}

} // namespace lexus_head_unit
