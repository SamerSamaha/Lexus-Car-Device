#pragma once

#include <cstdint>

namespace lexus_head_unit {

class Clock {
public:
    Clock() = default;
    Clock(const Clock&) = delete;
    Clock& operator=(const Clock&) = delete;
    Clock(Clock&&) = delete;
    Clock& operator=(Clock&&) = delete;
    virtual ~Clock() = default;

    [[nodiscard]] virtual std::int64_t nowMilliseconds() const = 0;
};

class SteadyClock final : public Clock {
public:
    [[nodiscard]] std::int64_t nowMilliseconds() const override;
};

} // namespace lexus_head_unit
