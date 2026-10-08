#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace lexus_head_unit {

enum class ReadStatus {
    Ok,
    Timeout,
    Closed,
    Error,
};

struct ReadResult {
    std::size_t byteCount = 0;
    ReadStatus status = ReadStatus::Ok;
};

class ByteTransport {
public:
    ByteTransport() = default;
    ByteTransport(const ByteTransport&) = delete;
    ByteTransport& operator=(const ByteTransport&) = delete;
    ByteTransport(ByteTransport&&) = delete;
    ByteTransport& operator=(ByteTransport&&) = delete;
    virtual ~ByteTransport() = default;

    virtual bool open() = 0;
    virtual void close() = 0;
    [[nodiscard]] virtual bool isOpen() const = 0;

    virtual std::size_t write(const std::vector<std::uint8_t>& bytes) = 0;
    virtual ReadResult read(std::vector<std::uint8_t>& buffer,
                            std::size_t maxBytes,
                            std::int64_t timeoutMilliseconds) = 0;
};

} // namespace lexus_head_unit
