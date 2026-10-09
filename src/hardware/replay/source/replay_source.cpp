#include "lexus_head_unit/hardware/replay_source.h"

#include "lexus_head_unit/hardware/elm327_source_configuration.h"
#include "lexus_head_unit/hardware/recording.h"
#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/key_value_configuration.h"
#include "lexus_head_unit/service/link_detail.h"
#include "lexus_head_unit/service/vehicle_data_source.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lexus_head_unit {

namespace {

constexpr std::int64_t idleWhenFinishedMilliseconds = 1000;

std::int64_t firstTimestamp(const RecordingFile& recording) {
    return recording.events.empty() ? 0 : recording.events.front().timestampMilliseconds;
}

} // namespace

ReplayConfiguration
ReplayConfiguration::fromConfiguration(const KeyValueConfiguration& configuration) {
    ReplayConfiguration result;
    result.filePath = configuration.stringValue("replay.file", "");
    result.timing = configuration.stringValue("replay.timing", "original") == "fast"
                        ? ReplayTiming::Fast
                        : ReplayTiming::Original;
    return result;
}

ReplaySource::ReplaySource(RecordingFile recording,
                           const Elm327SourceConfiguration& sourceConfiguration,
                           ReplayTiming timing)
    : m_recordingErrors(std::move(recording.errors)),
      m_firstEventMilliseconds(firstTimestamp(recording)),
      m_transport(std::move(recording.events), m_clock),
      m_source(m_transport, m_clock, sourceConfiguration),
      m_timing(timing) {}

std::string_view ReplaySource::name() const {
    return "replay";
}

void ReplaySource::start(VehicleDataSourceListener& listener) {
    if (!m_recordingErrors.empty()) {
        return;
    }
    m_started = true;
    m_wallStart = std::chrono::steady_clock::now();
    m_source.start(listener);
}

void ReplaySource::runOnce() {
    if (!m_started || finished()) {
        return;
    }
    if (m_timing == ReplayTiming::Original && idleHintMilliseconds() > 0) {
        return;
    }
    // Move the clock to the next recorded event first: a wait the source made while recording
    // (a backoff, a poll interval) left no event, and must elapse here too.
    const std::optional<std::int64_t> next = m_transport.nextEventTimestamp();
    if (next.has_value()) {
        m_clock.advanceTo(*next);
    }
    // While not connected the source only opens; a CLOSE here is the stop() that ended the
    // recording, not something the source does, so it is consumed rather than replayed.
    if (m_transport.nextEventKind() == RecordingEventKind::Close &&
        m_source.connectionState() != ConnectionState::Connected) {
        m_transport.close();
        return;
    }
    m_source.runOnce();
}

void ReplaySource::stop() {
    m_source.stop();
    m_started = false;
}

ConnectionState ReplaySource::connectionState() const {
    return m_source.connectionState();
}

LinkDetail ReplaySource::linkDetail() const {
    return m_source.linkDetail();
}

SourceCounters ReplaySource::counters() const {
    return m_source.counters();
}

std::int64_t ReplaySource::idleHintMilliseconds() const {
    if (finished()) {
        return idleWhenFinishedMilliseconds;
    }
    if (m_timing == ReplayTiming::Fast || !m_started) {
        return 0;
    }
    const std::optional<std::int64_t> next = m_transport.nextEventTimestamp();
    if (!next.has_value()) {
        return 0;
    }
    const std::int64_t due = (*next - m_firstEventMilliseconds) - wallElapsedMilliseconds();
    return due > 0 ? due : 0;
}

bool ReplaySource::finished() const {
    return m_transport.finished() && m_source.connectionState() != ConnectionState::Connected;
}

const ReplayByteTransport& ReplaySource::transport() const {
    return m_transport;
}

const std::vector<std::string>& ReplaySource::recordingErrors() const {
    return m_recordingErrors;
}

const Clock& ReplaySource::clock() const {
    return m_clock;
}

std::int64_t ReplaySource::wallElapsedMilliseconds() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                 m_wallStart)
        .count();
}

} // namespace lexus_head_unit
