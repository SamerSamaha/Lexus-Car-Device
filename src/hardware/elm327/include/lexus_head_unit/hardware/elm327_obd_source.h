#pragma once

#include "lexus_head_unit/hardware/byte_transport.h"
#include "lexus_head_unit/hardware/elm327_protocol.h"
#include "lexus_head_unit/hardware/elm327_source_configuration.h"
#include "lexus_head_unit/hardware/obd_pid_decoder.h"
#include "lexus_head_unit/hardware/supported_pid_set.h"
#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/reconnect_backoff.h"
#include "lexus_head_unit/service/vehicle_data_source.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lexus_head_unit {

class Elm327ObdSource final : public VehicleDataSource {
public:
    Elm327ObdSource(ByteTransport& transport,
                    const Clock& clock,
                    Elm327SourceConfiguration configuration);

    [[nodiscard]] std::string_view name() const override;
    void start(VehicleDataSourceListener& listener) override;
    void runOnce() override;
    void stop() override;
    [[nodiscard]] ConnectionState connectionState() const override;
    [[nodiscard]] SourceCounters counters() const override;
    [[nodiscard]] std::int64_t idleHintMilliseconds() const override;
    void requestDiagnostics() override;

    [[nodiscard]] const SupportedPidSet& supportedPids() const;
    [[nodiscard]] const std::vector<ObdPid>& pollList() const;
    [[nodiscard]] const std::string& adapterIdentity() const;
    [[nodiscard]] const std::string& adapterVoltageText() const;
    [[nodiscard]] const std::string& protocolNumberText() const;
    [[nodiscard]] std::uint64_t connectionAttempts() const;
    [[nodiscard]] std::int64_t nextAttemptAtMilliseconds() const;
    [[nodiscard]] const Elm327Protocol& protocol() const;
    [[nodiscard]] const Elm327SourceConfiguration& configuration() const;

private:
    void attemptConnection();
    bool runSetupCommands();
    bool discoverSupportedPids();
    void pollNextPid();
    void readDiagnostics();
    // Executes one diagnostics request; empty when the link failed and was declared lost.
    std::optional<Elm327Reply> diagnosticsRequest(std::uint8_t mode,
                                                  std::optional<std::uint8_t> pid);
    void handleDataReply(const Elm327Reply& reply, ObdPid pid);
    void noteAdapterReply();
    void linkLost();
    void scheduleRetry();
    void raise(ConnectionTrigger trigger);

    ByteTransport* m_transport;
    const Clock* m_clock;
    Elm327SourceConfiguration m_configuration;
    Elm327Protocol m_protocol;
    ConnectionStateMachine m_machine;
    ReconnectBackoff m_backoff;
    SupportedPidSet m_supportedPids;
    std::vector<ObdPid> m_pollList;
    std::size_t m_pollIndex = 0;
    std::int64_t m_lastAdapterReplyAtMilliseconds = 0;
    std::int64_t m_lastRequestAtMilliseconds = 0;
    std::int64_t m_nextAttemptAtMilliseconds = 0;
    std::string m_adapterIdentity;
    std::string m_adapterVoltageText;
    std::string m_protocolNumberText;
    VehicleDataSourceListener* m_listener = nullptr;
    SourceCounters m_counters;
    std::uint64_t m_connectionAttempts = 0;
    std::atomic<bool> m_diagnosticsRequested{false};
};

} // namespace lexus_head_unit
