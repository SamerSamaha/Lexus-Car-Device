// Verifies: REQ-008

#include "lexus_head_unit/service/reconnect_backoff.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace {

using lexus_head_unit::ReconnectBackoff;

TEST(ReconnectBackoffTest, DefaultScheduleIsOneTwoFourEightThenTenSeconds) {
    ReconnectBackoff backoff;
    std::vector<std::int64_t> delays;
    delays.reserve(7);
    for (int attempt = 0; attempt < 7; ++attempt) {
        delays.push_back(backoff.nextDelayMilliseconds());
    }
    EXPECT_EQ(delays, (std::vector<std::int64_t>{1000, 2000, 4000, 8000, 10000, 10000, 10000}));
    EXPECT_EQ(backoff.attemptCount(), 7U);
}

TEST(ReconnectBackoffTest, ResetStartsTheScheduleOver) {
    ReconnectBackoff backoff;
    backoff.nextDelayMilliseconds();
    backoff.nextDelayMilliseconds();
    backoff.reset();
    EXPECT_EQ(backoff.attemptCount(), 0U);
    EXPECT_EQ(backoff.nextDelayMilliseconds(), 1000);
}

TEST(ReconnectBackoffTest, CustomScheduleIsCappedByTheCap) {
    ReconnectBackoff backoff({500, 30000}, 5000);
    EXPECT_EQ(backoff.nextDelayMilliseconds(), 500);
    EXPECT_EQ(backoff.nextDelayMilliseconds(), 5000);
    EXPECT_EQ(backoff.nextDelayMilliseconds(), 5000);
    EXPECT_EQ(backoff.capMilliseconds(), 5000);
    EXPECT_EQ(backoff.scheduleMilliseconds().size(), 2U);
}

} // namespace
