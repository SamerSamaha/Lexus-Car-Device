#pragma once

#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/diagnostics_report.h"
#include "lexus_head_unit/service/signal_sample.h"
#include "lexus_head_unit/service/signal_store.h"
#include "lexus_head_unit/service/vehicle_data_source.h"

#include <cstdint>
#include <functional>
#include <optional>

namespace lexus_head_unit {

class SignalStoreFeeder final : public VehicleDataSourceListener {
public:
    using TransitionHook = std::function<void(const ConnectionTransition&)>;
    using DiagnosticsHook = std::function<void(const DiagnosticsReport&)>;

    explicit SignalStoreFeeder(SignalStore& store);

    void onSample(const SignalSample& sample) override;
    void onConnectionChanged(const ConnectionTransition& transition) override;
    // Diagnostics do not go into the store; they are handed to the hook (DN-030).
    void onDiagnostics(const DiagnosticsReport& report) override;

    [[nodiscard]] ConnectionState connectionState() const;
    [[nodiscard]] std::optional<ConnectionTransition> latestTransition() const;
    [[nodiscard]] std::uint64_t transitionCount() const;
    [[nodiscard]] std::uint64_t acceptedSampleCount() const;
    [[nodiscard]] std::uint64_t rejectedSampleCount() const;

    void setTransitionHook(TransitionHook hook);
    void setDiagnosticsHook(DiagnosticsHook hook);

private:
    SignalStore* m_store;
    std::optional<ConnectionTransition> m_latestTransition;
    std::uint64_t m_transitionCount = 0;
    std::uint64_t m_acceptedSampleCount = 0;
    std::uint64_t m_rejectedSampleCount = 0;
    TransitionHook m_transitionHook;
    DiagnosticsHook m_diagnosticsHook;
};

} // namespace lexus_head_unit
