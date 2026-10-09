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
    // Exports the object, then claims the service name. False, with the reason, if either
    // fails (for example, another service already owns the name).
    bool registerOn(QDBusConnection connection, QString& error);
    [[nodiscard]] quint64 publishedSampleCount() const;

public slots:
    Q_SCRIPTABLE QList<lexus_head_unit::DBusSample> GetSamples() const;
    Q_SCRIPTABLE lexus_head_unit::DBusConnectionState GetConnection() const;
    Q_SCRIPTABLE uint GetInterfaceVersion() const;

signals:
    Q_SCRIPTABLE void SampleChanged(
        uint signalId, double value, uint unit, qlonglong timestampMilliseconds, uint status);
    Q_SCRIPTABLE void
    ConnectionChanged(uint from, uint trigger, uint to, qlonglong timestampMilliseconds);

private:
    void applySample(const SignalSample& sample);
    void applyTransition(const ConnectionTransition& transition);

    VehicleDataServiceInbox m_inbox;
    std::array<SignalSample, signalCount> m_samples{};
    ConnectionState m_state = ConnectionState::Disconnected;
    std::optional<ConnectionTransition> m_lastTransition;
    quint64 m_publishedSampleCount = 0;
};

} // namespace lexus_head_unit
