#pragma once

#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/vehicle_data_source.h"

#include <memory>
#include <string>

namespace lexus_head_unit::testing {

// One harness per concrete source. The integration suite drives every source through the same
// steps; the harness knows how to make a particular source produce them.
class SourceHarness {
public:
    SourceHarness() = default;
    SourceHarness(const SourceHarness&) = delete;
    SourceHarness& operator=(const SourceHarness&) = delete;
    SourceHarness(SourceHarness&&) = delete;
    SourceHarness& operator=(SourceHarness&&) = delete;
    virtual ~SourceHarness() = default;

    [[nodiscard]] virtual std::string name() const = 0;
    virtual std::unique_ptr<VehicleDataSource> makeSource(const Clock& clock) = 0;

    // Arrange for the next runOnce() calls to deliver this sample. advance() may be needed after.
    virtual void produceSample(SignalId signalId, double value) = 0;
    // Arrange for the link to be lost, then restored, on the following runOnce() calls.
    virtual void breakLink() = 0;
    virtual void restoreLink() = 0;
    // Drive the source until it has nothing pending; returns the number of runOnce() calls made.
    virtual int drain(VehicleDataSource& source) = 0;
};

} // namespace lexus_head_unit::testing
