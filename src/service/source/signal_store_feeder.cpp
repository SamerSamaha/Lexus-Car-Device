#include "lexus_head_unit/service/signal_store_feeder.h"

#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/signal_sample.h"
#include "lexus_head_unit/service/signal_store.h"

#include <cstdint>
#include <optional>
#include <utility>

namespace lexus_head_unit {

SignalStoreFeeder::SignalStoreFeeder(SignalStore& store) : m_store(&store) {}

void SignalStoreFeeder::onSample(const SignalSample& sample) {
    if (m_store->update(sample) == UpdateResult::Accepted) {
        ++m_acceptedSampleCount;
    } else {
        ++m_rejectedSampleCount;
    }
}

void SignalStoreFeeder::onConnectionChanged(const ConnectionTransition& transition) {
    m_latestTransition = transition;
    ++m_transitionCount;
    if (m_transitionHook) {
        m_transitionHook(transition);
    }
}

ConnectionState SignalStoreFeeder::connectionState() const {
    if (m_latestTransition.has_value()) {
        return m_latestTransition->to;
    }
    return ConnectionState::Disconnected;
}

std::optional<ConnectionTransition> SignalStoreFeeder::latestTransition() const {
    return m_latestTransition;
}

std::uint64_t SignalStoreFeeder::transitionCount() const {
    return m_transitionCount;
}

std::uint64_t SignalStoreFeeder::acceptedSampleCount() const {
    return m_acceptedSampleCount;
}

std::uint64_t SignalStoreFeeder::rejectedSampleCount() const {
    return m_rejectedSampleCount;
}

void SignalStoreFeeder::setTransitionHook(TransitionHook hook) {
    m_transitionHook = std::move(hook);
}

} // namespace lexus_head_unit
