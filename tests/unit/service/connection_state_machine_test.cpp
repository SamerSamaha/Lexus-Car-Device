// Verifies: REQ-007

#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/manual_clock.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

using lexus_head_unit::allConnectionStates;
using lexus_head_unit::allConnectionTriggers;
using lexus_head_unit::ConnectionState;
using lexus_head_unit::ConnectionStateMachine;
using lexus_head_unit::ConnectionTransition;
using lexus_head_unit::connectionTransitionTable;
using lexus_head_unit::ConnectionTrigger;
using lexus_head_unit::ManualClock;
using lexus_head_unit::toString;
using lexus_head_unit::TransitionResult;

// The written transition table of DN-007, kept here independently of the library's copy so
// that a change in either shows up as a failure.
struct ExpectedRule {
    ConnectionState from;
    ConnectionTrigger trigger;
    ConnectionState to;
};

constexpr std::array<ExpectedRule, 8> expectedRules = {{
    {ConnectionState::Disconnected, ConnectionTrigger::StartRequested, ConnectionState::Connecting},
    {ConnectionState::Connecting,
     ConnectionTrigger::HandshakeSucceeded,
     ConnectionState::Connected},
    {ConnectionState::Connecting, ConnectionTrigger::HandshakeFailed, ConnectionState::Error},
    {ConnectionState::Connecting, ConnectionTrigger::StopRequested, ConnectionState::Disconnected},
    {ConnectionState::Connected, ConnectionTrigger::LinkLost, ConnectionState::Error},
    {ConnectionState::Connected, ConnectionTrigger::StopRequested, ConnectionState::Disconnected},
    {ConnectionState::Error, ConnectionTrigger::BackoffElapsed, ConnectionState::Connecting},
    {ConnectionState::Error, ConnectionTrigger::StopRequested, ConnectionState::Disconnected},
}};

std::optional<ConnectionState> expectedTarget(ConnectionState from, ConnectionTrigger trigger) {
    for (const ExpectedRule& rule : expectedRules) {
        if (rule.from == from && rule.trigger == trigger) {
            return rule.to;
        }
    }
    return std::nullopt;
}

// Drives a fresh machine into the given state along legal transitions.
void driveTo(ConnectionStateMachine& machine, ConnectionState target) {
    if (target == ConnectionState::Disconnected) {
        return;
    }
    ASSERT_EQ(machine.handle(ConnectionTrigger::StartRequested), TransitionResult::Accepted);
    if (target == ConnectionState::Connecting) {
        return;
    }
    ASSERT_EQ(machine.handle(ConnectionTrigger::HandshakeSucceeded), TransitionResult::Accepted);
    if (target == ConnectionState::Connected) {
        return;
    }
    ASSERT_EQ(machine.handle(ConnectionTrigger::LinkLost), TransitionResult::Accepted);
    ASSERT_EQ(machine.state(), ConnectionState::Error);
}

// Checks one (state, trigger) pair on a fresh machine against the written table.
// Returns true if the pair is legal in the written table.
bool checkPair(const ManualClock& clock, ConnectionState from, ConnectionTrigger trigger) {
    ConnectionStateMachine machine{clock};
    driveTo(machine, from);
    EXPECT_EQ(machine.state(), from);

    const std::optional<ConnectionState> expected = expectedTarget(from, trigger);
    const TransitionResult result = machine.handle(trigger);
    const std::string pairName =
        std::string(toString(from)) + " + " + std::string(toString(trigger));
    if (expected.has_value()) {
        EXPECT_EQ(result, TransitionResult::Accepted) << pairName;
        EXPECT_EQ(machine.state(), *expected) << pairName;
        return true;
    }
    EXPECT_EQ(result, TransitionResult::Rejected) << pairName;
    EXPECT_EQ(machine.state(), from) << pairName;
    return false;
}

class ConnectionStateMachineTest : public ::testing::Test {
protected:
    ManualClock m_clock{5000};
    ConnectionStateMachine m_machine{m_clock};
    std::vector<ConnectionTransition> m_transitions;

    void SetUp() override {
        m_machine.setTransitionListener([this](const ConnectionTransition& transition) {
            m_transitions.push_back(transition);
        });
    }

    // Applies each trigger 100 ms apart; returns the state after each one and counts acceptances.
    std::vector<ConnectionState> applyAll(const std::vector<ConnectionTrigger>& triggers,
                                          std::size_t& acceptedCount) {
        std::vector<ConnectionState> reachedStates;
        for (const ConnectionTrigger trigger : triggers) {
            m_clock.advanceMilliseconds(100);
            if (m_machine.handle(trigger) == TransitionResult::Accepted) {
                ++acceptedCount;
            }
            reachedStates.push_back(m_machine.state());
        }
        return reachedStates;
    }
};

TEST_F(ConnectionStateMachineTest, StartsDisconnectedWithNothingRecorded) {
    EXPECT_EQ(m_machine.state(), ConnectionState::Disconnected);
    EXPECT_FALSE(m_machine.lastTransition().has_value());
    EXPECT_EQ(m_machine.acceptedTransitionCount(), 0U);
    EXPECT_EQ(m_machine.rejectedTransitionCount(), 0U);
}

TEST_F(ConnectionStateMachineTest, EveryStateAndTriggerPairMatchesTheWrittenTable) {
    std::size_t acceptedPairs = 0;
    std::size_t rejectedPairs = 0;
    for (const ConnectionState from : allConnectionStates) {
        for (const ConnectionTrigger trigger : allConnectionTriggers) {
            if (checkPair(m_clock, from, trigger)) {
                ++acceptedPairs;
            } else {
                ++rejectedPairs;
            }
        }
    }
    EXPECT_EQ(acceptedPairs, 8U);
    EXPECT_EQ(rejectedPairs, 16U);
}

TEST_F(ConnectionStateMachineTest, LibraryTableEqualsTheWrittenTableWithoutDuplicates) {
    const auto& table = connectionTransitionTable();
    ASSERT_EQ(table.size(), expectedRules.size());
    std::set<std::pair<ConnectionState, ConnectionTrigger>> seenPairs;
    for (const auto& rule : table) {
        EXPECT_TRUE(seenPairs.insert({rule.from, rule.trigger}).second);
        const std::optional<ConnectionState> expected = expectedTarget(rule.from, rule.trigger);
        EXPECT_EQ(rule.to, expected.value_or(ConnectionState::Disconnected));
        EXPECT_TRUE(expected.has_value());
    }
}

TEST_F(ConnectionStateMachineTest, RejectedTriggerIsCountedAndNotNotified) {
    EXPECT_EQ(m_machine.handle(ConnectionTrigger::LinkLost), TransitionResult::Rejected);
    EXPECT_EQ(m_machine.handle(ConnectionTrigger::StopRequested), TransitionResult::Rejected);
    EXPECT_EQ(m_machine.rejectedTransitionCount(), 2U);
    EXPECT_EQ(m_machine.acceptedTransitionCount(), 0U);
    EXPECT_EQ(m_machine.state(), ConnectionState::Disconnected);
    EXPECT_TRUE(m_transitions.empty());
    EXPECT_FALSE(m_machine.lastTransition().has_value());
}

TEST_F(ConnectionStateMachineTest, AcceptedTriggerRecordsFromTriggerToAndClockTime) {
    m_clock.setMilliseconds(7000);
    ASSERT_EQ(m_machine.handle(ConnectionTrigger::StartRequested), TransitionResult::Accepted);

    ASSERT_TRUE(m_machine.lastTransition().has_value());
    const ConnectionTransition last = m_machine.lastTransition().value_or(ConnectionTransition{});
    EXPECT_EQ(last.from, ConnectionState::Disconnected);
    EXPECT_EQ(last.trigger, ConnectionTrigger::StartRequested);
    EXPECT_EQ(last.to, ConnectionState::Connecting);
    EXPECT_EQ(last.timestampMilliseconds, 7000);
    ASSERT_EQ(m_transitions.size(), 1U);
    EXPECT_EQ(m_transitions.front().to, ConnectionState::Connecting);
    EXPECT_EQ(m_transitions.front().timestampMilliseconds, 7000);
    EXPECT_EQ(m_machine.acceptedTransitionCount(), 1U);
}

TEST_F(ConnectionStateMachineTest, MainScenarioRunsEndToEndInOrder) {
    const std::vector<ConnectionTrigger> triggers = {
        ConnectionTrigger::StartRequested,
        ConnectionTrigger::HandshakeSucceeded,
        ConnectionTrigger::LinkLost,
        ConnectionTrigger::BackoffElapsed,
        ConnectionTrigger::HandshakeSucceeded,
        ConnectionTrigger::StopRequested,
    };
    const std::vector<ConnectionState> expectedStates = {
        ConnectionState::Connecting,
        ConnectionState::Connected,
        ConnectionState::Error,
        ConnectionState::Connecting,
        ConnectionState::Connected,
        ConnectionState::Disconnected,
    };
    std::size_t acceptedCount = 0;
    const std::vector<ConnectionState> reachedStates = applyAll(triggers, acceptedCount);

    EXPECT_EQ(acceptedCount, triggers.size());
    EXPECT_EQ(reachedStates, expectedStates);
    ASSERT_EQ(m_transitions.size(), 6U);
    EXPECT_EQ(m_transitions.at(2).trigger, ConnectionTrigger::LinkLost);
    EXPECT_EQ(m_transitions.at(2).to, ConnectionState::Error);
    EXPECT_EQ(m_transitions.at(1).timestampMilliseconds, 5200);
    EXPECT_EQ(m_transitions.at(5).to, ConnectionState::Disconnected);
    EXPECT_EQ(m_machine.rejectedTransitionCount(), 0U);
}

TEST_F(ConnectionStateMachineTest, EmptyListenerStillRecordsTheTransition) {
    m_machine.setTransitionListener(nullptr);
    ASSERT_EQ(m_machine.handle(ConnectionTrigger::StartRequested), TransitionResult::Accepted);
    EXPECT_TRUE(m_transitions.empty());
    ASSERT_TRUE(m_machine.lastTransition().has_value());
    const ConnectionTransition last = m_machine.lastTransition().value_or(ConnectionTransition{});
    EXPECT_EQ(last.to, ConnectionState::Connecting);
}

TEST_F(ConnectionStateMachineTest, EveryStateAndTriggerHasAName) {
    std::set<std::string> stateNames;
    for (const ConnectionState state : allConnectionStates) {
        stateNames.insert(std::string(toString(state)));
    }
    EXPECT_EQ(stateNames.size(), allConnectionStates.size());
    std::set<std::string> triggerNames;
    for (const ConnectionTrigger trigger : allConnectionTriggers) {
        triggerNames.insert(std::string(toString(trigger)));
    }
    EXPECT_EQ(triggerNames.size(), allConnectionTriggers.size());
    EXPECT_EQ(toString(TransitionResult::Accepted), "Accepted");
    EXPECT_EQ(toString(TransitionResult::Rejected), "Rejected");
}

} // namespace
