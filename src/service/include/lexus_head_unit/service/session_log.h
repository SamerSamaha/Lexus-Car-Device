#pragma once

#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/link_detail.h"
#include "lexus_head_unit/service/vehicle_data_source.h"

#include <cstdint>
#include <fstream>
#include <string>

namespace lexus_head_unit {

// A CSV log of one session (LHU-024, REQ-019): every connection transition with its cause, and
// the source's counters at a fixed interval, so a parked session can show whether the OBD link
// held and how fast it polled while audio played, and every change of the link detail (DN-042,
// event "detail" with the detail in the state column). One row per event:
//   milliseconds,event,state,trigger,from,to,requests_sent,samples_emitted,malformed_inputs
// Not thread-safe: the service calls it only from the worker thread, where the source and its
// counters live.
class SessionLog {
public:
    SessionLog(const std::string& path, std::int64_t countersIntervalMilliseconds);

    [[nodiscard]] bool isOpen() const;
    void recordTransition(const ConnectionTransition& transition);
    void recordLinkDetail(std::int64_t nowMilliseconds, LinkDetail detail);
    // Writes a counters row when the interval has passed since the last one (or on the first
    // call); returns true when it wrote.
    bool recordCountersIfDue(std::int64_t nowMilliseconds,
                             ConnectionState state,
                             const SourceCounters& counters);
    [[nodiscard]] std::uint64_t rowsWritten() const;

    static constexpr const char* header =
        "milliseconds,event,state,trigger,from,to,requests_sent,samples_emitted,malformed_inputs";

private:
    void writeRow(const std::string& row);

    std::ofstream m_file;
    std::int64_t m_intervalMilliseconds;
    std::int64_t m_lastCountersMilliseconds = 0;
    bool m_countersWritten = false;
    std::uint64_t m_rowsWritten = 0;
};

} // namespace lexus_head_unit
