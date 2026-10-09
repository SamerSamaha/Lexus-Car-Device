#include "lexus_head_unit/service/power_status.h"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace lexus_head_unit {

namespace {

constexpr std::string_view throttledPrefix = "throttled=0x";
constexpr int hexBase = 16;
constexpr std::uint32_t underVoltageBit = 0;
constexpr std::uint32_t frequencyCappedBit = 1;
constexpr std::uint32_t throttledBit = 2;
constexpr std::uint32_t softTemperatureLimitBit = 3;
constexpr std::uint32_t sinceBootOffset = 16;

bool bitSet(std::uint32_t value, std::uint32_t bit) {
    return ((value >> bit) & 1U) != 0;
}

std::string_view trimmed(std::string_view text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

} // namespace

bool PowerFlags::anyCurrent() const {
    return underVoltage || frequencyCapped || throttled || softTemperatureLimit;
}

bool PowerFlags::anyOccurred() const {
    return underVoltageOccurred || frequencyCappedOccurred || throttledOccurred ||
           softTemperatureLimitOccurred;
}

PowerFlags powerFlagsFromValue(std::uint32_t value) {
    PowerFlags flags;
    flags.raw = value;
    flags.underVoltage = bitSet(value, underVoltageBit);
    flags.frequencyCapped = bitSet(value, frequencyCappedBit);
    flags.throttled = bitSet(value, throttledBit);
    flags.softTemperatureLimit = bitSet(value, softTemperatureLimitBit);
    flags.underVoltageOccurred = bitSet(value, underVoltageBit + sinceBootOffset);
    flags.frequencyCappedOccurred = bitSet(value, frequencyCappedBit + sinceBootOffset);
    flags.throttledOccurred = bitSet(value, throttledBit + sinceBootOffset);
    flags.softTemperatureLimitOccurred = bitSet(value, softTemperatureLimitBit + sinceBootOffset);
    return flags;
}

std::optional<PowerFlags> decodeGetThrottled(std::string_view output) {
    const std::string_view text = trimmed(output);
    if (text.substr(0, throttledPrefix.size()) != throttledPrefix) {
        return std::nullopt;
    }
    const std::string_view digits = text.substr(throttledPrefix.size());
    std::uint32_t value = 0;
    const std::from_chars_result parsed =
        std::from_chars(digits.data(), digits.data() + digits.size(), value, hexBase);
    if (digits.empty() || parsed.ec != std::errc() || parsed.ptr != digits.data() + digits.size()) {
        return std::nullopt;
    }
    return powerFlagsFromValue(value);
}

std::vector<std::string> currentFlagNames(const PowerFlags& flags) {
    std::vector<std::string> names;
    if (flags.underVoltage) {
        names.emplace_back("Under-voltage");
    }
    if (flags.frequencyCapped) {
        names.emplace_back("Frequency capped");
    }
    if (flags.throttled) {
        names.emplace_back("Throttled");
    }
    if (flags.softTemperatureLimit) {
        names.emplace_back("Soft temperature limit");
    }
    return names;
}

} // namespace lexus_head_unit
