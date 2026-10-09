#pragma once

#include "lexus_head_unit/hardware/byte_transport.h"
#include "lexus_head_unit/hardware/can_frame_reader.h"
#include "lexus_head_unit/hardware/fake_source.h"
#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/key_value_configuration.h"
#include "lexus_head_unit/service/signal_store.h"
#include "lexus_head_unit/service/vehicle_data_source.h"

#include <cstdint>
#include <memory>
#include <string>

namespace lexus_head_unit::app {

// The one place that knows every concrete source (architecture section 4.3).
struct BuiltSource {
    std::unique_ptr<ByteTransport> transport;
    // When record.file is set: the recorder wrapped around transport (DN-029).
    std::unique_ptr<ByteTransport> recorder;
    std::unique_ptr<CanFrameReader> canReader;
    std::unique_ptr<VehicleDataSource> source;
    FakeSource* fakeSource = nullptr;
    std::string kind;
    std::int64_t minimumCycleMilliseconds = 0;
};

// Per-signal staleness overrides: staleness.<snake_case_name>_ms, else staleness.default_ms.
void applyStalenessConfiguration(const KeyValueConfiguration& configuration, SignalStore& store);

// Builds the source named by source.kind ("elm327", "can", "replay" or "fake"); an unknown kind
// gives "fake". With record.file set, the ELM327 transport is recorded to that file.
BuiltSource buildSource(const KeyValueConfiguration& configuration,
                        const std::string& kindOverride,
                        const Clock& clock);

// Scripts eight moving values into a fake source; called once per worker cycle.
class FakeVehicleDemo {
public:
    explicit FakeVehicleDemo(FakeSource& source);
    void scriptNextCycle(std::int64_t nowMilliseconds);

private:
    FakeSource* m_source;
    std::int64_t m_startMilliseconds = -1;
};

} // namespace lexus_head_unit::app
