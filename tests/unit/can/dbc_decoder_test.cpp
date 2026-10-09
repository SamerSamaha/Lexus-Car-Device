// Verifies: REQ-005, REQ-010

#include "lexus_head_unit/hardware/can_frame.h"
#include "lexus_head_unit/hardware/dbc_database.h"
#include "lexus_head_unit/hardware/dbc_decoder.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {

using lexus_head_unit::CanFrame;
using lexus_head_unit::DbcDatabase;
using lexus_head_unit::DbcDecoder;
using lexus_head_unit::DecodeKind;
using lexus_head_unit::DecodeResult;
using Bytes = std::array<std::uint8_t, lexus_head_unit::canMaximumDataLength>;

constexpr std::uint32_t engineStatus = 0x100;
constexpr std::uint32_t vehicleMotion = 0x200;
constexpr std::uint32_t powerStatus = 0x300;
constexpr std::uint32_t tripComputer = 0x18FEF100;

DbcDatabase projectDatabase() {
    return DbcDatabase::loadFromFile(LEXUS_HEAD_UNIT_DBC_PATH);
}

CanFrame frameOf(std::uint32_t identifier, std::uint8_t length, Bytes data, bool extended = false) {
    CanFrame frame;
    frame.identifier = identifier;
    frame.extended = extended;
    frame.length = length;
    frame.data = data;
    return frame;
}

// The value of one named signal in a decoded result; NaN-free sentinel when absent.
double valueOf(const DecodeResult& result, const std::string& name) {
    for (const auto& decoded : result.values) {
        if (decoded.name == name) {
            return decoded.value;
        }
    }
    return -999999.0;
}

class DbcDecoderTest : public ::testing::Test {
protected:
    double decodeOne(std::uint32_t identifier,
                     std::uint8_t length,
                     Bytes data,
                     const std::string& name,
                     bool extended = false) {
        const DecodeResult result = m_decoder.decode(frameOf(identifier, length, data, extended));
        EXPECT_EQ(result.kind, DecodeKind::Decoded) << name;
        return valueOf(result, name);
    }

    DbcDecoder m_decoder{projectDatabase()};
};

TEST_F(DbcDecoderTest, CommittedDbcParsesWithItsFourMessagesAndSeventeenSignals) {
    const DbcDatabase& database = m_decoder.database();
    EXPECT_TRUE(database.errors().empty()) << database.errors().front();
    ASSERT_EQ(database.messages().size(), 4U);
    EXPECT_EQ(database.signalCount(), 17U);
    ASSERT_NE(database.find(tripComputer, true), nullptr);
    EXPECT_EQ(database.find(tripComputer, false), nullptr);
    EXPECT_EQ(database.find(powerStatus, false)->length, 4U);
}

TEST_F(DbcDecoderTest, LittleEndianUnsignedAcrossBytesWithScale) {
    EXPECT_DOUBLE_EQ(decodeOne(engineStatus, 8, {0x10, 0x27}, "EngineSpeed"), 2500.0);
    EXPECT_DOUBLE_EQ(decodeOne(engineStatus, 8, {0, 0, 0, 0, 0xFF, 0x03}, "ThrottlePosition"),
                     102.3);
    EXPECT_DOUBLE_EQ(decodeOne(engineStatus, 8, {0, 0, 0, 0, 0x00, 0xFC}, "ThrottlePosition"), 0.0);
    EXPECT_DOUBLE_EQ(decodeOne(tripComputer, 8, {0xFF, 0xFF, 0xFF, 0xFF}, "Odometer", true),
                     429496729.5);
}

TEST_F(DbcDecoderTest, LittleEndianSignedAndOffset) {
    EXPECT_DOUBLE_EQ(
        decodeOne(engineStatus, 8, {0, 0, 0, 0, 0, 0xFC, 0x03}, "IntakeAirTemperature"), -1.0);
    EXPECT_DOUBLE_EQ(
        decodeOne(engineStatus, 8, {0, 0, 0, 0, 0, 0x00, 0x02}, "IntakeAirTemperature"), -128.0);
    EXPECT_DOUBLE_EQ(decodeOne(engineStatus, 8, {0, 0, 0}, "CoolantTemperature"), -40.0);
    EXPECT_DOUBLE_EQ(decodeOne(vehicleMotion, 8, {0, 0, 0, 0, 0x00, 0x80}, "YawRate"), -327.68);
}

TEST_F(DbcDecoderTest, BigEndianUnsignedAndSignedAcrossBytes) {
    EXPECT_DOUBLE_EQ(decodeOne(vehicleMotion, 8, {0x27, 0x10}, "VehicleSpeed"), 100.0);
    EXPECT_DOUBLE_EQ(decodeOne(vehicleMotion, 8, {0, 0, 0xFF, 0xF0}, "LongitudinalAcceleration"),
                     -0.01);
    EXPECT_DOUBLE_EQ(decodeOne(vehicleMotion, 8, {0, 0, 0x80, 0x00}, "LongitudinalAcceleration"),
                     -20.48);
    EXPECT_DOUBLE_EQ(decodeOne(vehicleMotion, 8, {0, 0, 0x7F, 0xF0}, "LongitudinalAcceleration"),
                     20.47);
    EXPECT_DOUBLE_EQ(decodeOne(vehicleMotion, 8, {0, 0, 0, 0, 0, 0, 0x20, 0x00}, "SteeringAngle"),
                     -824.2);
    EXPECT_DOUBLE_EQ(
        decodeOne(tripComputer, 8, {0, 0, 0, 0, 0x01, 0x02, 0x03}, "TripFuelUsed", true), 66.051);
}

TEST_F(DbcDecoderTest, SingleBitsShortBigEndianAndFractionalScaleWithOffset) {
    EXPECT_DOUBLE_EQ(decodeOne(engineStatus, 8, {0, 0, 0, 0, 0, 0, 0, 0x80}, "EngineRunning"), 1.0);
    EXPECT_DOUBLE_EQ(decodeOne(powerStatus, 4, {0, 0, 0, 0x01}, "IgnitionOn"), 1.0);
    EXPECT_DOUBLE_EQ(decodeOne(powerStatus, 4, {0, 0, 0, 0x70}, "HeadlampState"), 7.0);
    EXPECT_DOUBLE_EQ(decodeOne(powerStatus, 4, {0, 0, 0, 0x40}, "HeadlampState"), 4.0);
    EXPECT_DOUBLE_EQ(decodeOne(powerStatus, 4, {0xE8, 0x30}, "ControlModuleVoltage"), 12.52);
    EXPECT_DOUBLE_EQ(
        decodeOne(tripComputer, 8, {0, 0, 0, 0, 0, 0, 0, 0xFF}, "AmbientPressure", true), 177.5);
}

// "kind name; values n" for one frame, so a list of frames compares in one expectation.
std::string summaryOf(const DecodeResult& result) {
    return std::string(lexus_head_unit::toString(result.kind)) + "; values " +
           std::to_string(result.values.size());
}

TEST_F(DbcDecoderTest, NamedMalformedFramesAreCountedAndYieldNoValue) {
    const std::vector<std::pair<CanFrame, DecodeKind>> cases = {
        {frameOf(engineStatus, 0, {}), DecodeKind::WrongLength},
        {frameOf(engineStatus, 7, {}), DecodeKind::WrongLength},
        {frameOf(powerStatus, 8, {}), DecodeKind::WrongLength},
        {frameOf(powerStatus, 3, {}), DecodeKind::WrongLength},
        {frameOf(engineStatus, 9, {}), DecodeKind::InvalidFrame},
        {frameOf(engineStatus, 15, {}), DecodeKind::InvalidFrame},
        {frameOf(0x123, 8, {}), DecodeKind::UnknownIdentifier},
        {frameOf(engineStatus, 8, {}, true), DecodeKind::UnknownIdentifier},
        {frameOf(tripComputer, 8, {}, false), DecodeKind::UnknownIdentifier},
    };
    std::vector<std::string> observed;
    std::vector<std::string> expected;
    for (const auto& [frame, kind] : cases) {
        observed.push_back(summaryOf(m_decoder.decode(frame)));
        expected.push_back(std::string(lexus_head_unit::toString(kind)) + "; values 0");
    }
    EXPECT_EQ(observed, expected);
    const auto counters = m_decoder.counters();
    EXPECT_EQ(
        std::make_tuple(counters.decoded,
                        counters.wrongLength,
                        counters.invalidFrame,
                        counters.unknownIdentifier),
        std::make_tuple(std::uint64_t{0}, std::uint64_t{4}, std::uint64_t{2}, std::uint64_t{3}));
}

// Decodes frameCount random frames (random identifier of five, random length 0 to 15, random
// bytes) and returns how many signal values came out in total.
std::uint64_t decodeRandomFrames(DbcDecoder& decoder, int frameCount) {
    // NOLINTNEXTLINE(cert-msc32-c,cert-msc51-cpp)
    std::mt19937 generator(20261009U);
    std::uniform_int_distribution<int> byteDistribution(0, 255);
    std::uniform_int_distribution<int> lengthDistribution(0, 15);
    std::uniform_int_distribution<int> choiceDistribution(0, 4);
    const std::array<std::uint32_t, 5> identifiers = {
        engineStatus, vehicleMotion, powerStatus, tripComputer, 0x7FF};
    std::uint64_t valueCount = 0;
    for (int index = 0; index < frameCount; ++index) {
        Bytes data{};
        for (auto& byte : data) {
            byte = static_cast<std::uint8_t>(byteDistribution(generator));
        }
        const std::uint32_t identifier =
            identifiers.at(static_cast<std::size_t>(choiceDistribution(generator)));
        const auto length = static_cast<std::uint8_t>(lengthDistribution(generator));
        valueCount += decoder.decode(frameOf(identifier, length, data, identifier == tripComputer))
                          .values.size();
    }
    return valueCount;
}

TEST_F(DbcDecoderTest, HundredThousandRandomFramesNeverCrashAndCountersAddUp) {
    constexpr int frameCount = 100000;
    const std::uint64_t valueCount = decodeRandomFrames(m_decoder, frameCount);
    const auto counters = m_decoder.counters();
    EXPECT_EQ(counters.decoded + counters.unknownIdentifier + counters.wrongLength +
                  counters.invalidFrame,
              static_cast<std::uint64_t>(frameCount));
    const bool everyKindSeen = counters.decoded > 0 && counters.wrongLength > 0 &&
                               counters.invalidFrame > 0 && counters.unknownIdentifier > 0;
    EXPECT_TRUE(everyKindSeen);
    EXPECT_GT(valueCount, counters.decoded);
}

// The only error a DBC text gives, or "N errors" when it is not exactly one.
std::string onlyErrorOf(const std::string& text) {
    const DbcDatabase database = DbcDatabase::parse(text);
    if (database.errors().size() != 1) {
        return std::to_string(database.errors().size()) + " errors";
    }
    return database.errors().front();
}

// Each malformed DBC text and the error it must produce.
TEST(DbcDatabaseTest, EachDefectGivesItsErrorAndTheRestStillParses) {
    const std::vector<std::pair<std::string, std::string>> cases = {
        {"BO_ 1 A: 8 X\n SG_ S : 60|8@1+ (1,0) [0|1] \"\" Y\n", "lies outside"},
        {"BO_ 1 A: 2 X\n SG_ S : 8|16@0+ (1,0) [0|1] \"\" Y\n", "lies outside"},
        {"BO_ 1 A: 8 X\n SG_ S M : 0|8@1+ (1,0) [0|1] \"\" Y\n", "multiplexed"},
        {"BO_ 1 A: 8 X\n SG_ S m3 : 0|8@1+ (1,0) [0|1] \"\" Y\n", "multiplexed"},
        {"BO_ 1 A: 8 X\n SG_ S : 0|0@1+ (1,0) [0|1] \"\" Y\n", "1 to 64 bits"},
        {"BO_ 1 A: 8 X\n SG_ S : 0|65@1+ (1,0) [0|1] \"\" Y\n", "1 to 64 bits"},
        {"BO_ 1 A: 8 X\n SG_ S : 0|8@1+ (x,0) [0|1] \"\" Y\n", "malformed numbers"},
        {"BO_ 1 A: 8 X\n SG_ broken\n", "malformed SG_ line"},
        {"BO_ 1 A: 9 X\n", "above 8 bytes"},
        {"BO_ 2048 A: 8 X\n", "above 0x7FF"},
        {"BO_ one A: 8 X\n", "malformed BO_ line"},
        {" SG_ S : 0|8@1+ (1,0) [0|1] \"\" Y\n", "outside a message"},
        {"BO_ 1 A: 8 X\nBO_ 1 B: 8 X\n", "duplicate message identifier"},
        {"SIG_VALTYPE_ 1 S : 1;\n", "float signals"},
        {"SG_MUL_VAL_ 1 S M 0-0;\n", "extended multiplexing"},
    };
    std::vector<std::string> wrong;
    for (const auto& [text, expected] : cases) {
        const std::string error = onlyErrorOf(text);
        if (error.find(expected) == std::string::npos) {
            std::string description = text;
            description += " gave '";
            description += error;
            description += "', expected '";
            description += expected;
            description += "'";
            wrong.push_back(description);
        }
    }
    EXPECT_EQ(wrong, std::vector<std::string>{});
    const DbcDatabase mixed =
        DbcDatabase::parse("BO_ 1 A: 8 X\n SG_ Bad : 63|8@1+ (1,0) [0|1] \"\" Y\n"
                           " SG_ Good : 0|8@1+ (2,1) [0|1] \"V\" Y\n");
    ASSERT_EQ(mixed.messages().size(), 1U);
    ASSERT_EQ(mixed.messages().front().signalList.size(), 1U);
    EXPECT_EQ(mixed.messages().front().signalList.front().name, "Good");
    EXPECT_EQ(mixed.errors().front().rfind("line 2: ", 0), 0U);
}

TEST(DbcDatabaseTest, MissingFileIsOneError) {
    const DbcDatabase database = DbcDatabase::loadFromFile("/nonexistent/vehicle.dbc");
    ASSERT_EQ(database.errors().size(), 1U);
    EXPECT_EQ(database.errors().front(), "cannot open /nonexistent/vehicle.dbc");
}

} // namespace
