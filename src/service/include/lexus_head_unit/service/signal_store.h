#pragma once

#include "lexus_head_unit/service/signal_definition.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"

#include <array>
#include <cstdint>
#include <functional>
#include <string_view>

namespace lexus_head_unit {

enum class UpdateResult {
    Accepted,
    RejectedOutOfOrder,
    RejectedUnitMismatch,
    RejectedUnknownSignal,
};

class SignalStore {
public:
    using ChangeListener = std::function<void(const SignalSample&)>;
    using Snapshot = std::array<SignalSample, signalCount>;

    SignalStore();
    explicit SignalStore(const SignalDefinitionTable& definitions);

    UpdateResult update(const SignalSample& sample);
    bool markStale(SignalId signalId);

    [[nodiscard]] const SignalSample& latest(SignalId signalId) const;
    [[nodiscard]] Snapshot snapshot() const;

    bool setStalenessTimeout(SignalId signalId, std::int64_t timeoutMilliseconds);
    [[nodiscard]] std::int64_t stalenessTimeoutMilliseconds(SignalId signalId) const;

    void setChangeListener(ChangeListener listener);

    [[nodiscard]] std::uint64_t rejectedOutOfOrderCount() const;
    [[nodiscard]] std::uint64_t rejectedUnitMismatchCount() const;
    [[nodiscard]] std::uint64_t rejectedUnknownSignalCount() const;

private:
    void notify(const SignalSample& sample) const;

    std::array<SignalSample, signalCount> m_latestSamples{};
    std::array<std::int64_t, signalCount> m_stalenessTimeoutsMilliseconds{};
    std::uint64_t m_rejectedOutOfOrderCount = 0;
    std::uint64_t m_rejectedUnitMismatchCount = 0;
    std::uint64_t m_rejectedUnknownSignalCount = 0;
    ChangeListener m_changeListener;
};

std::string_view toString(UpdateResult result);

} // namespace lexus_head_unit
