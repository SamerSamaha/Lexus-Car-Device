// Verifies: REQ-002

#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"
#include "lexus_head_unit/service/signal_store.h"
#include "lexus_head_unit/service/signal_store_feeder.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace {

using lexus_head_unit::ConnectionState;
using lexus_head_unit::ConnectionTransition;
using lexus_head_unit::ConnectionTrigger;
using lexus_head_unit::SignalId;
using lexus_head_unit::SignalSample;
using lexus_head_unit::SignalStatus;
using lexus_head_unit::SignalStore;
using lexus_head_unit::SignalStoreFeeder;
using lexus_head_unit::Unit;

SignalSample coolantSample(double degreesCelsius, std::int64_t timestampMilliseconds) {
    SignalSample sample;
    sample.signalId = SignalId::CoolantTemperature;
    sample.value = degreesCelsius;
    sample.unit = Unit::DegreesCelsius;
    sample.timestampMilliseconds = timestampMilliseconds;
    return sample;
}

class SignalStoreFeederTest : public ::testing::Test {
protected:
    SignalStore m_store;
    SignalStoreFeeder m_feeder{m_store};
};

TEST_F(SignalStoreFeederTest, StartsDisconnectedWithNoTransition) {
    EXPECT_EQ(m_feeder.connectionState(), ConnectionState::Disconnected);
    EXPECT_FALSE(m_feeder.latestTransition().has_value());
    EXPECT_EQ(m_feeder.transitionCount(), 0U);
}

TEST_F(SignalStoreFeederTest, SampleLandsInTheStoreAsValid) {
    m_feeder.onSample(coolantSample(88.0, 500));
    EXPECT_EQ(m_store.latest(SignalId::CoolantTemperature).status, SignalStatus::Valid);
    EXPECT_DOUBLE_EQ(m_store.latest(SignalId::CoolantTemperature).value, 88.0);
    EXPECT_EQ(m_feeder.acceptedSampleCount(), 1U);
    EXPECT_EQ(m_feeder.rejectedSampleCount(), 0U);
}

TEST_F(SignalStoreFeederTest, RejectedSampleIsCountedByFeederAndStore) {
    m_feeder.onSample(coolantSample(88.0, 500));
    m_feeder.onSample(coolantSample(89.0, 400));
    EXPECT_EQ(m_feeder.acceptedSampleCount(), 1U);
    EXPECT_EQ(m_feeder.rejectedSampleCount(), 1U);
    EXPECT_EQ(m_store.rejectedOutOfOrderCount(), 1U);
    EXPECT_DOUBLE_EQ(m_store.latest(SignalId::CoolantTemperature).value, 88.0);
}

TEST_F(SignalStoreFeederTest, TransitionsAreKeptCountedAndForwarded) {
    std::vector<ConnectionTransition> forwarded;
    m_feeder.setTransitionHook([&forwarded](const ConnectionTransition& transition) {
        forwarded.push_back(transition);
    });

    ConnectionTransition first;
    first.from = ConnectionState::Disconnected;
    first.trigger = ConnectionTrigger::StartRequested;
    first.to = ConnectionState::Connecting;
    first.timestampMilliseconds = 10;
    ConnectionTransition second;
    second.from = ConnectionState::Connecting;
    second.trigger = ConnectionTrigger::HandshakeSucceeded;
    second.to = ConnectionState::Connected;
    second.timestampMilliseconds = 20;

    m_feeder.onConnectionChanged(first);
    m_feeder.onConnectionChanged(second);

    EXPECT_EQ(m_feeder.connectionState(), ConnectionState::Connected);
    EXPECT_EQ(m_feeder.transitionCount(), 2U);
    ASSERT_TRUE(m_feeder.latestTransition().has_value());
    const ConnectionTransition latest =
        m_feeder.latestTransition().value_or(ConnectionTransition{});
    EXPECT_EQ(latest.timestampMilliseconds, 20);
    ASSERT_EQ(forwarded.size(), 2U);
    EXPECT_EQ(forwarded.at(0).to, ConnectionState::Connecting);
}

} // namespace
