// Verifies: REQ-024

#include "lexus_head_unit/hub/ignition_off_shutdown_policy.h"
#include "lexus_head_unit/service/link_detail.h"

#include <gtest/gtest.h>

#include <cstdint>

namespace {

using lexus_head_unit::IgnitionOffShutdownPhase;
using lexus_head_unit::IgnitionOffShutdownPolicy;
using lexus_head_unit::IgnitionOffShutdownSettings;
using lexus_head_unit::LinkDetail;

constexpr std::int64_t quiet = 300000;
constexpr std::int64_t countdown = 60000;

IgnitionOffShutdownPolicy livePolicyAt(std::int64_t liveAt) {
    IgnitionOffShutdownPolicy policy(IgnitionOffShutdownSettings{quiet, countdown});
    policy.onLinkDetail(LinkDetail::Live, liveAt);
    return policy;
}

TEST(IgnitionOffShutdownPolicyTest, NothingHappensBeforeTheVehicleHasBeenLive) {
    IgnitionOffShutdownPolicy policy(IgnitionOffShutdownSettings{quiet, countdown});
    EXPECT_EQ(policy.phase(), IgnitionOffShutdownPhase::WaitingForLive);
    policy.onLinkDetail(LinkDetail::AdapterWithoutVehicle, 0);
    policy.onLinkDetail(LinkDetail::SearchingForAdapter, 0);
    EXPECT_EQ(policy.update(10 * quiet), IgnitionOffShutdownPhase::WaitingForLive);
}

TEST(IgnitionOffShutdownPolicyTest, CountdownStartsAtTheQuietTimeAndShutdownIsDueOnceAtItsEnd) {
    IgnitionOffShutdownPolicy policy = livePolicyAt(0);
    policy.onLinkDetail(LinkDetail::AdapterWithoutVehicle, 1000);
    EXPECT_EQ(policy.update(1000 + quiet - 1), IgnitionOffShutdownPhase::Quiet);
    EXPECT_EQ(policy.update(1000 + quiet), IgnitionOffShutdownPhase::CountingDown);
    EXPECT_EQ(policy.millisecondsRemaining(1000 + quiet), countdown);
    EXPECT_EQ(policy.millisecondsRemaining(1000 + quiet + 59500), 500);
    EXPECT_EQ(policy.update(1000 + quiet + countdown - 1), IgnitionOffShutdownPhase::CountingDown);
    EXPECT_EQ(policy.update(1000 + quiet + countdown), IgnitionOffShutdownPhase::ShutdownDue);
    EXPECT_EQ(policy.update(1000 + quiet + countdown + 1), IgnitionOffShutdownPhase::Done);
    policy.onLinkDetail(LinkDetail::Live, 2 * quiet);
    EXPECT_EQ(policy.phase(), IgnitionOffShutdownPhase::Done);
}

TEST(IgnitionOffShutdownPolicyTest, LiveDataDuringTheQuietTimeOrTheCountdownStartsAgain) {
    IgnitionOffShutdownPolicy policy = livePolicyAt(0);
    policy.onLinkDetail(LinkDetail::AdapterWithoutVehicle, 0);
    policy.onLinkDetail(LinkDetail::Live, quiet - 1);
    EXPECT_EQ(policy.update(quiet + 1), IgnitionOffShutdownPhase::Live);
    policy.onLinkDetail(LinkDetail::LinkLostRetrying, quiet + 1);
    EXPECT_EQ(policy.update((2 * quiet) + 1), IgnitionOffShutdownPhase::CountingDown);
    policy.onLinkDetail(LinkDetail::Live, (2 * quiet) + 2);
    EXPECT_EQ(policy.update(3 * quiet), IgnitionOffShutdownPhase::Live);
    EXPECT_EQ(policy.millisecondsRemaining(3 * quiet), 0);
}

TEST(IgnitionOffShutdownPolicyTest, ARepeatedQuietDetailDoesNotRestartTheQuietTime) {
    IgnitionOffShutdownPolicy policy = livePolicyAt(0);
    policy.onLinkDetail(LinkDetail::AdapterWithoutVehicle, 0);
    policy.onLinkDetail(LinkDetail::LinkLostRetrying, quiet / 2);
    policy.onLinkDetail(LinkDetail::Idle, quiet - 1);
    EXPECT_EQ(policy.update(quiet), IgnitionOffShutdownPhase::CountingDown);
}

TEST(IgnitionOffShutdownPolicyTest, CancelHoldsUntilTheVehicleIsLiveAgain) {
    IgnitionOffShutdownPolicy policy = livePolicyAt(0);
    policy.cancel(); // not counting down: no effect
    EXPECT_EQ(policy.phase(), IgnitionOffShutdownPhase::Live);
    policy.onLinkDetail(LinkDetail::AdapterWithoutVehicle, 0);
    ASSERT_EQ(policy.update(quiet), IgnitionOffShutdownPhase::CountingDown);
    policy.cancel();
    EXPECT_EQ(policy.update(10 * quiet), IgnitionOffShutdownPhase::Cancelled);
    policy.onLinkDetail(LinkDetail::AdapterWithoutVehicle, 10 * quiet);
    EXPECT_EQ(policy.update(20 * quiet), IgnitionOffShutdownPhase::Cancelled);
    policy.onLinkDetail(LinkDetail::Live, 20 * quiet);
    policy.onLinkDetail(LinkDetail::AdapterWithoutVehicle, 20 * quiet);
    EXPECT_EQ(policy.update(21 * quiet), IgnitionOffShutdownPhase::CountingDown);
}

TEST(IgnitionOffShutdownPolicyTest, AQuietTimeOfZeroTurnsThePolicyOff) {
    IgnitionOffShutdownPolicy policy(IgnitionOffShutdownSettings{0, countdown});
    policy.onLinkDetail(LinkDetail::Live, 0);
    policy.onLinkDetail(LinkDetail::AdapterWithoutVehicle, 0);
    EXPECT_EQ(policy.update(100 * quiet), IgnitionOffShutdownPhase::Disabled);
    EXPECT_EQ(toString(IgnitionOffShutdownPhase::Disabled), "Disabled");
}

TEST(IgnitionOffShutdownPolicyTest, DefaultsAreFiveMinutesAndOneMinute) {
    const IgnitionOffShutdownSettings defaults;
    EXPECT_EQ(defaults.quietMilliseconds, quiet);
    EXPECT_EQ(defaults.countdownMilliseconds, countdown);
}

} // namespace
