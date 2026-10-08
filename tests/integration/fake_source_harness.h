#pragma once

#include "lexus_head_unit/hardware/fake_source.h"
#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/vehicle_data_source.h"
#include "source_harness.h"

#include <memory>
#include <string>

namespace lexus_head_unit::testing {

class FakeSourceHarness final : public SourceHarness {
public:
    [[nodiscard]] std::string name() const override {
        return "fake";
    }

    std::unique_ptr<VehicleDataSource> makeSource(const Clock& clock) override {
        auto source = std::make_unique<FakeSource>(clock);
        m_source = source.get();
        return source;
    }

    void produceSample(SignalId signalId, double value) override {
        m_source->scriptSample(signalId, value);
    }

    void breakLink() override {
        m_source->scriptLinkLoss();
    }

    void restoreLink() override {
        m_source->scriptReconnect();
    }

    int drain(VehicleDataSource& source) override {
        int calls = 0;
        while (m_source->pendingSteps() > 0) {
            source.runOnce();
            ++calls;
        }
        return calls;
    }

private:
    FakeSource* m_source = nullptr;
};

} // namespace lexus_head_unit::testing
