#pragma once

#include "lexus_head_unit/hardware/elm327_protocol.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace lexus_head_unit {

// The messages in an adapter reply (DN-030): a multi-frame reply (a length line, then "0:",
// "1:" ... lines) is joined and cut to its length; otherwise every line is one message, one
// per answering ECU. Spaces are optional. Lines that are not hexadecimal are skipped.
std::vector<std::vector<std::uint8_t>> obdMessages(const Elm327Reply& reply);

// A trouble code from its two bytes (SAE J2012): letter, digit 0 to 3, three hex digits.
std::string troubleCodeText(std::uint8_t firstByte, std::uint8_t secondByte);

// The stored trouble codes of a Mode 03 reply, in reply order, from every answering ECU.
// NO DATA is zero codes. Empty optional when the reply carries no Mode 03 message.
std::optional<std::vector<std::string>> decodeTroubleCodes(const Elm327Reply& reply);

// The vehicle identification number of a Mode 09 PID 02 reply: 17 characters, or nothing.
std::optional<std::string> decodeVehicleIdentification(const Elm327Reply& reply);

// The text of a code: from the table of generic codes the project explains, else its category.
std::string troubleCodeDescription(const std::string& code);

} // namespace lexus_head_unit
