#pragma once

#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/diagnostics_report.h"
#include "lexus_head_unit/service/signal_sample.h"

#include <cstdint>
#include <string_view>

namespace lexus_head_unit {

struct SourceCounters {
    std::uint64_t samplesEmitted = 0;
    std::uint64_t malformedInputs = 0;
    std::uint64_t requestsSent = 0;
    std::uint64_t rejectedTransitions = 0;
};

class VehicleDataSourceListener {
public:
    VehicleDataSourceListener() = default;
    VehicleDataSourceListener(const VehicleDataSourceListener&) = delete;
    VehicleDataSourceListener& operator=(const VehicleDataSourceListener&) = delete;
    VehicleDataSourceListener(VehicleDataSourceListener&&) = delete;
    VehicleDataSourceListener& operator=(VehicleDataSourceListener&&) = delete;
    virtual ~VehicleDataSourceListener() = default;

    virtual void onSample(const SignalSample& sample) = 0;
    virtual void onConnectionChanged(const ConnectionTransition& transition) = 0;
    // The result of a diagnostics read the source was asked for (DN-030). Default: ignored.
    virtual void onDiagnostics(const DiagnosticsReport& report);
};

class VehicleDataSource {
public:
    VehicleDataSource() = default;
    VehicleDataSource(const VehicleDataSource&) = delete;
    VehicleDataSource& operator=(const VehicleDataSource&) = delete;
    VehicleDataSource(VehicleDataSource&&) = delete;
    VehicleDataSource& operator=(VehicleDataSource&&) = delete;
    virtual ~VehicleDataSource() = default;

    [[nodiscard]] virtual std::string_view name() const = 0;
    virtual void start(VehicleDataSourceListener& listener) = 0;
    virtual void runOnce() = 0;
    virtual void stop() = 0;
    [[nodiscard]] virtual ConnectionState connectionState() const = 0;
    [[nodiscard]] virtual SourceCounters counters() const = 0;
    // Milliseconds until the next runOnce() can do useful work; 0 means call again at once.
    [[nodiscard]] virtual std::int64_t idleHintMilliseconds() const;
    // Asks for one read of the stored trouble codes and the vehicle identification on a later
    // runOnce() while connected (DN-030). Safe from any thread. Default: sources that cannot
    // read diagnostics ignore it.
    virtual void requestDiagnostics();
};

} // namespace lexus_head_unit
