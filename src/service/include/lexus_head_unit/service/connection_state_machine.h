#pragma once

#include "lexus_head_unit/service/clock.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace lexus_head_unit {

enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Error,
};

constexpr std::size_t connectionStateCount = 4;

constexpr std::array<ConnectionState, connectionStateCount> allConnectionStates = {
    ConnectionState::Disconnected,
    ConnectionState::Connecting,
    ConnectionState::Connected,
    ConnectionState::Error,
};

enum class ConnectionTrigger {
    StartRequested,
    HandshakeSucceeded,
    HandshakeFailed,
    LinkLost,
    BackoffElapsed,
    StopRequested,
};

constexpr std::size_t connectionTriggerCount = 6;

constexpr std::array<ConnectionTrigger, connectionTriggerCount> allConnectionTriggers = {
    ConnectionTrigger::StartRequested,
    ConnectionTrigger::HandshakeSucceeded,
    ConnectionTrigger::HandshakeFailed,
    ConnectionTrigger::LinkLost,
    ConnectionTrigger::BackoffElapsed,
    ConnectionTrigger::StopRequested,
};

enum class TransitionResult {
    Accepted,
    Rejected,
};

struct ConnectionTransition {
    ConnectionState from = ConnectionState::Disconnected;
    ConnectionTrigger trigger = ConnectionTrigger::StartRequested;
    ConnectionState to = ConnectionState::Disconnected;
    std::int64_t timestampMilliseconds = 0;
};

struct TransitionRule {
    ConnectionState from;
    ConnectionTrigger trigger;
    ConnectionState to;
};

constexpr std::size_t connectionTransitionRuleCount = 8;

const std::array<TransitionRule, connectionTransitionRuleCount>& connectionTransitionTable();

class ConnectionStateMachine {
public:
    using TransitionListener = std::function<void(const ConnectionTransition&)>;

    explicit ConnectionStateMachine(const Clock& clock);

    TransitionResult handle(ConnectionTrigger trigger);

    [[nodiscard]] ConnectionState state() const;
    [[nodiscard]] std::optional<ConnectionTransition> lastTransition() const;
    [[nodiscard]] std::uint64_t acceptedTransitionCount() const;
    [[nodiscard]] std::uint64_t rejectedTransitionCount() const;

    void setTransitionListener(TransitionListener listener);

private:
    const Clock* m_clock;
    ConnectionState m_state = ConnectionState::Disconnected;
    std::optional<ConnectionTransition> m_lastTransition;
    std::uint64_t m_acceptedTransitionCount = 0;
    std::uint64_t m_rejectedTransitionCount = 0;
    TransitionListener m_listener;
};

std::string_view toString(ConnectionState state);
std::string_view toString(ConnectionTrigger trigger);
std::string_view toString(TransitionResult result);

} // namespace lexus_head_unit
