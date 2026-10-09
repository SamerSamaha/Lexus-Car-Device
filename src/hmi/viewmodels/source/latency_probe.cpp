#include "lexus_head_unit/hmi/latency_probe.h"

#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"

#include <QObject>
#include <QtGlobal>

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <string>
#include <utility>

// Qt declares its macros in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace lexus_head_unit {

LatencyProbe::LatencyProbe(TimeSource now, const std::string& path, QObject* parent)
    : QObject(parent), m_now(std::move(now)), m_file(path, std::ios::out | std::ios::trunc) {
    if (m_file.is_open()) {
        m_file << header << '\n';
        m_file.flush();
    }
    connect(this,
            &LatencyProbe::framePresentedAt,
            this,
            &LatencyProbe::onFramePresentedAt,
            Qt::QueuedConnection);
}

bool LatencyProbe::isOpen() const {
    return m_file.is_open();
}

std::uint64_t LatencyProbe::rowsWritten() const {
    return m_rowsWritten;
}

std::size_t LatencyProbe::pendingSamples() const {
    return m_pending.size();
}

void LatencyProbe::markFramePresented() {
    emit framePresentedAt(static_cast<qint64>(m_now()));
}

void LatencyProbe::onSample(SignalSample sample) {
    if (sample.status != SignalStatus::Valid) {
        return;
    }
    m_pending.push_back(Pending{sample, m_now()});
}

void LatencyProbe::onFramePresentedAt(qint64 frameMilliseconds) {
    // A sample that arrived after the swap waits for the next frame.
    while (!m_pending.empty() && m_pending.front().arrivalMilliseconds <= frameMilliseconds) {
        const Pending& pending = m_pending.front();
        if (m_file.is_open()) {
            m_file << toString(pending.sample.signalId) << ','
                   << pending.sample.timestampMilliseconds << ',' << pending.arrivalMilliseconds
                   << ',' << frameMilliseconds << '\n';
            ++m_rowsWritten;
        }
        m_pending.pop_front();
    }
    m_file.flush();
}

} // namespace lexus_head_unit
// NOLINTEND(misc-include-cleaner)
