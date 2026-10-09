#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lexus_head_unit {

// Byte order as written in a DBC signal: @1 is little endian (Intel), @0 is big endian
// (Motorola).
enum class ByteOrder {
    LittleEndian,
    BigEndian,
};

// One signal of a DBC message (DN-027). Bit n is bit n % 8 of byte n / 8. For LittleEndian the
// start bit is the least significant bit; for BigEndian it is the most significant bit.
struct DbcSignal {
    std::string name;
    std::uint32_t startBit = 0;
    std::uint32_t length = 0;
    ByteOrder byteOrder = ByteOrder::LittleEndian;
    bool isSigned = false;
    double scale = 1.0;
    double offset = 0.0;
    double minimum = 0.0;
    double maximum = 0.0;
    std::string unit;
};

struct DbcMessage {
    std::uint32_t identifier = 0;
    bool extended = false;
    std::string name;
    std::uint32_t length = 0;
    std::string sender;
    std::vector<DbcSignal> signalList;
};

// The messages of a DBC file. Only BO_ and SG_ lines are read; every other keyword is skipped.
// Multiplexed signals and float signals are reported as errors rather than decoded wrongly.
class DbcDatabase {
public:
    static DbcDatabase parse(std::string_view text);
    // A database with one error when the file cannot be opened.
    static DbcDatabase loadFromFile(const std::string& path);

    [[nodiscard]] const std::vector<DbcMessage>& messages() const;
    [[nodiscard]] const DbcMessage* find(std::uint32_t identifier, bool extended) const;
    // "line 12: ..." texts; empty when the text parsed cleanly.
    [[nodiscard]] const std::vector<std::string>& errors() const;
    [[nodiscard]] std::size_t signalCount() const;

private:
    void addMessage(DbcMessage message, std::size_t lineNumber);

    std::vector<DbcMessage> m_messages;
    std::map<std::pair<std::uint32_t, bool>, std::size_t> m_indexByIdentifier;
    std::vector<std::string> m_errors;
};

// The bit positions a signal occupies, most significant first; empty if any lies outside a
// message of messageLength bytes.
std::vector<std::uint32_t> signalBitPositions(const DbcSignal& signal, std::uint32_t messageLength);

} // namespace lexus_head_unit
