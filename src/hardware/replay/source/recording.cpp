#include "lexus_head_unit/hardware/recording.h"

#include "lexus_head_unit/hardware/byte_transport.h"
#include "lexus_head_unit/service/clock.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace lexus_head_unit {

namespace {

struct KindName {
    RecordingEventKind kind;
    std::string_view text;
};

constexpr std::array<KindName, 8> kindNames = {{
    {RecordingEventKind::OpenOk, "OPEN_OK"},
    {RecordingEventKind::OpenFail, "OPEN_FAIL"},
    {RecordingEventKind::Close, "CLOSE"},
    {RecordingEventKind::Write, "WRITE"},
    {RecordingEventKind::Read, "READ"},
    {RecordingEventKind::ReadTimeout, "READ_TIMEOUT"},
    {RecordingEventKind::ReadClosed, "READ_CLOSED"},
    {RecordingEventKind::ReadError, "READ_ERROR"},
}};

constexpr std::string_view hexDigits = "0123456789abcdef";
constexpr unsigned int nibbleBits = 4;
constexpr unsigned int lowNibbleMask = 0x0F;

std::string toHex(const std::vector<std::uint8_t>& bytes) {
    std::string text;
    text.reserve(bytes.size() * 2);
    for (const std::uint8_t byte : bytes) {
        text.push_back(hexDigits.at(static_cast<unsigned int>(byte) >> nibbleBits));
        text.push_back(hexDigits.at(static_cast<unsigned int>(byte) & lowNibbleMask));
    }
    return text;
}

std::optional<unsigned int> nibbleOf(char character) {
    const std::size_t position = hexDigits.find(static_cast<char>(
        character >= 'A' && character <= 'F' ? character - 'A' + 'a' : character));
    if (position == std::string_view::npos) {
        return std::nullopt;
    }
    return static_cast<unsigned int>(position);
}

std::optional<std::vector<std::uint8_t>> fromHex(std::string_view text) {
    if (text.size() % 2 != 0) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> bytes;
    bytes.reserve(text.size() / 2);
    for (std::size_t index = 0; index < text.size(); index += 2) {
        const auto high = nibbleOf(text.at(index));
        const auto low = nibbleOf(text.at(index + 1));
        if (!high.has_value() || !low.has_value()) {
            return std::nullopt;
        }
        bytes.push_back(static_cast<std::uint8_t>((*high << nibbleBits) | *low));
    }
    return bytes;
}

bool carriesBytes(RecordingEventKind kind) {
    return kind == RecordingEventKind::Write || kind == RecordingEventKind::Read;
}

std::string lineError(std::size_t lineNumber, const std::string& message) {
    return "line " + std::to_string(lineNumber) + ": " + message;
}

std::vector<std::string_view> wordsOf(std::string_view line) {
    std::vector<std::string_view> words;
    std::size_t start = 0;
    while (start < line.size()) {
        const std::size_t first = line.find_first_not_of(" \t\r", start);
        if (first == std::string_view::npos) {
            break;
        }
        const std::size_t last = line.find_first_of(" \t\r", first);
        words.push_back(line.substr(
            first, last == std::string_view::npos ? std::string_view::npos : last - first));
        start = last == std::string_view::npos ? line.size() : last;
    }
    return words;
}

} // namespace

std::string_view toString(RecordingEventKind kind) {
    for (const KindName& entry : kindNames) {
        if (entry.kind == kind) {
            return entry.text;
        }
    }
    return "UNKNOWN";
}

std::optional<RecordingEventKind> recordingEventKindFromText(std::string_view text) {
    for (const KindName& entry : kindNames) {
        if (entry.text == text) {
            return entry.kind;
        }
    }
    return std::nullopt;
}

std::string formatRecordingEvent(const RecordingEvent& event) {
    std::string line = std::to_string(event.timestampMilliseconds);
    line += ' ';
    line += toString(event.kind);
    if (carriesBytes(event.kind)) {
        line += ' ';
        line += toHex(event.bytes);
    }
    if (event.kind == RecordingEventKind::Write) {
        line += ' ';
        line += std::to_string(event.acceptedCount);
    }
    return line;
}

namespace {

// One event from the words of one line; on failure the reason, without the line number.
struct ParsedEvent {
    std::optional<RecordingEvent> event;
    std::string error;
};

ParsedEvent parseEvent(const std::vector<std::string_view>& words) {
    RecordingEvent event;
    const std::string_view time = words.front();
    const std::from_chars_result parsed =
        std::from_chars(time.data(), time.data() + time.size(), event.timestampMilliseconds);
    const std::optional<RecordingEventKind> kind =
        words.size() > 1 ? recordingEventKindFromText(words.at(1)) : std::nullopt;
    if (parsed.ec != std::errc() || !kind.has_value()) {
        return {std::nullopt, "malformed event"};
    }
    event.kind = *kind;
    if (carriesBytes(event.kind)) {
        const std::optional<std::vector<std::uint8_t>> bytes =
            words.size() > 2 ? fromHex(words.at(2)) : std::vector<std::uint8_t>{};
        if (!bytes.has_value()) {
            return {std::nullopt, "malformed hex bytes"};
        }
        event.bytes = *bytes;
        event.acceptedCount = event.bytes.size();
    }
    if (event.kind == RecordingEventKind::Write && words.size() > 3) {
        const std::string_view count = words.at(3);
        const std::from_chars_result countParsed =
            std::from_chars(count.data(), count.data() + count.size(), event.acceptedCount);
        if (countParsed.ec != std::errc() || event.acceptedCount > event.bytes.size()) {
            return {std::nullopt, "malformed accepted count"};
        }
    }
    return {std::move(event), {}};
}

} // namespace

RecordingFile parseRecording(std::string_view text) {
    RecordingFile file;
    std::size_t lineNumber = 0;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t end = text.find('\n', start);
        const std::string_view line = text.substr(
            start, end == std::string_view::npos ? std::string_view::npos : end - start);
        start = end == std::string_view::npos ? text.size() : end + 1;
        ++lineNumber;
        const std::vector<std::string_view> words = wordsOf(line);
        if (words.empty() || words.front().front() == '#') {
            continue;
        }
        ParsedEvent parsed = parseEvent(words);
        if (parsed.event.has_value()) {
            file.events.push_back(std::move(*parsed.event));
        } else {
            file.errors.push_back(lineError(lineNumber, parsed.error));
        }
    }
    return file;
}

RecordingFile readRecording(const std::string& path) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        RecordingFile file;
        file.errors.push_back("cannot open " + path);
        return file;
    }
    std::stringstream contents;
    contents << stream.rdbuf();
    return parseRecording(contents.str());
}

RecordingByteTransport::RecordingByteTransport(ByteTransport& inner,
                                               const Clock& clock,
                                               const std::string& path)
    : m_inner(&inner), m_clock(&clock), m_file(path, std::ios::out | std::ios::trunc) {
    if (m_file.is_open()) {
        m_file << recordingHeader << '\n';
        m_file.flush();
    }
}

bool RecordingByteTransport::open() {
    const bool opened = m_inner->open();
    append(opened ? RecordingEventKind::OpenOk : RecordingEventKind::OpenFail);
    return opened;
}

void RecordingByteTransport::close() {
    m_inner->close();
    append(RecordingEventKind::Close);
}

bool RecordingByteTransport::isOpen() const {
    return m_inner->isOpen();
}

std::size_t RecordingByteTransport::write(const std::vector<std::uint8_t>& bytes) {
    const std::size_t written = m_inner->write(bytes);
    append(RecordingEventKind::Write, bytes, std::min(written, bytes.size()));
    return written;
}

ReadResult RecordingByteTransport::read(std::vector<std::uint8_t>& buffer,
                                        std::size_t maxBytes,
                                        std::int64_t timeoutMilliseconds) {
    const ReadResult result = m_inner->read(buffer, maxBytes, timeoutMilliseconds);
    switch (result.status) {
    case ReadStatus::Ok:
        append(RecordingEventKind::Read,
               std::vector<std::uint8_t>(buffer.begin(),
                                         buffer.begin() + static_cast<std::ptrdiff_t>(std::min(
                                                              result.byteCount, buffer.size()))));
        break;
    case ReadStatus::Timeout:
        append(RecordingEventKind::ReadTimeout);
        break;
    case ReadStatus::Closed:
        append(RecordingEventKind::ReadClosed);
        break;
    case ReadStatus::Error:
        append(RecordingEventKind::ReadError);
        break;
    }
    return result;
}

bool RecordingByteTransport::fileIsOpen() const {
    return m_file.is_open();
}

std::uint64_t RecordingByteTransport::eventCount() const {
    return m_eventCount;
}

std::uint64_t RecordingByteTransport::fileWriteFailures() const {
    return m_fileWriteFailures;
}

void RecordingByteTransport::append(RecordingEventKind kind,
                                    const std::vector<std::uint8_t>& bytes,
                                    std::size_t acceptedCount) {
    RecordingEvent event;
    event.timestampMilliseconds = m_clock->nowMilliseconds();
    event.kind = kind;
    event.bytes = bytes;
    event.acceptedCount = acceptedCount;
    m_file << formatRecordingEvent(event) << '\n';
    m_file.flush();
    if (!m_file.good()) {
        ++m_fileWriteFailures;
        m_file.clear();
    }
    ++m_eventCount;
}

std::int64_t ReplayClock::nowMilliseconds() const {
    return m_nowMilliseconds;
}

void ReplayClock::advanceTo(std::int64_t milliseconds) {
    m_nowMilliseconds = std::max(m_nowMilliseconds, milliseconds);
}

ReplayByteTransport::ReplayByteTransport(std::vector<RecordingEvent> events, ReplayClock& clock)
    : m_events(std::move(events)), m_clock(&clock) {
    if (!m_events.empty()) {
        m_clock->advanceTo(m_events.front().timestampMilliseconds);
    }
}

const RecordingEvent* ReplayByteTransport::consume() {
    if (m_nextIndex >= m_events.size()) {
        return nullptr;
    }
    const RecordingEvent& event = m_events.at(m_nextIndex);
    ++m_nextIndex;
    m_clock->advanceTo(event.timestampMilliseconds);
    return &event;
}

void ReplayByteTransport::diverge(const std::string& what) {
    ++m_divergenceCount;
    m_lastDivergence = what;
}

bool ReplayByteTransport::open() {
    if (m_divergenceCount > 0) {
        return false;
    }
    const RecordingEvent* event = consume();
    if (event == nullptr) {
        return false;
    }
    if (event->kind != RecordingEventKind::OpenOk && event->kind != RecordingEventKind::OpenFail) {
        diverge("open() where the recording has " + std::string(toString(event->kind)));
        return false;
    }
    m_open = event->kind == RecordingEventKind::OpenOk;
    m_pendingRead.clear();
    return m_open;
}

void ReplayByteTransport::close() {
    m_open = false;
    m_pendingRead.clear();
    if (m_nextIndex < m_events.size() &&
        m_events.at(m_nextIndex).kind == RecordingEventKind::Close) {
        consume();
    }
}

bool ReplayByteTransport::isOpen() const {
    return m_open;
}

std::size_t ReplayByteTransport::write(const std::vector<std::uint8_t>& bytes) {
    if (m_divergenceCount > 0) {
        return 0;
    }
    const RecordingEvent* event = consume();
    if (event == nullptr) {
        return 0;
    }
    if (event->kind != RecordingEventKind::Write || event->bytes != bytes) {
        diverge("write of " + std::to_string(bytes.size()) + " bytes where the recording has " +
                std::string(toString(event->kind)) + " of " + std::to_string(event->bytes.size()));
        return 0;
    }
    return event->acceptedCount;
}

ReadResult ReplayByteTransport::read(std::vector<std::uint8_t>& buffer,
                                     std::size_t maxBytes,
                                     std::int64_t /*timeoutMilliseconds*/) {
    buffer.clear();
    if (m_divergenceCount > 0) {
        return ReadResult{0, ReadStatus::Error};
    }
    if (m_pendingRead.empty()) {
        const RecordingEvent* event = consume();
        if (event == nullptr) {
            return ReadResult{0, ReadStatus::Closed};
        }
        switch (event->kind) {
        case RecordingEventKind::Read:
            m_pendingRead = event->bytes;
            if (m_pendingRead.empty()) {
                return ReadResult{0, ReadStatus::Ok};
            }
            break;
        case RecordingEventKind::ReadTimeout:
            return ReadResult{0, ReadStatus::Timeout};
        case RecordingEventKind::ReadClosed:
            return ReadResult{0, ReadStatus::Closed};
        case RecordingEventKind::ReadError:
            return ReadResult{0, ReadStatus::Error};
        case RecordingEventKind::OpenOk:
        case RecordingEventKind::OpenFail:
        case RecordingEventKind::Close:
        case RecordingEventKind::Write:
            diverge("read() where the recording has " + std::string(toString(event->kind)));
            return ReadResult{0, ReadStatus::Error};
        }
    }
    const std::size_t count = std::min(maxBytes, m_pendingRead.size());
    buffer.assign(m_pendingRead.begin(),
                  m_pendingRead.begin() + static_cast<std::ptrdiff_t>(count));
    m_pendingRead.erase(m_pendingRead.begin(),
                        m_pendingRead.begin() + static_cast<std::ptrdiff_t>(count));
    return ReadResult{count, ReadStatus::Ok};
}

std::optional<std::int64_t> ReplayByteTransport::nextEventTimestamp() const {
    if (m_nextIndex >= m_events.size()) {
        return std::nullopt;
    }
    return m_events.at(m_nextIndex).timestampMilliseconds;
}

std::optional<RecordingEventKind> ReplayByteTransport::nextEventKind() const {
    if (m_nextIndex >= m_events.size()) {
        return std::nullopt;
    }
    return m_events.at(m_nextIndex).kind;
}

bool ReplayByteTransport::finished() const {
    return m_nextIndex >= m_events.size() && m_pendingRead.empty();
}

std::uint64_t ReplayByteTransport::divergenceCount() const {
    return m_divergenceCount;
}

std::size_t ReplayByteTransport::eventsRemaining() const {
    return m_events.size() - m_nextIndex;
}

std::string ReplayByteTransport::lastDivergence() const {
    return m_lastDivergence;
}

} // namespace lexus_head_unit
