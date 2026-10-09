#include "lexus_head_unit/hardware/dbc_decoder.h"

#include "lexus_head_unit/hardware/can_frame.h"
#include "lexus_head_unit/hardware/dbc_database.h"

#include <array>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace lexus_head_unit {

namespace {

constexpr std::uint32_t bitsPerByte = 8;
constexpr std::uint32_t fullWidth = 64;
constexpr std::uint32_t nextByteMostSignificantStep = 15;

bool bitAt(const std::array<std::uint8_t, canMaximumDataLength>& data, std::uint32_t position) {
    const std::uint8_t byte = data.at(position / bitsPerByte);
    return ((static_cast<unsigned int>(byte) >> (position % bitsPerByte)) & 1U) != 0;
}

} // namespace

std::int64_t extractRawValue(const std::array<std::uint8_t, canMaximumDataLength>& data,
                             const DbcSignal& signal) {
    // The bits are visited most significant first for both byte orders (the same walk as
    // signalBitPositions, without allocating), so the value is built by shifting left.
    std::uint64_t raw = 0;
    if (signal.byteOrder == ByteOrder::LittleEndian) {
        for (std::uint32_t index = signal.length; index > 0; --index) {
            raw = (raw << 1U) | (bitAt(data, signal.startBit + index - 1) ? 1U : 0U);
        }
    } else {
        std::uint32_t position = signal.startBit;
        for (std::uint32_t index = 0; index < signal.length; ++index) {
            raw = (raw << 1U) | (bitAt(data, position) ? 1U : 0U);
            position =
                position % bitsPerByte == 0 ? position + nextByteMostSignificantStep : position - 1;
        }
    }
    if (!signal.isSigned || signal.length == 0) {
        return static_cast<std::int64_t>(raw);
    }
    if (signal.length < fullWidth) {
        const std::uint64_t signBit = std::uint64_t{1} << (signal.length - 1);
        if ((raw & signBit) != 0) {
            raw |= ~((signBit << 1U) - 1U);
        }
    }
    return static_cast<std::int64_t>(raw);
}

double physicalValue(const DbcSignal& signal, std::int64_t rawValue) {
    return (static_cast<double>(rawValue) * signal.scale) + signal.offset;
}

DbcDecoder::DbcDecoder(DbcDatabase database) : m_database(std::move(database)) {}

DecodeResult DbcDecoder::decode(const CanFrame& frame) {
    DecodeResult result;
    if (frame.length > canMaximumDataLength) {
        ++m_counters.invalidFrame;
        result.kind = DecodeKind::InvalidFrame;
        return result;
    }
    const DbcMessage* message = m_database.find(frame.identifier, frame.extended);
    if (message == nullptr) {
        ++m_counters.unknownIdentifier;
        result.kind = DecodeKind::UnknownIdentifier;
        return result;
    }
    result.messageName = message->name;
    if (frame.length != message->length) {
        ++m_counters.wrongLength;
        result.kind = DecodeKind::WrongLength;
        return result;
    }
    result.kind = DecodeKind::Decoded;
    result.values.reserve(message->signalList.size());
    for (const DbcSignal& signal : message->signalList) {
        result.values.push_back(DecodedSignal{
            signal.name, physicalValue(signal, extractRawValue(frame.data, signal)), signal.unit});
    }
    ++m_counters.decoded;
    return result;
}

const DbcDatabase& DbcDecoder::database() const {
    return m_database;
}

DecoderCounters DbcDecoder::counters() const {
    return m_counters;
}

std::string_view toString(DecodeKind kind) {
    switch (kind) {
    case DecodeKind::Decoded:
        return "decoded";
    case DecodeKind::UnknownIdentifier:
        return "unknown-identifier";
    case DecodeKind::WrongLength:
        return "wrong-length";
    case DecodeKind::InvalidFrame:
        return "invalid-frame";
    }
    return "unknown";
}

} // namespace lexus_head_unit
