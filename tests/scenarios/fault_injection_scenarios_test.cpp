// Verifies: REQ-006, REQ-007, REQ-010, REQ-008

#include "emulator_process.h"
#include "lexus_head_unit/hardware/elm327_obd_source.h"
#include "lexus_head_unit/hardware/elm327_source_configuration.h"
#include "lexus_head_unit/hardware/file_descriptor_byte_transport.h"
#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_store.h"
#include "lexus_head_unit/service/signal_store_feeder.h"
#include "lexus_head_unit/service/staleness_monitor.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>

namespace {

using lexus_head_unit::ConnectionState;
using lexus_head_unit::Elm327ObdSource;
using lexus_head_unit::Elm327SourceConfiguration;
using lexus_head_unit::FileDescriptorByteTransport;
using lexus_head_unit::SignalId;
using lexus_head_unit::SignalStatus;
using lexus_head_unit::SignalStore;
using lexus_head_unit::SignalStoreFeeder;
using lexus_head_unit::StalenessMonitor;
using lexus_head_unit::SteadyClock;
using lexus_head_unit::testing::EmulatorProcess;

class FaultInjectionScenariosTest : public ::testing::Test {
protected:
    EmulatorProcess m_emulator;
    SteadyClock m_clock;
    SignalStore m_store;
    StalenessMonitor m_monitor{m_store, m_clock};
    SignalStoreFeeder m_feeder{m_store};
    std::unique_ptr<FileDescriptorByteTransport> m_transport;
    std::unique_ptr<Elm327ObdSource> m_source;

    void SetUp() override {
        ASSERT_TRUE(m_emulator.start()) << m_emulator.lastError();
        m_transport = std::make_unique<FileDescriptorByteTransport>(m_emulator.linkPath());
        Elm327SourceConfiguration configuration;
        configuration.devicePath = m_emulator.linkPath();
        m_source = std::make_unique<Elm327ObdSource>(*m_transport, m_clock, configuration);
        m_source->start(m_feeder);
        ASSERT_EQ(m_source->connectionState(), ConnectionState::Connected);
    }

    void TearDown() override {
        m_source->stop();
    }

    // Runs the loop until the source is in the state or the limit passes; returns elapsed ms.
    std::int64_t runUntilState(ConnectionState state, std::int64_t limitMilliseconds) {
        const std::int64_t startedAt = m_clock.nowMilliseconds();
        while (m_source->connectionState() != state &&
               m_clock.nowMilliseconds() - startedAt < limitMilliseconds) {
            m_source->runOnce();
            const std::int64_t hint =
                std::clamp<std::int64_t>(m_source->idleHintMilliseconds(), 1, 50);
            if (m_source->connectionState() != state) {
                waitMilliseconds(hint);
            }
        }
        return m_clock.nowMilliseconds() - startedAt;
    }

    // One pass over the poll list, then a staleness check.
    void pollCycle(int cycles = 1) {
        for (int cycle = 0; cycle < cycles; ++cycle) {
            for (std::size_t poll = 0; poll < m_source->pollList().size(); ++poll) {
                m_source->runOnce();
            }
            m_monitor.check();
        }
    }

    static void waitMilliseconds(std::int64_t milliseconds) {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    }
};

TEST_F(FaultInjectionScenariosTest, StalePidMakesOnlyThatSignalStale) {
    pollCycle(2);
    ASSERT_EQ(m_store.latest(SignalId::VehicleSpeed).status, SignalStatus::Valid);
    ASSERT_EQ(m_store.latest(SignalId::EngineRpm).status, SignalStatus::Valid);

    ASSERT_EQ(m_emulator.control("stale 0D"), "OK");
    const std::uint64_t malformedBefore = m_source->counters().malformedInputs;
    waitMilliseconds(1100);
    pollCycle(1);

    EXPECT_EQ(m_store.latest(SignalId::VehicleSpeed).status, SignalStatus::Stale);
    EXPECT_EQ(m_store.latest(SignalId::EngineRpm).status, SignalStatus::Valid);
    EXPECT_EQ(m_store.latest(SignalId::CoolantTemperature).status, SignalStatus::Valid);
    EXPECT_EQ(m_source->connectionState(), ConnectionState::Connected);
    EXPECT_GE(m_source->counters().malformedInputs, malformedBefore + 1);

    ASSERT_EQ(m_emulator.control("unstale 0D"), "OK");
    pollCycle(1);
    EXPECT_EQ(m_store.latest(SignalId::VehicleSpeed).status, SignalStatus::Valid);
}

TEST_F(FaultInjectionScenariosTest, CorruptRepliesAreCountedAndNeverBecomeValidSamples) {
    pollCycle(1);
    const std::uint64_t samplesBefore = m_source->counters().samplesEmitted;
    const std::uint64_t malformedBefore = m_source->counters().malformedInputs;
    ASSERT_EQ(m_emulator.control("corrupt 8"), "OK");

    pollCycle(1);

    EXPECT_EQ(m_source->counters().malformedInputs, malformedBefore + 8);
    EXPECT_EQ(m_source->counters().samplesEmitted, samplesBefore);
    EXPECT_EQ(m_source->connectionState(), ConnectionState::Connected);
    pollCycle(1);
    EXPECT_EQ(m_source->counters().samplesEmitted, samplesBefore + 8);
}

TEST_F(FaultInjectionScenariosTest,
       IgnitionOffGivesStaleSignalsAndIgnitionOnRecoversWithoutRestart) {
    pollCycle(1);
    ASSERT_EQ(m_emulator.control("ignition off"), "OK");
    waitMilliseconds(1100);
    pollCycle(1);
    EXPECT_EQ(m_store.latest(SignalId::VehicleSpeed).status, SignalStatus::Stale);
    EXPECT_EQ(m_store.latest(SignalId::FuelLevel).status, SignalStatus::Stale);
    EXPECT_EQ(m_source->connectionState(), ConnectionState::Connected);

    ASSERT_EQ(m_emulator.control("ignition on"), "OK");
    pollCycle(1);
    EXPECT_EQ(m_store.latest(SignalId::VehicleSpeed).status, SignalStatus::Valid);
    EXPECT_EQ(m_store.latest(SignalId::FuelLevel).status, SignalStatus::Valid);
    EXPECT_EQ(m_feeder.transitionCount(), 2U);
}

TEST_F(FaultInjectionScenariosTest, SilentAdapterLosesTheLinkWithinTwoSecondsAndRecovers) {
    pollCycle(1);
    ASSERT_EQ(m_emulator.control("silence 3"), "OK");
    const std::int64_t detectedAfter = runUntilState(ConnectionState::Error, 4000);
    EXPECT_EQ(m_source->connectionState(), ConnectionState::Error);
    EXPECT_LE(detectedAfter, 2500) << detectedAfter;
    RecordProperty("detected_after_ms", static_cast<int>(detectedAfter));

    waitMilliseconds(1100);
    m_monitor.check();
    EXPECT_EQ(m_store.latest(SignalId::VehicleSpeed).status, SignalStatus::Stale);

    // The emulator answers again after 3 s; the retries are due 1, 2, 4 s after the loss.
    runUntilState(ConnectionState::Connected, 15000);
    EXPECT_EQ(m_source->connectionState(), ConnectionState::Connected);
    pollCycle(1);
    EXPECT_EQ(m_store.latest(SignalId::VehicleSpeed).status, SignalStatus::Valid);
    EXPECT_EQ(m_emulator.forbiddenRequestCount(), 0);
}

} // namespace
