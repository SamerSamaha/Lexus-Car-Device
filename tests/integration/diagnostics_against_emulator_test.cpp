// Verifies: REQ-021, REQ-001

#include "emulator_process.h"
#include "lexus_head_unit/hardware/elm327_obd_source.h"
#include "lexus_head_unit/hardware/elm327_source_configuration.h"
#include "lexus_head_unit/hardware/file_descriptor_byte_transport.h"
#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/diagnostics_report.h"
#include "lexus_head_unit/service/signal_store.h"
#include "lexus_head_unit/service/signal_store_feeder.h"

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

namespace {

using lexus_head_unit::ConnectionState;
using lexus_head_unit::DiagnosticsReport;
using lexus_head_unit::Elm327ObdSource;
using lexus_head_unit::Elm327SourceConfiguration;
using lexus_head_unit::FileDescriptorByteTransport;
using lexus_head_unit::SignalStore;
using lexus_head_unit::SignalStoreFeeder;
using lexus_head_unit::SteadyClock;
using lexus_head_unit::testing::EmulatorProcess;

std::vector<std::string> codesOf(const DiagnosticsReport& report) {
    std::vector<std::string> codes;
    codes.reserve(report.troubleCodes.size());
    for (const lexus_head_unit::TroubleCode& code : report.troubleCodes) {
        codes.push_back(code.code);
    }
    return codes;
}

struct Session {
    explicit Session(std::vector<std::string> arguments)
        : emulator(std::move(arguments)), transport(emulator.linkPath()), feeder(store) {
        configuration.devicePath = emulator.linkPath();
        feeder.setDiagnosticsHook([this](const DiagnosticsReport& report) {
            reports.push_back(report);
        });
    }

    EmulatorProcess emulator;
    FileDescriptorByteTransport transport;
    SteadyClock clock;
    Elm327SourceConfiguration configuration;
    SignalStore store;
    SignalStoreFeeder feeder;
    std::vector<DiagnosticsReport> reports;
};

void expectSixCodesAndTheIdentification(const DiagnosticsReport& report) {
    EXPECT_TRUE(report.codesRead);
    EXPECT_EQ(codesOf(report),
              (std::vector<std::string>{"P0133", "C0035", "B1000", "U0100", "P0420", "P0171"}));
    ASSERT_EQ(report.troubleCodes.size(), 6U);
    EXPECT_EQ(report.troubleCodes.at(4).description, "Catalyst efficiency below threshold, bank 1");
    EXPECT_TRUE(report.identificationRead);
    // The emulator's default identification: the hypothetical value in the allowlist.
    EXPECT_EQ(report.vehicleIdentification, "1M8GDM9AXKP042788");
}

void expectOneReadAndNothingForbidden(EmulatorProcess& emulator) {
    EXPECT_EQ(emulator.requestCount("03"), 1);
    EXPECT_EQ(emulator.requestCount("0902"), 1);
    EXPECT_EQ(emulator.forbiddenRequestCount(), 0);
}

TEST(DiagnosticsAgainstEmulatorTest, SixStoredCodesAcrossFourSystemsAndTheIdentification) {
    Session session({"--dtc", "P0133", "C0035", "B1000", "U0100", "P0420", "P0171"});
    ASSERT_TRUE(session.emulator.start()) << session.emulator.lastError();
    Elm327ObdSource source(session.transport, session.clock, session.configuration);
    source.start(session.feeder);
    ASSERT_EQ(source.connectionState(), ConnectionState::Connected);

    source.requestDiagnostics();
    source.runOnce();
    source.runOnce();
    source.stop();

    ASSERT_EQ(session.reports.size(), 1U);
    expectSixCodesAndTheIdentification(session.reports.front());
    expectOneReadAndNothingForbidden(session.emulator);
    EXPECT_EQ(source.counters().malformedInputs, 0U);
}

void expectAnEmptyReadThenPolling(Session& session) {
    ASSERT_EQ(session.reports.size(), 1U);
    EXPECT_TRUE(session.reports.front().codesRead);
    EXPECT_TRUE(session.reports.front().troubleCodes.empty());
    EXPECT_EQ(session.emulator.requestCount("03"), 1);
    EXPECT_GE(session.emulator.requestCount("010D"), 1);
    EXPECT_EQ(session.emulator.forbiddenRequestCount(), 0);
}

TEST(DiagnosticsAgainstEmulatorTest, NoStoredCodesIsAnEmptyListAndPollingCarriesOn) {
    Session session({});
    ASSERT_TRUE(session.emulator.start()) << session.emulator.lastError();
    Elm327ObdSource source(session.transport, session.clock, session.configuration);
    source.start(session.feeder);
    ASSERT_EQ(source.connectionState(), ConnectionState::Connected);

    source.requestDiagnostics();
    for (int poll = 0; poll < 10; ++poll) {
        source.runOnce();
    }
    source.stop();

    expectAnEmptyReadThenPolling(session);
}

} // namespace
