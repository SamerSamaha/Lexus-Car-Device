// Verifies: REQ-003, REQ-006

#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"
#include "lexus_head_unit/service/signal_store.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace {

using lexus_head_unit::allSignalIds;
using lexus_head_unit::signalCount;
using lexus_head_unit::SignalId;
using lexus_head_unit::SignalSample;
using lexus_head_unit::SignalStatus;
using lexus_head_unit::SignalStore;
using lexus_head_unit::toString;
using lexus_head_unit::Unit;
using lexus_head_unit::UpdateResult;

SignalSample speedSample(double kilometresPerHour, std::int64_t timestampMilliseconds) {
    SignalSample sample;
    sample.signalId = SignalId::VehicleSpeed;
    sample.value = kilometresPerHour;
    sample.unit = Unit::KilometresPerHour;
    sample.timestampMilliseconds = timestampMilliseconds;
    sample.status = SignalStatus::NeverReceived;
    return sample;
}

// A value outside the enumeration, as a corrupt cast or a newer sender could produce it.
SignalId unknownSignalId() {
    const auto outOfRange = static_cast<int>(signalCount);
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    return static_cast<SignalId>(outOfRange);
}

class SignalStoreTest : public ::testing::Test {
protected:
    SignalStore m_store;
    std::vector<SignalSample> m_notifications;

    void SetUp() override {
        m_store.setChangeListener([this](const SignalSample& sample) {
            m_notifications.push_back(sample);
        });
    }
};

TEST_F(SignalStoreTest, EverySignalStartsNeverReceived) {
    for (const SignalId signalId : allSignalIds) {
        const SignalSample& stored = m_store.latest(signalId);
        EXPECT_EQ(stored.signalId, signalId);
        EXPECT_EQ(stored.status, SignalStatus::NeverReceived);
    }
}

TEST_F(SignalStoreTest, EverySignalStartsWithItsDefinedUnitAndTimestampZero) {
    EXPECT_EQ(m_store.latest(SignalId::EngineRpm).unit, Unit::RevolutionsPerMinute);
    EXPECT_EQ(m_store.latest(SignalId::ControlModuleVoltage).unit, Unit::Volts);
    EXPECT_EQ(m_store.latest(SignalId::FuelLevel).unit, Unit::Percent);
    EXPECT_EQ(m_store.latest(SignalId::VehicleSpeed).timestampMilliseconds, 0);
}

TEST_F(SignalStoreTest, FirstUpdateMovesNeverReceivedToValidWithExactFields) {
    EXPECT_EQ(m_store.update(speedSample(63.0, 5000)), UpdateResult::Accepted);

    const SignalSample& stored = m_store.latest(SignalId::VehicleSpeed);
    EXPECT_EQ(stored.status, SignalStatus::Valid);
    EXPECT_DOUBLE_EQ(stored.value, 63.0);
    EXPECT_EQ(stored.unit, Unit::KilometresPerHour);
    EXPECT_EQ(stored.timestampMilliseconds, 5000);
}

TEST_F(SignalStoreTest, SourceSuppliedStatusIsIgnoredAndValidIsSet) {
    SignalSample sample = speedSample(10.0, 100);
    sample.status = SignalStatus::Stale;
    EXPECT_EQ(m_store.update(sample), UpdateResult::Accepted);
    EXPECT_EQ(m_store.latest(SignalId::VehicleSpeed).status, SignalStatus::Valid);
}

TEST_F(SignalStoreTest, SamplesOneMillisecondApartAreBothAccepted) {
    EXPECT_EQ(m_store.update(speedSample(10.0, 1000)), UpdateResult::Accepted);
    EXPECT_EQ(m_store.update(speedSample(11.0, 1001)), UpdateResult::Accepted);
    EXPECT_DOUBLE_EQ(m_store.latest(SignalId::VehicleSpeed).value, 11.0);
    EXPECT_EQ(m_store.latest(SignalId::VehicleSpeed).timestampMilliseconds, 1001);
}

TEST_F(SignalStoreTest, OlderTimestampIsRejectedCountedAndLeavesTheStoredSample) {
    ASSERT_EQ(m_store.update(speedSample(50.0, 2000)), UpdateResult::Accepted);
    m_notifications.clear();

    EXPECT_EQ(m_store.update(speedSample(99.0, 1999)), UpdateResult::RejectedOutOfOrder);

    EXPECT_EQ(m_store.rejectedOutOfOrderCount(), 1U);
    EXPECT_DOUBLE_EQ(m_store.latest(SignalId::VehicleSpeed).value, 50.0);
    EXPECT_EQ(m_store.latest(SignalId::VehicleSpeed).timestampMilliseconds, 2000);
    EXPECT_EQ(m_store.latest(SignalId::VehicleSpeed).status, SignalStatus::Valid);
    EXPECT_TRUE(m_notifications.empty());
}

TEST_F(SignalStoreTest, EqualTimestampIsRejectedAsOutOfOrder) {
    ASSERT_EQ(m_store.update(speedSample(50.0, 2000)), UpdateResult::Accepted);
    EXPECT_EQ(m_store.update(speedSample(51.0, 2000)), UpdateResult::RejectedOutOfOrder);
    EXPECT_EQ(m_store.rejectedOutOfOrderCount(), 1U);
    EXPECT_DOUBLE_EQ(m_store.latest(SignalId::VehicleSpeed).value, 50.0);
}

TEST_F(SignalStoreTest, FirstSampleIsAcceptedWhateverItsTimestamp) {
    EXPECT_EQ(m_store.update(speedSample(5.0, 0)), UpdateResult::Accepted);
    EXPECT_EQ(m_store.latest(SignalId::VehicleSpeed).status, SignalStatus::Valid);
}

TEST_F(SignalStoreTest, WrongUnitIsRejectedAndCounted) {
    SignalSample sample = speedSample(63.0, 100);
    sample.unit = Unit::RevolutionsPerMinute;
    EXPECT_EQ(m_store.update(sample), UpdateResult::RejectedUnitMismatch);
    EXPECT_EQ(m_store.rejectedUnitMismatchCount(), 1U);
    EXPECT_EQ(m_store.latest(SignalId::VehicleSpeed).status, SignalStatus::NeverReceived);
    EXPECT_TRUE(m_notifications.empty());
}

TEST_F(SignalStoreTest, UnknownSignalIsRejectedAndCountedWithoutIndexingOutOfBounds) {
    SignalSample sample = speedSample(1.0, 100);
    sample.signalId = unknownSignalId();
    EXPECT_EQ(m_store.update(sample), UpdateResult::RejectedUnknownSignal);
    EXPECT_EQ(m_store.rejectedUnknownSignalCount(), 1U);
    EXPECT_FALSE(m_store.markStale(unknownSignalId()));
    EXPECT_FALSE(m_store.setStalenessTimeout(unknownSignalId(), 500));
}

TEST_F(SignalStoreTest, MarkStaleMovesValidToStaleKeepingTheValueAndNotifiesOnce) {
    ASSERT_EQ(m_store.update(speedSample(63.0, 100)), UpdateResult::Accepted);
    m_notifications.clear();

    EXPECT_TRUE(m_store.markStale(SignalId::VehicleSpeed));
    EXPECT_EQ(m_store.latest(SignalId::VehicleSpeed).status, SignalStatus::Stale);
    EXPECT_DOUBLE_EQ(m_store.latest(SignalId::VehicleSpeed).value, 63.0);
    EXPECT_EQ(m_store.latest(SignalId::VehicleSpeed).timestampMilliseconds, 100);
    ASSERT_EQ(m_notifications.size(), 1U);
    EXPECT_EQ(m_notifications.front().status, SignalStatus::Stale);

    EXPECT_FALSE(m_store.markStale(SignalId::VehicleSpeed));
    EXPECT_EQ(m_notifications.size(), 1U);
}

TEST_F(SignalStoreTest, MarkStaleOnNeverReceivedIsANoOp) {
    EXPECT_FALSE(m_store.markStale(SignalId::FuelLevel));
    EXPECT_EQ(m_store.latest(SignalId::FuelLevel).status, SignalStatus::NeverReceived);
    EXPECT_TRUE(m_notifications.empty());
}

TEST_F(SignalStoreTest, UpdateAfterStaleMovesBackToValid) {
    ASSERT_EQ(m_store.update(speedSample(63.0, 100)), UpdateResult::Accepted);
    ASSERT_TRUE(m_store.markStale(SignalId::VehicleSpeed));
    EXPECT_EQ(m_store.update(speedSample(64.0, 2000)), UpdateResult::Accepted);
    EXPECT_EQ(m_store.latest(SignalId::VehicleSpeed).status, SignalStatus::Valid);
    EXPECT_DOUBLE_EQ(m_store.latest(SignalId::VehicleSpeed).value, 64.0);
}

TEST_F(SignalStoreTest, ListenerReceivesEveryChangeInOrder) {
    ASSERT_EQ(m_store.update(speedSample(1.0, 100)), UpdateResult::Accepted);
    ASSERT_EQ(m_store.update(speedSample(2.0, 200)), UpdateResult::Accepted);
    ASSERT_TRUE(m_store.markStale(SignalId::VehicleSpeed));

    ASSERT_EQ(m_notifications.size(), 3U);
    EXPECT_DOUBLE_EQ(m_notifications.at(0).value, 1.0);
    EXPECT_EQ(m_notifications.at(0).status, SignalStatus::Valid);
    EXPECT_DOUBLE_EQ(m_notifications.at(1).value, 2.0);
    EXPECT_EQ(m_notifications.at(1).status, SignalStatus::Valid);
    EXPECT_DOUBLE_EQ(m_notifications.at(2).value, 2.0);
    EXPECT_EQ(m_notifications.at(2).status, SignalStatus::Stale);
}

TEST_F(SignalStoreTest, EmptyListenerStopsNotificationsWithoutChangingBehaviour) {
    m_store.setChangeListener(nullptr);
    EXPECT_EQ(m_store.update(speedSample(1.0, 100)), UpdateResult::Accepted);
    EXPECT_TRUE(m_store.markStale(SignalId::VehicleSpeed));
    EXPECT_TRUE(m_notifications.empty());
    EXPECT_EQ(m_store.latest(SignalId::VehicleSpeed).status, SignalStatus::Stale);
}

TEST_F(SignalStoreTest, StalenessTimeoutDefaultsToOneSecondAndRejectsNonPositiveValues) {
    EXPECT_EQ(m_store.stalenessTimeoutMilliseconds(SignalId::EngineRpm), 1000);
    EXPECT_FALSE(m_store.setStalenessTimeout(SignalId::EngineRpm, 0));
    EXPECT_FALSE(m_store.setStalenessTimeout(SignalId::EngineRpm, -1));
    EXPECT_EQ(m_store.stalenessTimeoutMilliseconds(SignalId::EngineRpm), 1000);
    EXPECT_TRUE(m_store.setStalenessTimeout(SignalId::EngineRpm, 250));
    EXPECT_EQ(m_store.stalenessTimeoutMilliseconds(SignalId::EngineRpm), 250);
    EXPECT_EQ(m_store.stalenessTimeoutMilliseconds(SignalId::VehicleSpeed), 1000);
}

TEST_F(SignalStoreTest, SnapshotCopiesEverySignal) {
    ASSERT_EQ(m_store.update(speedSample(63.0, 100)), UpdateResult::Accepted);
    const SignalStore::Snapshot snapshot = m_store.snapshot();
    ASSERT_EQ(snapshot.size(), signalCount);
    EXPECT_DOUBLE_EQ(snapshot.at(0).value, 63.0);
    EXPECT_EQ(snapshot.at(0).status, SignalStatus::Valid);
    EXPECT_EQ(snapshot.at(1).status, SignalStatus::NeverReceived);
}

TEST_F(SignalStoreTest, UpdateResultsHaveNames) {
    EXPECT_EQ(toString(UpdateResult::Accepted), "Accepted");
    EXPECT_EQ(toString(UpdateResult::RejectedOutOfOrder), "RejectedOutOfOrder");
    EXPECT_EQ(toString(UpdateResult::RejectedUnitMismatch), "RejectedUnitMismatch");
    EXPECT_EQ(toString(UpdateResult::RejectedUnknownSignal), "RejectedUnknownSignal");
}

} // namespace
