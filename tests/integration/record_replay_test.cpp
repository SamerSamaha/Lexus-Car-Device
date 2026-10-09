// Verifies: REQ-015

#include "emulator_process.h"
#include "lexus_head_unit/hardware/elm327_obd_source.h"
#include "lexus_head_unit/hardware/elm327_source_configuration.h"
#include "lexus_head_unit/hardware/file_descriptor_byte_transport.h"
#include "lexus_head_unit/hardware/recording.h"
#include "lexus_head_unit/hardware/replay_source.h"
#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"
#include "lexus_head_unit/service/vehicle_data_source.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <tuple>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {

using lexus_head_unit::ConnectionState;
using lexus_head_unit::ConnectionTransition;
using lexus_head_unit::Elm327ObdSource;
using lexus_head_unit::Elm327SourceConfiguration;
using lexus_head_unit::FileDescriptorByteTransport;
using lexus_head_unit::RecordingByteTransport;
using lexus_head_unit::ReplaySource;
using lexus_head_unit::ReplayTiming;
using lexus_head_unit::SignalSample;
using lexus_head_unit::SteadyClock;
using lexus_head_unit::testing::EmulatorProcess;

constexpr std::size_t liveSampleTarget = 40;

class Capture final : public lexus_head_unit::VehicleDataSourceListener {
public:
    void onSample(const SignalSample& sample) override {
        samples.push_back(sample);
    }
    void onConnectionChanged(const ConnectionTransition& transition) override {
        transitions.push_back(transition);
    }
    std::vector<SignalSample> samples;
    std::vector<ConnectionTransition> transitions;
};

using SampleFields = std::
    tuple<lexus_head_unit::SignalId, double, lexus_head_unit::Unit, lexus_head_unit::SignalStatus>;
using TransitionFields =
    std::tuple<ConnectionState, lexus_head_unit::ConnectionTrigger, ConnectionState>;

std::vector<SampleFields> fieldsOf(const std::vector<SignalSample>& samples) {
    std::vector<SampleFields> fields;
    fields.reserve(samples.size());
    for (const SignalSample& sample : samples) {
        fields.emplace_back(sample.signalId, sample.value, sample.unit, sample.status);
    }
    return fields;
}

std::vector<TransitionFields> fieldsOf(const std::vector<ConnectionTransition>& transitions) {
    std::vector<TransitionFields> fields;
    fields.reserve(transitions.size());
    for (const ConnectionTransition& transition : transitions) {
        fields.emplace_back(transition.from, transition.trigger, transition.to);
    }
    return fields;
}

// Calls runOnce until the condition holds or the wall-clock limit passes.
bool runUntil(lexus_head_unit::VehicleDataSource& source,
              const std::function<bool()>& condition,
              int limitMilliseconds) {
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(limitMilliseconds);
    while (!condition() && std::chrono::steady_clock::now() < deadline) {
        source.runOnce();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return condition();
}

// A live session against the emulator through the recorder: connect, poll until enough
// samples, lose the link, fail one reconnect attempt, stop.
void recordLiveSession(const std::string& path,
                       const Elm327SourceConfiguration& configuration,
                       Capture& live) {
    EmulatorProcess emulator;
    if (!emulator.start()) {
        ADD_FAILURE() << "emulator did not start: " << emulator.lastError();
        return;
    }
    const SteadyClock clock;
    FileDescriptorByteTransport device(emulator.linkPath());
    RecordingByteTransport recorder(device, clock, path);
    Elm327ObdSource source(recorder, clock, configuration);
    source.start(live);
    EXPECT_TRUE(runUntil(
        source,
        [&live]() {
            return live.samples.size() >= liveSampleTarget;
        },
        20000));
    emulator.kill();
    EXPECT_TRUE(runUntil(
        source,
        [&source]() {
            return source.connectionState() == ConnectionState::Error;
        },
        5000));
    const std::uint64_t attemptsBefore = source.connectionAttempts();
    EXPECT_TRUE(runUntil(
        source,
        [&source, attemptsBefore]() {
            return source.connectionAttempts() > attemptsBefore;
        },
        3000));
    source.stop();
    std::cout << "live session: " << live.samples.size() << " samples, " << live.transitions.size()
              << " transitions, " << recorder.eventCount() << " recorded events\n";
}

// Plays a recording back as fast as possible into the capture; the source is left stopped.
void replayInto(lexus_head_unit::RecordingFile recording,
                const Elm327SourceConfiguration& configuration,
                Capture& replayed,
                std::uint64_t& divergences,
                bool& finished) {
    ReplaySource replay(std::move(recording), configuration, ReplayTiming::Fast);
    replay.start(replayed);
    for (int run = 0; run < 1000000 && !replay.finished(); ++run) {
        replay.runOnce();
    }
    finished = replay.finished();
    divergences = replay.transport().divergenceCount();
    if (divergences > 0) {
        std::cout << "first divergence: " << replay.transport().lastDivergence() << "\n";
    }
    replay.stop();
}

std::size_t countSameTimestamps(const std::vector<SignalSample>& left,
                                const std::vector<SignalSample>& right) {
    std::size_t same = 0;
    for (std::size_t index = 0; index < left.size() && index < right.size(); ++index) {
        if (left.at(index).timestampMilliseconds == right.at(index).timestampMilliseconds) {
            ++same;
        }
    }
    return same;
}

TEST(RecordReplayTest, ReplayReproducesTheLiveSamplesAndTransitionsInOrder) {
    const std::string path = "/tmp/lexus_record_replay_" + std::to_string(::getpid()) + ".rec";
    Elm327SourceConfiguration configuration;
    configuration.devicePath = "/unused/in/replay";
    Capture live;
    recordLiveSession(path, configuration, live);
    ASSERT_GE(live.samples.size(), liveSampleTarget);

    auto recording = lexus_head_unit::readRecording(path);
    static_cast<void>(std::remove(path.c_str()));
    ASSERT_TRUE(recording.errors.empty()) << recording.errors.front();
    Capture replayed;
    std::uint64_t divergences = 0;
    bool finished = false;
    replayInto(std::move(recording), configuration, replayed, divergences, finished);
    EXPECT_EQ(std::make_pair(finished, divergences), std::make_pair(true, std::uint64_t{0}));

    EXPECT_EQ(fieldsOf(replayed.samples), fieldsOf(live.samples));
    EXPECT_EQ(fieldsOf(replayed.transitions), fieldsOf(live.transitions));
    const std::size_t sameTimestamp = countSameTimestamps(live.samples, replayed.samples);
    std::cout << "replay: " << replayed.samples.size() << " samples, " << sameTimestamp
              << " with the recorded timestamp exactly\n";
}

TEST(RecordReplayTest, MalformedRecordingNeverStarts) {
    ReplaySource replay(lexus_head_unit::parseRecording("1 OPEN_OK\nnonsense\n"),
                        Elm327SourceConfiguration{},
                        ReplayTiming::Fast);
    Capture capture;
    replay.start(capture);
    EXPECT_EQ(replay.connectionState(), ConnectionState::Disconnected);
    EXPECT_EQ(replay.recordingErrors().size(), 1U);
    EXPECT_TRUE(capture.transitions.empty());
}

} // namespace
