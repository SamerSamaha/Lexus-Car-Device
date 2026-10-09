// Verifies: REQ-021, REQ-001

#include "lexus_head_unit/hardware/elm327_obd_source.h"
#include "lexus_head_unit/hardware/elm327_source_configuration.h"
#include "lexus_head_unit/hardware/fake_byte_transport.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/diagnostics_report.h"
#include "lexus_head_unit/service/manual_clock.h"
#include "lexus_head_unit/service/signal_sample.h"
#include "lexus_head_unit/service/vehicle_data_source.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace {

using lexus_head_unit::ConnectionState;
using lexus_head_unit::ConnectionTransition;
using lexus_head_unit::DiagnosticsReport;
using lexus_head_unit::Elm327ObdSource;
using lexus_head_unit::Elm327SourceConfiguration;
using lexus_head_unit::FakeByteTransport;
using lexus_head_unit::ManualClock;
using lexus_head_unit::SignalSample;

class DiagnosticsListener final : public lexus_head_unit::VehicleDataSourceListener {
public:
    void onSample(const SignalSample& /*sample*/) override {}
    void onConnectionChanged(const ConnectionTransition& /*transition*/) override {}
    void onDiagnostics(const DiagnosticsReport& report) override {
        reports.push_back(report);
    }
    std::vector<DiagnosticsReport> reports;
};

void scriptAdapterWithDiagnostics(FakeByteTransport& transport) {
    transport.scriptReply("ATZ", "ATZ\rELM327 v1.5\r\r>");
    for (const char* setup : {"ATE0", "ATL0", "ATS0", "ATH0", "ATSP0"}) {
        transport.scriptReply(setup, "OK\r\r>");
    }
    transport.scriptReply("ATI", "ELM327 v1.5\r\r>");
    transport.scriptReply("ATRV", "14.1V\r\r>");
    transport.scriptReply("ATDPN", "A6\r\r>");
    transport.scriptReply("0100", "4100BE3FA813\r\r>");
    transport.scriptReply("0120", "41208007A001\r\r>");
    transport.scriptReply("0140", "4140FED00400\r\r>");
    transport.scriptReply("010D", "410D3C\r\r>");
    transport.scriptReply("010C", "410C1AF8\r\r>");
    transport.scriptReply("0105", "41057B\r\r>");
    transport.scriptReply("0104", "410433\r\r>");
    transport.scriptReply("0111", "411119\r\r>");
    transport.scriptReply("010F", "410F41\r\r>");
    transport.scriptReply("0142", "41423714\r\r>");
    transport.scriptReply("012F", "412FA0\r\r>");
    // Two stored codes, single frame; the hypothetical allowlisted VIN, multi-frame.
    transport.scriptReply("03", "430201330420\r\r>");
    transport.scriptReply("0902", "014\r0:490201314D38\r1:47444D3941584B\r2:50303432373838\r\r>");
}

std::size_t timesWritten(const FakeByteTransport& transport, const std::string& command) {
    const auto& written = transport.writtenCommands();
    return static_cast<std::size_t>(std::count(written.begin(), written.end(), command));
}

class Elm327DiagnosticsTest : public ::testing::Test {
protected:
    Elm327DiagnosticsTest() {
        scriptAdapterWithDiagnostics(m_transport);
        m_configuration.devicePath = "fake";
    }

    FakeByteTransport m_transport;
    ManualClock m_clock{100000};
    Elm327SourceConfiguration m_configuration;
    DiagnosticsListener m_listener;
};

TEST_F(Elm327DiagnosticsTest, ARequestReadsCodesAndIdentificationOnceOnTheNextRun) {
    Elm327ObdSource source(m_transport, m_clock, m_configuration);
    source.start(m_listener);
    ASSERT_EQ(source.connectionState(), ConnectionState::Connected);
    source.requestDiagnostics();
    source.runOnce();
    ASSERT_EQ(m_listener.reports.size(), 1U);
    const DiagnosticsReport& report = m_listener.reports.front();
    EXPECT_TRUE(report.codesRead);
    ASSERT_EQ(report.troubleCodes.size(), 2U);
    EXPECT_EQ(report.troubleCodes.at(0).code, "P0133");
    EXPECT_EQ(report.troubleCodes.at(1).description, "Catalyst efficiency below threshold, bank 1");
    EXPECT_TRUE(report.identificationRead);
    EXPECT_EQ(report.vehicleIdentification, "1M8GDM9AXKP042788");
    EXPECT_EQ(report.timestampMilliseconds, 100000);
    EXPECT_EQ(timesWritten(m_transport, "03"), 1U);
    EXPECT_EQ(timesWritten(m_transport, "0902"), 1U);
    // The next run polls live values again; no second read without a second request.
    source.runOnce();
    EXPECT_EQ(m_listener.reports.size(), 1U);
    EXPECT_EQ(timesWritten(m_transport, "03"), 1U);
}

TEST_F(Elm327DiagnosticsTest, WithoutARequestThereIsNoDiagnosticsTraffic) {
    Elm327ObdSource source(m_transport, m_clock, m_configuration);
    source.start(m_listener);
    for (int run = 0; run < 20; ++run) {
        source.runOnce();
    }
    EXPECT_TRUE(m_listener.reports.empty());
    EXPECT_EQ(timesWritten(m_transport, "03"), 0U);
    EXPECT_EQ(timesWritten(m_transport, "0902"), 0U);
}

TEST_F(Elm327DiagnosticsTest, NoDataIsZeroCodesAndAnUnreadableIdentificationIsNotRead) {
    m_transport.replaceReply("03", "NO DATA\r\r>");
    m_transport.replaceReply("0902", "?\r\r>");
    Elm327ObdSource source(m_transport, m_clock, m_configuration);
    source.start(m_listener);
    source.requestDiagnostics();
    source.runOnce();
    ASSERT_EQ(m_listener.reports.size(), 1U);
    EXPECT_TRUE(m_listener.reports.front().codesRead);
    EXPECT_TRUE(m_listener.reports.front().troubleCodes.empty());
    EXPECT_FALSE(m_listener.reports.front().identificationRead);
    EXPECT_EQ(m_listener.reports.front().vehicleIdentification, "");
}

TEST_F(Elm327DiagnosticsTest, ARequestWhileDisconnectedWaitsForTheConnection) {
    Elm327ObdSource source(m_transport, m_clock, m_configuration);
    source.requestDiagnostics();
    source.start(m_listener);
    EXPECT_TRUE(m_listener.reports.empty());
    source.runOnce();
    EXPECT_EQ(m_listener.reports.size(), 1U);
}

} // namespace
