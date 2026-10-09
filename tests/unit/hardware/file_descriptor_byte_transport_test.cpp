// Fixture test for the real transport, over a pseudo-terminal created in the test.

#include "lexus_head_unit/hardware/byte_transport.h"
#include "lexus_head_unit/hardware/file_descriptor_byte_transport.h"

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

// glibc declares the pseudo-terminal functions in internal headers; <cstdlib> is the public one.
// NOLINTBEGIN(misc-include-cleaner)
namespace {

using lexus_head_unit::FileDescriptorByteTransport;
using lexus_head_unit::ReadResult;
using lexus_head_unit::ReadStatus;

class PseudoTerminal {
public:
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
    PseudoTerminal() : m_master(posix_openpt(O_RDWR | O_NOCTTY)) {
        if (m_master >= 0 && grantpt(m_master) == 0 && unlockpt(m_master) == 0) {
            std::array<char, 256> name{};
            if (ptsname_r(m_master, name.data(), name.size()) == 0) {
                m_slavePath = name.data();
            }
        }
    }

    ~PseudoTerminal() {
        closeMaster();
    }

    PseudoTerminal(const PseudoTerminal&) = delete;
    PseudoTerminal& operator=(const PseudoTerminal&) = delete;
    PseudoTerminal(PseudoTerminal&&) = delete;
    PseudoTerminal& operator=(PseudoTerminal&&) = delete;

    [[nodiscard]] bool isValid() const {
        return m_master >= 0 && !m_slavePath.empty();
    }

    [[nodiscard]] const std::string& slavePath() const {
        return m_slavePath;
    }

    void writeFromFarEnd(const std::string& text) const {
        const ssize_t written = ::write(m_master, text.data(), text.size());
        ASSERT_EQ(written, static_cast<ssize_t>(text.size()));
    }

    [[nodiscard]] std::string readAtFarEnd(std::size_t maxBytes) const {
        std::vector<char> buffer(maxBytes);
        const ssize_t count = ::read(m_master, buffer.data(), maxBytes);
        if (count <= 0) {
            return {};
        }
        return {buffer.data(), static_cast<std::size_t>(count)};
    }

    void closeMaster() {
        if (m_master >= 0) {
            ::close(m_master);
            m_master = -1;
        }
    }

private:
    int m_master = -1;
    std::string m_slavePath;
};

TEST(FileDescriptorByteTransportTest, OpensAPseudoTerminalInRawModeAndExchangesBytes) {
    const PseudoTerminal terminal;
    ASSERT_TRUE(terminal.isValid());
    FileDescriptorByteTransport transport(terminal.slavePath());
    EXPECT_FALSE(transport.isOpen());
    ASSERT_TRUE(transport.open());
    EXPECT_TRUE(transport.isOpen());
    EXPECT_TRUE(transport.isTerminal());
    EXPECT_EQ(transport.devicePath(), terminal.slavePath());

    const std::string request = "010D\r";
    EXPECT_EQ(transport.write(std::vector<std::uint8_t>(request.begin(), request.end())), 5U);
    EXPECT_EQ(terminal.readAtFarEnd(16), request);

    terminal.writeFromFarEnd("410D3C\r\r>");
    std::vector<std::uint8_t> buffer;
    const ReadResult result = transport.read(buffer, 64, 1000);
    EXPECT_EQ(result.status, ReadStatus::Ok);
    EXPECT_EQ(std::string(buffer.begin(), buffer.end()), "410D3C\r\r>");
}

TEST(FileDescriptorByteTransportTest, ZeroTimeoutReadReturnsAtOnceWithTimeout) {
    const PseudoTerminal terminal;
    ASSERT_TRUE(terminal.isValid());
    FileDescriptorByteTransport transport(terminal.slavePath());
    ASSERT_TRUE(transport.open());
    std::vector<std::uint8_t> buffer;
    const auto before = std::chrono::steady_clock::now();
    const ReadResult result = transport.read(buffer, 64, 0);
    const auto elapsed = std::chrono::steady_clock::now() - before;
    EXPECT_EQ(result.status, ReadStatus::Timeout);
    EXPECT_LT(elapsed, std::chrono::milliseconds(50));
    const ReadResult shortWait = transport.read(buffer, 64, 20);
    EXPECT_EQ(shortWait.status, ReadStatus::Timeout);
}

TEST(FileDescriptorByteTransportTest, FarEndClosingGivesClosed) {
    PseudoTerminal terminal;
    ASSERT_TRUE(terminal.isValid());
    FileDescriptorByteTransport transport(terminal.slavePath());
    ASSERT_TRUE(transport.open());
    terminal.closeMaster();
    std::vector<std::uint8_t> buffer;
    const ReadResult result = transport.read(buffer, 64, 1000);
    EXPECT_EQ(result.status, ReadStatus::Closed);
    transport.close();
    EXPECT_FALSE(transport.isOpen());
    EXPECT_EQ(transport.read(buffer, 64, 0).status, ReadStatus::Closed);
    EXPECT_EQ(transport.write({0x41}), 0U);
}

TEST(FileDescriptorByteTransportTest, NonexistentPathDoesNotOpen) {
    FileDescriptorByteTransport transport("/nonexistent/lexus-head-unit-device");
    EXPECT_FALSE(transport.open());
    EXPECT_FALSE(transport.isOpen());
}

} // namespace
// NOLINTEND(misc-include-cleaner)
