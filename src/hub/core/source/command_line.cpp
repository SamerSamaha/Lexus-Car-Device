#include "lexus_head_unit/hub/command_line.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lexus_head_unit {

namespace {

bool isSeparator(char character) {
    return character == ' ' || character == '\t' || character == '\n' || character == '\r';
}

} // namespace

std::optional<std::vector<std::string>> splitCommandLine(std::string_view text) {
    std::vector<std::string> arguments;
    std::string current;
    bool insideQuotes = false;
    bool wordStarted = false;
    bool escapeNext = false;
    for (const char character : text) {
        if (escapeNext) {
            current += character;
            escapeNext = false;
            continue;
        }
        if (insideQuotes) {
            if (character == '\\') {
                escapeNext = true;
            } else if (character == '"') {
                insideQuotes = false;
            } else {
                current += character;
            }
            continue;
        }
        if (character == '"') {
            insideQuotes = true;
            wordStarted = true;
        } else if (isSeparator(character)) {
            if (wordStarted) {
                arguments.push_back(current);
                current.clear();
                wordStarted = false;
            }
        } else {
            current += character;
            wordStarted = true;
        }
    }
    if (insideQuotes || escapeNext) {
        return std::nullopt;
    }
    if (wordStarted) {
        arguments.push_back(current);
    }
    return arguments;
}

} // namespace lexus_head_unit
