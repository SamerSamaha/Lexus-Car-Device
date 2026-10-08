// Verifies: REQ-003

#include "lexus_head_unit/service/clock.h"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

namespace {

using lexus_head_unit::SteadyClock;

TEST(SteadyClockTest, NeverGoesBackwardsAndAdvancesWithRealTime) {
    const SteadyClock clock;
    const auto first = clock.nowMilliseconds();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    const auto second = clock.nowMilliseconds();
    EXPECT_GE(second, first + 4);
}

} // namespace
