// Verifies: REQ-015

#include "lexus_head_unit/hardware/byte_transport.h"
#include "lexus_head_unit/hardware/fake_byte_transport.h"
#include "lexus_head_unit/hardware/recording.h"
#include "lexus_head_unit/service/manual_clock.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <tuple>
#include <unistd.h>
#include <vector>

namespace {

using lexus_head_unit::FakeByteTransport;
using lexus_head_unit::ManualClock;
using lexus_head_unit::parseRecording;
using lexus_head_unit::ReadResult;
using lexus_head_unit::ReadStatus;
using lexus_head_unit::RecordingByteTransport;
using lexus_head_unit::RecordingEvent;
using lexus_head_unit::RecordingEventKind;
using lexus_head_unit::ReplayByteTransport;
using lexus_head_unit::ReplayClock;
using Bytes = std::vector<std::uint8_t>;

Bytes bytesOf(const std::string& text) {
    return {text.begin(), text.end()};
}

RecordingEvent eventOf(std::int64_t time, RecordingEventKind kind, const std::string& text = "") {
    RecordingEvent event;
    event.timestampMilliseconds = time;
    event.kind = kind;
    event.bytes = bytesOf(text);
    event.acceptedCount = event.bytes.size();
    return event;
}

auto fieldsOf(const RecordingEvent& event) {
    return std::make_tuple(event.timestampMilliseconds, event.kind, event.bytes);
}

TEST(RecordingFormatTest, EveryEventKindRoundTripsThroughText) {
    const std::vector<RecordingEvent> events = {
        eventOf(100, RecordingEventKind::OpenOk),
        eventOf(101, RecordingEventKind::OpenFail),
        eventOf(102, RecordingEventKind::Write, "ATZ\r"),
        eventOf(150, RecordingEventKind::Read, "ELM327 v1.5\r\r>"),
        eventOf(151, RecordingEventKind::Read, ""),
        eventOf(1200, RecordingEventKind::ReadTimeout),
        eventOf(1300, RecordingEventKind::ReadClosed),
        eventOf(1301, RecordingEventKind::ReadError),
        eventOf(1302, RecordingEventKind::Close),
    };
    std::string text = std::string(lexus_head_unit::recordingHeader) + "\n";
    for (const RecordingEvent& event : events) {
        text += lexus_head_unit::formatRecordingEvent(event) + "\n";
    }
    EXPECT_NE(text.find("102 WRITE 41545a0d 4\n"), std::string::npos) << text;
    const auto parsed = parseRecording(text);
    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.events.size(), events.size());
    for (std::size_t index = 0; index < events.size(); ++index) {
        EXPECT_EQ(fieldsOf(parsed.events.at(index)), fieldsOf(events.at(index))) << index;
    }
}

TEST(RecordingFormatTest, MalformedLinesAreNumberedErrorsAndTheRestParses) {
    const auto parsed = parseRecording("# header\n10 OPEN_OK\nx READ 41\n20 JUMP\n30 READ 4\n40 "
                                       "READ zz\n\n50 READ 41\n60 WRITE 41 2\n70 WRITE 4142 0\n");
    EXPECT_EQ(parsed.errors,
              (std::vector<std::string>{"line 3: malformed event",
                                        "line 4: malformed event",
                                        "line 5: malformed hex bytes",
                                        "line 6: malformed hex bytes",
                                        "line 9: malformed accepted count"}));
    ASSERT_EQ(parsed.events.size(), 3U);
    EXPECT_EQ(parsed.events.back().acceptedCount, 0U);
    EXPECT_EQ(parsed.events.back().bytes.size(), 2U);
    EXPECT_EQ(lexus_head_unit::readRecording("/nonexistent.rec").errors.front(),
              "cannot open /nonexistent.rec");
}

TEST(RecordingTransportTest, RecorderWritesWhatTheTransportDidAndReplayGivesItBack) {
    const std::string path = "/tmp/lexus_recording_test_" + std::to_string(::getpid()) + ".rec";
    ManualClock clock{1000};
    FakeByteTransport fake;
    fake.queueBytes("ELM327 v1.5\r\r>");
    {
        RecordingByteTransport recorder(fake, clock, path);
        ASSERT_TRUE(recorder.fileIsOpen());
        ASSERT_TRUE(recorder.open());
        clock.advanceMilliseconds(5);
        EXPECT_EQ(recorder.write(bytesOf("ATZ\r")), 4U);
        Bytes buffer;
        const ReadResult first = recorder.read(buffer, 64, 100);
        EXPECT_EQ(first.status, ReadStatus::Ok);
        clock.advanceMilliseconds(100);
        EXPECT_EQ(recorder.read(buffer, 64, 100).status, ReadStatus::Timeout);
        recorder.close();
        EXPECT_EQ(recorder.eventCount(), 5U);
        EXPECT_EQ(recorder.fileWriteFailures(), 0U);
    }
    const auto recording = lexus_head_unit::readRecording(path);
    static_cast<void>(std::remove(path.c_str()));
    ASSERT_TRUE(recording.errors.empty());
    ASSERT_EQ(recording.events.size(), 5U);
    EXPECT_EQ(recording.events.at(2).bytes, bytesOf("ELM327 v1.5\r\r>"));

    ReplayClock replayClock;
    ReplayByteTransport replay(recording.events, replayClock);
    EXPECT_EQ(replayClock.nowMilliseconds(), 1000);
    ASSERT_TRUE(replay.open());
    EXPECT_EQ(replay.write(bytesOf("ATZ\r")), 4U);
    EXPECT_EQ(replayClock.nowMilliseconds(), 1005);
    Bytes buffer;
    EXPECT_EQ(replay.read(buffer, 6, 100).byteCount, 6U);
    EXPECT_EQ(buffer, bytesOf("ELM327"));
    EXPECT_EQ(replay.read(buffer, 64, 100).byteCount, 8U);
    EXPECT_EQ(buffer, bytesOf(" v1.5\r\r>"));
    EXPECT_EQ(replay.read(buffer, 64, 100).status, ReadStatus::Timeout);
    EXPECT_EQ(replayClock.nowMilliseconds(), 1105);
    replay.close();
    EXPECT_TRUE(replay.finished());
    EXPECT_EQ(replay.read(buffer, 64, 100).status, ReadStatus::Closed);
    EXPECT_EQ(replay.divergenceCount(), 0U);
}

TEST(RecordingTransportTest, ADifferentWriteIsADivergenceAndReadsThenFail) {
    ReplayClock clock;
    ReplayByteTransport replay({eventOf(1, RecordingEventKind::OpenOk),
                                eventOf(2, RecordingEventKind::Write, "0100\r"),
                                eventOf(3, RecordingEventKind::Read, "41 00 BE 1F A8 13\r>")},
                               clock);
    ASSERT_TRUE(replay.open());
    EXPECT_EQ(replay.write(bytesOf("0120\r")), 0U);
    EXPECT_EQ(replay.divergenceCount(), 1U);
    EXPECT_EQ(replay.lastDivergence(), "write of 5 bytes where the recording has WRITE of 5");
    Bytes buffer;
    EXPECT_EQ(replay.read(buffer, 64, 100).status, ReadStatus::Error);
    EXPECT_FALSE(replay.open());
}

TEST(RecordingTransportTest, ReadWhereTheRecordingHasAWriteIsADivergence) {
    ReplayClock clock;
    ReplayByteTransport replay(
        {eventOf(1, RecordingEventKind::OpenOk), eventOf(2, RecordingEventKind::Write, "ATZ\r")},
        clock);
    ASSERT_TRUE(replay.open());
    Bytes buffer;
    EXPECT_EQ(replay.read(buffer, 64, 0).status, ReadStatus::Error);
    EXPECT_EQ(replay.lastDivergence(), "read() where the recording has WRITE");
    ReplayClock otherClock;
    ReplayByteTransport openMismatch({eventOf(1, RecordingEventKind::Close)}, otherClock);
    EXPECT_FALSE(openMismatch.open());
    EXPECT_EQ(openMismatch.divergenceCount(), 1U);
}

} // namespace
