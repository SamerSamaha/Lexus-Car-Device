#include "lexus_head_unit/service/session_log.h"

#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/vehicle_data_source.h"

#include <cstdint>
#include <fstream>
#include <ios>
#include <string>

namespace lexus_head_unit {

SessionLog::SessionLog(const std::string& path, std::int64_t countersIntervalMilliseconds)
    : m_file(path, std::ios::out | std::ios::trunc),
      m_intervalMilliseconds(countersIntervalMilliseconds) {
    if (m_file.is_open()) {
        m_file << header << '\n';
        m_file.flush();
    }
}

bool SessionLog::isOpen() const {
    return m_file.is_open();
}

void SessionLog::recordTransition(const ConnectionTransition& transition) {
    std::string row = std::to_string(transition.timestampMilliseconds);
    row += ",transition,";
    row += toString(transition.to);
    row += ',';
    row += toString(transition.trigger);
    row += ',';
    row += toString(transition.from);
    row += ',';
    row += toString(transition.to);
    row += ",,,";
    writeRow(row);
}

bool SessionLog::recordCountersIfDue(std::int64_t nowMilliseconds,
                                     ConnectionState state,
                                     const SourceCounters& counters) {
    if (m_countersWritten &&
        nowMilliseconds - m_lastCountersMilliseconds < m_intervalMilliseconds) {
        return false;
    }
    m_countersWritten = true;
    m_lastCountersMilliseconds = nowMilliseconds;
    std::string row = std::to_string(nowMilliseconds);
    row += ",counters,";
    row += toString(state);
    row += ",,,,";
    row += std::to_string(counters.requestsSent);
    row += ',';
    row += std::to_string(counters.samplesEmitted);
    row += ',';
    row += std::to_string(counters.malformedInputs);
    writeRow(row);
    return true;
}

std::uint64_t SessionLog::rowsWritten() const {
    return m_rowsWritten;
}

void SessionLog::writeRow(const std::string& row) {
    if (!m_file.is_open()) {
        return;
    }
    m_file << row << '\n';
    m_file.flush();
    ++m_rowsWritten;
}

} // namespace lexus_head_unit
