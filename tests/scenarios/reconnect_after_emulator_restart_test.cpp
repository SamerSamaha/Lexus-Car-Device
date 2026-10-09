// Verifies: REQ-008

#include "emulator_process.h"
#include "lexus_head_unit/hardware/elm327_obd_source.h"
#include "lexus_head_unit/hardware/elm327_source_configuration.h"
#include "lexus_head_unit/hardware/file_descriptor_byte_transport.h"
#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/signal_store.h"
#include "lexus_head_unit/service/signal_store_feeder.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

namespace {

using lexus_head_unit::ConnectionState;
using lexus_head_unit::ConnectionTransition;
using lexus_head_unit::ConnectionTrigger;
using lexus_head_unit::Elm327ObdSource;
using lexus_head_unit::Elm327SourceConfiguration;
using lexus_head_unit::FileDescriptorByteTransport;
using lexus_head_unit::SignalStore;
using lexus_head_unit::SignalStoreFeeder;
using lexus_head_unit::SteadyClock;
using lexus_head_unit::testing::EmulatorProcess;

constexpr int trialCount = 20;
constexpr std::int64_t detectionLimitMilliseconds = 2000;
constexpr std::int64_t recoveryLimitMilliseconds = 15000;

// Sleeps as the worker loop would: at most 50 ms, at least 1 ms.
void sleepForHint(std::int64_t hintMilliseconds) {
    const std::int64_t bounded = std::clamp<std::int64_t>(hintMilliseconds, 1, 50);
    std::this_thread::sleep_for(std::chrono::milliseconds(bounded));
}

// Runs the worker loop as the application does until the predicate holds or the limit passes.
template <typename Predicate>
std::int64_t runUntil(Elm327ObdSource& source,
                      const SteadyClock& clock,
                      std::int64_t limitMilliseconds,
                      Predicate predicate) {
    const std::int64_t startedAt = clock.nowMilliseconds();
    while (!predicate()) {
        source.runOnce();
        if (predicate()) {
            break;
        }
        if (clock.nowMilliseconds() - startedAt > limitMilliseconds) {
            break;
        }
        sleepForHint(source.idleHintMilliseconds());
    }
    return clock.nowMilliseconds() - startedAt;
}

struct TrialTimes {
    std::vector<std::int64_t> detectionMilliseconds;
    std::vector<std::int64_t> recoveryMilliseconds;
};

// Kills and restarts the emulator trialCount times, recording detection and recovery times.
TrialTimes runTrials(EmulatorProcess& emulator, Elm327ObdSource& source, const SteadyClock& clock) {
    TrialTimes times;
    for (int trial = 0; trial < trialCount; ++trial) {
        emulator.kill();
        times.detectionMilliseconds.push_back(
            runUntil(source, clock, detectionLimitMilliseconds + 1000, [&source]() {
                return source.connectionState() == ConnectionState::Error;
            }));
        if (!emulator.start()) {
            ADD_FAILURE() << emulator.lastError();
            return times;
        }
        times.recoveryMilliseconds.push_back(
            runUntil(source, clock, recoveryLimitMilliseconds + 1000, [&source]() {
                return source.connectionState() == ConnectionState::Connected;
            }));
    }
    return times;
}

// The differences between consecutive attempt times, for the first count attempts.
std::vector<std::int64_t> intervalsOf(const std::vector<std::int64_t>& attemptTimes,
                                      std::size_t count) {
    std::vector<std::int64_t> intervals;
    for (std::size_t index = 0; index + 1 < attemptTimes.size() && index + 1 < count; ++index) {
        intervals.push_back(attemptTimes.at(index + 1) - attemptTimes.at(index));
    }
    return intervals;
}

// Each interval must be within 10% of the expected schedule value.
void checkIntervals(const std::vector<std::int64_t>& intervals,
                    const std::vector<std::int64_t>& expected) {
    ASSERT_EQ(intervals.size(), expected.size()) << ::testing::PrintToString(intervals);
    for (std::size_t index = 0; index < intervals.size(); ++index) {
        const auto tolerance = static_cast<double>(expected.at(index)) / 10.0;
        EXPECT_NEAR(static_cast<double>(intervals.at(index)),
                    static_cast<double>(expected.at(index)),
                    tolerance)
            << "interval " << index << " of " << ::testing::PrintToString(intervals);
        ::testing::Test::RecordProperty("interval_ms_" + std::to_string(index),
                                        static_cast<int>(intervals.at(index)));
    }
}

TEST(ReconnectAfterEmulatorRestartTest,
     TwentyKillsAreDetectedWithinTwoSecondsAndRecoveredWithinFifteen) {
    EmulatorProcess emulator;
    ASSERT_TRUE(emulator.start()) << emulator.lastError();
    FileDescriptorByteTransport transport(emulator.linkPath());
    const SteadyClock clock;
    Elm327SourceConfiguration configuration;
    configuration.devicePath = emulator.linkPath();
    Elm327ObdSource source(transport, clock, configuration);
    SignalStore store;
    SignalStoreFeeder feeder(store);
    source.start(feeder);
    ASSERT_EQ(source.connectionState(), ConnectionState::Connected);

    const TrialTimes times = runTrials(emulator, source, clock);
    source.stop();

    ASSERT_EQ(times.detectionMilliseconds.size(), static_cast<std::size_t>(trialCount));
    ASSERT_EQ(times.recoveryMilliseconds.size(), static_cast<std::size_t>(trialCount));
    const auto detectedInTime =
        static_cast<int>(std::count_if(times.detectionMilliseconds.begin(),
                                       times.detectionMilliseconds.end(),
                                       [](std::int64_t value) {
                                           return value <= detectionLimitMilliseconds;
                                       }));
    const auto recoveredInTime =
        static_cast<int>(std::count_if(times.recoveryMilliseconds.begin(),
                                       times.recoveryMilliseconds.end(),
                                       [](std::int64_t value) {
                                           return value <= recoveryLimitMilliseconds;
                                       }));
    for (std::size_t trial = 0; trial < times.detectionMilliseconds.size(); ++trial) {
        RecordProperty("detection_ms_trial_" + std::to_string(trial),
                       static_cast<int>(times.detectionMilliseconds.at(trial)));
        RecordProperty("recovery_ms_trial_" + std::to_string(trial),
                       static_cast<int>(times.recoveryMilliseconds.at(trial)));
    }
    EXPECT_EQ(detectedInTime, trialCount) << ::testing::PrintToString(times.detectionMilliseconds);
    EXPECT_EQ(recoveredInTime, trialCount) << ::testing::PrintToString(times.recoveryMilliseconds);
}

TEST(ReconnectAfterEmulatorRestartTest, RetryIntervalsFollowOneTwoFourEightTenTenSeconds) {
    EmulatorProcess emulator;
    ASSERT_TRUE(emulator.start()) << emulator.lastError();
    FileDescriptorByteTransport transport(emulator.linkPath());
    const SteadyClock clock;
    Elm327SourceConfiguration configuration;
    configuration.devicePath = emulator.linkPath();
    Elm327ObdSource source(transport, clock, configuration);
    SignalStore store;
    SignalStoreFeeder feeder(store);
    std::vector<std::int64_t> attemptTimes;
    feeder.setTransitionHook([&attemptTimes](const ConnectionTransition& transition) {
        if (transition.trigger == ConnectionTrigger::HandshakeFailed) {
            attemptTimes.push_back(transition.timestampMilliseconds);
        }
    });
    source.start(feeder);
    ASSERT_EQ(source.connectionState(), ConnectionState::Connected);

    emulator.kill();
    runUntil(source, clock, 3000, [&source]() {
        return source.connectionState() == ConnectionState::Error;
    });
    ASSERT_EQ(source.connectionState(), ConnectionState::Error);
    // Stay dead long enough for six failed attempts: 1 + 2 + 4 + 8 + 10 + 10 = 35 s.
    runUntil(source, clock, 37000, [&attemptTimes]() {
        return attemptTimes.size() >= 6;
    });
    source.stop();

    ASSERT_GE(attemptTimes.size(), 6U) << ::testing::PrintToString(attemptTimes);
    const std::vector<std::int64_t> intervals = intervalsOf(attemptTimes, 6);
    checkIntervals(intervals, {2000, 4000, 8000, 10000, 10000});
}

} // namespace
