// Verifies: REQ-022, REQ-015

#include "emulator_process.h"
#include "lexus_head_unit/hardware/elm327_obd_source.h"
#include "lexus_head_unit/hardware/elm327_source_configuration.h"
#include "lexus_head_unit/hardware/file_descriptor_byte_transport.h"
#include "lexus_head_unit/hardware/recording.h"
#include "lexus_head_unit/hardware/replay_source.h"
#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/derived_signal_engine.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"
#include "lexus_head_unit/service/signal_store.h"
#include "lexus_head_unit/service/signal_store_feeder.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>
#include <tuple>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {

using lexus_head_unit::DerivedSignalEngine;
using lexus_head_unit::Elm327ObdSource;
using lexus_head_unit::Elm327SourceConfiguration;
using lexus_head_unit::FileDescriptorByteTransport;
using lexus_head_unit::RecordingByteTransport;
using lexus_head_unit::ReplaySource;
using lexus_head_unit::ReplayTiming;
using lexus_head_unit::SignalId;
using lexus_head_unit::SignalSample;
using lexus_head_unit::SignalStore;
using lexus_head_unit::SignalStoreFeeder;
using lexus_head_unit::SteadyClock;
using lexus_head_unit::testing::EmulatorProcess;

constexpr std::uint64_t liveSampleTarget = 300;

// Everything a trip ends with: the engine totals and the last derived sample of each signal.
struct TripTotals {
    double distanceKilometres = 0.0;
    double fuelLitres = 0.0;
    std::vector<std::int64_t> bandMilliseconds;
    std::uint64_t gapsNotIntegrated = 0;
    std::uint64_t derivedSamples = 0;
    std::vector<std::tuple<SignalId, double, std::int64_t>> lastDerived;
};

TripTotals totalsOf(const SignalStoreFeeder& feeder, const SignalStore& store) {
    const DerivedSignalEngine& engine = feeder.derivedSignalEngine();
    TripTotals totals;
    totals.distanceKilometres = engine.tripDistanceKilometres();
    totals.fuelLitres = engine.tripFuelLitres();
    for (std::size_t band = 0; band < lexus_head_unit::rpmBandCount; ++band) {
        totals.bandMilliseconds.push_back(engine.rpmBandMilliseconds(band));
    }
    totals.gapsNotIntegrated = engine.gapsNotIntegrated();
    totals.derivedSamples = feeder.derivedSampleCount();
    for (const SignalId signalId : lexus_head_unit::derivedSignalIds) {
        const SignalSample& sample = store.latest(signalId);
        totals.lastDerived.emplace_back(signalId, sample.value, sample.timestampMilliseconds);
    }
    return totals;
}

void recordLiveSession(const std::string& path, const Elm327SourceConfiguration& configuration) {
    EmulatorProcess emulator;
    if (!emulator.start()) {
        ADD_FAILURE() << "emulator did not start: " << emulator.lastError();
        return;
    }
    const SteadyClock clock;
    FileDescriptorByteTransport device(emulator.linkPath());
    RecordingByteTransport recorder(device, clock, path);
    Elm327ObdSource source(recorder, clock, configuration);
    SignalStore store;
    SignalStoreFeeder feeder(store);
    source.start(feeder);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (feeder.acceptedSampleCount() < liveSampleTarget &&
           std::chrono::steady_clock::now() < deadline) {
        source.runOnce();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    EXPECT_GE(feeder.acceptedSampleCount(), liveSampleTarget);
    // End as the car does when the adapter goes away, so the recording ends on a closed link
    // rather than in the middle of an exchange.
    emulator.kill();
    const auto lossDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (source.connectionState() != lexus_head_unit::ConnectionState::Error &&
           std::chrono::steady_clock::now() < lossDeadline) {
        source.runOnce();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    source.stop();
}

TripTotals replayOnce(const std::string& path, const Elm327SourceConfiguration& configuration) {
    auto recording = lexus_head_unit::readRecording(path);
    EXPECT_TRUE(recording.errors.empty());
    ReplaySource replay(std::move(recording), configuration, ReplayTiming::Fast);
    SignalStore store;
    SignalStoreFeeder feeder(store);
    replay.start(feeder);
    for (int run = 0; run < 1000000 && !replay.finished(); ++run) {
        replay.runOnce();
    }
    EXPECT_TRUE(replay.finished());
    EXPECT_EQ(replay.transport().divergenceCount(), 0U) << replay.transport().lastDivergence();
    replay.stop();
    return totalsOf(feeder, store);
}

void expectNonZero(const TripTotals& totals) {
    EXPECT_GT(totals.distanceKilometres, 0.0);
    EXPECT_GT(totals.fuelLitres, 0.0);
    std::int64_t bandSum = 0;
    for (const std::int64_t band : totals.bandMilliseconds) {
        bandSum += band;
    }
    EXPECT_GT(bandSum, 0);
    EXPECT_GT(totals.derivedSamples, 0U);
}

TEST(TripReplayTest, ReplayingTheSameRecordingTwiceGivesIdenticalTotals) {
    const std::string path = "/tmp/lexus_trip_replay_" + std::to_string(::getpid()) + ".rec";
    Elm327SourceConfiguration configuration;
    configuration.devicePath = "/unused/in/replay";
    recordLiveSession(path, configuration);

    const TripTotals first = replayOnce(path, configuration);
    const TripTotals second = replayOnce(path, configuration);
    static_cast<void>(std::remove(path.c_str()));

    expectNonZero(first);
    // Exact equality, not a tolerance: only recorded timestamps feed the arithmetic.
    EXPECT_EQ(first.distanceKilometres, second.distanceKilometres);
    EXPECT_EQ(first.fuelLitres, second.fuelLitres);
    EXPECT_EQ(first.bandMilliseconds, second.bandMilliseconds);
    EXPECT_EQ(first.gapsNotIntegrated, second.gapsNotIntegrated);
    EXPECT_EQ(first.derivedSamples, second.derivedSamples);
    EXPECT_EQ(first.lastDerived, second.lastDerived);
    std::cout << "trip replay: " << first.distanceKilometres << " km, " << first.fuelLitres
              << " L, " << first.derivedSamples << " derived samples, " << first.gapsNotIntegrated
              << " gaps\n";
}

} // namespace
