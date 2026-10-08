#pragma once

#include "lexus_head_unit/service/signal_id.h"

#include <cstdint>

namespace lexus_head_unit {

struct SignalSample {
    SignalId signalId = SignalId::VehicleSpeed;
    double value = 0.0;
    Unit unit = Unit::KilometresPerHour;
    std::int64_t timestampMilliseconds = 0;
    SignalStatus status = SignalStatus::NeverReceived;
};

} // namespace lexus_head_unit
