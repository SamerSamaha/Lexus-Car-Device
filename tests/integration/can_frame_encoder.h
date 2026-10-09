#pragma once

// Test-only: builds a CAN frame that carries chosen physical values, the inverse of DbcDecoder.
// The product has no encoder and no way to send (REQ-001); tests need frames to feed the reader.

#include "lexus_head_unit/hardware/can_frame.h"
#include "lexus_head_unit/hardware/dbc_database.h"

#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace lexus_head_unit::testing {

// Writes the raw value of one signal into data, most significant bit first along the signal's
// bit walk (the same order the decoder reads).
inline void encodeRawValue(CanFrame& frame,
                           const DbcSignal& signal,
                           std::uint32_t messageLength,
                           std::int64_t rawValue) {
    const std::vector<std::uint32_t> positions = signalBitPositions(signal, messageLength);
    const auto raw = static_cast<std::uint64_t>(rawValue);
    for (std::size_t index = 0; index < positions.size(); ++index) {
        const std::uint32_t position = positions.at(index);
        const std::size_t shift = positions.size() - 1 - index;
        const bool bit = ((raw >> shift) & 1U) != 0;
        auto& byte = frame.data.at(position / 8);
        const auto mask = static_cast<std::uint8_t>(1U << (position % 8));
        byte = static_cast<std::uint8_t>(bit ? (byte | mask) : (byte & ~mask));
    }
}

// A frame of the message with the given physical values (signals not named are 0 raw).
inline CanFrame encodeMessage(const DbcMessage& message,
                              const std::map<std::string, double>& values) {
    CanFrame frame;
    frame.identifier = message.identifier;
    frame.extended = message.extended;
    frame.length = static_cast<std::uint8_t>(message.length);
    for (const DbcSignal& signal : message.signalList) {
        const auto found = values.find(signal.name);
        if (found == values.end()) {
            continue;
        }
        const auto raw =
            static_cast<std::int64_t>(std::llround((found->second - signal.offset) / signal.scale));
        encodeRawValue(frame, signal, message.length, raw);
    }
    return frame;
}

// The message that carries a signal of that name, or null.
inline const DbcMessage* messageCarrying(const DbcDatabase& database,
                                         const std::string& signalName) {
    for (const DbcMessage& message : database.messages()) {
        for (const DbcSignal& signal : message.signalList) {
            if (signal.name == signalName) {
                return &message;
            }
        }
    }
    return nullptr;
}

} // namespace lexus_head_unit::testing
