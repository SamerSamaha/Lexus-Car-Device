#include "lexus_head_unit/service_dbus/vehicle_data_service.h"

#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/signal_sample.h"
#include "lexus_head_unit/service/signal_store.h"
#include "lexus_head_unit/service_dbus/dbus_names.h"
#include "lexus_head_unit/service_dbus/dbus_types.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QList>
#include <QMetaType>
#include <QObject>
#include <QString>

#include <functional>
#include <utility>

// Qt declares its macros and types in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace lexus_head_unit {

VehicleDataService::VehicleDataService(QObject* parent) : QObject(parent) {
    registerDBusTypes();
    qRegisterMetaType<SignalSample>("lexus_head_unit::SignalSample");
    qRegisterMetaType<ConnectionTransition>("lexus_head_unit::ConnectionTransition");
    qRegisterMetaType<DiagnosticsReport>("lexus_head_unit::DiagnosticsReport");
    const SignalStore emptyStore;
    m_samples = emptyStore.snapshot();
    connect(&m_inbox,
            &VehicleDataServiceInbox::sampleQueued,
            this,
            &VehicleDataService::applySample,
            Qt::QueuedConnection);
    connect(&m_inbox,
            &VehicleDataServiceInbox::transitionQueued,
            this,
            &VehicleDataService::applyTransition,
            Qt::QueuedConnection);
    connect(&m_inbox,
            &VehicleDataServiceInbox::diagnosticsQueued,
            this,
            &VehicleDataService::applyDiagnostics,
            Qt::QueuedConnection);
}

void VehicleDataService::publishSample(const SignalSample& sample) {
    emit m_inbox.sampleQueued(sample);
}

void VehicleDataService::publishTransition(const ConnectionTransition& transition) {
    emit m_inbox.transitionQueued(transition);
}

void VehicleDataService::applySample(const SignalSample& sample) {
    if (!isKnownSignal(sample.signalId)) {
        return;
    }
    m_samples.at(indexOf(sample.signalId)) = sample;
    ++m_publishedSampleCount;
    const DBusSample wire = toDBus(sample);
    emit SampleChanged(
        wire.signalId, wire.value, wire.unit, wire.timestampMilliseconds, wire.status);
}

void VehicleDataService::applyTransition(const ConnectionTransition& transition) {
    m_state = transition.to;
    m_lastTransition = transition;
    const DBusConnectionState wire = toDBus(m_state, m_lastTransition);
    emit ConnectionChanged(wire.from, wire.trigger, wire.to, wire.timestampMilliseconds);
}

bool VehicleDataService::registerOn(QDBusConnection connection, QString& error) {
    if (!connection.isConnected()) {
        error = QStringLiteral("not connected to the bus: ") + connection.lastError().message();
        return false;
    }
    if (!connection.registerObject(dbus_names::objectPath(),
                                   this,
                                   QDBusConnection::ExportScriptableSlots |
                                       QDBusConnection::ExportScriptableSignals)) {
        error = QStringLiteral("could not export the object: ") + connection.lastError().message();
        return false;
    }
    if (!connection.registerService(dbus_names::serviceName())) {
        connection.unregisterObject(dbus_names::objectPath());
        error = QStringLiteral("could not own the name ") + dbus_names::serviceName() +
                QStringLiteral(": ") + connection.lastError().message();
        return false;
    }
    return true;
}

quint64 VehicleDataService::publishedSampleCount() const {
    return m_publishedSampleCount;
}

QList<DBusSample> VehicleDataService::GetSamples() const {
    QList<DBusSample> samples;
    samples.reserve(static_cast<qsizetype>(m_samples.size()));
    for (const SignalSample& sample : m_samples) {
        samples.append(toDBus(sample));
    }
    return samples;
}

void VehicleDataService::publishDiagnostics(const DiagnosticsReport& report) {
    emit m_inbox.diagnosticsQueued(report);
}

void VehicleDataService::setDiagnosticsRequester(std::function<void()> requester) {
    m_diagnosticsRequester = std::move(requester);
}

void VehicleDataService::applyDiagnostics(const DiagnosticsReport& report) {
    m_diagnostics = report;
    emit DiagnosticsChanged();
}

void VehicleDataService::RequestDiagnostics() {
    if (m_diagnosticsRequester) {
        m_diagnosticsRequester();
    }
}

DBusDiagnostics VehicleDataService::GetDiagnostics() const {
    return toDBus(m_diagnostics);
}

DBusConnectionState VehicleDataService::GetConnection() const {
    return toDBus(m_state, m_lastTransition);
}

// A member, not static: Qt D-Bus exports only member slots.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
uint VehicleDataService::GetInterfaceVersion() const {
    return dbus_names::interfaceVersion;
}

} // namespace lexus_head_unit
// NOLINTEND(misc-include-cleaner)
