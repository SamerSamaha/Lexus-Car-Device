// Verifies: REQ-004

#include "lexus_head_unit/hardware/obd_pid_decoder.h"
#include "lexus_head_unit/hardware/supported_pid_set.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace {

using lexus_head_unit::ObdPid;
using lexus_head_unit::pollablePids;
using lexus_head_unit::SupportedPidSet;

TEST(SupportedPidSetTest, StartsEmpty) {
    const SupportedPidSet supported;
    EXPECT_EQ(supported.count(), 0U);
    EXPECT_FALSE(supported.contains(0x0D));
    EXPECT_TRUE(pollablePids(supported).empty());
}

TEST(SupportedPidSetTest, BitThirtyOneOfTheFirstBitmapIsPidOne) {
    SupportedPidSet supported;
    ASSERT_TRUE(supported.addBitmap(0x00, {0x80, 0x00, 0x00, 0x00}));
    EXPECT_TRUE(supported.contains(0x01));
    EXPECT_EQ(supported.count(), 1U);
}

TEST(SupportedPidSetTest, LastBitOfABitmapChainsToTheNextOne) {
    SupportedPidSet supported;
    ASSERT_TRUE(supported.addBitmap(0x00, {0x00, 0x00, 0x00, 0x01}));
    EXPECT_TRUE(supported.contains(0x20));
    EXPECT_EQ(supported.nextBitmapPid(0x00).value_or(0xFF), 0x20);
    EXPECT_FALSE(supported.nextBitmapPid(0x20).has_value());
    ASSERT_TRUE(supported.addBitmap(0x20, {0x00, 0x00, 0x00, 0x00}));
    EXPECT_FALSE(supported.nextBitmapPid(0x20).has_value());
}

TEST(SupportedPidSetTest, DecodesATypicalBitmapIntoTheDocumentedPids) {
    // BE 3F A8 13 = 1011 1110 0011 1111 1010 1000 0001 0011: PIDs 01 03 04 05 06 07 0B 0C 0D 0E 0F
    // 10 11 13 15 1C 1F 20.
    SupportedPidSet supported;
    ASSERT_TRUE(supported.addBitmap(0x00, {0xBE, 0x3F, 0xA8, 0x13}));
    const std::vector<std::uint8_t> expected = {0x01,
                                                0x03,
                                                0x04,
                                                0x05,
                                                0x06,
                                                0x07,
                                                0x0B,
                                                0x0C,
                                                0x0D,
                                                0x0E,
                                                0x0F,
                                                0x10,
                                                0x11,
                                                0x13,
                                                0x15,
                                                0x1C,
                                                0x1F,
                                                0x20};
    for (const std::uint8_t pid : expected) {
        EXPECT_TRUE(supported.contains(pid)) << static_cast<int>(pid);
    }
    EXPECT_EQ(supported.count(), expected.size());
    EXPECT_FALSE(supported.contains(0x02));
    EXPECT_FALSE(supported.contains(0x2F));
}

TEST(SupportedPidSetTest, PollablePidsAreTheSupportedOnesOfTheEightInFixedOrder) {
    SupportedPidSet supported;
    ASSERT_TRUE(supported.addBitmap(0x00, {0xBE, 0x3F, 0xA8, 0x13}));
    ASSERT_TRUE(supported.addBitmap(0x20, {0x80, 0x07, 0xA0, 0x01}));
    ASSERT_TRUE(supported.addBitmap(0x40, {0xFE, 0xD0, 0x04, 0x00}));

    const std::vector<ObdPid> expected = {
        ObdPid::VehicleSpeed,
        ObdPid::EngineRpm,
        ObdPid::CoolantTemperature,
        ObdPid::EngineLoad,
        ObdPid::ThrottlePosition,
        ObdPid::IntakeAirTemperature,
        ObdPid::ControlModuleVoltage,
        ObdPid::FuelLevel,
    };
    EXPECT_EQ(pollablePids(supported), expected);

    SupportedPidSet withoutFuel;
    ASSERT_TRUE(withoutFuel.addBitmap(0x00, {0xBE, 0x3F, 0xA8, 0x13}));
    const std::vector<ObdPid> pollable = pollablePids(withoutFuel);
    EXPECT_EQ(pollable.size(), 6U);
    EXPECT_EQ(pollable.back(), ObdPid::IntakeAirTemperature);
}

TEST(SupportedPidSetTest, WrongBitmapLengthOrBaseIsRefused) {
    SupportedPidSet supported;
    EXPECT_FALSE(supported.addBitmap(0x00, {0xBE, 0x3F, 0xA8}));
    EXPECT_FALSE(supported.addBitmap(0x00, {0xBE, 0x3F, 0xA8, 0x13, 0x00}));
    EXPECT_FALSE(supported.addBitmap(0x01, {0xBE, 0x3F, 0xA8, 0x13}));
    EXPECT_FALSE(supported.addBitmap(0x0D, {0xBE, 0x3F, 0xA8, 0x13}));
    EXPECT_EQ(supported.count(), 0U);
    EXPECT_FALSE(supported.nextBitmapPid(0x01).has_value());
    EXPECT_FALSE(supported.nextBitmapPid(0xE0).has_value());
}

TEST(SupportedPidSetTest, InsertAndContainsCoverTheWholeByteRange) {
    SupportedPidSet supported;
    supported.insert(0x00);
    supported.insert(0xFF);
    EXPECT_TRUE(supported.contains(0x00));
    EXPECT_TRUE(supported.contains(0xFF));
    EXPECT_EQ(supported.count(), 2U);
}

} // namespace
