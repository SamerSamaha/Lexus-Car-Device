#pragma once

#include "lexus_head_unit/service/signal_id.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace lexus_head_unit {

constexpr std::int64_t defaultStalenessTimeoutMilliseconds = 1000;

struct SignalDefinition {
    SignalId id = SignalId::VehicleSpeed;
    std::string_view name;
    Unit unit = Unit::KilometresPerHour;
    std::int64_t stalenessTimeoutMilliseconds = defaultStalenessTimeoutMilliseconds;
};

using SignalDefinitionTable = std::array<SignalDefinition, signalCount>;

const SignalDefinitionTable& defaultSignalDefinitions();

const SignalDefinition& definitionOf(SignalId signalId);

} // namespace lexus_head_unit
