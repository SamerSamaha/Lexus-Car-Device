// Fixture test for the fake transport used by the ELM327 tests.

#include "lexus_head_unit/hardware/byte_transport.h"
#include "lexus_head_unit/hardware/fake_byte_transport.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace {

using lexus_head_unit::FakeByteTransport;
using lexus_head_unit::ReadResult;
using lexus_head_unit::ReadStatus;

std::vector<std::uint8_t> bytesOf(const std::string& text) {
    return {text.begin(), text.end()};
}

TEST(FakeByteTransportTest, ClosedTransportRefusesWritesAndReportsClosedOnRead) {
    FakeByteTransport transport;
    EXPECT_FALSE(transport.isOpen());
    EXPECT_EQ(transport.write(bytesOf("ATZ\r")), 0U);
    std::vector<std::uint8_t> buffer;
    EXPECT_EQ(transport.read(buffer, 16, 10).status, ReadStatus::Closed);
}

TEST(FakeByteTransportTest, ScriptedReplyIsQueuedWhenItsCommandIsWritten) {
    FakeByteTransport transport;
    ASSERT_TRUE(transport.open());
    transport.scriptReply("ATZ", "ELM327 v1.5\r\r>");
    EXPECT_EQ(transport.write(bytesOf("ATZ\r")), 4U);
    EXPECT_EQ(transport.writtenCommands(), (std::vector<std::string>{"ATZ"}));
    EXPECT_EQ(transport.pendingByteCount(), 14U);

    std::vector<std::uint8_t> buffer;
    const ReadResult result = transport.read(buffer, 100, 10);
    EXPECT_EQ(result.status, ReadStatus::Ok);
    EXPECT_EQ(result.byteCount, 14U);
    EXPECT_EQ(std::string(buffer.begin(), buffer.end()), "ELM327 v1.5\r\r>");
    EXPECT_EQ(transport.read(buffer, 100, 10).status, ReadStatus::Timeout);
}

TEST(FakeByteTransportTest, ChunkSizeLimitsEachRead) {
    FakeByteTransport transport;
    ASSERT_TRUE(transport.open());
    transport.setChunkSize(3);
    transport.queueBytes("ABCDEFG");
    std::vector<std::uint8_t> buffer;
    EXPECT_EQ(transport.read(buffer, 100, 10).byteCount, 3U);
    EXPECT_EQ(transport.read(buffer, 2, 10).byteCount, 2U);
    EXPECT_EQ(transport.read(buffer, 100, 10).byteCount, 2U);
    EXPECT_EQ(transport.read(buffer, 100, 10).status, ReadStatus::Timeout);
}

TEST(FakeByteTransportTest, LastScriptedReplyRepeatsAndUnopenableTransportStaysClosed) {
    FakeByteTransport transport;
    ASSERT_TRUE(transport.open());
    transport.scriptReply("010D", "410D10\r\r>");
    transport.scriptReply("010D", "410D20\r\r>");
    std::vector<std::uint8_t> buffer;
    transport.write(bytesOf("010D\r"));
    transport.read(buffer, 100, 10);
    EXPECT_EQ(std::string(buffer.begin(), buffer.end()), "410D10\r\r>");
    transport.write(bytesOf("010D\r"));
    transport.read(buffer, 100, 10);
    EXPECT_EQ(std::string(buffer.begin(), buffer.end()), "410D20\r\r>");
    transport.write(bytesOf("010D\r"));
    transport.read(buffer, 100, 10);
    EXPECT_EQ(std::string(buffer.begin(), buffer.end()), "410D20\r\r>");

    FakeByteTransport unopenable;
    unopenable.setOpenable(false);
    EXPECT_FALSE(unopenable.open());
    EXPECT_FALSE(unopenable.isOpen());
}

} // namespace
