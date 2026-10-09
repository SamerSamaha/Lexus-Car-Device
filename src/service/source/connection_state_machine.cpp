#include "lexus_head_unit/service/connection_state_machine.h"

#include "lexus_head_unit/service/clock.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace lexus_head_unit {

namespace {

constexpr std::array<TransitionRule, connectionTransitionRuleCount> transitionTable = {{
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

std::optional<ConnectionState> targetOf(ConnectionState from, ConnectionTrigger trigger) {
    for (const TransitionRule& rule : transitionTable) {
        if (rule.from == from && rule.trigger == trigger) {
            return rule.to;
        }
    }
    return std::nullopt;
}

} // namespace

const std::array<TransitionRule, connectionTransitionRuleCount>& connectionTransitionTable() {
    return transitionTable;
}

ConnectionStateMachine::ConnectionStateMachine(const Clock& clock) : m_clock(&clock) {}

TransitionResult ConnectionStateMachine::handle(ConnectionTrigger trigger) {
    const std::optional<ConnectionState> target = targetOf(m_state, trigger);
    if (!target.has_value()) {
        ++m_rejectedTransitionCount;
        return TransitionResult::Rejected;
    }
    ConnectionTransition transition;
    transition.from = m_state;
    transition.trigger = trigger;
    transition.to = *target;
    transition.timestampMilliseconds = m_clock->nowMilliseconds();
    m_state = *target;
    m_lastTransition = transition;
    ++m_acceptedTransitionCount;
    if (m_listener) {
        m_listener(transition);
    }
    return TransitionResult::Accepted;
}

ConnectionState ConnectionStateMachine::state() const {
    return m_state;
}

std::optional<ConnectionTransition> ConnectionStateMachine::lastTransition() const {
    return m_lastTransition;
}

std::uint64_t ConnectionStateMachine::acceptedTransitionCount() const {
    return m_acceptedTransitionCount;
}

std::uint64_t ConnectionStateMachine::rejectedTransitionCount() const {
    return m_rejectedTransitionCount;
}

void ConnectionStateMachine::setTransitionListener(TransitionListener listener) {
    m_listener = std::move(listener);
}

std::string_view toString(ConnectionState state) {
    switch (state) {
    case ConnectionState::Disconnected:
        return "Disconnected";
    case ConnectionState::Connecting:
        return "Connecting";
    case ConnectionState::Connected:
        return "Connected";
    case ConnectionState::Error:
        return "Error";
    }
    return "UnknownState";
}

std::string_view toString(ConnectionTrigger trigger) {
    switch (trigger) {
    case ConnectionTrigger::StartRequested:
        return "StartRequested";
    case ConnectionTrigger::HandshakeSucceeded:
        return "HandshakeSucceeded";
    case ConnectionTrigger::HandshakeFailed:
        return "HandshakeFailed";
    case ConnectionTrigger::LinkLost:
        return "LinkLost";
    case ConnectionTrigger::BackoffElapsed:
        return "BackoffElapsed";
    case ConnectionTrigger::StopRequested:
        return "StopRequested";
    }
    return "UnknownTrigger";
}

std::string_view toString(TransitionResult result) {
    switch (result) {
    case TransitionResult::Accepted:
        return "Accepted";
    case TransitionResult::Rejected:
        return "Rejected";
    }
    return "UnknownResult";
}

} // namespace lexus_head_unit
