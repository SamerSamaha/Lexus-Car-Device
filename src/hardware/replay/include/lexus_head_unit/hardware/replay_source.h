#pragma once

#include "lexus_head_unit/hardware/elm327_obd_source.h"
#include "lexus_head_unit/hardware/elm327_source_configuration.h"
#include "lexus_head_unit/hardware/recording.h"
#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/key_value_configuration.h"
#include "lexus_head_unit/service/vehicle_data_source.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace lexus_head_unit {

enum class ReplayTiming {
    // As fast as the source can run: tests.
    Fast,
    // Paced by the recorded times against the wall clock: watching a drive again.
    Original,
};

struct ReplayConfiguration {
    std::string filePath;
    ReplayTiming timing = ReplayTiming::Original;

    // replay.file and replay.timing (original or fast).
    static ReplayConfiguration fromConfiguration(const KeyValueConfiguration& configuration);
};

// Plays a recording back through the unchanged ELM327 source (DN-029, REQ-015). The source
// runs on a ReplayClock that the recording drives, so its timing decisions repeat.
class ReplaySource final : public VehicleDataSource {
public:
    ReplaySource(RecordingFile recording,
                 const Elm327SourceConfiguration& sourceConfiguration,
                 ReplayTiming timing);

    [[nodiscard]] std::string_view name() const override;
    void start(VehicleDataSourceListener& listener) override;
    void runOnce() override;
    void stop() override;
    [[nodiscard]] ConnectionState connectionState() const override;
    [[nodiscard]] SourceCounters counters() const override;
    [[nodiscard]] std::int64_t idleHintMilliseconds() const override;

    // True once the recording is used up and the source has seen the end.
    [[nodiscard]] bool finished() const;
    [[nodiscard]] const ReplayByteTransport& transport() const;
    [[nodiscard]] const std::vector<std::string>& recordingErrors() const;
    [[nodiscard]] const Clock& clock() const;

private:
    [[nodiscard]] std::int64_t wallElapsedMilliseconds() const;

    std::vector<std::string> m_recordingErrors;
    std::int64_t m_firstEventMilliseconds = 0;
    ReplayClock m_clock;
    ReplayByteTransport m_transport;
    Elm327ObdSource m_source;
    ReplayTiming m_timing;
    std::chrono::steady_clock::time_point m_wallStart;
    bool m_started = false;
};

} // namespace lexus_head_unit
