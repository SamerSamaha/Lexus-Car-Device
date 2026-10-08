#include "lexus_head_unit/service/staleness_monitor.h"

#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"
#include "lexus_head_unit/service/signal_store.h"

#include <cstddef>
#include <cstdint>

namespace lexus_head_unit {

StalenessMonitor::StalenessMonitor(SignalStore& store, const Clock& clock)
    : m_store(&store), m_clock(&clock) {}

std::size_t StalenessMonitor::check() {
    const std::int64_t nowMilliseconds = m_clock->nowMilliseconds();
    std::size_t newlyStaleCount = 0;
    for (const SignalId signalId : allSignalIds) {
        const SignalSample& sample = m_store->latest(signalId);
        if (sample.status != SignalStatus::Valid) {
            continue;
        }
        const std::int64_t ageMilliseconds = nowMilliseconds - sample.timestampMilliseconds;
        if (ageMilliseconds > m_store->stalenessTimeoutMilliseconds(signalId)) {
            if (m_store->markStale(signalId)) {
                ++newlyStaleCount;
            }
        }
    }
    return newlyStaleCount;
}

} // namespace lexus_head_unit
