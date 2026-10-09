#pragma once

#include "can_frame_encoder.h"
#include "lexus_head_unit/hardware/dbc_database.h"
#include "lexus_head_unit/hardware/fake_can_frame_reader.h"
#include "lexus_head_unit/hardware/socket_can_dbc_source.h"
#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/vehicle_data_source.h"
#include "source_harness.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <string_view>

namespace lexus_head_unit::testing {

// The DBC signal name the CAN source maps to a project signal.
inline std::string dbcNameOf(SignalId signalId) {
    for (const std::string_view name : {"VehicleSpeed",
                                        "EngineSpeed",
                                        "CoolantTemperature",
                                        "EngineLoad",
                                        "ThrottlePosition",
                                        "IntakeAirTemperature",
                                        "ControlModuleVoltage",
                                        "FuelLevel"}) {
        if (signalIdForDbcName(name) == signalId) {
            return std::string(name);
        }
    }
    return {};
}

// The CAN source over the fake frame reader: a produced sample is a frame of the simulated
// vehicle carrying that value; the link is broken and restored on the fake.
class CanSourceHarness final : public SourceHarness {
public:
    [[nodiscard]] std::string name() const override {
        return "can";
    }

    std::unique_ptr<VehicleDataSource> makeSource(const Clock& clock) override {
        m_database = DbcDatabase::loadFromFile(LEXUS_HEAD_UNIT_DBC_PATH);
        if (!m_database.errors().empty()) {
            ADD_FAILURE() << "DBC: " << m_database.errors().front();
        }
        CanSourceConfiguration configuration;
        configuration.readTimeoutMilliseconds = 0;
        return std::make_unique<SocketCanDbcSource>(m_reader, m_database, clock, configuration);
    }

    void produceSample(SignalId signalId, double value) override {
        const std::string dbcName = dbcNameOf(signalId);
        const DbcMessage* message = messageCarrying(m_database, dbcName);
        if (message == nullptr) {
            ADD_FAILURE() << "no DBC message carries " << dbcName;
            return;
        }
        m_reader.queueFrame(encodeMessage(*message, {{dbcName, value}}));
    }

    void breakLink() override {
        m_reader.breakLink();
    }

    void restoreLink() override {
        m_reader.restoreLink();
    }

    int drain(VehicleDataSource& source) override {
        int calls = 0;
        // Reconnect when the suite has advanced the manual clock past the backoff.
        for (int attempt = 0; attempt < 5 && source.connectionState() == ConnectionState::Error;
             ++attempt) {
            source.runOnce();
            ++calls;
        }
        source.runOnce();
        ++calls;
        while (m_reader.queuedFrameCount() > 0 &&
               source.connectionState() == ConnectionState::Connected && calls < 100) {
            source.runOnce();
            ++calls;
        }
        return calls;
    }

    FakeCanFrameReader& reader() {
        return m_reader;
    }

private:
    FakeCanFrameReader m_reader;
    DbcDatabase m_database;
};

} // namespace lexus_head_unit::testing
