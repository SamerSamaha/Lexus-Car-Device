#include "lexus_head_unit/hardware/obd_diagnostics.h"

#include "lexus_head_unit/hardware/elm327_protocol.h"
#include "lexus_head_unit/hardware/hex.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lexus_head_unit {

namespace {

constexpr std::uint8_t mode03Response = 0x43;
constexpr std::uint8_t mode09Response = 0x49;
constexpr std::uint8_t vehicleIdentificationPid = 0x02;
constexpr std::size_t vehicleIdentificationLength = 17;
constexpr std::size_t troubleCodeLength = 5;
constexpr std::size_t maximumLengthDigits = 3;
constexpr std::size_t legacyCodesPerFrame = 3;
constexpr unsigned int letterShift = 6;
constexpr unsigned int digitShift = 4;
constexpr unsigned int twoBitMask = 0x03;
constexpr unsigned int nibbleMask = 0x0F;
constexpr int hexBase = 16;
constexpr std::string_view hexDigits = "0123456789ABCDEF";
constexpr std::array<char, 4> codeLetters = {'P', 'C', 'B', 'U'};

struct CodeText {
    std::string_view code;
    std::string_view text;
};

// Generic codes this project explains, in its own words. Every other code gets its category.
constexpr std::array<CodeText, 60> codeTable = {{
    {"P0100", "Mass air flow sensor circuit"},
    {"P0101", "Mass air flow sensor range or performance"},
    {"P0102", "Mass air flow sensor circuit low"},
    {"P0103", "Mass air flow sensor circuit high"},
    {"P0110", "Intake air temperature sensor circuit"},
    {"P0112", "Intake air temperature sensor circuit low"},
    {"P0113", "Intake air temperature sensor circuit high"},
    {"P0115", "Engine coolant temperature sensor circuit"},
    {"P0116", "Engine coolant temperature sensor range or performance"},
    {"P0117", "Engine coolant temperature sensor circuit low"},
    {"P0118", "Engine coolant temperature sensor circuit high"},
    {"P0120", "Throttle position sensor circuit"},
    {"P0121", "Throttle position sensor range or performance"},
    {"P0122", "Throttle position sensor circuit low"},
    {"P0123", "Throttle position sensor circuit high"},
    {"P0128", "Coolant temperature below thermostat regulating temperature"},
    {"P0130", "Oxygen sensor circuit, bank 1 sensor 1"},
    {"P0133", "Oxygen sensor slow response, bank 1 sensor 1"},
    {"P0135", "Oxygen sensor heater circuit, bank 1 sensor 1"},
    {"P0136", "Oxygen sensor circuit, bank 1 sensor 2"},
    {"P0141", "Oxygen sensor heater circuit, bank 1 sensor 2"},
    {"P0150", "Oxygen sensor circuit, bank 2 sensor 1"},
    {"P0155", "Oxygen sensor heater circuit, bank 2 sensor 1"},
    {"P0171", "Fuel system too lean, bank 1"},
    {"P0172", "Fuel system too rich, bank 1"},
    {"P0174", "Fuel system too lean, bank 2"},
    {"P0175", "Fuel system too rich, bank 2"},
    {"P0300", "Random or multiple cylinder misfire detected"},
    {"P0301", "Cylinder 1 misfire detected"},
    {"P0302", "Cylinder 2 misfire detected"},
    {"P0303", "Cylinder 3 misfire detected"},
    {"P0304", "Cylinder 4 misfire detected"},
    {"P0305", "Cylinder 5 misfire detected"},
    {"P0306", "Cylinder 6 misfire detected"},
    {"P0325", "Knock sensor circuit, bank 1"},
    {"P0335", "Crankshaft position sensor circuit"},
    {"P0340", "Camshaft position sensor circuit, bank 1"},
    {"P0400", "Exhaust gas recirculation flow"},
    {"P0401", "Exhaust gas recirculation flow insufficient"},
    {"P0420", "Catalyst efficiency below threshold, bank 1"},
    {"P0430", "Catalyst efficiency below threshold, bank 2"},
    {"P0440", "Evaporative emission system"},
    {"P0441", "Evaporative emission system incorrect purge flow"},
    {"P0442", "Evaporative emission system small leak"},
    {"P0446", "Evaporative emission system vent control circuit"},
    {"P0455", "Evaporative emission system large leak"},
    {"P0456", "Evaporative emission system very small leak"},
    {"P0500", "Vehicle speed sensor"},
    {"P0505", "Idle air control system"},
    {"P0560", "System voltage"},
    {"P0562", "System voltage low"},
    {"P0563", "System voltage high"},
    {"P0600", "Serial communication link"},
    {"P0700", "Transmission control system"},
    {"C0035", "Left front wheel speed sensor circuit"},
    {"B1000", "Electronic control unit malfunction"},
    {"U0100", "Lost communication with the engine control module"},
    {"U0101", "Lost communication with the transmission control module"},
    {"U0121", "Lost communication with the anti-lock brake system module"},
    {"U0140", "Lost communication with the body control module"},
}};

std::optional<std::vector<std::uint8_t>> bytesOfHex(std::string_view text) {
    if (text.empty() || text.size() % 2 != 0 || !isHexText(text)) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> bytes;
    bytes.reserve(text.size() / 2);
    for (std::size_t index = 0; index < text.size(); index += 2) {
        bytes.push_back(hexByte(text.substr(index, 2)).value_or(0));
    }
    return bytes;
}

bool isLengthLine(const std::string& line) {
    return !line.empty() && line.size() <= maximumLengthDigits && isHexText(line);
}

// "0:4306..." gives the bytes after the frame counter; anything else gives nothing.
std::optional<std::vector<std::uint8_t>> frameBytes(const std::string& line) {
    const std::size_t colon = line.find(':');
    if (colon == std::string::npos || colon == 0 || colon > 2 ||
        !isHexText(std::string_view(line).substr(0, colon))) {
        return std::nullopt;
    }
    return bytesOfHex(std::string_view(line).substr(colon + 1));
}

std::vector<std::string> codesFromMessage(const std::vector<std::uint8_t>& message) {
    std::vector<std::string> codes;
    if (message.size() < 2) {
        return codes;
    }
    const std::size_t count = message.at(1);
    // CAN: 43, count, then count pairs. Older protocols: 43, then three pairs padded with 0000.
    const bool canFormat = message.size() == 2 + (2 * count);
    const std::size_t first = canFormat ? 2 : 1;
    for (std::size_t index = first; index + 1 < message.size(); index += 2) {
        const std::uint8_t high = message.at(index);
        const std::uint8_t low = message.at(index + 1);
        if (!canFormat && high == 0 && low == 0) {
            continue;
        }
        codes.push_back(troubleCodeText(high, low));
    }
    if (!canFormat && codes.size() > legacyCodesPerFrame) {
        codes.resize(legacyCodesPerFrame);
    }
    return codes;
}

} // namespace

std::vector<std::vector<std::uint8_t>> obdMessages(const Elm327Reply& reply) {
    std::vector<std::vector<std::uint8_t>> messages;
    const std::vector<std::string>& lines = reply.lines;
    if (lines.size() >= 2 && isLengthLine(lines.front()) && frameBytes(lines.at(1)).has_value()) {
        const auto length = static_cast<std::size_t>(std::stoul(lines.front(), nullptr, hexBase));
        std::vector<std::uint8_t> joined;
        for (std::size_t index = 1; index < lines.size(); ++index) {
            const std::optional<std::vector<std::uint8_t>> bytes = frameBytes(lines.at(index));
            if (bytes.has_value()) {
                joined.insert(joined.end(), bytes->begin(), bytes->end());
            }
        }
        if (joined.size() > length) {
            joined.resize(length);
        }
        messages.push_back(joined);
        return messages;
    }
    for (const std::string& line : lines) {
        std::optional<std::vector<std::uint8_t>> bytes = bytesOfHex(line);
        if (bytes.has_value()) {
            messages.push_back(*bytes);
        }
    }
    return messages;
}

std::string troubleCodeText(std::uint8_t firstByte, std::uint8_t secondByte) {
    const unsigned int first = firstByte;
    const unsigned int second = secondByte;
    std::string code;
    code.push_back(codeLetters.at((first >> letterShift) & twoBitMask));
    code.push_back(hexDigits.at((first >> digitShift) & twoBitMask));
    code.push_back(hexDigits.at(first & nibbleMask));
    code.push_back(hexDigits.at(second >> digitShift));
    code.push_back(hexDigits.at(second & nibbleMask));
    return code;
}

std::optional<std::vector<std::string>> decodeTroubleCodes(const Elm327Reply& reply) {
    if (reply.kind == Elm327ReplyKind::NoData) {
        return std::vector<std::string>{};
    }
    bool sawMode03 = false;
    std::vector<std::string> codes;
    for (const std::vector<std::uint8_t>& message : obdMessages(reply)) {
        if (message.empty() || message.front() != mode03Response) {
            continue;
        }
        sawMode03 = true;
        const std::vector<std::string> fromMessage = codesFromMessage(message);
        codes.insert(codes.end(), fromMessage.begin(), fromMessage.end());
    }
    if (!sawMode03) {
        return std::nullopt;
    }
    return codes;
}

std::optional<std::string> decodeVehicleIdentification(const Elm327Reply& reply) {
    for (const std::vector<std::uint8_t>& message : obdMessages(reply)) {
        if (message.size() < 3 || message.at(0) != mode09Response ||
            message.at(1) != vehicleIdentificationPid) {
            continue;
        }
        std::string identification;
        for (std::size_t index = 3; index < message.size(); ++index) {
            const auto character = static_cast<char>(message.at(index));
            if (std::isalnum(static_cast<unsigned char>(character)) != 0) {
                identification.push_back(character);
            }
        }
        if (identification.size() == vehicleIdentificationLength) {
            return identification;
        }
    }
    return std::nullopt;
}

std::string troubleCodeDescription(const std::string& code) {
    const auto* const found =
        std::find_if(codeTable.begin(), codeTable.end(), [&code](const CodeText& entry) {
            return entry.code == code;
        });
    if (found != codeTable.end()) {
        return std::string(found->text);
    }
    if (code.size() != troubleCodeLength) {
        return "Unknown code";
    }
    std::string system;
    switch (code.front()) {
    case 'P':
        system = "powertrain";
        break;
    case 'C':
        system = "chassis";
        break;
    case 'B':
        system = "body";
        break;
    default:
        system = "network";
        break;
    }
    // SAE J2012: a second character of 0 is generic everywhere, 2 generic for powertrain,
    // 1 and 3 manufacturer-specific (P3 is shared, treated as manufacturer-specific here).
    const char digit = code.at(1);
    const bool generic = digit == '0' || (code.front() == 'P' && digit == '2');
    return (generic ? std::string("Generic ") : std::string("Manufacturer-specific ")) + system +
           " code; no text in this unit's table";
}

} // namespace lexus_head_unit
