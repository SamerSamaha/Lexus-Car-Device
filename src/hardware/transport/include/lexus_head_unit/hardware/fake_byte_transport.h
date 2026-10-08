#pragma once

#include "lexus_head_unit/hardware/byte_transport.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace lexus_head_unit {

class FakeByteTransport final : public ByteTransport {
public:
    // The reply text is delivered as is; the adapter's "\r\r>" framing is not added.
    void scriptReply(const std::string& command, const std::string& replyText);
    void queueBytes(const std::string& text);
    void setChunkSize(std::size_t chunkSize);
    void setOpenable(bool openable);

    [[nodiscard]] const std::vector<std::string>& writtenCommands() const;
    [[nodiscard]] std::size_t totalBytesWritten() const;
    [[nodiscard]] std::size_t pendingByteCount() const;

    bool open() override;
    void close() override;
    [[nodiscard]] bool isOpen() const override;
    std::size_t write(const std::vector<std::uint8_t>& bytes) override;
    ReadResult read(std::vector<std::uint8_t>& buffer,
                    std::size_t maxBytes,
                    std::int64_t timeoutMilliseconds) override;

private:
    bool m_open = false;
    bool m_openable = true;
    std::size_t m_chunkSize = 64;
    std::map<std::string, std::deque<std::string>> m_repliesByCommand;
    std::deque<std::uint8_t> m_pendingBytes;
    std::vector<std::string> m_writtenCommands;
    std::size_t m_totalBytesWritten = 0;
};

} // namespace lexus_head_unit
