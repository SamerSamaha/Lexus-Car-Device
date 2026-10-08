#include "lexus_head_unit/hardware/command_allowlist.h"

#include "lexus_head_unit/hardware/hex.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace lexus_head_unit {

namespace {

constexpr std::size_t modeOnlyLength = 2;
constexpr std::size_t modeAndPidLength = 4;

std::string upperCased(std::string_view text) {
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return result;
}

bool isSetupCommand(const std::string& command) {
    return std::find(elm327SetupCommands.begin(), elm327SetupCommands.end(), command) !=
           elm327SetupCommands.end();
}

} // namespace

AllowlistDecision CommandAllowlist::decide(std::string_view command) {
    const std::string normalised = upperCased(command);
    if (normalised.empty()) {
        return AllowlistDecision::RefusedMalformed;
    }
    if (isSetupCommand(normalised)) {
        return AllowlistDecision::Allowed;
    }
    if (normalised.rfind("AT", 0) == 0 || normalised.rfind("ST", 0) == 0) {
        return AllowlistDecision::RefusedUnknownAtCommand;
    }
    if (!isHexText(normalised)) {
        return AllowlistDecision::RefusedMalformed;
    }
    if (normalised.size() != modeOnlyLength && normalised.size() != modeAndPidLength) {
        return AllowlistDecision::RefusedMalformed;
    }
    const std::optional<std::uint8_t> mode = hexByte(normalised.substr(0, modeOnlyLength));
    if (!mode.has_value()) {
        return AllowlistDecision::RefusedMalformed;
    }
    const bool hasPid = normalised.size() == modeAndPidLength;
    if (*mode == obdModeStoredTroubleCodes) {
        return hasPid ? AllowlistDecision::RefusedMalformed : AllowlistDecision::Allowed;
    }
    if (*mode == obdModeCurrentData || *mode == obdModeVehicleInformation) {
        return hasPid ? AllowlistDecision::Allowed : AllowlistDecision::RefusedMalformed;
    }
    return AllowlistDecision::RefusedMode;
}

bool CommandAllowlist::isAllowed(std::string_view command) {
    return decide(command) == AllowlistDecision::Allowed;
}

bool CommandAllowlist::isAllowedMode(std::uint8_t mode) {
    return mode == obdModeCurrentData || mode == obdModeStoredTroubleCodes ||
           mode == obdModeVehicleInformation;
}

std::optional<std::string> CommandAllowlist::obdRequest(std::uint8_t mode,
                                                        std::optional<std::uint8_t> pid) {
    if (!isAllowedMode(mode)) {
        return std::nullopt;
    }
    if (mode == obdModeStoredTroubleCodes) {
        return pid.has_value() ? std::nullopt : std::optional<std::string>(hexText(mode));
    }
    if (!pid.has_value()) {
        return std::nullopt;
    }
    return hexText(mode) + hexText(*pid);
}

std::string_view toString(AllowlistDecision decision) {
    switch (decision) {
    case AllowlistDecision::Allowed:
        return "Allowed";
    case AllowlistDecision::RefusedMode:
        return "RefusedMode";
    case AllowlistDecision::RefusedUnknownAtCommand:
        return "RefusedUnknownAtCommand";
    case AllowlistDecision::RefusedMalformed:
        return "RefusedMalformed";
    }
    return "UnknownDecision";
}

} // namespace lexus_head_unit
