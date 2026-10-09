#include "lexus_head_unit/hardware/elm327_obd_source.h"

#include "lexus_head_unit/hardware/byte_transport.h"
#include "lexus_head_unit/hardware/command_allowlist.h"
#include "lexus_head_unit/hardware/elm327_protocol.h"
#include "lexus_head_unit/hardware/elm327_source_configuration.h"
#include "lexus_head_unit/hardware/obd_diagnostics.h"
#include "lexus_head_unit/hardware/obd_pid_decoder.h"
#include "lexus_head_unit/hardware/supported_pid_set.h"
#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/diagnostics_report.h"
#include "lexus_head_unit/service/link_detail.h"
#include "lexus_head_unit/service/reconnect_backoff.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"
#include "lexus_head_unit/service/vehicle_data_source.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lexus_head_unit {

namespace {

constexpr std::string_view resetCommand = "ATZ";
constexpr std::string_view identityCommand = "ATI";
constexpr std::string_view voltageCommand = "ATRV";
constexpr std::string_view protocolNumberCommand = "ATDPN";
constexpr std::int64_t idleWhenNothingToPollMilliseconds = 1000;
constexpr std::uint8_t firstBitmapPid = 0x00;
constexpr std::uint8_t vehicleIdentificationPid = 0x02;

bool isLinkFailure(Elm327ReplyKind kind) {
    return kind == Elm327ReplyKind::Timeout || kind == Elm327ReplyKind::LinkError;
}

std::string firstLineOr(const Elm327Reply& reply, const std::string& fallback) {
    return reply.lines.empty() ? fallback : reply.lines.front();
}

} // namespace

Elm327ObdSource::Elm327ObdSource(ByteTransport& transport,
                                 const Clock& clock,
                                 Elm327SourceConfiguration configuration)
    : m_transport(&transport),
      m_clock(&clock),
      m_configuration(std::move(configuration)),
      m_protocol(transport, clock, m_configuration.replyTimeoutMilliseconds),
      m_machine(clock),
      m_backoff(m_configuration.backoffScheduleMilliseconds,
                m_configuration.backoffCapMilliseconds) {
    m_machine.setTransitionListener([this](const ConnectionTransition& transition) {
        if (m_listener != nullptr) {
            m_listener->onConnectionChanged(transition);
        }
    });
}

std::string_view Elm327ObdSource::name() const {
    return "elm327";
}

void Elm327ObdSource::start(VehicleDataSourceListener& listener) {
    m_listener = &listener;
    if (m_machine.state() != ConnectionState::Disconnected) {
        ++m_counters.rejectedTransitions;
        return;
    }
    raise(ConnectionTrigger::StartRequested);
    attemptConnection();
}

void Elm327ObdSource::runOnce() {
    if (m_listener == nullptr) {
        return;
    }
    switch (m_machine.state()) {
    case ConnectionState::Connected:
        if (m_diagnosticsRequested.exchange(false)) {
            readDiagnostics();
        } else {
            pollNextPid();
        }
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

void Elm327ObdSource::stop() {
    if (m_machine.state() != ConnectionState::Disconnected) {
        raise(ConnectionTrigger::StopRequested);
    }
    m_transport->close();
    m_listener = nullptr;
    m_linkDetail = LinkDetail::Idle;
}

ConnectionState Elm327ObdSource::connectionState() const {
    return m_machine.state();
}

SourceCounters Elm327ObdSource::counters() const {
    SourceCounters counters = m_counters;
    counters.requestsSent = m_protocol.commandsSent();
    return counters;
}

std::int64_t Elm327ObdSource::idleHintMilliseconds() const {
    switch (m_machine.state()) {
    case ConnectionState::Error: {
        const std::int64_t remaining = m_nextAttemptAtMilliseconds - m_clock->nowMilliseconds();
        return remaining > 0 ? remaining : 0;
    }
    case ConnectionState::Connected:
        if (m_pollList.empty()) {
            return idleWhenNothingToPollMilliseconds;
        }
        return m_configuration.pollIntervalMilliseconds;
    case ConnectionState::Connecting:
    case ConnectionState::Disconnected:
        return 0;
    }
    return 0;
}

const SupportedPidSet& Elm327ObdSource::supportedPids() const {
    return m_supportedPids;
}

const std::vector<ObdPid>& Elm327ObdSource::pollList() const {
    return m_pollList;
}

const std::string& Elm327ObdSource::adapterIdentity() const {
    return m_adapterIdentity;
}

const std::string& Elm327ObdSource::adapterVoltageText() const {
    return m_adapterVoltageText;
}

const std::string& Elm327ObdSource::protocolNumberText() const {
    return m_protocolNumberText;
}

std::uint64_t Elm327ObdSource::connectionAttempts() const {
    return m_connectionAttempts;
}

std::int64_t Elm327ObdSource::nextAttemptAtMilliseconds() const {
    return m_nextAttemptAtMilliseconds;
}

const Elm327Protocol& Elm327ObdSource::protocol() const {
    return m_protocol;
}

const Elm327SourceConfiguration& Elm327ObdSource::configuration() const {
    return m_configuration;
}

void Elm327ObdSource::attemptConnection() {
    ++m_connectionAttempts;
    // An adapter that cannot be opened or does not answer its setup commands is missing; one
    // that answers them but gets no supported-PID bitmap is in a car that is switched off.
    const bool adapterAnswered = m_transport->open() && runSetupCommands();
    if (!adapterAnswered || !discoverSupportedPids()) {
        m_transport->close();
        m_linkDetail = adapterAnswered ? LinkDetail::AdapterWithoutVehicle : adapterMissingDetail();
        raise(ConnectionTrigger::HandshakeFailed);
        scheduleRetry();
        return;
    }
    m_pollList = pollablePids(m_supportedPids);
    m_pollIndex = 0;
    m_backoff.reset();
    noteAdapterReply();
    m_lastDataReplyAtMilliseconds = m_clock->nowMilliseconds();
    m_linkDetail = LinkDetail::Live;
    m_hasBeenLive = true;
    raise(ConnectionTrigger::HandshakeSucceeded);
}

bool Elm327ObdSource::runSetupCommands() {
    // Not std::all_of: each iteration records the adapter's replies as a side effect.
    // NOLINTNEXTLINE(readability-use-anyofallof)
    for (const std::string_view command : elm327SetupCommands) {
        const Elm327Reply reply = m_protocol.execute(command);
        if (isLinkFailure(reply.kind) || reply.kind == Elm327ReplyKind::Refused) {
            return false;
        }
        noteAdapterReply();
        if (command == resetCommand) {
            m_adapterIdentity = firstLineOr(reply, std::string());
        } else if (command == identityCommand) {
            m_adapterIdentity = firstLineOr(reply, m_adapterIdentity);
        } else if (command == voltageCommand) {
            m_adapterVoltageText = firstLineOr(reply, std::string());
        } else if (command == protocolNumberCommand) {
            m_protocolNumberText = firstLineOr(reply, std::string());
        } else if (reply.kind != Elm327ReplyKind::Ok) {
            return false;
        }
    }
    return true;
}

bool Elm327ObdSource::discoverSupportedPids() {
    m_supportedPids = SupportedPidSet();
    std::optional<std::uint8_t> basePid = firstBitmapPid;
    bool anyBitmap = false;
    while (basePid.has_value()) {
        const std::optional<std::string> request =
            CommandAllowlist::obdRequest(obdModeCurrentData, basePid);
        if (!request.has_value()) {
            break;
        }
        const Elm327Reply reply =
            m_protocol.execute(*request, m_configuration.discoveryTimeoutMilliseconds);
        if (isLinkFailure(reply.kind)) {
            return false;
        }
        noteAdapterReply();
        const std::optional<std::vector<std::uint8_t>> bitmap = mode01DataBytes(reply, *basePid);
        if (!bitmap.has_value() || !m_supportedPids.addBitmap(*basePid, *bitmap)) {
            break;
        }
        anyBitmap = true;
        basePid = m_supportedPids.nextBitmapPid(*basePid);
    }
    return anyBitmap;
}

void Elm327ObdSource::requestDiagnostics() {
    m_diagnosticsRequested.store(true);
}

std::optional<Elm327Reply> Elm327ObdSource::diagnosticsRequest(std::uint8_t mode,
                                                               std::optional<std::uint8_t> pid) {
    const std::optional<std::string> request = CommandAllowlist::obdRequest(mode, pid);
    if (!request.has_value()) {
        return Elm327Reply{};
    }
    m_lastRequestAtMilliseconds = m_clock->nowMilliseconds();
    Elm327Reply reply = m_protocol.execute(*request);
    if (reply.kind == Elm327ReplyKind::LinkError) {
        linkLost();
        return std::nullopt;
    }
    if (reply.kind != Elm327ReplyKind::Timeout) {
        noteAdapterReply();
    }
    return reply;
}

void Elm327ObdSource::readDiagnostics() {
    DiagnosticsReport report;
    const std::optional<Elm327Reply> codesReply =
        diagnosticsRequest(obdModeStoredTroubleCodes, std::nullopt);
    if (!codesReply.has_value()) {
        return;
    }
    const std::optional<std::vector<std::string>> codes = decodeTroubleCodes(*codesReply);
    report.codesRead = codes.has_value();
    for (const std::string& code : codes.value_or(std::vector<std::string>{})) {
        report.troubleCodes.push_back(TroubleCode{code, troubleCodeDescription(code)});
    }
    const std::optional<Elm327Reply> identificationReply =
        diagnosticsRequest(obdModeVehicleInformation, vehicleIdentificationPid);
    if (!identificationReply.has_value()) {
        return;
    }
    const std::optional<std::string> identification =
        decodeVehicleIdentification(*identificationReply);
    report.identificationRead = identification.has_value();
    report.vehicleIdentification = identification.value_or(std::string());
    report.timestampMilliseconds = m_clock->nowMilliseconds();
    if (m_listener != nullptr) {
        m_listener->onDiagnostics(report);
    }
}

void Elm327ObdSource::pollNextPid() {
    if (m_pollList.empty()) {
        return;
    }
    const ObdPid pid = m_pollList.at(m_pollIndex % m_pollList.size());
    m_pollIndex = (m_pollIndex + 1) % m_pollList.size();
    const std::optional<std::string> request =
        CommandAllowlist::obdRequest(obdModeCurrentData, pidByte(pid));
    if (!request.has_value()) {
        return;
    }
    m_lastRequestAtMilliseconds = m_clock->nowMilliseconds();
    const Elm327Reply reply = m_protocol.execute(*request);
    if (reply.kind == Elm327ReplyKind::LinkError) {
        linkLost();
        return;
    }
    if (reply.kind == Elm327ReplyKind::Timeout) {
        const std::int64_t silentFor =
            m_clock->nowMilliseconds() - m_lastAdapterReplyAtMilliseconds;
        if (silentFor >= m_configuration.linkLossTimeoutMilliseconds) {
            linkLost();
        }
        return;
    }
    noteAdapterReply();
    if (reply.kind == Elm327ReplyKind::Data) {
        m_lastDataReplyAtMilliseconds = m_clock->nowMilliseconds();
        handleDataReply(reply, pid);
        return;
    }
    ++m_counters.malformedInputs;
    const std::int64_t quietFor = m_clock->nowMilliseconds() - m_lastDataReplyAtMilliseconds;
    if (quietFor >= m_configuration.vehicleSilenceTimeoutMilliseconds) {
        vehicleWentQuiet();
    }
}

void Elm327ObdSource::handleDataReply(const Elm327Reply& reply, ObdPid pid) {
    const std::optional<std::vector<std::uint8_t>> dataBytes = mode01DataBytes(reply, pidByte(pid));
    if (!dataBytes.has_value()) {
        ++m_counters.malformedInputs;
        return;
    }
    const std::optional<DecodedPid> decoded = decodePid(pidByte(pid), *dataBytes);
    if (!decoded.has_value()) {
        ++m_counters.malformedInputs;
        return;
    }
    SignalSample sample;
    sample.signalId = decoded->signalId;
    sample.value = decoded->value;
    sample.unit = decoded->unit;
    sample.timestampMilliseconds = m_clock->nowMilliseconds();
    sample.status = SignalStatus::NeverReceived;
    ++m_counters.samplesEmitted;
    m_listener->onSample(sample);
}

void Elm327ObdSource::noteAdapterReply() {
    m_lastAdapterReplyAtMilliseconds = m_clock->nowMilliseconds();
}

void Elm327ObdSource::linkLost() {
    m_transport->close();
    m_linkDetail = LinkDetail::LinkLostRetrying;
    raise(ConnectionTrigger::LinkLost);
    scheduleRetry();
}

// The adapter answers but the vehicle does not (DN-042): without this the source would stay
// Connected with every signal Stale for as long as the ignition is off.
void Elm327ObdSource::vehicleWentQuiet() {
    m_transport->close();
    m_linkDetail = LinkDetail::AdapterWithoutVehicle;
    raise(ConnectionTrigger::LinkLost);
    scheduleRetry();
}

LinkDetail Elm327ObdSource::adapterMissingDetail() const {
    return m_hasBeenLive ? LinkDetail::LinkLostRetrying : LinkDetail::SearchingForAdapter;
}

LinkDetail Elm327ObdSource::linkDetail() const {
    return m_linkDetail;
}

void Elm327ObdSource::scheduleRetry() {
    m_nextAttemptAtMilliseconds = m_clock->nowMilliseconds() + m_backoff.nextDelayMilliseconds();
}

void Elm327ObdSource::raise(ConnectionTrigger trigger) {
    if (m_machine.handle(trigger) == TransitionResult::Rejected) {
        ++m_counters.rejectedTransitions;
    }
}

} // namespace lexus_head_unit
