#include "lexus_head_unit/service_dbus/vehicle_data_client.h"

#include "lexus_head_unit/service/clock.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/link_detail.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"
#include "lexus_head_unit/service/signal_store.h"
#include "lexus_head_unit/service_dbus/dbus_names.h"
#include "lexus_head_unit/service_dbus/dbus_types.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QList>
#include <QObject>
#include <QString>

#include <optional>
#include <utility>

// Qt declares its macros and types in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace lexus_head_unit {

namespace {

QDBusMessage methodCall(const QString& method) {
    return QDBusMessage::createMethodCall(
        dbus_names::serviceName(), dbus_names::objectPath(), dbus_names::interfaceName(), method);
}

} // namespace

VehicleDataClient::VehicleDataClient(QDBusConnection connection, QObject* parent)
    : QObject(parent),
      m_connection(std::move(connection)),
      m_watcher(dbus_names::serviceName(),
                m_connection,
                QDBusServiceWatcher::WatchForRegistration |
                    QDBusServiceWatcher::WatchForUnregistration) {
    registerDBusTypes();
    const SignalStore emptyStore;
    m_samples = emptyStore.snapshot();
    connect(&m_watcher,
            &QDBusServiceWatcher::serviceRegistered,
            this,
            &VehicleDataClient::onServiceRegistered);
    connect(&m_watcher,
            &QDBusServiceWatcher::serviceUnregistered,
            this,
            &VehicleDataClient::onServiceUnregistered);
}

void VehicleDataClient::start() {
    // Subscribe before fetching, so that nothing published in between is missed (DN-022, 4).
    m_connection.connect(dbus_names::serviceName(),
                         dbus_names::objectPath(),
                         dbus_names::interfaceName(),
                         QStringLiteral("SampleChanged"),
                         this,
                         SLOT(onSampleChanged(uint, double, uint, qlonglong, uint)));
    m_connection.connect(dbus_names::serviceName(),
                         dbus_names::objectPath(),
                         dbus_names::interfaceName(),
                         QStringLiteral("ConnectionChanged"),
                         this,
                         SLOT(onConnectionChanged(uint, uint, uint, qlonglong)));
    m_connection.connect(dbus_names::serviceName(),
                         dbus_names::objectPath(),
                         dbus_names::interfaceName(),
                         QStringLiteral("DiagnosticsChanged"),
                         this,
                         SLOT(onDiagnosticsChanged()));
    m_connection.connect(dbus_names::serviceName(),
                         dbus_names::objectPath(),
                         dbus_names::interfaceName(),
                         QStringLiteral("LinkDetailChanged"),
                         this,
                         SLOT(onLinkDetailChanged(uint)));
    fetchState();
    fetchDiagnostics();
}

void VehicleDataClient::requestDiagnostics() {
    m_connection.asyncCall(methodCall(QStringLiteral("RequestDiagnostics")));
}

DiagnosticsReport VehicleDataClient::latestDiagnostics() const {
    return m_diagnostics;
}

void VehicleDataClient::onDiagnosticsChanged() {
    fetchDiagnostics();
}

void VehicleDataClient::fetchDiagnostics() {
    auto* watcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(methodCall(QStringLiteral("GetDiagnostics"))), this);
    connect(
        watcher, &QDBusPendingCallWatcher::finished, this, &VehicleDataClient::onDiagnosticsReply);
}

void VehicleDataClient::onDiagnosticsReply(QDBusPendingCallWatcher* watcher) {
    const QDBusPendingReply<DBusDiagnostics> reply = *watcher;
    watcher->deleteLater();
    if (reply.isError()) {
        return;
    }
    const DiagnosticsReport report = fromDBus(reply.value());
    if (!report.codesRead && !report.identificationRead) {
        return;
    }
    m_diagnostics = report;
    emit diagnosticsArrived(report);
}

void VehicleDataClient::fetchState() {
    m_samplesFetched = false;
    m_connectionFetched = false;
    m_linkDetailFetched = false;
    auto* samplesWatcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(methodCall(QStringLiteral("GetSamples"))), this);
    connect(samplesWatcher,
            &QDBusPendingCallWatcher::finished,
            this,
            &VehicleDataClient::onSamplesReply);
    auto* connectionWatcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(methodCall(QStringLiteral("GetConnection"))), this);
    connect(connectionWatcher,
            &QDBusPendingCallWatcher::finished,
            this,
            &VehicleDataClient::onConnectionReply);
    auto* detailWatcher = new QDBusPendingCallWatcher(
        m_connection.asyncCall(methodCall(QStringLiteral("GetLinkDetail"))), this);
    connect(detailWatcher,
            &QDBusPendingCallWatcher::finished,
            this,
            &VehicleDataClient::onLinkDetailReply);
}

void VehicleDataClient::onLinkDetailReply(QDBusPendingCallWatcher* watcher) {
    const QDBusPendingReply<uint> reply = *watcher;
    watcher->deleteLater();
    if (reply.isError()) {
        return;
    }
    onLinkDetailChanged(reply.value());
    m_linkDetailFetched = true;
    noteFetched();
}

void VehicleDataClient::noteFetched() {
    if (hasInitialState()) {
        emit initialStateReceived();
    }
}

void VehicleDataClient::onLinkDetailChanged(uint detail) {
    const std::optional<LinkDetail> known = linkDetailFromNumber(detail);
    if (!known.has_value()) {
        ++m_malformedMessages;
        return;
    }
    applyLinkDetail(*known);
}

void VehicleDataClient::applyLinkDetail(LinkDetail detail) {
    m_linkDetail = detail;
    emit linkDetailChanged(detail);
}

LinkDetail VehicleDataClient::linkDetail() const {
    return m_linkDetail;
}

void VehicleDataClient::onSamplesReply(QDBusPendingCallWatcher* watcher) {
    const QDBusPendingReply<QList<DBusSample>> reply = *watcher;
    watcher->deleteLater();
    if (reply.isError()) {
        return;
    }
    setServiceAvailable(true);
    for (const DBusSample& wire : reply.value()) {
        const std::optional<SignalSample> sample = fromDBus(wire);
        if (!sample.has_value()) {
            ++m_malformedMessages;
            continue;
        }
        applySample(*sample);
    }
    m_samplesFetched = true;
    noteFetched();
}

void VehicleDataClient::onConnectionReply(QDBusPendingCallWatcher* watcher) {
    const QDBusPendingReply<DBusConnectionState> reply = *watcher;
    watcher->deleteLater();
    if (reply.isError()) {
        return;
    }
    setServiceAvailable(true);
    const DBusConnectionState wire = reply.value();
    const std::optional<ConnectionState> state = connectionStateFromDBus(wire.state);
    if (!state.has_value()) {
        ++m_malformedMessages;
        return;
    }
    if (wire.hasTransition) {
        const std::optional<ConnectionTransition> transition =
            transitionFromDBus(wire.from, wire.trigger, wire.to, wire.timestampMilliseconds);
        if (transition.has_value() && transition->to == *state) {
            applyTransition(*transition);
        } else {
            ++m_malformedMessages;
        }
    }
    m_state = *state;
    m_connectionFetched = true;
    noteFetched();
}

void VehicleDataClient::onSampleChanged(
    uint signalId, double value, uint unit, qlonglong timestampMilliseconds, uint status) {
    const std::optional<SignalSample> sample =
        fromDBus(DBusSample{signalId, value, unit, timestampMilliseconds, status});
    if (!sample.has_value()) {
        ++m_malformedMessages;
        return;
    }
    ++m_samplesReceived;
    applySample(*sample);
}

void VehicleDataClient::onConnectionChanged(uint fromState,
                                            uint trigger,
                                            uint toState,
                                            qlonglong timestampMilliseconds) {
    const std::optional<ConnectionTransition> transition =
        transitionFromDBus(fromState, trigger, toState, timestampMilliseconds);
    if (!transition.has_value()) {
        ++m_malformedMessages;
        return;
    }
    applyTransition(*transition);
}

void VehicleDataClient::onServiceRegistered() {
    setServiceAvailable(true);
    fetchState();
}

void VehicleDataClient::onServiceUnregistered() {
    setServiceAvailable(false);
    for (const SignalSample& held : m_samples) {
        if (held.status == SignalStatus::Valid) {
            SignalSample stale = held;
            stale.status = SignalStatus::Stale;
            applySample(stale);
        }
    }
    if (m_state != ConnectionState::Error && m_state != ConnectionState::Disconnected) {
        ConnectionTransition lost;
        lost.from = m_state;
        lost.trigger = ConnectionTrigger::LinkLost;
        lost.to = ConnectionState::Error;
        lost.timestampMilliseconds = SteadyClock().nowMilliseconds();
        applyTransition(lost);
    }
    if (m_linkDetail != LinkDetail::Idle) {
        applyLinkDetail(LinkDetail::LinkLostRetrying);
    }
}

void VehicleDataClient::setServiceAvailable(bool available) {
    if (available == m_serviceAvailable) {
        return;
    }
    m_serviceAvailable = available;
    emit serviceAvailabilityChanged(available);
}

void VehicleDataClient::applySample(const SignalSample& sample) {
    m_samples.at(indexOf(sample.signalId)) = sample;
    emit sampleArrived(sample);
}

void VehicleDataClient::applyTransition(const ConnectionTransition& transition) {
    m_state = transition.to;
    emit connectionChanged(transition);
}

SignalSample VehicleDataClient::latest(SignalId signalId) const {
    return m_samples.at(indexOf(signalId));
}

ConnectionState VehicleDataClient::connectionState() const {
    return m_state;
}

bool VehicleDataClient::hasInitialState() const {
    return m_samplesFetched && m_connectionFetched && m_linkDetailFetched;
}

bool VehicleDataClient::isServiceAvailable() const {
    return m_serviceAvailable;
}

quint64 VehicleDataClient::samplesReceived() const {
    return m_samplesReceived;
}

quint64 VehicleDataClient::malformedMessages() const {
    return m_malformedMessages;
}

} // namespace lexus_head_unit
// NOLINTEND(misc-include-cleaner)
