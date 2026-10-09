#include "lexus_head_unit/hardware/socket_can_dbc_source.h"

#include "lexus_head_unit/hardware/can_frame.h"
#include "lexus_head_unit/hardware/can_frame_reader.h"
#include "lexus_head_unit/hardware/dbc_database.h"
#include "lexus_head_unit/hardware/dbc_decoder.h"
#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/key_value_configuration.h"
#include "lexus_head_unit/service/signal_definition.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"
#include "lexus_head_unit/service/vehicle_data_source.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace lexus_head_unit {

namespace {

struct NameMapping {
    std::string_view dbcName;
    SignalId signalId;
};

constexpr std::array<NameMapping, gridSignalCount> nameMappings = {{
    {"VehicleSpeed", SignalId::VehicleSpeed},
    {"EngineSpeed", SignalId::EngineRpm},
    {"CoolantTemperature", SignalId::CoolantTemperature},
    {"EngineLoad", SignalId::EngineLoad},
    {"ThrottlePosition", SignalId::ThrottlePosition},
    {"IntakeAirTemperature", SignalId::IntakeAirTemperature},
    {"ControlModuleVoltage", SignalId::ControlModuleVoltage},
    {"FuelLevel", SignalId::FuelLevel},
}};

} // namespace

CanSourceConfiguration
CanSourceConfiguration::fromConfiguration(const KeyValueConfiguration& configuration) {
    CanSourceConfiguration result;
    result.interfaceName = configuration.stringValue("can.interface", result.interfaceName);
    result.dbcPath = configuration.stringValue("can.dbc", result.dbcPath);
    result.readTimeoutMilliseconds =
        configuration.integerValue("can.read_timeout_ms", result.readTimeoutMilliseconds);
    result.linkLossTimeoutMilliseconds =
        configuration.integerValue("can.link_loss_timeout_ms", result.linkLossTimeoutMilliseconds);
    result.backoffCapMilliseconds =
        configuration.integerValue("can.backoff_cap_ms", result.backoffCapMilliseconds);
    return result;
}

std::optional<SignalId> signalIdForDbcName(std::string_view dbcSignalName) {
    for (const NameMapping& mapping : nameMappings) {
        if (mapping.dbcName == dbcSignalName) {
            return mapping.signalId;
        }
    }
    return std::nullopt;
}

SocketCanDbcSource::SocketCanDbcSource(CanFrameReader& reader,
                                       DbcDatabase database,
                                       const Clock& clock,
                                       CanSourceConfiguration configuration)
    : m_reader(&reader),
      m_decoder(std::move(database)),
      m_clock(&clock),
      m_configuration(std::move(configuration)),
      m_machine(clock),
      m_backoff(m_configuration.backoffScheduleMilliseconds,
                m_configuration.backoffCapMilliseconds) {
    m_machine.setTransitionListener([this](const ConnectionTransition& transition) {
        if (m_listener != nullptr) {
            m_listener->onConnectionChanged(transition);
        }
    });
}

std::string_view SocketCanDbcSource::name() const {
    return "can";
}

void SocketCanDbcSource::start(VehicleDataSourceListener& listener) {
    m_listener = &listener;
    if (m_machine.state() != ConnectionState::Disconnected) {
        ++m_counters.rejectedTransitions;
        return;
    }
    raise(ConnectionTrigger::StartRequested);
    attemptConnection();
}

void SocketCanDbcSource::runOnce() {
    if (m_listener == nullptr) {
        return;
    }
    switch (m_machine.state()) {
    case ConnectionState::Connected:
        readFrames();
        break;
    case ConnectionState::Error:
        if (m_clock->nowMilliseconds() >= m_nextAttemptAtMilliseconds) {
            raise(ConnectionTrigger::BackoffElapsed);
            attemptConnection();
        }
        break;
    case ConnectionState::Connecting:
        attemptConnection();
        break;
    case ConnectionState::Disconnected:
        break;
    }
}

void SocketCanDbcSource::stop() {
    if (m_machine.state() != ConnectionState::Disconnected) {
        raise(ConnectionTrigger::StopRequested);
    }
    m_reader->close();
    m_listener = nullptr;
}

ConnectionState SocketCanDbcSource::connectionState() const {
    return m_machine.state();
}

SourceCounters SocketCanDbcSource::counters() const {
    SourceCounters counters = m_counters;
    const DecoderCounters decoder = m_decoder.counters();
    counters.malformedInputs =
        decoder.unknownIdentifier + decoder.wrongLength + decoder.invalidFrame;
    return counters;
}

std::int64_t SocketCanDbcSource::idleHintMilliseconds() const {
    if (m_machine.state() == ConnectionState::Error) {
        const std::int64_t remaining = m_nextAttemptAtMilliseconds - m_clock->nowMilliseconds();
        return remaining > 0 ? remaining : 0;
    }
    // Connected: the read itself waits up to readTimeoutMilliseconds, so the loop need not.
    return 0;
}

const DbcDecoder& SocketCanDbcSource::decoder() const {
    return m_decoder;
}

const std::string& SocketCanDbcSource::lastFailure() const {
    return m_lastFailure;
}

std::uint64_t SocketCanDbcSource::connectionAttempts() const {
    return m_connectionAttempts;
}

void SocketCanDbcSource::attemptConnection() {
    ++m_connectionAttempts;
    if (!m_decoder.database().errors().empty()) {
        m_lastFailure = "DBC error: " + m_decoder.database().errors().front();
    } else if (const std::string mismatch = unitMismatch(); !mismatch.empty()) {
        m_lastFailure = mismatch;
    } else if (!m_reader->open()) {
        m_lastFailure = m_reader->lastError();
    } else {
        m_lastFailure.clear();
        m_backoff.reset();
        m_lastFrameAtMilliseconds = m_clock->nowMilliseconds();
        raise(ConnectionTrigger::HandshakeSucceeded);
        return;
    }
    raise(ConnectionTrigger::HandshakeFailed);
    scheduleRetry();
}

std::string SocketCanDbcSource::unitMismatch() const {
    for (const DbcMessage& message : m_decoder.database().messages()) {
        for (const DbcSignal& signal : message.signalList) {
            const std::optional<SignalId> signalId = signalIdForDbcName(signal.name);
            if (!signalId.has_value()) {
                continue;
            }
            const std::string_view expected = toString(definitionOf(*signalId).unit);
            if (signal.unit != expected) {
                return "DBC signal " + signal.name + " has unit '" + signal.unit + "', expected '" +
                       std::string(expected) + "'";
            }
        }
    }
    return {};
}

void SocketCanDbcSource::readFrames() {
    for (int frameNumber = 0; frameNumber < m_configuration.maximumFramesPerRun; ++frameNumber) {
        // Wait for the first frame only; then take what is already there.
        const CanReadResult result =
            m_reader->read(frameNumber == 0 ? m_configuration.readTimeoutMilliseconds : 0);
        if (result.status == CanReadStatus::Frame) {
            m_lastFrameAtMilliseconds = m_clock->nowMilliseconds();
            handleFrame(result.frame);
            continue;
        }
        if (result.status == CanReadStatus::Closed || result.status == CanReadStatus::Error) {
            m_lastFailure = m_reader->lastError();
            linkLost();
            return;
        }
        break;
    }
    if (m_clock->nowMilliseconds() - m_lastFrameAtMilliseconds >=
        m_configuration.linkLossTimeoutMilliseconds) {
        m_lastFailure =
            "no frame for " + std::to_string(m_configuration.linkLossTimeoutMilliseconds) + " ms";
        linkLost();
    }
}

void SocketCanDbcSource::handleFrame(const CanFrame& frame) {
    const DecodeResult result = m_decoder.decode(frame);
    if (result.kind != DecodeKind::Decoded) {
        return;
    }
    const std::int64_t now = m_clock->nowMilliseconds();
    for (const DecodedSignal& decoded : result.values) {
        const std::optional<SignalId> signalId = signalIdForDbcName(decoded.name);
        if (!signalId.has_value()) {
            continue;
        }
        SignalSample sample;
        sample.signalId = *signalId;
        sample.value = decoded.value;
        sample.unit = definitionOf(*signalId).unit;
        sample.timestampMilliseconds = now;
        sample.status = SignalStatus::Valid;
        ++m_counters.samplesEmitted;
        m_listener->onSample(sample);
    }
}

void SocketCanDbcSource::linkLost() {
    m_reader->close();
    raise(ConnectionTrigger::LinkLost);
    scheduleRetry();
}

void SocketCanDbcSource::scheduleRetry() {
    m_nextAttemptAtMilliseconds = m_clock->nowMilliseconds() + m_backoff.nextDelayMilliseconds();
}

void SocketCanDbcSource::raise(ConnectionTrigger trigger) {
    if (m_machine.handle(trigger) == TransitionResult::Rejected) {
        ++m_counters.rejectedTransitions;
    }
}

} // namespace lexus_head_unit
