#pragma once

#include "lexus_head_unit/hardware/can_frame.h"
#include "lexus_head_unit/hardware/dbc_database.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace lexus_head_unit {

enum class DecodeKind {
    Decoded,
    UnknownIdentifier,
    WrongLength,
    InvalidFrame,
};

struct DecodedSignal {
    std::string name;
    double value = 0.0;
    std::string unit;
};

struct DecodeResult {
    DecodeKind kind = DecodeKind::InvalidFrame;
    std::string messageName;
    std::vector<DecodedSignal> values;
};

struct DecoderCounters {
    std::uint64_t decoded = 0;
    std::uint64_t unknownIdentifier = 0;
    std::uint64_t wrongLength = 0;
    std::uint64_t invalidFrame = 0;
};

// The raw value of a signal in up to 8 data bytes, sign-extended when the signal is signed.
// The caller guarantees the signal fits the data (DbcDatabase checks this when parsing).
std::int64_t extractRawValue(const std::array<std::uint8_t, canMaximumDataLength>& data,
                             const DbcSignal& signal);
double physicalValue(const DbcSignal& signal, std::int64_t rawValue);

// Decodes frames against a DBC database (REQ-005). A frame whose length differs from its
// message's is a counted error and yields no value (REQ-010). Not thread-safe; one per reader.
class DbcDecoder {
public:
    explicit DbcDecoder(DbcDatabase database);

    DecodeResult decode(const CanFrame& frame);
    [[nodiscard]] const DbcDatabase& database() const;
    [[nodiscard]] DecoderCounters counters() const;

private:
    DbcDatabase m_database;
    DecoderCounters m_counters;
};

std::string_view toString(DecodeKind kind);

} // namespace lexus_head_unit
