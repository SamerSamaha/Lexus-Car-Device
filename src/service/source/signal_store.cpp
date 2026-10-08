#include "lexus_head_unit/service/signal_store.h"

#include "lexus_head_unit/service/signal_definition.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"

#include <cstdint>
#include <string_view>
#include <utility>

namespace lexus_head_unit {

SignalStore::SignalStore() : SignalStore(defaultSignalDefinitions()) {}

SignalStore::SignalStore(const SignalDefinitionTable& definitions) {
    for (const SignalDefinition& definition : definitions) {
        if (!isKnownSignal(definition.id)) {
            continue;
        }
        SignalSample& stored = m_latestSamples.at(indexOf(definition.id));
        stored.signalId = definition.id;
        stored.value = 0.0;
        stored.unit = definition.unit;
        stored.timestampMilliseconds = 0;
        stored.status = SignalStatus::NeverReceived;
        m_stalenessTimeoutsMilliseconds.at(indexOf(definition.id)) =
            definition.stalenessTimeoutMilliseconds;
    }
}

UpdateResult SignalStore::update(const SignalSample& sample) {
    if (!isKnownSignal(sample.signalId)) {
        ++m_rejectedUnknownSignalCount;
        return UpdateResult::RejectedUnknownSignal;
    }
    SignalSample& stored = m_latestSamples.at(indexOf(sample.signalId));
    if (sample.unit != stored.unit) {
        ++m_rejectedUnitMismatchCount;
        return UpdateResult::RejectedUnitMismatch;
    }
    const bool isFirstSample = stored.status == SignalStatus::NeverReceived;
    if (!isFirstSample && sample.timestampMilliseconds <= stored.timestampMilliseconds) {
        ++m_rejectedOutOfOrderCount;
        return UpdateResult::RejectedOutOfOrder;
    }
    stored.value = sample.value;
    stored.timestampMilliseconds = sample.timestampMilliseconds;
    stored.status = SignalStatus::Valid;
    notify(stored);
    return UpdateResult::Accepted;
}

bool SignalStore::markStale(SignalId signalId) {
    if (!isKnownSignal(signalId)) {
        return false;
    }
    SignalSample& stored = m_latestSamples.at(indexOf(signalId));
    if (stored.status != SignalStatus::Valid) {
        return false;
    }
    stored.status = SignalStatus::Stale;
    notify(stored);
    return true;
}

const SignalSample& SignalStore::latest(SignalId signalId) const {
    return m_latestSamples.at(indexOf(signalId));
}

SignalStore::Snapshot SignalStore::snapshot() const {
    return m_latestSamples;
}

bool SignalStore::setStalenessTimeout(SignalId signalId, std::int64_t timeoutMilliseconds) {
    if (!isKnownSignal(signalId) || timeoutMilliseconds <= 0) {
        return false;
    }
    m_stalenessTimeoutsMilliseconds.at(indexOf(signalId)) = timeoutMilliseconds;
    return true;
}

std::int64_t SignalStore::stalenessTimeoutMilliseconds(SignalId signalId) const {
    return m_stalenessTimeoutsMilliseconds.at(indexOf(signalId));
}

void SignalStore::setChangeListener(ChangeListener listener) {
    m_changeListener = std::move(listener);
}

std::uint64_t SignalStore::rejectedOutOfOrderCount() const {
    return m_rejectedOutOfOrderCount;
}

std::uint64_t SignalStore::rejectedUnitMismatchCount() const {
    return m_rejectedUnitMismatchCount;
}

std::uint64_t SignalStore::rejectedUnknownSignalCount() const {
    return m_rejectedUnknownSignalCount;
}

void SignalStore::notify(const SignalSample& sample) const {
    if (m_changeListener) {
        m_changeListener(sample);
    }
}

std::string_view toString(UpdateResult result) {
    switch (result) {
    case UpdateResult::Accepted:
        return "Accepted";
    case UpdateResult::RejectedOutOfOrder:
        return "RejectedOutOfOrder";
    case UpdateResult::RejectedUnitMismatch:
        return "RejectedUnitMismatch";
    case UpdateResult::RejectedUnknownSignal:
        return "RejectedUnknownSignal";
    }
    return "UnknownResult";
}

} // namespace lexus_head_unit
