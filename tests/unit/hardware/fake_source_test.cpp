// Verifies: REQ-002

#include "lexus_head_unit/hardware/fake_source.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/manual_clock.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"
#include "lexus_head_unit/service/vehicle_data_source.h"

#include <gtest/gtest.h>

#include <vector>

namespace {

using lexus_head_unit::ConnectionState;
using lexus_head_unit::ConnectionTransition;
using lexus_head_unit::ConnectionTrigger;
using lexus_head_unit::FakeSource;
using lexus_head_unit::ManualClock;
using lexus_head_unit::SignalId;
using lexus_head_unit::SignalSample;
using lexus_head_unit::Unit;
using lexus_head_unit::VehicleDataSourceListener;

class RecordingListener final : public VehicleDataSourceListener {
public:
    std::vector<SignalSample> samples;
    std::vector<ConnectionTransition> transitions;

    void onSample(const SignalSample& sample) override {
        samples.push_back(sample);
    }

    void onConnectionChanged(const ConnectionTransition& transition) override {
        transitions.push_back(transition);
    }
};

class FakeSourceTest : public ::testing::Test {
protected:
    ManualClock m_clock{1000};
    FakeSource m_source{m_clock};
    RecordingListener m_listener;
};

TEST_F(FakeSourceTest, StartsDisconnectedAndIsNamedFake) {
    EXPECT_EQ(m_source.connectionState(), ConnectionState::Disconnected);
    EXPECT_EQ(m_source.name(), "fake");
    EXPECT_EQ(m_source.pendingSteps(), 0U);
}

TEST_F(FakeSourceTest, StartConnectsThroughConnectingAndStopDisconnects) {
    m_source.start(m_listener);
    EXPECT_EQ(m_source.connectionState(), ConnectionState::Connected);
    ASSERT_EQ(m_listener.transitions.size(), 2U);
    EXPECT_EQ(m_listener.transitions.at(0).to, ConnectionState::Connecting);
    EXPECT_EQ(m_listener.transitions.at(1).to, ConnectionState::Connected);
    EXPECT_EQ(m_listener.transitions.at(1).trigger, ConnectionTrigger::HandshakeSucceeded);

    m_source.stop();
    EXPECT_EQ(m_source.connectionState(), ConnectionState::Disconnected);
    ASSERT_EQ(m_listener.transitions.size(), 3U);
    EXPECT_EQ(m_listener.transitions.at(2).trigger, ConnectionTrigger::StopRequested);
}

TEST_F(FakeSourceTest, ScriptedSampleIsEmittedWithTheClockTimeAndDefinedUnit) {
    m_source.start(m_listener);
    m_source.scriptSample(SignalId::EngineRpm, 850.0);
    m_clock.setMilliseconds(4321);
    EXPECT_EQ(m_source.pendingSteps(), 1U);

    m_source.runOnce();

    EXPECT_EQ(m_source.pendingSteps(), 0U);
    ASSERT_EQ(m_listener.samples.size(), 1U);
    EXPECT_EQ(m_listener.samples.front().signalId, SignalId::EngineRpm);
    EXPECT_DOUBLE_EQ(m_listener.samples.front().value, 850.0);
    EXPECT_EQ(m_listener.samples.front().unit, Unit::RevolutionsPerMinute);
    EXPECT_EQ(m_listener.samples.front().timestampMilliseconds, 4321);
    EXPECT_EQ(m_source.counters().samplesEmitted, 1U);
}

TEST_F(FakeSourceTest, RunOnceWithoutStartOrAfterStopEmitsNothingAndKeepsTheScript) {
    m_source.scriptSample(SignalId::VehicleSpeed, 10.0);
    m_source.runOnce();
    EXPECT_TRUE(m_listener.samples.empty());
    EXPECT_EQ(m_source.pendingSteps(), 1U);

    m_source.start(m_listener);
    m_source.stop();
    m_source.runOnce();
    EXPECT_TRUE(m_listener.samples.empty());
    EXPECT_EQ(m_source.pendingSteps(), 1U);
}

TEST_F(FakeSourceTest, MalformedInputIsCountedAndEmitsNoSample) {
    m_source.start(m_listener);
    m_source.scriptMalformedInput();
    m_source.runOnce();
    EXPECT_TRUE(m_listener.samples.empty());
    EXPECT_EQ(m_source.counters().malformedInputs, 1U);
    EXPECT_EQ(m_source.connectionState(), ConnectionState::Connected);
}

TEST_F(FakeSourceTest, LinkLossThenReconnectWalksErrorConnectingConnected) {
    m_source.start(m_listener);
    m_source.scriptLinkLoss();
    m_source.scriptReconnect();

    m_source.runOnce();
    EXPECT_EQ(m_source.connectionState(), ConnectionState::Error);
    m_source.runOnce();
    EXPECT_EQ(m_source.connectionState(), ConnectionState::Connected);

    ASSERT_EQ(m_listener.transitions.size(), 5U);
    EXPECT_EQ(m_listener.transitions.at(2).trigger, ConnectionTrigger::LinkLost);
    EXPECT_EQ(m_listener.transitions.at(3).trigger, ConnectionTrigger::BackoffElapsed);
    EXPECT_EQ(m_listener.transitions.at(4).trigger, ConnectionTrigger::HandshakeSucceeded);
    EXPECT_EQ(m_source.counters().rejectedTransitions, 0U);
}

TEST_F(FakeSourceTest, ScriptedHandshakeFailureLeavesTheSourceInError) {
    m_source.scriptHandshakeFailure();
    m_source.start(m_listener);
    EXPECT_EQ(m_source.connectionState(), ConnectionState::Error);
    ASSERT_EQ(m_listener.transitions.size(), 2U);
    EXPECT_EQ(m_listener.transitions.at(1).trigger, ConnectionTrigger::HandshakeFailed);

    m_source.scriptReconnect();
    m_source.runOnce();
    EXPECT_EQ(m_source.connectionState(), ConnectionState::Connected);
}

TEST_F(FakeSourceTest, SecondStartIsRejectedAndCounted) {
    m_source.start(m_listener);
    m_source.start(m_listener);
    EXPECT_EQ(m_source.connectionState(), ConnectionState::Connected);
    EXPECT_EQ(m_source.counters().rejectedTransitions, 1U);
}

TEST_F(FakeSourceTest, StopWhileDisconnectedIsNotCountedAsARejectedTransition) {
    m_source.stop();
    EXPECT_EQ(m_source.counters().rejectedTransitions, 0U);
    EXPECT_TRUE(m_listener.transitions.empty());
}

TEST_F(FakeSourceTest, LinkLossWhileNotConnectedIsRejectedAndCounted) {
    m_source.scriptHandshakeFailure();
    m_source.start(m_listener);
    m_source.scriptLinkLoss();
    m_source.runOnce();
    EXPECT_EQ(m_source.connectionState(), ConnectionState::Error);
    EXPECT_EQ(m_source.counters().rejectedTransitions, 1U);
}

} // namespace
