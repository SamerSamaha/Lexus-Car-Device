#pragma once

#include "lexus_head_unit/hardware/can_frame.h"

#include <cstdint>
#include <string>

namespace lexus_head_unit {

enum class CanReadStatus {
    Frame,
    Timeout,
    Closed,
    Error,
};

struct CanReadResult {
    CanReadStatus status = CanReadStatus::Timeout;
    CanFrame frame;
};

// Delivers raw CAN frames (DN-028). Deliberately has no write, send or transmit member: the CAN
// path only listens (REQ-001). A compile-time test fails if one is ever added.
class CanFrameReader {
public:
    CanFrameReader() = default;
    CanFrameReader(const CanFrameReader&) = delete;
    CanFrameReader& operator=(const CanFrameReader&) = delete;
    CanFrameReader(CanFrameReader&&) = delete;
    CanFrameReader& operator=(CanFrameReader&&) = delete;
    virtual ~CanFrameReader() = default;

    virtual bool open() = 0;
    // Waits at most timeoutMilliseconds for one frame; 0 does not wait.
    virtual CanReadResult read(std::int64_t timeoutMilliseconds) = 0;
    virtual void close() = 0;
    [[nodiscard]] virtual bool isOpen() const = 0;
    // The last failure, for logs.
    [[nodiscard]] virtual std::string lastError() const = 0;
};

} // namespace lexus_head_unit
