#include "lexus_head_unit/service/manual_clock.h"

#include <cstdint>

namespace lexus_head_unit {

ManualClock::ManualClock(std::int64_t startMilliseconds) : m_nowMilliseconds(startMilliseconds) {}

std::int64_t ManualClock::nowMilliseconds() const {
    return m_nowMilliseconds;
}

void ManualClock::advanceMilliseconds(std::int64_t milliseconds) {
    m_nowMilliseconds += milliseconds;
}

void ManualClock::setMilliseconds(std::int64_t milliseconds) {
    m_nowMilliseconds = milliseconds;
}

} // namespace lexus_head_unit
