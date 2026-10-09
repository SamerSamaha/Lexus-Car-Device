// Verifies: REQ-002

#include "can_source_harness.h"
#include "elm327_source_harness.h"
#include "fake_source_harness.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/manual_clock.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"
#include "lexus_head_unit/service/signal_store.h"
#include "lexus_head_unit/service/signal_store_feeder.h"
#include "lexus_head_unit/service/staleness_monitor.h"
#include "lexus_head_unit/service/vehicle_data_source.h"
#include "source_harness.h"

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>

namespace {

using lexus_head_unit::ConnectionState;
using lexus_head_unit::ManualClock;
using lexus_head_unit::SignalId;
using lexus_head_unit::SignalStatus;
using lexus_head_unit::SignalStore;
using lexus_head_unit::SignalStoreFeeder;
using lexus_head_unit::StalenessMonitor;
using lexus_head_unit::Unit;
using lexus_head_unit::VehicleDataSource;
using lexus_head_unit::testing::CanSourceHarness;
using lexus_head_unit::testing::Elm327SourceHarness;
using lexus_head_unit::testing::FakeSourceHarness;
using lexus_head_unit::testing::SourceHarness;

using HarnessFactory = std::function<std::unique_ptr<SourceHarness>()>;

// The whole service layer wired as the application wires it, with the source under test.
class ServiceAgainstEachSourceTest : public ::testing::TestWithParam<HarnessFactory> {
protected:
    ManualClock m_clock{100000};
    SignalStore m_store;
    StalenessMonitor m_monitor{m_store, m_clock};
    SignalStoreFeeder m_feeder{m_store};
    std::unique_ptr<SourceHarness> m_harness;
    std::unique_ptr<VehicleDataSource> m_source;

    void SetUp() override {
        m_harness = GetParam()();
        m_source = m_harness->makeSource(m_clock);
    }

    void TearDown() override {
        m_source->stop();
    }
};

TEST_P(ServiceAgainstEachSourceTest, StartReachesConnectedAndStopReachesDisconnected) {
    m_source->start(m_feeder);
    m_harness->drain(*m_source);
    EXPECT_EQ(m_source->connectionState(), ConnectionState::Connected);
    EXPECT_EQ(m_feeder.connectionState(), ConnectionState::Connected);

    m_source->stop();
    EXPECT_EQ(m_source->connectionState(), ConnectionState::Disconnected);
    EXPECT_EQ(m_feeder.connectionState(), ConnectionState::Disconnected);
}

TEST_P(ServiceAgainstEachSourceTest, ProducedSampleIsValidInTheStoreWithItsDefinedUnit) {
    m_source->start(m_feeder);
    m_harness->produceSample(SignalId::VehicleSpeed, 63.0);
    m_harness->drain(*m_source);

    // The fake delivers the scripted value; a real source delivers its own, so the suite
    // checks status, unit and timestamp, and the fake's exact value is checked in its own test.
    const auto& stored = m_store.latest(SignalId::VehicleSpeed);
    EXPECT_EQ(stored.status, SignalStatus::Valid);
    EXPECT_EQ(stored.unit, Unit::KilometresPerHour);
    EXPECT_GE(stored.timestampMilliseconds, 100000);
    EXPECT_GE(m_feeder.acceptedSampleCount(), 1U);
    EXPECT_GE(m_source->counters().samplesEmitted, 1U);
    EXPECT_EQ(m_source->counters().samplesEmitted,
              m_feeder.acceptedSampleCount() + m_feeder.rejectedSampleCount());
}

TEST_P(ServiceAgainstEachSourceTest, LinkLossReachesErrorAndSignalsGoStaleThenRecover) {
    m_source->start(m_feeder);
    m_harness->produceSample(SignalId::EngineRpm, 800.0);
    m_harness->drain(*m_source);
    ASSERT_EQ(m_store.latest(SignalId::EngineRpm).status, SignalStatus::Valid);

    m_harness->breakLink();
    m_harness->drain(*m_source);
    EXPECT_EQ(m_feeder.connectionState(), ConnectionState::Error);

    m_clock.advanceMilliseconds(1100);
    EXPECT_GE(m_monitor.check(), 1U);
    EXPECT_EQ(m_store.latest(SignalId::EngineRpm).status, SignalStatus::Stale);

    m_harness->restoreLink();
    m_harness->produceSample(SignalId::EngineRpm, 820.0);
    m_harness->drain(*m_source);
    EXPECT_EQ(m_feeder.connectionState(), ConnectionState::Connected);
    EXPECT_EQ(m_store.latest(SignalId::EngineRpm).status, SignalStatus::Valid);
    EXPECT_GT(m_store.latest(SignalId::EngineRpm).timestampMilliseconds, 100000);
}

TEST_P(ServiceAgainstEachSourceTest, CountersAgreeBetweenSourceFeederAndStore) {
    m_source->start(m_feeder);
    m_harness->produceSample(SignalId::FuelLevel, 50.0);
    m_clock.advanceMilliseconds(10);
    m_harness->produceSample(SignalId::FuelLevel, 49.0);
    m_harness->drain(*m_source);
    m_clock.advanceMilliseconds(10);
    m_harness->produceSample(SignalId::ThrottlePosition, 12.0);
    m_harness->drain(*m_source);

    EXPECT_EQ(m_source->counters().samplesEmitted,
              m_feeder.acceptedSampleCount() + m_feeder.rejectedSampleCount());
    EXPECT_EQ(m_feeder.rejectedSampleCount(),
              m_store.rejectedOutOfOrderCount() + m_store.rejectedUnitMismatchCount() +
                  m_store.rejectedUnknownSignalCount());
    EXPECT_EQ(m_source->counters().rejectedTransitions, 0U);
}

std::string harnessName(const ::testing::TestParamInfo<HarnessFactory>& info) {
    return info.param()->name();
}

INSTANTIATE_TEST_SUITE_P(EverySource,
                         ServiceAgainstEachSourceTest,
                         ::testing::Values(HarnessFactory([]() {
                                               return std::make_unique<FakeSourceHarness>();
                                           }),
                                           HarnessFactory([]() {
                                               return std::make_unique<CanSourceHarness>();
                                           }),
                                           HarnessFactory([]() {
                                               return std::make_unique<Elm327SourceHarness>();
                                           })),
                         harnessName);

} // namespace
