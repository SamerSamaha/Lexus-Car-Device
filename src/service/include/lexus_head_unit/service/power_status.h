#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lexus_head_unit {

// The Raspberry Pi firmware's power and thermal flags, as `vcgencmd get_throttled` reports
// them (DN-025, REQ-020). Bits 0 to 3 are the current state; bits 16 to 19 the same four
// conditions since boot. The thermal logger (tools/measure/log_thermal_power.py) reads the
// same bits.
struct PowerFlags {
    std::uint32_t raw = 0;
    bool underVoltage = false;
    bool frequencyCapped = false;
    bool throttled = false;
    bool softTemperatureLimit = false;
    bool underVoltageOccurred = false;
    bool frequencyCappedOccurred = false;
    bool throttledOccurred = false;
    bool softTemperatureLimitOccurred = false;

    [[nodiscard]] bool anyCurrent() const;
    [[nodiscard]] bool anyOccurred() const;
};

PowerFlags powerFlagsFromValue(std::uint32_t value);
// "throttled=0x50005" (with or without a trailing newline). Empty for anything else.
std::optional<PowerFlags> decodeGetThrottled(std::string_view output);
// The current flags as short names, in bit order: "Under-voltage", "Frequency capped",
// "Throttled", "Soft temperature limit".
std::vector<std::string> currentFlagNames(const PowerFlags& flags);

} // namespace lexus_head_unit
