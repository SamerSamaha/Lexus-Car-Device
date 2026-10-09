#pragma once

#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/signal_store.h"

#include <cstddef>

namespace lexus_head_unit {

class StalenessMonitor {
public:
    StalenessMonitor(SignalStore& store, const Clock& clock);

    std::size_t check();

private:
    SignalStore* m_store;
    const Clock* m_clock;
};

} // namespace lexus_head_unit
