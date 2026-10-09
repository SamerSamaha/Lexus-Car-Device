#include "lexus_head_unit/hardware/reconnect_backoff.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace lexus_head_unit {

namespace {

constexpr std::int64_t defaultCapMilliseconds = 10000;
constexpr std::int64_t firstDelayMilliseconds = 1000;
constexpr std::int64_t secondDelayMilliseconds = 2000;
constexpr std::int64_t thirdDelayMilliseconds = 4000;
constexpr std::int64_t fourthDelayMilliseconds = 8000;

std::vector<std::int64_t> defaultSchedule() {
    return {firstDelayMilliseconds,
            secondDelayMilliseconds,
            thirdDelayMilliseconds,
            fourthDelayMilliseconds};
}

} // namespace

ReconnectBackoff::ReconnectBackoff()
    : ReconnectBackoff(defaultSchedule(), defaultCapMilliseconds) {}

ReconnectBackoff::ReconnectBackoff(std::vector<std::int64_t> scheduleMilliseconds,
                                   std::int64_t capMilliseconds)
    : m_scheduleMilliseconds(std::move(scheduleMilliseconds)), m_capMilliseconds(capMilliseconds) {}

std::int64_t ReconnectBackoff::nextDelayMilliseconds() {
    const std::int64_t delay = m_attemptCount < m_scheduleMilliseconds.size()
                                   ? m_scheduleMilliseconds.at(m_attemptCount)
                                   : m_capMilliseconds;
    ++m_attemptCount;
    return delay < m_capMilliseconds ? delay : m_capMilliseconds;
}

void ReconnectBackoff::reset() {
    m_attemptCount = 0;
}

std::size_t ReconnectBackoff::attemptCount() const {
    return m_attemptCount;
}

const std::vector<std::int64_t>& ReconnectBackoff::scheduleMilliseconds() const {
    return m_scheduleMilliseconds;
}

std::int64_t ReconnectBackoff::capMilliseconds() const {
    return m_capMilliseconds;
}

} // namespace lexus_head_unit
