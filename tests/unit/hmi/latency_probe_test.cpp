// The probe and the marker are the instruments of REQ-009 and REQ-013 (LHU-032); those
// requirements are met only by measurements on the Pi, so this file carries no tag.

#include "lexus_head_unit/hmi/latency_probe.h"
#include "lexus_head_unit/process_support/first_frame_marker.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"

#include <QCoreApplication>

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

// Qt declares its macros in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace {

using lexus_head_unit::FirstFrameMarker;
using lexus_head_unit::LatencyProbe;
using lexus_head_unit::SignalId;
using lexus_head_unit::SignalSample;
using lexus_head_unit::SignalStatus;

std::vector<std::string> linesOf(const std::string& path) {
    std::ifstream file(path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    return lines;
}

SignalSample sampleAt(SignalId signalId, std::int64_t timestamp, SignalStatus status) {
    SignalSample sample;
    sample.signalId = signalId;
    sample.timestampMilliseconds = timestamp;
    sample.status = status;
    return sample;
}

TEST(LatencyProbeTest, EachValidSampleIsClosedByTheFirstFrameAfterIt) {
    const std::string path = "/tmp/lexus_latency_test_" + std::to_string(::getpid()) + ".csv";
    std::int64_t now = 1000;
    {
        LatencyProbe probe(
            [&now]() {
                return now;
            },
            path);
        ASSERT_TRUE(probe.isOpen());
        probe.onSample(sampleAt(SignalId::VehicleSpeed, 990, SignalStatus::Valid));
        now = 1002;
        probe.onSample(sampleAt(SignalId::EngineRpm, 995, SignalStatus::Valid));
        probe.onSample(sampleAt(SignalId::FuelLevel, 900, SignalStatus::Stale));
        EXPECT_EQ(probe.pendingSamples(), 2U);
        probe.onFramePresentedAt(1001);
        EXPECT_EQ(probe.pendingSamples(), 1U);
        // Through the queued signal, as the render thread does it.
        now = 1016;
        probe.markFramePresented();
        QCoreApplication::processEvents();
        EXPECT_EQ(probe.pendingSamples(), 0U);
        EXPECT_EQ(probe.rowsWritten(), 2U);
    }
    const std::vector<std::string> lines = linesOf(path);
    static_cast<void>(std::remove(path.c_str()));
    const std::vector<std::string> expected = {
        LatencyProbe::header, "VehicleSpeed,990,1000,1001", "EngineRpm,995,1002,1016"};
    EXPECT_EQ(lines, expected);
}

TEST(FirstFrameMarkerTest, WritesTheTimeSinceBootOnceOnly) {
    const std::string path = "/tmp/lexus_first_frame_test_" + std::to_string(::getpid()) + ".txt";
    FirstFrameMarker marker(path);
    EXPECT_FALSE(marker.written());
    EXPECT_TRUE(marker.markFramePresented());
    EXPECT_FALSE(marker.markFramePresented());
    EXPECT_TRUE(marker.written());
    const std::vector<std::string> lines = linesOf(path);
    static_cast<void>(std::remove(path.c_str()));
    ASSERT_EQ(lines.size(), 1U);
    EXPECT_EQ(lines.front().rfind("first_frame_boottime_ms=", 0), 0U);
    const std::int64_t written = std::stoll(lines.front().substr(24));
    EXPECT_GT(written, 0);
    EXPECT_LE(written, FirstFrameMarker::bootTimeMilliseconds());
}

} // namespace

int main(int argumentCount, char** argumentValues) {
    const QCoreApplication application(argumentCount, argumentValues);
    ::testing::InitGoogleTest(&argumentCount, argumentValues);
    return RUN_ALL_TESTS();
}
// NOLINTEND(misc-include-cleaner)
