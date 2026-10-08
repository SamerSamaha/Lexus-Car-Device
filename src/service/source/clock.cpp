#include "lexus_head_unit/service/clock.h"

#include <chrono>
#include <cstdint>

namespace lexus_head_unit {

std::int64_t SteadyClock::nowMilliseconds() const {
    const auto sinceStart = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(sinceStart).count();
}

} // namespace lexus_head_unit
