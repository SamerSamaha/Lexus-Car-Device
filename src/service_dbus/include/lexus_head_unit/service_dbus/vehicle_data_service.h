#pragma once

#include "lexus_head_unit/qt/value_types.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"
#include "lexus_head_unit/service_dbus/dbus_types.h"

#include <QDBusConnection>
#include <QList>
#include <QObject>
#include <QString>
#include <QtGlobal>

#include <array>
#include <functional>
#include <optional>

namespace lexus_head_unit {

// The queued hop from the worker thread to the service's thread. A separate object on
// purpose: Qt D-Bus watches every signal of an exported object and refuses (and, for a type it
// cannot marshal, crashes on) one emitted from another thread, so the cross-thread signals
// must not belong to the exported object itself.
class VehicleDataServiceInbox : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;

signals:
    void sampleQueued(lexus_head_unit::SignalSample sample);
    void transitionQueued(lexus_head_unit::ConnectionTransition transition);
    void diagnosticsQueued(lexus_head_unit::DiagnosticsReport report);
};

// The object the vehicle-data service exports on D-Bus (DN-022). publishSample and
// publishTransition may be called from any thread; they hand the value to this object's
// thread, which updates the mirror and emits the D-Bus signal.
class VehicleDataService : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.samersamaha.LexusHeadUnit.VehicleData1")

public:
    explicit VehicleDataService(QObject* parent = nullptr);

    void publishSample(const SignalSample& sample);
    void publishTransition(const ConnectionTransition& transition);
    // Any thread: the source's diagnostics report (DN-030).
    void publishDiagnostics(const DiagnosticsReport& report);
    // Called on RequestDiagnostics(); the service process passes the source's
    // requestDiagnostics(), which is safe from this thread.
    void setDiagnosticsRequester(std::function<void()> requester);
    // Exports the object, then claims the service name. False, with the reason, if either
    // fails (for example, another service already owns the name).
    bool registerOn(QDBusConnection connection, QString& error);
    [[nodiscard]] quint64 publishedSampleCount() const;

public slots:
    Q_SCRIPTABLE QList<lexus_head_unit::DBusSample> GetSamples() const;
    Q_SCRIPTABLE lexus_head_unit::DBusConnectionState GetConnection() const;
    Q_SCRIPTABLE uint GetInterfaceVersion() const;
    Q_SCRIPTABLE void RequestDiagnostics();
    Q_SCRIPTABLE lexus_head_unit::DBusDiagnostics GetDiagnostics() const;

signals:
    Q_SCRIPTABLE void SampleChanged(
        uint signalId, double value, uint unit, qlonglong timestampMilliseconds, uint status);
    Q_SCRIPTABLE void
    ConnectionChanged(uint from, uint trigger, uint to, qlonglong timestampMilliseconds);
    Q_SCRIPTABLE void DiagnosticsChanged();

private:
    void applySample(const SignalSample& sample);
    void applyTransition(const ConnectionTransition& transition);
    void applyDiagnostics(const DiagnosticsReport& report);

    VehicleDataServiceInbox m_inbox;
    std::array<SignalSample, signalCount> m_samples{};
    ConnectionState m_state = ConnectionState::Disconnected;
    std::optional<ConnectionTransition> m_lastTransition;
    quint64 m_publishedSampleCount = 0;
    DiagnosticsReport m_diagnostics;
    std::function<void()> m_diagnosticsRequester;
};

} // namespace lexus_head_unit
