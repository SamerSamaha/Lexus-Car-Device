#include "lexus_head_unit/hardware/file_descriptor_byte_transport.h"

#include "lexus_head_unit/hardware/byte_transport.h"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

namespace lexus_head_unit {

namespace {

constexpr int invalidFileDescriptor = -1;
constexpr int pollTimeoutLimitMilliseconds = 60000;

bool configureRawTerminal(int fileDescriptor) {
    termios settings{};
    if (tcgetattr(fileDescriptor, &settings) != 0) {
        return false;
    }
    cfmakeraw(&settings);
    settings.c_cc[VMIN] = 0;
    settings.c_cc[VTIME] = 0;
    cfsetispeed(&settings, B38400);
    cfsetospeed(&settings, B38400);
    return tcsetattr(fileDescriptor, TCSANOW, &settings) == 0;
}

} // namespace

FileDescriptorByteTransport::FileDescriptorByteTransport(std::string devicePath)
    : m_devicePath(std::move(devicePath)) {}

FileDescriptorByteTransport::~FileDescriptorByteTransport() {
    close();
}

const std::string& FileDescriptorByteTransport::devicePath() const {
    return m_devicePath;
}

bool FileDescriptorByteTransport::isTerminal() const {
    return m_isTerminal;
}

bool FileDescriptorByteTransport::open() {
    if (m_fileDescriptor != invalidFileDescriptor) {
        return true;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
    const int fileDescriptor = ::open(m_devicePath.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fileDescriptor < 0) {
        return false;
    }
    m_isTerminal = isatty(fileDescriptor) == 1;
    if (m_isTerminal && !configureRawTerminal(fileDescriptor)) {
        ::close(fileDescriptor);
        return false;
    }
    m_fileDescriptor = fileDescriptor;
    return true;
}

void FileDescriptorByteTransport::close() {
    if (m_fileDescriptor != invalidFileDescriptor) {
        ::close(m_fileDescriptor);
        m_fileDescriptor = invalidFileDescriptor;
    }
}

bool FileDescriptorByteTransport::isOpen() const {
    return m_fileDescriptor != invalidFileDescriptor;
}

std::size_t FileDescriptorByteTransport::write(const std::vector<std::uint8_t>& bytes) {
    if (!isOpen()) {
        return 0;
    }
    std::size_t written = 0;
    while (written < bytes.size()) {
        const ssize_t result =
            ::write(m_fileDescriptor, &bytes.at(written), bytes.size() - written);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return written;
        }
        written += static_cast<std::size_t>(result);
    }
    return written;
}

ReadResult FileDescriptorByteTransport::read(std::vector<std::uint8_t>& buffer,
                                             std::size_t maxBytes,
                                             std::int64_t timeoutMilliseconds) {
    buffer.clear();
    if (!isOpen()) {
        return ReadResult{0, ReadStatus::Closed};
    }
    pollfd descriptor{};
    descriptor.fd = m_fileDescriptor;
    descriptor.events = POLLIN;
    const int boundedTimeout = static_cast<int>(
        std::clamp<std::int64_t>(timeoutMilliseconds, 0, pollTimeoutLimitMilliseconds));
    const int ready = ::poll(&descriptor, 1, boundedTimeout);
    if (ready < 0) {
        return ReadResult{0, errno == EINTR ? ReadStatus::Timeout : ReadStatus::Error};
    }
    if (ready == 0) {
        return ReadResult{0, ReadStatus::Timeout};
    }
    buffer.resize(maxBytes);
    const ssize_t count = ::read(m_fileDescriptor, buffer.data(), maxBytes);
    if (count < 0) {
        buffer.clear();
        if (errno == EAGAIN || errno == EINTR) {
            return ReadResult{0, ReadStatus::Timeout};
        }
        return ReadResult{0, ReadStatus::Closed};
    }
    if (count == 0) {
        buffer.clear();
        return ReadResult{0, ReadStatus::Closed};
    }
    buffer.resize(static_cast<std::size_t>(count));
    return ReadResult{static_cast<std::size_t>(count), ReadStatus::Ok};
}

} // namespace lexus_head_unit
