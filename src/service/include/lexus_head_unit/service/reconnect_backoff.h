#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace lexus_head_unit {

// The retry schedule of REQ-008: 1, 2, 4, 8 s, then the cap (10 s) for every further attempt.
class ReconnectBackoff {
public:
    ReconnectBackoff();
    ReconnectBackoff(std::vector<std::int64_t> scheduleMilliseconds, std::int64_t capMilliseconds);

    std::int64_t nextDelayMilliseconds();
    void reset();
    [[nodiscard]] std::size_t attemptCount() const;
    [[nodiscard]] const std::vector<std::int64_t>& scheduleMilliseconds() const;
    [[nodiscard]] std::int64_t capMilliseconds() const;

private:
    std::vector<std::int64_t> m_scheduleMilliseconds;
    std::int64_t m_capMilliseconds;
    std::size_t m_attemptCount = 0;
};

} // namespace lexus_head_unit
