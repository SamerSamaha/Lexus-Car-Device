#include "lexus_head_unit/hardware/dbc_database.h"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace lexus_head_unit {

namespace {

constexpr std::uint64_t extendedIdentifierFlag = 0x80000000U;
constexpr std::uint32_t extendedIdentifierMask = 0x1FFFFFFFU;
constexpr std::uint32_t standardIdentifierMaximum = 0x7FFU;
constexpr std::uint32_t maximumMessageLength = 8;
constexpr std::uint32_t maximumSignalLength = 64;
constexpr std::uint32_t bitsPerByte = 8;
constexpr std::int64_t nextByteMostSignificantStep = 15;

std::string trimmed(std::string_view text) {
    const std::string_view whitespace = " \t\r\n";
    const std::size_t first = text.find_first_not_of(whitespace);
    if (first == std::string_view::npos) {
        return {};
    }
    const std::size_t last = text.find_last_not_of(whitespace);
    return std::string(text.substr(first, last - first + 1));
}

bool startsWith(std::string_view text, std::string_view prefix) {
    return text.substr(0, prefix.size()) == prefix;
}

// Locale-independent number parsing: a DBC file always uses a decimal point, whatever the
// process locale says.
template <typename Number>
std::optional<Number> parseNumber(const std::string& text) {
    const std::string clean = trimmed(text);
    Number value{};
    const char* const first = clean.data();
    // from_chars takes a pointer range; the end of the string is the only pointer computed.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const char* const last = first + clean.size();
    const std::from_chars_result result = std::from_chars(first, last, value);
    if (result.ec != std::errc() || result.ptr != last || clean.empty()) {
        return std::nullopt;
    }
    return value;
}

std::string lineError(std::size_t lineNumber, const std::string& message) {
    return "line " + std::to_string(lineNumber) + ": " + message;
}

// Capture groups of the SG_ pattern below, by meaning.
enum SignalGroup : std::uint8_t {
    SignalName = 1,
    MultiplexIndicator = 2,
    StartBit = 3,
    Length = 4,
    ByteOrderDigit = 5,
    SignSymbol = 6,
    Scale = 7,
    Offset = 8,
    Minimum = 9,
    Maximum = 10,
    UnitText = 11,
};

const std::regex& messagePattern() {
    static const std::regex pattern(R"(^BO_\s+(\d+)\s+(\w+)\s*:\s*(\d+)\s+(\w+)$)");
    return pattern;
}

const std::regex& signalPattern() {
    static const std::regex pattern(
        R"dbc(^SG_\s+(\w+)\s*([^:]*?)\s*:\s*(\d+)\|(\d+)@([01])([+-])\s*\(([^,]+),([^)]+)\)\s*\[([^|]+)\|([^\]]+)\]\s*"([^"]*)"\s*(.*)$)dbc");
    return pattern;
}

struct ParsedLine {
    std::optional<DbcMessage> message;
    std::optional<DbcSignal> signal;
    std::string error;
};

ParsedLine parseMessageLine(const std::string& line) {
    std::smatch match;
    if (!std::regex_match(line, match, messagePattern())) {
        return {std::nullopt, std::nullopt, "malformed BO_ line"};
    }
    const std::optional<std::uint64_t> rawIdentifier = parseNumber<std::uint64_t>(match[1].str());
    const std::optional<std::uint32_t> length = parseNumber<std::uint32_t>(match[3].str());
    if (!rawIdentifier.has_value() || !length.has_value()) {
        return {std::nullopt, std::nullopt, "malformed BO_ numbers"};
    }
    DbcMessage message;
    message.extended = (*rawIdentifier & extendedIdentifierFlag) != 0;
    message.identifier = static_cast<std::uint32_t>(*rawIdentifier) & extendedIdentifierMask;
    message.name = match[2].str();
    message.length = *length;
    message.sender = match[4].str();
    if (!message.extended && message.identifier > standardIdentifierMaximum) {
        return {std::nullopt, std::nullopt, "standard identifier above 0x7FF"};
    }
    if (message.length > maximumMessageLength) {
        return {std::nullopt, std::nullopt, "message length above 8 bytes"};
    }
    return {message, std::nullopt, {}};
}

ParsedLine parseSignalLine(const std::string& line) {
    std::smatch match;
    if (!std::regex_match(line, match, signalPattern())) {
        return {std::nullopt, std::nullopt, "malformed SG_ line"};
    }
    if (!trimmed(match[MultiplexIndicator].str()).empty()) {
        return {std::nullopt,
                std::nullopt,
                "multiplexed signal '" + match[SignalName].str() + "' is not supported"};
    }
    DbcSignal signal;
    signal.name = match[SignalName].str();
    const auto startBit = parseNumber<std::uint32_t>(match[StartBit].str());
    const auto length = parseNumber<std::uint32_t>(match[Length].str());
    const auto scale = parseNumber<double>(match[Scale].str());
    const auto offset = parseNumber<double>(match[Offset].str());
    const auto minimum = parseNumber<double>(match[Minimum].str());
    const auto maximum = parseNumber<double>(match[Maximum].str());
    if (!startBit || !length || !scale || !offset || !minimum || !maximum) {
        return {std::nullopt, std::nullopt, "malformed numbers in signal '" + signal.name + "'"};
    }
    signal.startBit = *startBit;
    signal.length = *length;
    signal.byteOrder =
        match[ByteOrderDigit].str() == "1" ? ByteOrder::LittleEndian : ByteOrder::BigEndian;
    signal.isSigned = match[SignSymbol].str() == "-";
    signal.scale = *scale;
    signal.offset = *offset;
    signal.minimum = *minimum;
    signal.maximum = *maximum;
    signal.unit = match[UnitText].str();
    if (signal.length == 0 || signal.length > maximumSignalLength) {
        return {
            std::nullopt, std::nullopt, "signal '" + signal.name + "' length must be 1 to 64 bits"};
    }
    return {std::nullopt, signal, {}};
}

} // namespace

std::vector<std::uint32_t> signalBitPositions(const DbcSignal& signal,
                                              std::uint32_t messageLength) {
    const std::int64_t bitCount = static_cast<std::int64_t>(messageLength) * bitsPerByte;
    std::vector<std::uint32_t> positions;
    positions.reserve(signal.length);
    if (signal.byteOrder == ByteOrder::LittleEndian) {
        for (std::uint32_t index = signal.length; index > 0; --index) {
            const std::int64_t position = static_cast<std::int64_t>(signal.startBit) + index - 1;
            if (position >= bitCount) {
                return {};
            }
            positions.push_back(static_cast<std::uint32_t>(position));
        }
        return positions;
    }
    std::int64_t position = signal.startBit;
    for (std::uint32_t index = 0; index < signal.length; ++index) {
        if (position < 0 || position >= bitCount) {
            return {};
        }
        positions.push_back(static_cast<std::uint32_t>(position));
        position =
            position % bitsPerByte == 0 ? position + nextByteMostSignificantStep : position - 1;
    }
    return positions;
}

DbcDatabase DbcDatabase::parse(std::string_view text) {
    DbcDatabase database;
    std::optional<DbcMessage> current;
    std::size_t currentLine = 0;
    const auto finishMessage = [&database, &current, &currentLine]() {
        if (current.has_value()) {
            database.addMessage(std::move(*current), currentLine);
            current.reset();
        }
    };
    std::istringstream stream{std::string(text)};
    std::string rawLine;
    std::size_t lineNumber = 0;
    while (std::getline(stream, rawLine)) {
        ++lineNumber;
        const std::string line = trimmed(rawLine);
        if (startsWith(line, "SG_ ")) {
            const ParsedLine parsed = parseSignalLine(line);
            if (!current.has_value()) {
                database.m_errors.push_back(lineError(lineNumber, "SG_ line outside a message"));
            } else if (!parsed.signal.has_value()) {
                database.m_errors.push_back(lineError(lineNumber, parsed.error));
            } else if (signalBitPositions(*parsed.signal, current->length).empty()) {
                database.m_errors.push_back(
                    lineError(lineNumber,
                              "signal '" + parsed.signal->name + "' lies outside its message's " +
                                  std::to_string(current->length) + " bytes"));
            } else {
                current->signalList.push_back(*parsed.signal);
            }
            continue;
        }
        finishMessage();
        if (startsWith(line, "BO_ ")) {
            const ParsedLine parsed = parseMessageLine(line);
            if (parsed.message.has_value()) {
                current = parsed.message;
                currentLine = lineNumber;
            } else {
                database.m_errors.push_back(lineError(lineNumber, parsed.error));
            }
        } else if (startsWith(line, "SIG_VALTYPE_")) {
            database.m_errors.push_back(lineError(lineNumber, "float signals are not supported"));
        } else if (startsWith(line, "SG_MUL_VAL_")) {
            database.m_errors.push_back(
                lineError(lineNumber, "extended multiplexing is not supported"));
        }
    }
    finishMessage();
    return database;
}

DbcDatabase DbcDatabase::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        DbcDatabase database;
        database.m_errors.push_back("cannot open " + path);
        return database;
    }
    std::stringstream contents;
    contents << file.rdbuf();
    return parse(contents.str());
}

void DbcDatabase::addMessage(DbcMessage message, std::size_t lineNumber) {
    const std::pair<std::uint32_t, bool> key{message.identifier, message.extended};
    if (m_indexByIdentifier.count(key) != 0) {
        m_errors.push_back(lineError(
            lineNumber, "duplicate message identifier " + std::to_string(message.identifier)));
        return;
    }
    m_indexByIdentifier.emplace(key, m_messages.size());
    m_messages.push_back(std::move(message));
}

const std::vector<DbcMessage>& DbcDatabase::messages() const {
    return m_messages;
}

const DbcMessage* DbcDatabase::find(std::uint32_t identifier, bool extended) const {
    const auto found = m_indexByIdentifier.find({identifier, extended});
    return found == m_indexByIdentifier.end() ? nullptr : &m_messages.at(found->second);
}

const std::vector<std::string>& DbcDatabase::errors() const {
    return m_errors;
}

std::size_t DbcDatabase::signalCount() const {
    std::size_t count = 0;
    for (const DbcMessage& message : m_messages) {
        count += message.signalList.size();
    }
    return count;
}

} // namespace lexus_head_unit
