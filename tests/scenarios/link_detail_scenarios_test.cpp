// Verifies: REQ-023, REQ-008

#include "emulator_process.h"
#include "lexus_head_unit/hardware/elm327_obd_source.h"
#include "lexus_head_unit/hardware/elm327_source_configuration.h"
#include "lexus_head_unit/hardware/file_descriptor_byte_transport.h"
#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/link_detail.h"
#include "lexus_head_unit/service/signal_store.h"
#include "lexus_head_unit/service/signal_store_feeder.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <unistd.h>

namespace {

using lexus_head_unit::ConnectionState;
using lexus_head_unit::Elm327ObdSource;
using lexus_head_unit::Elm327SourceConfiguration;
using lexus_head_unit::FileDescriptorByteTransport;
using lexus_head_unit::LinkDetail;
using lexus_head_unit::SignalStore;
using lexus_head_unit::SignalStoreFeeder;
using lexus_head_unit::SteadyClock;
using lexus_head_unit::testing::EmulatorProcess;

// The four situations of the car day (DN-042) against the emulator, in real time: no adapter,
// adapter with the ignition off, live, and the adapter gone after live data.
class LinkDetailScenariosTest : public ::testing::Test {
protected:
    EmulatorProcess m_emulator;
    SteadyClock m_clock;
    SignalStore m_store;
    SignalStoreFeeder m_feeder{m_store};
    std::unique_ptr<FileDescriptorByteTransport> m_transport;
    std::unique_ptr<Elm327ObdSource> m_source;

    void TearDown() override {
        if (m_source) {
            m_source->stop();
        }
    }

    void startSourceOn(const std::string& devicePath) {
        m_transport = std::make_unique<FileDescriptorByteTransport>(devicePath);
        Elm327SourceConfiguration configuration;
        configuration.devicePath = devicePath;
        m_source = std::make_unique<Elm327ObdSource>(*m_transport, m_clock, configuration);
        m_source->start(m_feeder);
    }

    // The emulator with the ignition as given, and the source started on it.
    void startOnEmulator(bool ignitionOn) {
        ASSERT_TRUE(m_emulator.start()) << m_emulator.lastError();
        if (!ignitionOn) {
            ASSERT_EQ(m_emulator.control("ignition off"), "OK");
        }
        startSourceOn(m_emulator.linkPath());
    }

    void pollTimes(int count) {
        for (int poll = 0; poll < count; ++poll) {
            m_source->runOnce();
        }
    }

    // Runs the source until it reports the detail or the limit passes; returns elapsed ms.
    std::int64_t runUntilDetail(LinkDetail detail, std::int64_t limitMilliseconds) {
        const std::int64_t startedAt = m_clock.nowMilliseconds();
        while (m_source->linkDetail() != detail &&
               m_clock.nowMilliseconds() - startedAt < limitMilliseconds) {
            m_source->runOnce();
            if (m_source->linkDetail() != detail) {
                const std::int64_t hint =
                    std::clamp<std::int64_t>(m_source->idleHintMilliseconds(), 1, 50);
                std::this_thread::sleep_for(std::chrono::milliseconds(hint));
            }
        }
        return m_clock.nowMilliseconds() - startedAt;
    }
};

TEST_F(LinkDetailScenariosTest, NoAdapterIsSearchingAndAnAdapterThatAppearsGoesLive) {
    const std::string missing = "/tmp/lexus_no_adapter_" + std::to_string(::getpid());
    startSourceOn(missing);
    EXPECT_EQ(m_source->connectionState(), ConnectionState::Error);
    EXPECT_EQ(m_source->linkDetail(), LinkDetail::SearchingForAdapter);
    // Still searching after retries.
    EXPECT_GE(runUntilDetail(LinkDetail::Live, 3500), 3500);
    EXPECT_EQ(m_source->linkDetail(), LinkDetail::SearchingForAdapter);
    EXPECT_GE(m_source->connectionAttempts(), 3U);
}

TEST_F(LinkDetailScenariosTest, IgnitionOffAtStartThenOnGoesLiveWithin15Seconds) {
    startOnEmulator(false);
    EXPECT_EQ(m_source->connectionState(), ConnectionState::Error);
    EXPECT_EQ(m_source->linkDetail(), LinkDetail::AdapterWithoutVehicle);

    ASSERT_EQ(m_emulator.control("ignition on"), "OK");
    const std::int64_t elapsed = runUntilDetail(LinkDetail::Live, 15000);
    EXPECT_LT(elapsed, 15000);
    EXPECT_EQ(m_source->connectionState(), ConnectionState::Connected);
}

TEST_F(LinkDetailScenariosTest, IgnitionOffWhileLiveIsReportedAfterTheVehicleSilenceTimeout) {
    startOnEmulator(true);
    ASSERT_EQ(m_source->linkDetail(), LinkDetail::Live);
    pollTimes(20);

    ASSERT_EQ(m_emulator.control("ignition off"), "OK");
    const std::int64_t elapsed = runUntilDetail(LinkDetail::AdapterWithoutVehicle, 10000);
    // 5000 ms of adapter replies without vehicle data, plus at most one request.
    EXPECT_GE(elapsed, 4500);
    EXPECT_LT(elapsed, 7000);
    EXPECT_EQ(m_source->connectionState(), ConnectionState::Error);

    ASSERT_EQ(m_emulator.control("ignition on"), "OK");
    EXPECT_LT(runUntilDetail(LinkDetail::Live, 15000), 15000);
}

TEST_F(LinkDetailScenariosTest, AdapterGoneAfterLiveDataIsLinkLostRetryingWithinTwoSeconds) {
    startOnEmulator(true);
    ASSERT_EQ(m_source->linkDetail(), LinkDetail::Live);

    m_emulator.kill();
    const std::int64_t elapsed = runUntilDetail(LinkDetail::LinkLostRetrying, 5000);
    EXPECT_LT(elapsed, 2000);
    // A retry while the adapter is still missing keeps saying the link was lost.
    EXPECT_GE(runUntilDetail(LinkDetail::Live, 2500), 2500);
    EXPECT_EQ(m_source->linkDetail(), LinkDetail::LinkLostRetrying);
    EXPECT_GE(m_source->connectionAttempts(), 2U);
}

} // namespace
