#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lexus_head_unit {

// Splits a command line into an argument vector without a shell: whitespace separates words,
// double quotes group words, and inside quotes a backslash takes the next character literally.
// Returns nothing when a quote is not closed or a backslash ends the text inside quotes.
std::optional<std::vector<std::string>> splitCommandLine(std::string_view text);

} // namespace lexus_head_unit
