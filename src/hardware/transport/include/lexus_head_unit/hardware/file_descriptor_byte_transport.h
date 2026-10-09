#pragma once

#include "lexus_head_unit/hardware/byte_transport.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace lexus_head_unit {

// A serial device, pseudo-terminal or any other path that can be opened for reading and writing.
class FileDescriptorByteTransport final : public ByteTransport {
public:
    explicit FileDescriptorByteTransport(std::string devicePath);
    ~FileDescriptorByteTransport() override;

    FileDescriptorByteTransport(const FileDescriptorByteTransport&) = delete;
    FileDescriptorByteTransport& operator=(const FileDescriptorByteTransport&) = delete;
    FileDescriptorByteTransport(FileDescriptorByteTransport&&) = delete;
    FileDescriptorByteTransport& operator=(FileDescriptorByteTransport&&) = delete;

    [[nodiscard]] const std::string& devicePath() const;
    [[nodiscard]] bool isTerminal() const;

    bool open() override;
    void close() override;
    [[nodiscard]] bool isOpen() const override;
    std::size_t write(const std::vector<std::uint8_t>& bytes) override;
    ReadResult read(std::vector<std::uint8_t>& buffer,
                    std::size_t maxBytes,
                    std::int64_t timeoutMilliseconds) override;

private:
    std::string m_devicePath;
    int m_fileDescriptor = -1;
    bool m_isTerminal = false;
};

} // namespace lexus_head_unit
