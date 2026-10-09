#pragma once

#include "lexus_head_unit/hardware/can_frame_reader.h"
#include "lexus_head_unit/hardware/dbc_decoder.h"
#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/key_value_configuration.h"
#include "lexus_head_unit/service/reconnect_backoff.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/vehicle_data_source.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lexus_head_unit {

struct CanSourceConfiguration {
    std::string interfaceName = "vcan0";
    std::string dbcPath = "dbc/simulated_vehicle.dbc";
    std::int64_t readTimeoutMilliseconds = 50;
    std::int64_t linkLossTimeoutMilliseconds = 2000;
    int maximumFramesPerRun = 256;
    std::vector<std::int64_t> backoffScheduleMilliseconds = {1000, 2000, 4000, 8000};
    std::int64_t backoffCapMilliseconds = 10000;

    // can.interface, can.dbc, can.read_timeout_ms, can.link_loss_timeout_ms, can.backoff_cap_ms.
    static CanSourceConfiguration fromConfiguration(const KeyValueConfiguration& configuration);
};

// The project signal a DBC signal name stands for; empty for DBC signals the head unit does
// not show.
std::optional<SignalId> signalIdForDbcName(std::string_view dbcSignalName);

// VehicleDataSource over a CanFrameReader and a DbcDecoder (DN-028). Listens only: neither it
// nor its reader can transmit (REQ-001).
class SocketCanDbcSource final : public VehicleDataSource {
public:
    SocketCanDbcSource(CanFrameReader& reader,
                       DbcDatabase database,
                       const Clock& clock,
                       CanSourceConfiguration configuration);

    [[nodiscard]] std::string_view name() const override;
    void start(VehicleDataSourceListener& listener) override;
    void runOnce() override;
    void stop() override;
    [[nodiscard]] ConnectionState connectionState() const override;
    [[nodiscard]] SourceCounters counters() const override;
    [[nodiscard]] std::int64_t idleHintMilliseconds() const override;

    [[nodiscard]] const DbcDecoder& decoder() const;
    // Why the last connection attempt failed; empty after a success.
    [[nodiscard]] const std::string& lastFailure() const;
    [[nodiscard]] std::uint64_t connectionAttempts() const;

private:
    void attemptConnection();
    // Empty when every DBC signal the head unit maps has the project unit.
    [[nodiscard]] std::string unitMismatch() const;
    void readFrames();
    void handleFrame(const CanFrame& frame);
    void linkLost();
    void scheduleRetry();
    void raise(ConnectionTrigger trigger);

    CanFrameReader* m_reader;
    DbcDecoder m_decoder;
    const Clock* m_clock;
    CanSourceConfiguration m_configuration;
    ConnectionStateMachine m_machine;
    ReconnectBackoff m_backoff;
    VehicleDataSourceListener* m_listener = nullptr;
    SourceCounters m_counters;
    std::int64_t m_lastFrameAtMilliseconds = 0;
    std::int64_t m_nextAttemptAtMilliseconds = 0;
    std::uint64_t m_connectionAttempts = 0;
    std::string m_lastFailure;
};

} // namespace lexus_head_unit
