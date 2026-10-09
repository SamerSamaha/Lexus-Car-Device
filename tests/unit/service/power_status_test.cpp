// Verifies: REQ-020

#include "lexus_head_unit/service/power_status.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace {

using lexus_head_unit::currentFlagNames;
using lexus_head_unit::decodeGetThrottled;
using lexus_head_unit::PowerFlags;
using Names = std::vector<std::string>;

auto currentOf(const PowerFlags& flags) {
    return std::make_tuple(
        flags.underVoltage, flags.frequencyCapped, flags.throttled, flags.softTemperatureLimit);
}

auto occurredOf(const PowerFlags& flags) {
    return std::make_tuple(flags.underVoltageOccurred,
                           flags.frequencyCappedOccurred,
                           flags.throttledOccurred,
                           flags.softTemperatureLimitOccurred);
}

TEST(PowerStatusTest, NoFlagsAtZero) {
    const PowerFlags flags = decodeGetThrottled("throttled=0x0\n").value_or(PowerFlags{});
    EXPECT_FALSE(flags.anyCurrent());
    EXPECT_FALSE(flags.anyOccurred());
    EXPECT_EQ(currentFlagNames(flags), Names{});
    EXPECT_TRUE(decodeGetThrottled("throttled=0x0").has_value());
}

TEST(PowerStatusTest, EachCurrentBitAndEachSinceBootBitMapToItsFlag) {
    const std::array<std::string, 4> names = {
        "Under-voltage", "Frequency capped", "Throttled", "Soft temperature limit"};
    for (std::uint32_t bit = 0; bit < 4; ++bit) {
        const PowerFlags current = lexus_head_unit::powerFlagsFromValue(1U << bit);
        const PowerFlags sinceBoot = lexus_head_unit::powerFlagsFromValue(1U << (bit + 16));
        EXPECT_EQ(currentFlagNames(current), Names{names.at(bit)}) << bit;
        EXPECT_TRUE(current.anyCurrent() && !current.anyOccurred()) << bit;
        EXPECT_TRUE(!sinceBoot.anyCurrent() && sinceBoot.anyOccurred()) << bit;
    }
}

TEST(PowerStatusTest, TheLoggersExampleValue) {
    const PowerFlags flags = decodeGetThrottled("throttled=0x50005").value_or(PowerFlags{});
    EXPECT_EQ(flags.raw, 0x50005U);
    EXPECT_EQ(currentOf(flags), std::make_tuple(true, false, true, false));
    EXPECT_EQ(occurredOf(flags), std::make_tuple(true, false, true, false));
    EXPECT_EQ(currentFlagNames(flags), (Names{"Under-voltage", "Throttled"}));
}

TEST(PowerStatusTest, AnythingElseIsNotAReading) {
    for (const char* output : {"",
                               "throttled=",
                               "throttled=0x",
                               "throttled=0xZZ",
                               "throttled=50005",
                               "error=1",
                               "throttled=0x5 extra",
                               "VCHI initialization failed"}) {
        EXPECT_EQ(decodeGetThrottled(output), std::nullopt) << output;
    }
}

} // namespace
