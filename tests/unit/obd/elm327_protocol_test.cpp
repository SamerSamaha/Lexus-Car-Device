// Verifies: REQ-010, REQ-001

#include "lexus_head_unit/hardware/elm327_protocol.h"
#include "lexus_head_unit/hardware/fake_byte_transport.h"
#include "lexus_head_unit/service/manual_clock.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace {

using lexus_head_unit::cleanReplyLines;
using lexus_head_unit::Elm327Protocol;
using lexus_head_unit::Elm327Reply;
using lexus_head_unit::Elm327ReplyKind;
using lexus_head_unit::FakeByteTransport;
using lexus_head_unit::ManualClock;
using lexus_head_unit::mode01DataBytes;
using lexus_head_unit::toString;

class Elm327ProtocolTest : public ::testing::Test {
protected:
    FakeByteTransport m_transport;
    ManualClock m_clock{1000};
    Elm327Protocol m_protocol{m_transport, m_clock, 500};

    void SetUp() override {
        ASSERT_TRUE(m_transport.open());
    }
};

TEST_F(Elm327ProtocolTest, MainScenarioSpeedRequestGivesOneDataByte) {
    m_transport.scriptReply("010D", "410D3C\r\r>");
    const Elm327Reply reply = m_protocol.execute("010D");

    EXPECT_EQ(reply.kind, Elm327ReplyKind::Data);
    ASSERT_EQ(reply.lines.size(), 1U);
    EXPECT_EQ(reply.lines.front(), "410D3C");
    EXPECT_EQ(mode01DataBytes(reply, 0x0D).value_or(std::vector<std::uint8_t>{}),
              (std::vector<std::uint8_t>{0x3C}));
    EXPECT_EQ(m_transport.writtenCommands(), (std::vector<std::string>{"010D"}));
    EXPECT_EQ(m_transport.totalBytesWritten(), 5U);
    EXPECT_EQ(m_protocol.commandsSent(), 1U);
}

TEST_F(Elm327ProtocolTest, EchoSpacesAndSearchingAreRemoved) {
    m_transport.scriptReply("010C", "010C\rSEARCHING...\r41 0C 1A F8\r\r>");
    const Elm327Reply reply = m_protocol.execute("010C");
    EXPECT_EQ(reply.kind, Elm327ReplyKind::Data);
    ASSERT_EQ(reply.lines.size(), 1U);
    EXPECT_EQ(reply.lines.front(), "410C1AF8");
    EXPECT_EQ(mode01DataBytes(reply, 0x0C).value_or(std::vector<std::uint8_t>{}),
              (std::vector<std::uint8_t>{0x1A, 0xF8}));
}

TEST_F(Elm327ProtocolTest, TwoModulesAnsweringGivesTheFirstLine) {
    m_transport.scriptReply("0100", "4100BE3FA813\r4100801BA001\r\r>");
    const Elm327Reply reply = m_protocol.execute("0100");
    EXPECT_EQ(reply.kind, Elm327ReplyKind::Data);
    ASSERT_EQ(reply.lines.size(), 2U);
    EXPECT_EQ(mode01DataBytes(reply, 0x00).value_or(std::vector<std::uint8_t>{}),
              (std::vector<std::uint8_t>{0xBE, 0x3F, 0xA8, 0x13}));
}

TEST_F(Elm327ProtocolTest, AtRepliesAreClassifiedOkOrText) {
    m_transport.scriptReply("ATE0", "ATE0\rOK\r\r>");
    m_transport.scriptReply("ATI", "ELM327 v1.5\r\r>");
    m_transport.scriptReply("ATRV", "12.6V\r\r>");
    m_transport.scriptReply("ATDPN", "A6\r\r>");
    EXPECT_EQ(m_protocol.execute("ATE0").kind, Elm327ReplyKind::Ok);
    const Elm327Reply identity = m_protocol.execute("ATI");
    EXPECT_EQ(identity.kind, Elm327ReplyKind::Text);
    EXPECT_EQ(identity.lines.front(), "ELM327V1.5");
    EXPECT_EQ(m_protocol.execute("ATRV").kind, Elm327ReplyKind::Text);
    const Elm327Reply protocolNumber = m_protocol.execute("ATDPN");
    EXPECT_EQ(protocolNumber.kind, Elm327ReplyKind::Data);
    EXPECT_EQ(protocolNumber.lines.front(), "A6");
}

TEST_F(Elm327ProtocolTest, ReplyDeliveredInSingleByteChunksIsAccumulated) {
    m_transport.setChunkSize(1);
    m_transport.scriptReply("0105", "41057B\r\r>");
    const Elm327Reply reply = m_protocol.execute("0105");
    EXPECT_EQ(reply.kind, Elm327ReplyKind::Data);
    EXPECT_EQ(reply.rawText, "41057B\r\r>");
    EXPECT_EQ(m_transport.pendingByteCount(), 0U);
}

TEST_F(Elm327ProtocolTest, StaleBytesBeforeTheNextCommandAreDiscardedAndCounted) {
    m_transport.setChunkSize(9);
    m_transport.scriptReply("010D", "410D10\r\r>410D20\r\r>");
    m_transport.scriptReply("010D", "410D30\r\r>");
    const Elm327Reply first = m_protocol.execute("010D");
    EXPECT_EQ(first.lines.front(), "410D10");
    EXPECT_EQ(m_transport.pendingByteCount(), 9U);

    const Elm327Reply second = m_protocol.execute("010D");
    EXPECT_EQ(second.kind, Elm327ReplyKind::Data);
    EXPECT_EQ(second.lines.front(), "410D30");
    EXPECT_EQ(m_protocol.discardedStaleByteCount(), 9U);
}

TEST_F(Elm327ProtocolTest, TextWhereDataWasExpectedIsMalformedButTextForAnAtCommandIsText) {
    m_transport.scriptReply("010D", "HELLO\r\r>");
    m_transport.scriptReply("ATI", "HELLO\r\r>");
    EXPECT_EQ(m_protocol.execute("010D").kind, Elm327ReplyKind::Malformed);
    EXPECT_EQ(m_protocol.execute("ATI").kind, Elm327ReplyKind::Text);
    EXPECT_EQ(m_protocol.malformedReplyCount(), 1U);
}

TEST_F(Elm327ProtocolTest, RefusedCommandWritesNothingAndIsCounted) {
    const Elm327Reply reply = m_protocol.execute("0400");
    EXPECT_EQ(reply.kind, Elm327ReplyKind::Refused);
    EXPECT_EQ(m_transport.totalBytesWritten(), 0U);
    EXPECT_EQ(m_protocol.refusedCommandCount(), 1U);
    EXPECT_EQ(m_protocol.commandsSent(), 0U);
}

TEST_F(Elm327ProtocolTest, ClosedTransportGivesLinkErrorWithoutWriting) {
    m_transport.close();
    const Elm327Reply reply = m_protocol.execute("010D");
    EXPECT_EQ(reply.kind, Elm327ReplyKind::LinkError);
    EXPECT_EQ(m_protocol.linkErrorCount(), 1U);
    EXPECT_EQ(m_transport.totalBytesWritten(), 0U);
}

TEST_F(Elm327ProtocolTest, NoReplyWithinTheTimeoutGivesTimeout) {
    const Elm327Reply reply = m_protocol.execute("010D");
    EXPECT_EQ(reply.kind, Elm327ReplyKind::Timeout);
    EXPECT_EQ(m_protocol.timeoutCount(), 1U);
    EXPECT_EQ(m_protocol.replyTimeoutMilliseconds(), 500);
}

TEST_F(Elm327ProtocolTest, DeadlineFromTheClockStopsAPartialReply) {
    m_transport.scriptReply("010D", "410D");
    m_transport.setChunkSize(2);
    const Elm327Reply reply = m_protocol.execute("010D");
    EXPECT_EQ(reply.kind, Elm327ReplyKind::Timeout);
    EXPECT_EQ(reply.rawText, "410D");
}

TEST_F(Elm327ProtocolTest, CleanLinesHandlesLineFeedsAndLowerCase) {
    const std::vector<std::string> lines = cleanReplyLines("41 0d 3c\n\n>", "010d");
    EXPECT_EQ(lines, (std::vector<std::string>{"410D3C"}));
    const std::vector<std::string> echoOnly = cleanReplyLines("010D\r\r>", "010D");
    EXPECT_TRUE(echoOnly.empty());
}

TEST_F(Elm327ProtocolTest, EveryKindHasAName) {
    EXPECT_EQ(toString(Elm327ReplyKind::Data), "Data");
    EXPECT_EQ(toString(Elm327ReplyKind::NoData), "NoData");
    EXPECT_EQ(toString(Elm327ReplyKind::UnableToConnect), "UnableToConnect");
    EXPECT_EQ(toString(Elm327ReplyKind::Refused), "Refused");
    EXPECT_EQ(toString(Elm327ReplyKind::LinkError), "LinkError");
    EXPECT_EQ(toString(Elm327ReplyKind::Timeout), "Timeout");
}

} // namespace
