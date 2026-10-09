// Verifies: REQ-021

#include "lexus_head_unit/hardware/elm327_protocol.h"
#include "lexus_head_unit/hardware/obd_diagnostics.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using lexus_head_unit::decodeTroubleCodes;
using lexus_head_unit::decodeVehicleIdentification;
using lexus_head_unit::Elm327Reply;
using lexus_head_unit::Elm327ReplyKind;
using lexus_head_unit::troubleCodeDescription;
using lexus_head_unit::troubleCodeText;
using Codes = std::vector<std::string>;

// The protocol hands over lines upper-cased with spaces removed (cleanReplyLines).
Elm327Reply replyOf(std::vector<std::string> lines, Elm327ReplyKind kind = Elm327ReplyKind::Data) {
    Elm327Reply reply;
    reply.kind = kind;
    reply.lines = std::move(lines);
    return reply;
}

Elm327Reply replyFromAdapterText(const std::string& text) {
    Elm327Reply reply;
    reply.lines = lexus_head_unit::cleanReplyLines(text, "03");
    reply.kind = lexus_head_unit::classifyReplyLines(reply.lines, true);
    return reply;
}

TEST(TroubleCodeTest, EachLetterAndDigitFromTheTwoBytes) {
    EXPECT_EQ(troubleCodeText(0x01, 0x33), "P0133");
    EXPECT_EQ(troubleCodeText(0x04, 0x20), "P0420");
    EXPECT_EQ(troubleCodeText(0x22, 0x01), "P2201");
    EXPECT_EQ(troubleCodeText(0x40, 0x35), "C0035");
    EXPECT_EQ(troubleCodeText(0x90, 0x00), "B1000");
    EXPECT_EQ(troubleCodeText(0xC1, 0x00), "U0100");
    EXPECT_EQ(troubleCodeText(0xFF, 0xFF), "U3FFF");
}

TEST(TroubleCodeTest, ZeroOneAndTwoCodesInOneFrame) {
    EXPECT_EQ(decodeTroubleCodes(replyOf({"4300"})), Codes{});
    EXPECT_EQ(decodeTroubleCodes(replyOf({"43010133"})), Codes{"P0133"});
    EXPECT_EQ(decodeTroubleCodes(replyOf({"430201330420"})), (Codes{"P0133", "P0420"}));
}

TEST(TroubleCodeTest, SixCodesAcrossPCBUInAMultiFrameReplyWithAndWithoutSpaces) {
    // 43 06 then six codes: 14 bytes, in a first frame of 6 and a second of 8 (7 used).
    const Codes expected = {"P0133", "C0035", "B1000", "U0100", "P0420", "P0171"};
    const std::string spaced = "00E\r0: 43 06 01 33 40 35\r1: 90 00 C1 00 04 20 01\r2: 71\r\r>";
    EXPECT_EQ(decodeTroubleCodes(replyFromAdapterText(spaced)), expected);
    const std::string unspaced = "00E\r0:430601334035\r1:9000C100042001\r2:71\r\r>";
    const Elm327Reply reply = replyFromAdapterText(unspaced);
    EXPECT_EQ(reply.kind, Elm327ReplyKind::Data);
    EXPECT_EQ(decodeTroubleCodes(reply), expected);
}

TEST(TroubleCodeTest, TwoEcusOlderFormatNoDataAndMalformed) {
    EXPECT_EQ(decodeTroubleCodes(replyOf({"43010133", "43010420"})), (Codes{"P0133", "P0420"}));
    // Older format: no count byte, three pairs padded with zeros.
    EXPECT_EQ(decodeTroubleCodes(replyOf({"430133042000000"})), std::nullopt);
    EXPECT_EQ(decodeTroubleCodes(replyOf({"43013304200000"})), (Codes{"P0133", "P0420"}));
    EXPECT_EQ(decodeTroubleCodes(replyOf({}, Elm327ReplyKind::NoData)), Codes{});
    EXPECT_EQ(decodeTroubleCodes(replyOf({"410D3C"})), std::nullopt);
    EXPECT_EQ(decodeTroubleCodes(replyOf({"GARBAGE"}, Elm327ReplyKind::Malformed)), std::nullopt);
}

TEST(VehicleIdentificationTest, DecodedFromAMultiFrameReplyWithAndWithoutSpaces) {
    // The hypothetical VIN allowlisted in tools/private_data_allowlist.txt; not a real vehicle.
    const std::string expected = "1M8GDM9AXKP042788";
    const std::string spaced =
        "014\r0: 49 02 01 31 4D 38\r1: 47 44 4D 39 41 58 4B\r2: 50 30 34 32 37 38 38\r\r>";
    EXPECT_EQ(decodeVehicleIdentification(replyFromAdapterText(spaced)), expected);
    const std::string unspaced = "014\r0:490201314D38\r1:47444D3941584B\r2:50303432373838\r\r>";
    EXPECT_EQ(decodeVehicleIdentification(replyFromAdapterText(unspaced)), expected);
}

TEST(VehicleIdentificationTest, WrongLengthOrOtherReplyIsNothing) {
    EXPECT_EQ(decodeVehicleIdentification(replyOf({"490201314D38"})), std::nullopt);
    EXPECT_EQ(decodeVehicleIdentification(replyOf({"4300"})), std::nullopt);
    EXPECT_EQ(decodeVehicleIdentification(replyOf({}, Elm327ReplyKind::NoData)), std::nullopt);
}

TEST(TroubleCodeDescriptionTest, TableEntriesAndEveryCategory) {
    EXPECT_EQ(troubleCodeDescription("P0420"), "Catalyst efficiency below threshold, bank 1");
    EXPECT_EQ(troubleCodeDescription("U0100"), "Lost communication with the engine control module");
    EXPECT_EQ(troubleCodeDescription("P0999"),
              "Generic powertrain code; no text in this unit's table");
    EXPECT_EQ(troubleCodeDescription("P2999"),
              "Generic powertrain code; no text in this unit's table");
    EXPECT_EQ(troubleCodeDescription("P1234"),
              "Manufacturer-specific powertrain code; no text in this unit's table");
    EXPECT_EQ(troubleCodeDescription("C1234"),
              "Manufacturer-specific chassis code; no text in this unit's table");
    EXPECT_EQ(troubleCodeDescription("B0001"), "Generic body code; no text in this unit's table");
    EXPECT_EQ(troubleCodeDescription("U3000"),
              "Manufacturer-specific network code; no text in this unit's table");
    EXPECT_EQ(troubleCodeDescription("X"), "Unknown code");
}

} // namespace
