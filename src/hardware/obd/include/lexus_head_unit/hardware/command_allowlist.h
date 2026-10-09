#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace lexus_head_unit {

enum class AllowlistDecision {
    Allowed,
    RefusedMode,
    RefusedUnknownAtCommand,
    RefusedMalformed,
};

constexpr std::size_t elm327SetupCommandCount = 9;

// Base ELM327 commands only; no adapter-specific "ST" commands, so the adapter stays swappable.
constexpr std::array<std::string_view, elm327SetupCommandCount> elm327SetupCommands = {
    "ATZ",
    "ATE0",
    "ATL0",
    "ATS0",
    "ATH0",
    "ATSP0",
    "ATI",
    "ATRV",
    "ATDPN",
};

constexpr std::uint8_t obdModeCurrentData = 0x01;
constexpr std::uint8_t obdModeStoredTroubleCodes = 0x03;
constexpr std::uint8_t obdModeVehicleInformation = 0x09;

class CommandAllowlist {
public:
    [[nodiscard]] static AllowlistDecision decide(std::string_view command);
    [[nodiscard]] static bool isAllowed(std::string_view command);
    [[nodiscard]] static bool isAllowedMode(std::uint8_t mode);
    [[nodiscard]] static std::optional<std::string> obdRequest(std::uint8_t mode,
                                                               std::optional<std::uint8_t> pid);
};

std::string_view toString(AllowlistDecision decision);

} // namespace lexus_head_unit
