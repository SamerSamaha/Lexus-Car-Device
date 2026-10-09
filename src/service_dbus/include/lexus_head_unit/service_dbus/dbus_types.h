#pragma once

#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/diagnostics_report.h"
#include "lexus_head_unit/service/signal_sample.h"

#include <QDBusArgument>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QtGlobal>

#include <optional>

namespace lexus_head_unit {

// One sample on the wire: D-Bus signature (uduxu). The enumerations travel as their numbers.
struct DBusSample {
    quint32 signalId = 0;
    double value = 0.0;
    quint32 unit = 0;
    qint64 timestampMilliseconds = 0;
    quint32 status = 0;
};

// The connection state and the last transition: D-Bus signature (ubuuux).
struct DBusConnectionState {
    quint32 state = 0;
    bool hasTransition = false;
    quint32 from = 0;
    quint32 trigger = 0;
    quint32 to = 0;
    qint64 timestampMilliseconds = 0;
};

// One trouble code on the wire: (ss).
struct DBusTroubleCode {
    QString code;
    QString description;
};

// A diagnostics report on the wire (DN-030): (bba(ss)sx). The identification is the vehicle's
// VIN, which crosses only the user's own session bus to the screen.
struct DBusDiagnostics {
    bool codesRead = false;
    bool identificationRead = false;
    QList<DBusTroubleCode> troubleCodes;
    QString vehicleIdentification;
    qint64 timestampMilliseconds = 0;
};

DBusDiagnostics toDBus(const DiagnosticsReport& report);
DiagnosticsReport fromDBus(const DBusDiagnostics& diagnostics);

DBusSample toDBus(const SignalSample& sample);
// Empty when any enumeration number is out of range.
std::optional<SignalSample> fromDBus(const DBusSample& sample);

DBusConnectionState toDBus(ConnectionState state,
                           const std::optional<ConnectionTransition>& lastTransition);
std::optional<ConnectionState> connectionStateFromDBus(quint32 state);
std::optional<ConnectionTransition> transitionFromDBus(quint32 fromState,
                                                       quint32 trigger,
                                                       quint32 toState,
                                                       qint64 timestampMilliseconds);

// Registers DBusSample, QList<DBusSample> and DBusConnectionState with Qt D-Bus; safe to call
// more than once.
void registerDBusTypes();

QDBusArgument& operator<<(QDBusArgument& argument, const DBusSample& sample);
const QDBusArgument& operator>>(const QDBusArgument& argument, DBusSample& sample);
QDBusArgument& operator<<(QDBusArgument& argument, const DBusTroubleCode& code);
const QDBusArgument& operator>>(const QDBusArgument& argument, DBusTroubleCode& code);
QDBusArgument& operator<<(QDBusArgument& argument, const DBusDiagnostics& diagnostics);
const QDBusArgument& operator>>(const QDBusArgument& argument, DBusDiagnostics& diagnostics);
QDBusArgument& operator<<(QDBusArgument& argument, const DBusConnectionState& state);
const QDBusArgument& operator>>(const QDBusArgument& argument, DBusConnectionState& state);

} // namespace lexus_head_unit

Q_DECLARE_METATYPE(lexus_head_unit::DBusSample)
Q_DECLARE_METATYPE(lexus_head_unit::DBusConnectionState)
Q_DECLARE_METATYPE(lexus_head_unit::DBusTroubleCode)
Q_DECLARE_METATYPE(lexus_head_unit::DBusDiagnostics)
