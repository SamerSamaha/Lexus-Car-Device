#pragma once

#include "lexus_head_unit/hardware/can_frame_reader.h"

#include <cstdint>
#include <string>

namespace lexus_head_unit {

// CanFrameReader over a Linux SocketCAN raw socket bound to one interface (vcan0 on the Pi).
// Receives only: there is no member that writes to the socket (REQ-001, DN-028).
class SocketCanFrameReader final : public CanFrameReader {
public:
    explicit SocketCanFrameReader(std::string interfaceName);
    SocketCanFrameReader(const SocketCanFrameReader&) = delete;
    SocketCanFrameReader& operator=(const SocketCanFrameReader&) = delete;
    SocketCanFrameReader(SocketCanFrameReader&&) = delete;
    SocketCanFrameReader& operator=(SocketCanFrameReader&&) = delete;
    ~SocketCanFrameReader() override;

    bool open() override;
    CanReadResult read(std::int64_t timeoutMilliseconds) override;
    void close() override;
    [[nodiscard]] bool isOpen() const override;
    [[nodiscard]] std::string lastError() const override;

private:
    std::string m_interfaceName;
    int m_socket = -1;
    std::string m_lastError;
};

} // namespace lexus_head_unit
