#pragma once

#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/vehicle_data_source.h"

#include <cstddef>
#include <deque>
#include <string_view>

namespace lexus_head_unit {

class FakeSource final : public VehicleDataSource {
public:
    explicit FakeSource(const Clock& clock);

    void scriptSample(SignalId signalId, double value);
    void scriptMalformedInput();
    void scriptLinkLoss();
    void scriptReconnect();
    void scriptHandshakeFailure();
    [[nodiscard]] std::size_t pendingSteps() const;

    [[nodiscard]] std::string_view name() const override;
    void start(VehicleDataSourceListener& listener) override;
    void runOnce() override;
    void stop() override;
    [[nodiscard]] ConnectionState connectionState() const override;
    [[nodiscard]] SourceCounters counters() const override;

private:
    enum class StepKind {
        Sample,
        MalformedInput,
        LinkLoss,
        Reconnect,
    };

    struct ScriptStep {
        StepKind kind = StepKind::Sample;
        SignalId signalId = SignalId::VehicleSpeed;
        double value = 0.0;
    };

    void emitSample(const ScriptStep& step);
    void handshake();

    const Clock* m_clock;
    ConnectionStateMachine m_machine;
    std::deque<ScriptStep> m_script;
    VehicleDataSourceListener* m_listener = nullptr;
    SourceCounters m_counters;
    bool m_failNextHandshake = false;
};

} // namespace lexus_head_unit
