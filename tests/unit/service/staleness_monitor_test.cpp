// Verifies: REQ-006, REQ-003

#include "lexus_head_unit/service/manual_clock.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"
#include "lexus_head_unit/service/signal_store.h"
#include "lexus_head_unit/service/staleness_monitor.h"

#include <gtest/gtest.h>

namespace {

using lexus_head_unit::ManualClock;
using lexus_head_unit::SignalId;
using lexus_head_unit::SignalSample;
using lexus_head_unit::SignalStatus;
using lexus_head_unit::SignalStore;
using lexus_head_unit::StalenessMonitor;
using lexus_head_unit::Unit;
using lexus_head_unit::UpdateResult;

class StalenessMonitorTest : public ::testing::Test {
protected:
    ManualClock m_clock{10000};
    SignalStore m_store;
    StalenessMonitor m_monitor{m_store, m_clock};

    void updateNow(SignalId signalId, Unit unit, double value) {
        SignalSample sample;
        sample.signalId = signalId;
        sample.value = value;
        sample.unit = unit;
        sample.timestampMilliseconds = m_clock.nowMilliseconds();
        ASSERT_EQ(m_store.update(sample), UpdateResult::Accepted);
    }
};

TEST_F(StalenessMonitorTest, SignalIsValidAtTimeoutAndStaleOneHundredMillisecondsLater) {
    updateNow(SignalId::VehicleSpeed, Unit::KilometresPerHour, 50.0);

    m_clock.advanceMilliseconds(1000);
    EXPECT_EQ(m_monitor.check(), 0U);
    EXPECT_EQ(m_store.latest(SignalId::VehicleSpeed).status, SignalStatus::Valid);

    m_clock.advanceMilliseconds(100);
    EXPECT_EQ(m_monitor.check(), 1U);
    EXPECT_EQ(m_store.latest(SignalId::VehicleSpeed).status, SignalStatus::Stale);
    EXPECT_DOUBLE_EQ(m_store.latest(SignalId::VehicleSpeed).value, 50.0);
}

TEST_F(StalenessMonitorTest, PerSignalTimeoutOfTwoHundredFiftyMillisecondsIsHonoured) {
    ASSERT_TRUE(m_store.setStalenessTimeout(SignalId::EngineRpm, 250));
    updateNow(SignalId::EngineRpm, Unit::RevolutionsPerMinute, 800.0);
    updateNow(SignalId::VehicleSpeed, Unit::KilometresPerHour, 50.0);

    m_clock.advanceMilliseconds(250);
    EXPECT_EQ(m_monitor.check(), 0U);
    EXPECT_EQ(m_store.latest(SignalId::EngineRpm).status, SignalStatus::Valid);

    m_clock.advanceMilliseconds(100);
    EXPECT_EQ(m_monitor.check(), 1U);
    EXPECT_EQ(m_store.latest(SignalId::EngineRpm).status, SignalStatus::Stale);
    EXPECT_EQ(m_store.latest(SignalId::VehicleSpeed).status, SignalStatus::Valid);
}

TEST_F(StalenessMonitorTest, OnlyTheExpiredSignalChangesAndASecondCheckChangesNothing) {
    updateNow(SignalId::VehicleSpeed, Unit::KilometresPerHour, 50.0);
    m_clock.advanceMilliseconds(600);
    updateNow(SignalId::CoolantTemperature, Unit::DegreesCelsius, 85.0);

    m_clock.advanceMilliseconds(500);
    EXPECT_EQ(m_monitor.check(), 1U);
    EXPECT_EQ(m_store.latest(SignalId::VehicleSpeed).status, SignalStatus::Stale);
    EXPECT_EQ(m_store.latest(SignalId::CoolantTemperature).status, SignalStatus::Valid);
    EXPECT_EQ(m_store.latest(SignalId::FuelLevel).status, SignalStatus::NeverReceived);

    EXPECT_EQ(m_monitor.check(), 0U);
}

TEST_F(StalenessMonitorTest, FreshSampleAfterStaleReturnsToValid) {
    updateNow(SignalId::VehicleSpeed, Unit::KilometresPerHour, 50.0);
    m_clock.advanceMilliseconds(1500);
    ASSERT_EQ(m_monitor.check(), 1U);

    updateNow(SignalId::VehicleSpeed, Unit::KilometresPerHour, 52.0);
    EXPECT_EQ(m_store.latest(SignalId::VehicleSpeed).status, SignalStatus::Valid);
    EXPECT_EQ(m_monitor.check(), 0U);
}

TEST_F(StalenessMonitorTest, NeverReceivedSignalsAreNeverMarkedStale) {
    m_clock.advanceMilliseconds(100000);
    EXPECT_EQ(m_monitor.check(), 0U);
    EXPECT_EQ(m_store.latest(SignalId::ThrottlePosition).status, SignalStatus::NeverReceived);
}

TEST_F(StalenessMonitorTest, ClockEarlierThanTheSampleLeavesItValid) {
    updateNow(SignalId::VehicleSpeed, Unit::KilometresPerHour, 50.0);
    m_clock.setMilliseconds(0);
    EXPECT_EQ(m_monitor.check(), 0U);
    EXPECT_EQ(m_store.latest(SignalId::VehicleSpeed).status, SignalStatus::Valid);
}

TEST_F(StalenessMonitorTest, ManualClockAdvancesAndSets) {
    EXPECT_EQ(m_clock.nowMilliseconds(), 10000);
    m_clock.advanceMilliseconds(5);
    EXPECT_EQ(m_clock.nowMilliseconds(), 10005);
    m_clock.setMilliseconds(42);
    EXPECT_EQ(m_clock.nowMilliseconds(), 42);
}

} // namespace
