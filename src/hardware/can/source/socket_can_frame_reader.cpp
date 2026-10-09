#include "lexus_head_unit/hardware/socket_can_frame_reader.h"

#include "lexus_head_unit/hardware/can_frame.h"
#include "lexus_head_unit/hardware/can_frame_reader.h"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <utility>

#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

// glibc and the kernel headers declare these symbols in internal headers; the public headers
// above are the right ones to include.
// NOLINTBEGIN(misc-include-cleaner)
namespace lexus_head_unit {

namespace {

std::string errorText(const std::string& what) {
    return what + ": " + std::strerror(errno);
}

int boundedTimeout(std::int64_t timeoutMilliseconds) {
    if (timeoutMilliseconds <= 0) {
        return 0;
    }
    if (timeoutMilliseconds > std::numeric_limits<int>::max()) {
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(timeoutMilliseconds);
}

} // namespace

SocketCanFrameReader::SocketCanFrameReader(std::string interfaceName)
    : m_interfaceName(std::move(interfaceName)) {}

SocketCanFrameReader::~SocketCanFrameReader() {
    close();
}

bool SocketCanFrameReader::open() {
    close();
    if (m_interfaceName.empty() || m_interfaceName.size() >= IFNAMSIZ) {
        m_lastError = "invalid interface name '" + m_interfaceName + "'";
        return false;
    }
    const unsigned int interfaceIndex = ::if_nametoindex(m_interfaceName.c_str());
    if (interfaceIndex == 0) {
        m_lastError = errorText("no interface " + m_interfaceName);
        return false;
    }
    const int socketDescriptor = ::socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK | SOCK_CLOEXEC, CAN_RAW);
    if (socketDescriptor < 0) {
        m_lastError = errorText("socket(PF_CAN)");
        return false;
    }
    sockaddr_can address{};
    address.can_family = AF_CAN;
    address.can_ifindex = static_cast<int>(interfaceIndex);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    if (::bind(socketDescriptor, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        m_lastError = errorText("bind " + m_interfaceName);
        ::close(socketDescriptor);
        return false;
    }
    m_socket = socketDescriptor;
    m_lastError.clear();
    return true;
}

CanReadResult SocketCanFrameReader::read(std::int64_t timeoutMilliseconds) {
    CanReadResult result;
    if (m_socket < 0) {
        result.status = CanReadStatus::Closed;
        return result;
    }
    pollfd descriptor{};
    descriptor.fd = m_socket;
    descriptor.events = POLLIN;
    const int ready = ::poll(&descriptor, 1, boundedTimeout(timeoutMilliseconds));
    if (ready < 0) {
        result.status = errno == EINTR ? CanReadStatus::Timeout : CanReadStatus::Error;
        m_lastError = errorText("poll");
        return result;
    }
    if (ready == 0) {
        result.status = CanReadStatus::Timeout;
        return result;
    }
    if ((static_cast<unsigned int>(descriptor.revents) &
         static_cast<unsigned int>(POLLERR | POLLHUP | POLLNVAL)) != 0) {
        result.status = CanReadStatus::Error;
        m_lastError = "socket error on " + m_interfaceName;
        return result;
    }
    can_frame raw{};
    const ssize_t count = ::read(m_socket, &raw, sizeof(raw));
    if (count < 0) {
        if (errno == EAGAIN || errno == EINTR) {
            result.status = CanReadStatus::Timeout;
            return result;
        }
        result.status =
            errno == ENETDOWN || errno == ENODEV ? CanReadStatus::Closed : CanReadStatus::Error;
        m_lastError = errorText("read " + m_interfaceName);
        return result;
    }
    if (static_cast<std::size_t>(count) != sizeof(raw)) {
        result.status = CanReadStatus::Error;
        m_lastError = "short CAN read on " + m_interfaceName;
        return result;
    }
    result.status = CanReadStatus::Frame;
    result.frame.extended = (raw.can_id & CAN_EFF_FLAG) != 0;
    result.frame.identifier = raw.can_id & (result.frame.extended ? CAN_EFF_MASK : CAN_SFF_MASK);
    // The kernel keeps the length in a union with the old can_dlc name; len is the current one.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access)
    result.frame.length = raw.len;
    for (std::size_t index = 0; index < canMaximumDataLength; ++index) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        result.frame.data.at(index) = raw.data[index];
    }
    return result;
}

void SocketCanFrameReader::close() {
    if (m_socket >= 0) {
        ::close(m_socket);
        m_socket = -1;
    }
}

bool SocketCanFrameReader::isOpen() const {
    return m_socket >= 0;
}

std::string SocketCanFrameReader::lastError() const {
    return m_lastError;
}

} // namespace lexus_head_unit
// NOLINTEND(misc-include-cleaner)
