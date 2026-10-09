#pragma once

#include "lexus_head_unit/hardware/byte_transport.h"
#include "lexus_head_unit/service/clock.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lexus_head_unit {

// One line of a recording (DN-029): "<milliseconds> <event> [<hex bytes>]"; a WRITE line also
// ends with the number of bytes the transport accepted, which can be fewer than were offered
// (a dead device accepts 0).
enum class RecordingEventKind {
    OpenOk,
    OpenFail,
    Close,
    Write,
    Read,
    ReadTimeout,
    ReadClosed,
    ReadError,
};

struct RecordingEvent {
    std::int64_t timestampMilliseconds = 0;
    RecordingEventKind kind = RecordingEventKind::Read;
    // READ: the bytes received. WRITE: the bytes offered.
    std::vector<std::uint8_t> bytes;
    // WRITE only: how many of them the transport accepted.
    std::size_t acceptedCount = 0;
};

struct RecordingFile {
    std::vector<RecordingEvent> events;
    // "line 7: ..." texts; empty when the file parsed cleanly.
    std::vector<std::string> errors;
};

inline constexpr std::string_view recordingHeader = "# lexus-head-unit recording 1 source=elm327";

std::string_view toString(RecordingEventKind kind);
std::optional<RecordingEventKind> recordingEventKindFromText(std::string_view text);
std::string formatRecordingEvent(const RecordingEvent& event);
// Parses text (the whole file) into events; never throws.
RecordingFile parseRecording(std::string_view text);
RecordingFile readRecording(const std::string& path);

// A ByteTransport decorator that appends every call and its result to a recording file, then
// returns what the wrapped transport returned. The file is flushed after every event so a
// crash loses at most the last one.
class RecordingByteTransport final : public ByteTransport {
public:
    RecordingByteTransport(ByteTransport& inner, const Clock& clock, const std::string& path);

    bool open() override;
    void close() override;
    [[nodiscard]] bool isOpen() const override;
    std::size_t write(const std::vector<std::uint8_t>& bytes) override;
    ReadResult read(std::vector<std::uint8_t>& buffer,
                    std::size_t maxBytes,
                    std::int64_t timeoutMilliseconds) override;

    [[nodiscard]] bool fileIsOpen() const;
    [[nodiscard]] std::uint64_t eventCount() const;
    [[nodiscard]] std::uint64_t fileWriteFailures() const;

private:
    void append(RecordingEventKind kind,
                const std::vector<std::uint8_t>& bytes = {},
                std::size_t acceptedCount = 0);

    ByteTransport* m_inner;
    const Clock* m_clock;
    std::ofstream m_file;
    std::uint64_t m_eventCount = 0;
    std::uint64_t m_fileWriteFailures = 0;
};

// A Clock whose time is set by the replay: the recorded time of the latest event consumed.
class ReplayClock final : public Clock {
public:
    [[nodiscard]] std::int64_t nowMilliseconds() const override;
    void advanceTo(std::int64_t milliseconds);

private:
    std::int64_t m_nowMilliseconds = 0;
};

// A ByteTransport that answers from a recording (DN-029). Writes must equal the recorded ones;
// the first difference is a divergence, after which reads return Error. The end of the
// recording reads as Closed.
class ReplayByteTransport final : public ByteTransport {
public:
    ReplayByteTransport(std::vector<RecordingEvent> events, ReplayClock& clock);

    bool open() override;
    void close() override;
    [[nodiscard]] bool isOpen() const override;
    std::size_t write(const std::vector<std::uint8_t>& bytes) override;
    ReadResult read(std::vector<std::uint8_t>& buffer,
                    std::size_t maxBytes,
                    std::int64_t timeoutMilliseconds) override;

    // The time of the next event, if any; the replay source moves its clock there before
    // letting the source run, so that waits (backoff) elapse as they did when recording.
    [[nodiscard]] std::optional<std::int64_t> nextEventTimestamp() const;
    [[nodiscard]] std::optional<RecordingEventKind> nextEventKind() const;
    [[nodiscard]] bool finished() const;
    [[nodiscard]] std::uint64_t divergenceCount() const;
    [[nodiscard]] std::size_t eventsRemaining() const;
    [[nodiscard]] std::string lastDivergence() const;

private:
    const RecordingEvent* consume();
    void diverge(const std::string& what);

    std::vector<RecordingEvent> m_events;
    std::size_t m_nextIndex = 0;
    ReplayClock* m_clock;
    bool m_open = false;
    std::vector<std::uint8_t> m_pendingRead;
    std::uint64_t m_divergenceCount = 0;
    std::string m_lastDivergence;
};

} // namespace lexus_head_unit
