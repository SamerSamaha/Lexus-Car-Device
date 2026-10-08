#include "lexus_head_unit/hardware/fake_byte_transport.h"

#include "lexus_head_unit/hardware/byte_transport.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace lexus_head_unit {

namespace {

constexpr char carriageReturn = '\r';

} // namespace

void FakeByteTransport::scriptReply(const std::string& command, const std::string& replyText) {
    m_repliesByCommand[command].push_back(replyText);
}

void FakeByteTransport::queueBytes(const std::string& text) {
    for (const char character : text) {
        m_pendingBytes.push_back(static_cast<std::uint8_t>(character));
    }
}

void FakeByteTransport::setChunkSize(std::size_t chunkSize) {
    m_chunkSize = std::max<std::size_t>(1, chunkSize);
}

void FakeByteTransport::setOpenable(bool openable) {
    m_openable = openable;
}

const std::vector<std::string>& FakeByteTransport::writtenCommands() const {
    return m_writtenCommands;
}

std::size_t FakeByteTransport::totalBytesWritten() const {
    return m_totalBytesWritten;
}

std::size_t FakeByteTransport::pendingByteCount() const {
    return m_pendingBytes.size();
}

bool FakeByteTransport::open() {
    m_open = m_openable;
    return m_open;
}

void FakeByteTransport::close() {
    m_open = false;
}

bool FakeByteTransport::isOpen() const {
    return m_open;
}

std::size_t FakeByteTransport::write(const std::vector<std::uint8_t>& bytes) {
    if (!m_open) {
        return 0;
    }
    m_totalBytesWritten += bytes.size();
    std::string command;
    for (const std::uint8_t byte : bytes) {
        if (byte != static_cast<std::uint8_t>(carriageReturn)) {
            command.push_back(static_cast<char>(byte));
        }
    }
    m_writtenCommands.push_back(command);
    auto replies = m_repliesByCommand.find(command);
    if (replies != m_repliesByCommand.end() && !replies->second.empty()) {
        queueBytes(replies->second.front());
        if (replies->second.size() > 1) {
            replies->second.pop_front();
        }
    }
    return bytes.size();
}

ReadResult FakeByteTransport::read(std::vector<std::uint8_t>& buffer,
                                   std::size_t maxBytes,
                                   std::int64_t timeoutMilliseconds) {
    static_cast<void>(timeoutMilliseconds);
    buffer.clear();
    if (!m_open) {
        return ReadResult{0, ReadStatus::Closed};
    }
    if (m_pendingBytes.empty()) {
        return ReadResult{0, ReadStatus::Timeout};
    }
    const std::size_t count = std::min({maxBytes, m_chunkSize, m_pendingBytes.size()});
    for (std::size_t index = 0; index < count; ++index) {
        buffer.push_back(m_pendingBytes.front());
        m_pendingBytes.pop_front();
    }
    return ReadResult{count, ReadStatus::Ok};
}

} // namespace lexus_head_unit
