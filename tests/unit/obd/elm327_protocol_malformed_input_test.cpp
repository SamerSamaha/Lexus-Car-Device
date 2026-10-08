// Verifies: REQ-010

#include "lexus_head_unit/hardware/elm327_protocol.h"
#include "lexus_head_unit/hardware/fake_byte_transport.h"
#include "lexus_head_unit/hardware/hex.h"
#include "lexus_head_unit/hardware/obd_pid_decoder.h"
#include "lexus_head_unit/service/manual_clock.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {

using lexus_head_unit::classifyReplyLines;
using lexus_head_unit::cleanReplyLines;
using lexus_head_unit::decodePid;
using lexus_head_unit::Elm327Protocol;
using lexus_head_unit::Elm327Reply;
using lexus_head_unit::Elm327ReplyKind;
using lexus_head_unit::FakeByteTransport;
using lexus_head_unit::isHexText;
using lexus_head_unit::ManualClock;
using lexus_head_unit::mode01DataBytes;

struct CorpusEntry {
    std::string replyText;
    Elm327ReplyKind expectedKind;
};

const std::vector<CorpusEntry>& namedCorpus() {
    static const std::vector<CorpusEntry> corpus = {
        {"NO DATA\r\r>", Elm327ReplyKind::NoData},
        {"?\r\r>", Elm327ReplyKind::UnknownCommand},
        {"CAN ERROR\r\r>", Elm327ReplyKind::CanError},
        {"BUFFER FULL\r\r>", Elm327ReplyKind::BufferFull},
        {"STOPPED\r\r>", Elm327ReplyKind::Stopped},
        {"UNABLE TO CONNECT\r\r>", Elm327ReplyKind::UnableToConnect},
        {"SEARCHING...\rUNABLE TO CONNECT\r\r>", Elm327ReplyKind::UnableToConnect},
        {"BUS INIT: ...ERROR\r\r>", Elm327ReplyKind::BusInitError},
        {"ERROR\r\r>", Elm327ReplyKind::AdapterError},
        {"7F 01 12\r\r>", Elm327ReplyKind::NegativeResponse},
        {"41 0D ZZ\r\r>", Elm327ReplyKind::Malformed},
        {"410D3\r\r>", Elm327ReplyKind::Malformed},
        {"\r\r>", Elm327ReplyKind::Malformed},
        {">", Elm327ReplyKind::Malformed},
        {"\x01\x02\x03\r\r>", Elm327ReplyKind::Malformed},
        {"410D\r\r>", Elm327ReplyKind::Data},
    };
    return corpus;
}

class Elm327ProtocolMalformedInputTest : public ::testing::Test {
protected:
    FakeByteTransport m_transport;
    ManualClock m_clock{0};
    Elm327Protocol m_protocol{m_transport, m_clock, 1000};

    void SetUp() override {
        ASSERT_TRUE(m_transport.open());
    }

    // A fresh transport and protocol per reply, so entries cannot bleed into each other.
    Elm327Reply replyTo(const std::string& text) {
        FakeByteTransport transport;
        transport.open();
        transport.scriptReply("010D", text);
        Elm327Protocol protocol(transport, m_clock, 1000);
        return protocol.execute("010D");
    }
};

TEST_F(Elm327ProtocolMalformedInputTest, EveryNamedCorpusEntryHasItsKindAndNoUsableBytes) {
    std::size_t checked = 0;
    for (const CorpusEntry& entry : namedCorpus()) {
        const Elm327Reply reply = replyTo(entry.replyText);
        EXPECT_EQ(reply.kind, entry.expectedKind) << entry.replyText;
        const std::optional<std::vector<std::uint8_t>> bytes = mode01DataBytes(reply, 0x0D);
        const bool decodes =
            bytes.has_value() &&
            decodePid(0x0D, bytes.value_or(std::vector<std::uint8_t>{})).has_value();
        EXPECT_FALSE(decodes) << entry.replyText;
        ++checked;
    }
    EXPECT_EQ(checked, namedCorpus().size());
}

TEST_F(Elm327ProtocolMalformedInputTest, TruncatedDataReplyGivesTooFewBytesForTheDecoder) {
    const Elm327Reply reply = replyTo("410D\r\r>");
    EXPECT_EQ(reply.kind, Elm327ReplyKind::Data);
    const std::optional<std::vector<std::uint8_t>> bytes = mode01DataBytes(reply, 0x0D);
    EXPECT_TRUE(bytes.has_value());
    EXPECT_TRUE(bytes.value_or(std::vector<std::uint8_t>{0x01}).empty());
    EXPECT_FALSE(decodePid(0x0D, bytes.value_or(std::vector<std::uint8_t>{})).has_value());
}

TEST_F(Elm327ProtocolMalformedInputTest, OversizeReplyWithoutPromptIsMalformed) {
    const std::string runaway(5000, 'A');
    m_transport.scriptReply("010D", runaway);
    const Elm327Reply reply = m_protocol.execute("010D");
    EXPECT_EQ(reply.kind, Elm327ReplyKind::Malformed);
    EXPECT_EQ(m_protocol.malformedReplyCount(), 1U);
}

TEST_F(Elm327ProtocolMalformedInputTest, ReplyForAnotherPidGivesNoBytes) {
    const Elm327Reply reply = replyTo("410C1AF8\r\r>");
    EXPECT_EQ(reply.kind, Elm327ReplyKind::Data);
    EXPECT_FALSE(mode01DataBytes(reply, 0x0D).has_value());
}

// Returns true when the parser produced bytes that the decoder accepted for PID 0x0D.
bool decodesSpeed(const std::string& text) {
    Elm327Reply reply;
    reply.rawText = text;
    reply.lines = cleanReplyLines(text, "010D");
    reply.kind = classifyReplyLines(reply.lines, true);
    const std::optional<std::vector<std::uint8_t>> bytes = mode01DataBytes(reply, 0x0D);
    return bytes.has_value() &&
           decodePid(0x0D, bytes.value_or(std::vector<std::uint8_t>{})).has_value();
}

// A line decodes only if, after cleaning, it is "410D" followed by exactly two hex digits.
bool isWellFormedSpeedLine(const std::vector<std::string>& lines) {
    for (const std::string& line : lines) {
        if (line.rfind("410D", 0) == 0) {
            return line.size() == 6 && isHexText(line);
        }
    }
    return false;
}

TEST_F(Elm327ProtocolMalformedInputTest, OneHundredThousandRandomStringsNeverCrashOrMisdecode) {
    // A fixed seed on purpose: the run is reproducible and a failure can be replayed.
    // NOLINTNEXTLINE(cert-msc32-c,cert-msc51-cpp)
    std::mt19937 generator(20261010);
    std::uniform_int_distribution<int> lengthDistribution(0, 24);
    std::uniform_int_distribution<int> alphabetDistribution(0, 11);
    const std::string alphabet = "0123456789ABCDEF\r> ?NOZ\n";
    std::size_t decoded = 0;
    std::size_t wellFormed = 0;
    for (int iteration = 0; iteration < 100000; ++iteration) {
        std::string text;
        const int length = lengthDistribution(generator);
        for (int index = 0; index < length; ++index) {
            const auto position = static_cast<std::size_t>(alphabetDistribution(generator)) * 2;
            text.push_back(alphabet.at(position % alphabet.size()));
        }
        const bool decodes = decodesSpeed(text);
        const bool allLinesHex = [&text]() {
            const std::vector<std::string> lines = cleanReplyLines(text, "010D");
            return classifyReplyLines(lines, true) == Elm327ReplyKind::Data &&
                   isWellFormedSpeedLine(lines);
        }();
        EXPECT_EQ(decodes, allLinesHex) << text;
        decoded += decodes ? 1 : 0;
        wellFormed += allLinesHex ? 1 : 0;
    }
    EXPECT_EQ(decoded, wellFormed);
}

} // namespace
