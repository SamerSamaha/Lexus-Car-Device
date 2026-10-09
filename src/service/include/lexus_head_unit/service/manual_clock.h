#pragma once

#include "lexus_head_unit/service/clock.h"

#include <cstdint>

namespace lexus_head_unit {

class ManualClock final : public Clock {
public:
    explicit ManualClock(std::int64_t startMilliseconds = 0);

    [[nodiscard]] std::int64_t nowMilliseconds() const override;

    void advanceMilliseconds(std::int64_t milliseconds);
    void setMilliseconds(std::int64_t milliseconds);

private:
    std::int64_t m_nowMilliseconds;
};

} // namespace lexus_head_unit
