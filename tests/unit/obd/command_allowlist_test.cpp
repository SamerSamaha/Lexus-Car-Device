// Verifies: REQ-001

#include "lexus_head_unit/hardware/command_allowlist.h"
#include "lexus_head_unit/hardware/elm327_protocol.h"
#include "lexus_head_unit/hardware/fake_byte_transport.h"
#include "lexus_head_unit/hardware/hex.h"
#include "lexus_head_unit/service/manual_clock.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {

using lexus_head_unit::AllowlistDecision;
using lexus_head_unit::CommandAllowlist;
using lexus_head_unit::Elm327Protocol;
using lexus_head_unit::Elm327ReplyKind;
using lexus_head_unit::elm327SetupCommands;
using lexus_head_unit::FakeByteTransport;
using lexus_head_unit::hexText;
using lexus_head_unit::ManualClock;
using lexus_head_unit::toString;

constexpr std::uint8_t modeClearTroubleCodes = 0x04;

struct ModeSweepResult {
    std::vector<int> modesWritten;
    std::size_t bytesWrittenForRefusedModes = 0;
};

// Offers every mode value to the protocol and records which ones reached the transport.
ModeSweepResult sweepAllModes(Elm327Protocol& protocol, const FakeByteTransport& transport) {
    ModeSweepResult result;
    for (int mode = 0; mode < 256; ++mode) {
        const std::string modeText = hexText(static_cast<std::uint8_t>(mode));
        const std::string command = mode == 0x03 ? modeText : modeText + "0D";
        const std::size_t bytesBefore = transport.totalBytesWritten();
        protocol.execute(command);
        const std::size_t bytesWritten = transport.totalBytesWritten() - bytesBefore;
        if (bytesWritten > 0) {
            result.modesWritten.push_back(mode);
        }
        if (mode != 0x01 && mode != 0x03 && mode != 0x09) {
            result.bytesWrittenForRefusedModes += bytesWritten;
        }
    }
    return result;
}

// The setup commands the allowlist accepts.
std::size_t allowedSetupCommandCount() {
    std::size_t allowed = 0;
    for (const auto setupCommand : elm327SetupCommands) {
        if (CommandAllowlist::isAllowed(setupCommand)) {
            ++allowed;
        }
    }
    return allowed;
}

// The modes for which obdRequest can build any request text at all.
std::vector<int> buildableModes() {
    std::vector<int> modes;
    for (int mode = 0; mode < 256; ++mode) {
        const auto modeByte = static_cast<std::uint8_t>(mode);
        if (CommandAllowlist::obdRequest(modeByte, 0x00).has_value() ||
            CommandAllowlist::obdRequest(modeByte, std::nullopt).has_value()) {
            modes.push_back(mode);
        }
    }
    return modes;
}

class CommandAllowlistTest : public ::testing::Test {
protected:
    FakeByteTransport m_transport;
    ManualClock m_clock{0};
    Elm327Protocol m_protocol{m_transport, m_clock, 1000};

    void SetUp() override {
        ASSERT_TRUE(m_transport.open());
        for (int mode = 0; mode < 256; ++mode) {
            const std::string modeText = hexText(static_cast<std::uint8_t>(mode));
            m_transport.scriptReply(modeText + "0D", "7F0112\r\r>");
            m_transport.scriptReply(modeText, "7F0112\r\r>");
        }
    }
};

TEST_F(CommandAllowlistTest, OnlyModesOneThreeAndNineOfAllTwoHundredFiftySixReachTheTransport) {
    const ModeSweepResult result = sweepAllModes(m_protocol, m_transport);
    EXPECT_EQ(result.modesWritten, (std::vector<int>{0x01, 0x03, 0x09}));
    EXPECT_EQ(result.bytesWrittenForRefusedModes, 0U);
    EXPECT_EQ(m_protocol.refusedCommandCount(), 253U);
    EXPECT_EQ(m_protocol.commandsSent(), 3U);
}

TEST_F(CommandAllowlistTest, ModeFourClearTroubleCodesIsRefusedAndWritesNothing) {
    const std::string clearCodesCommand = hexText(modeClearTroubleCodes);
    EXPECT_EQ(CommandAllowlist::decide(clearCodesCommand), AllowlistDecision::RefusedMode);
    EXPECT_EQ(CommandAllowlist::decide(clearCodesCommand + "00"), AllowlistDecision::RefusedMode);
    EXPECT_FALSE(CommandAllowlist::isAllowedMode(modeClearTroubleCodes));
    EXPECT_FALSE(CommandAllowlist::obdRequest(modeClearTroubleCodes, std::nullopt).has_value());
    EXPECT_FALSE(CommandAllowlist::obdRequest(modeClearTroubleCodes, 0x00).has_value());

    const auto reply = m_protocol.execute(clearCodesCommand);
    EXPECT_EQ(reply.kind, Elm327ReplyKind::Refused);
    EXPECT_EQ(m_transport.totalBytesWritten(), 0U);
    EXPECT_TRUE(m_transport.writtenCommands().empty());
}

TEST_F(CommandAllowlistTest, EverySetupCommandIsAllowed) {
    EXPECT_EQ(allowedSetupCommandCount(), elm327SetupCommands.size());
    EXPECT_EQ(elm327SetupCommands.size(), 9U);
}

TEST_F(CommandAllowlistTest, NoOtherAtOrStCommandIsAllowed) {
    EXPECT_EQ(CommandAllowlist::decide("ATSH7E0"), AllowlistDecision::RefusedUnknownAtCommand);
    EXPECT_EQ(CommandAllowlist::decide("ATPB"), AllowlistDecision::RefusedUnknownAtCommand);
    EXPECT_EQ(CommandAllowlist::decide("ATCRA"), AllowlistDecision::RefusedUnknownAtCommand);
    EXPECT_EQ(CommandAllowlist::decide("ATWM"), AllowlistDecision::RefusedUnknownAtCommand);
    EXPECT_EQ(CommandAllowlist::decide("STI"), AllowlistDecision::RefusedUnknownAtCommand);
    EXPECT_EQ(CommandAllowlist::decide("STP33"), AllowlistDecision::RefusedUnknownAtCommand);
    EXPECT_EQ(CommandAllowlist::decide("atz"), AllowlistDecision::Allowed);
}

TEST_F(CommandAllowlistTest, MalformedShapesAreRefusedBeforeTheModeIsEvenConsidered) {
    EXPECT_EQ(CommandAllowlist::decide(""), AllowlistDecision::RefusedMalformed);
    EXPECT_EQ(CommandAllowlist::decide("0"), AllowlistDecision::RefusedMalformed);
    EXPECT_EQ(CommandAllowlist::decide("010"), AllowlistDecision::RefusedMalformed);
    EXPECT_EQ(CommandAllowlist::decide("01 0D"), AllowlistDecision::RefusedMalformed);
    EXPECT_EQ(CommandAllowlist::decide("010D1"), AllowlistDecision::RefusedMalformed);
    EXPECT_EQ(CommandAllowlist::decide("010D0C"), AllowlistDecision::RefusedMalformed);
    EXPECT_EQ(CommandAllowlist::decide("01ZZ"), AllowlistDecision::RefusedMalformed);
    EXPECT_EQ(CommandAllowlist::decide("01"), AllowlistDecision::RefusedMalformed);
    EXPECT_EQ(CommandAllowlist::decide("0300"), AllowlistDecision::RefusedMalformed);
    EXPECT_EQ(CommandAllowlist::decide("09"), AllowlistDecision::RefusedMalformed);
    EXPECT_EQ(CommandAllowlist::decide("\r"), AllowlistDecision::RefusedMalformed);
}

TEST_F(CommandAllowlistTest, AllowedShapesAreExact) {
    EXPECT_EQ(CommandAllowlist::decide("010D"), AllowlistDecision::Allowed);
    EXPECT_EQ(CommandAllowlist::decide("010d"), AllowlistDecision::Allowed);
    EXPECT_EQ(CommandAllowlist::decide("0100"), AllowlistDecision::Allowed);
    EXPECT_EQ(CommandAllowlist::decide("03"), AllowlistDecision::Allowed);
    EXPECT_EQ(CommandAllowlist::decide("0902"), AllowlistDecision::Allowed);
    EXPECT_TRUE(CommandAllowlist::isAllowed("0100"));
}

TEST_F(CommandAllowlistTest, RequestBuilderOnlyBuildsAllowedModes) {
    EXPECT_EQ(CommandAllowlist::obdRequest(0x01, 0x0D).value_or(""), "010D");
    EXPECT_EQ(CommandAllowlist::obdRequest(0x03, std::nullopt).value_or(""), "03");
    EXPECT_EQ(CommandAllowlist::obdRequest(0x09, 0x02).value_or(""), "0902");
    EXPECT_FALSE(CommandAllowlist::obdRequest(0x01, std::nullopt).has_value());
    EXPECT_FALSE(CommandAllowlist::obdRequest(0x03, 0x00).has_value());
    EXPECT_FALSE(CommandAllowlist::obdRequest(0x02, 0x00).has_value());
    EXPECT_FALSE(CommandAllowlist::obdRequest(0x0A, std::nullopt).has_value());
    EXPECT_FALSE(CommandAllowlist::obdRequest(0x22, 0x01).has_value());
    EXPECT_EQ(buildableModes(), (std::vector<int>{0x01, 0x03, 0x09}));
}

TEST_F(CommandAllowlistTest, DecisionsHaveNames) {
    EXPECT_EQ(toString(AllowlistDecision::Allowed), "Allowed");
    EXPECT_EQ(toString(AllowlistDecision::RefusedMode), "RefusedMode");
    EXPECT_EQ(toString(AllowlistDecision::RefusedUnknownAtCommand), "RefusedUnknownAtCommand");
    EXPECT_EQ(toString(AllowlistDecision::RefusedMalformed), "RefusedMalformed");
}

} // namespace
