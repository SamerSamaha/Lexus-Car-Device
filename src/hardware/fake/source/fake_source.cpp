#include "lexus_head_unit/hardware/fake_source.h"

#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/diagnostics_report.h"
#include "lexus_head_unit/service/signal_definition.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"
#include "lexus_head_unit/service/vehicle_data_source.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace lexus_head_unit {

FakeSource::FakeSource(const Clock& clock) : m_clock(&clock), m_machine(clock) {
    m_machine.setTransitionListener([this](const ConnectionTransition& transition) {
        if (m_listener != nullptr) {
            m_listener->onConnectionChanged(transition);
        }
    });
}

void FakeSource::scriptSample(SignalId signalId, double value) {
    ScriptStep step;
    step.kind = StepKind::Sample;
    step.signalId = signalId;
    step.value = value;
    m_script.push_back(step);
}

void FakeSource::scriptMalformedInput() {
    ScriptStep step;
    step.kind = StepKind::MalformedInput;
    m_script.push_back(step);
}

void FakeSource::scriptLinkLoss() {
    ScriptStep step;
    step.kind = StepKind::LinkLoss;
    m_script.push_back(step);
}

void FakeSource::scriptReconnect() {
    ScriptStep step;
    step.kind = StepKind::Reconnect;
    m_script.push_back(step);
}

void FakeSource::scriptHandshakeFailure() {
    m_failNextHandshake = true;
}

std::size_t FakeSource::pendingSteps() const {
    return m_script.size();
}

std::string_view FakeSource::name() const {
    return "fake";
}

void FakeSource::start(VehicleDataSourceListener& listener) {
    m_listener = &listener;
    if (m_machine.handle(ConnectionTrigger::StartRequested) == TransitionResult::Rejected) {
        ++m_counters.rejectedTransitions;
        return;
    }
    handshake();
}

void FakeSource::runOnce() {
    if (m_listener != nullptr && m_machine.state() == ConnectionState::Connected &&
        m_diagnosticsRequested.exchange(false)) {
        ++m_diagnosticsRequests;
        m_listener->onDiagnostics(m_scriptedDiagnostics);
    }
    if (m_listener == nullptr || m_script.empty()) {
        return;
    }
    const ScriptStep step = m_script.front();
    m_script.pop_front();
    switch (step.kind) {
    case StepKind::Sample:
        emitSample(step);
        break;
    case StepKind::MalformedInput:
        ++m_counters.malformedInputs;
        break;
    case StepKind::LinkLoss:
        if (m_machine.handle(ConnectionTrigger::LinkLost) == TransitionResult::Rejected) {
            ++m_counters.rejectedTransitions;
        }
        break;
    case StepKind::Reconnect:
        if (m_machine.handle(ConnectionTrigger::BackoffElapsed) == TransitionResult::Rejected) {
            ++m_counters.rejectedTransitions;
            break;
        }
        handshake();
        break;
    }
}

void FakeSource::scriptDiagnostics(const DiagnosticsReport& report) {
    m_scriptedDiagnostics = report;
}

std::uint64_t FakeSource::diagnosticsRequests() const {
    return m_diagnosticsRequests;
}

void FakeSource::requestDiagnostics() {
    m_diagnosticsRequested.store(true);
}

void FakeSource::stop() {
    if (m_machine.state() != ConnectionState::Disconnected) {
        if (m_machine.handle(ConnectionTrigger::StopRequested) == TransitionResult::Rejected) {
            ++m_counters.rejectedTransitions;
        }
    }
    m_listener = nullptr;
}

ConnectionState FakeSource::connectionState() const {
    return m_machine.state();
}

SourceCounters FakeSource::counters() const {
    return m_counters;
}

void FakeSource::emitSample(const ScriptStep& step) {
    SignalSample sample;
    sample.signalId = step.signalId;
    sample.value = step.value;
    sample.unit =
        isKnownSignal(step.signalId) ? definitionOf(step.signalId).unit : Unit::KilometresPerHour;
    sample.timestampMilliseconds = m_clock->nowMilliseconds();
    sample.status = SignalStatus::NeverReceived;
    ++m_counters.samplesEmitted;
    m_listener->onSample(sample);
}

void FakeSource::handshake() {
    const ConnectionTrigger outcome = m_failNextHandshake ? ConnectionTrigger::HandshakeFailed
                                                          : ConnectionTrigger::HandshakeSucceeded;
    m_failNextHandshake = false;
    if (m_machine.handle(outcome) == TransitionResult::Rejected) {
        ++m_counters.rejectedTransitions;
    }
}

} // namespace lexus_head_unit
