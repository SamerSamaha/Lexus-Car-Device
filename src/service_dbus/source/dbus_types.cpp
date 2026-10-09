#include "lexus_head_unit/service_dbus/dbus_types.h"

#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"

#include <QDBusArgument>
#include <QDBusMetaType>
#include <QList>
#include <QtGlobal>

#include <optional>

// Qt declares its macros and types in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace lexus_head_unit {

namespace {

constexpr quint32 unitCount = 5;
constexpr quint32 statusCount = 3;

template <typename Enumeration>
quint32 numberOf(Enumeration value) {
    return static_cast<quint32>(value);
}

} // namespace

DBusSample toDBus(const SignalSample& sample) {
    DBusSample wire;
    wire.signalId = numberOf(sample.signalId);
    wire.value = sample.value;
    wire.unit = numberOf(sample.unit);
    wire.timestampMilliseconds = sample.timestampMilliseconds;
    wire.status = numberOf(sample.status);
    return wire;
}

std::optional<SignalSample> fromDBus(const DBusSample& sample) {
    if (sample.signalId >= signalCount || sample.unit >= unitCount ||
        sample.status >= statusCount) {
        return std::nullopt;
    }
    SignalSample result;
    result.signalId = static_cast<SignalId>(sample.signalId);
    result.value = sample.value;
    result.unit = static_cast<Unit>(sample.unit);
    result.timestampMilliseconds = sample.timestampMilliseconds;
    result.status = static_cast<SignalStatus>(sample.status);
    return result;
}

DBusConnectionState toDBus(ConnectionState state,
                           const std::optional<ConnectionTransition>& lastTransition) {
    DBusConnectionState wire;
    wire.state = numberOf(state);
    wire.hasTransition = lastTransition.has_value();
    if (lastTransition.has_value()) {
        wire.from = numberOf(lastTransition->from);
        wire.trigger = numberOf(lastTransition->trigger);
        wire.to = numberOf(lastTransition->to);
        wire.timestampMilliseconds = lastTransition->timestampMilliseconds;
    }
    return wire;
}

std::optional<ConnectionState> connectionStateFromDBus(quint32 state) {
    if (state >= connectionStateCount) {
        return std::nullopt;
    }
    return static_cast<ConnectionState>(state);
}

std::optional<ConnectionTransition> transitionFromDBus(quint32 fromState,
                                                       quint32 trigger,
                                                       quint32 toState,
                                                       qint64 timestampMilliseconds) {
    const std::optional<ConnectionState> from = connectionStateFromDBus(fromState);
    const std::optional<ConnectionState> destination = connectionStateFromDBus(toState);
    if (!from.has_value() || !destination.has_value() || trigger >= connectionTriggerCount) {
        return std::nullopt;
    }
    ConnectionTransition transition;
    transition.from = *from;
    transition.trigger = static_cast<ConnectionTrigger>(trigger);
    transition.to = *destination;
    transition.timestampMilliseconds = timestampMilliseconds;
    return transition;
}

DBusDiagnostics toDBus(const DiagnosticsReport& report) {
    DBusDiagnostics wire;
    wire.codesRead = report.codesRead;
    wire.identificationRead = report.identificationRead;
    for (const TroubleCode& code : report.troubleCodes) {
        wire.troubleCodes.append(DBusTroubleCode{QString::fromStdString(code.code),
                                                 QString::fromStdString(code.description)});
    }
    wire.vehicleIdentification = QString::fromStdString(report.vehicleIdentification);
    wire.timestampMilliseconds = report.timestampMilliseconds;
    return wire;
}

DiagnosticsReport fromDBus(const DBusDiagnostics& diagnostics) {
    DiagnosticsReport report;
    report.codesRead = diagnostics.codesRead;
    report.identificationRead = diagnostics.identificationRead;
    for (const DBusTroubleCode& code : diagnostics.troubleCodes) {
        report.troubleCodes.push_back(
            TroubleCode{code.code.toStdString(), code.description.toStdString()});
    }
    report.vehicleIdentification = diagnostics.vehicleIdentification.toStdString();
    report.timestampMilliseconds = diagnostics.timestampMilliseconds;
    return report;
}

void registerDBusTypes() {
    qDBusRegisterMetaType<DBusTroubleCode>();
    qDBusRegisterMetaType<QList<DBusTroubleCode>>();
    qDBusRegisterMetaType<DBusDiagnostics>();
    qDBusRegisterMetaType<DBusSample>();
    qDBusRegisterMetaType<QList<DBusSample>>();
    qDBusRegisterMetaType<DBusConnectionState>();
}

QDBusArgument& operator<<(QDBusArgument& argument, const DBusSample& sample) {
    argument.beginStructure();
    argument << sample.signalId << sample.value << sample.unit << sample.timestampMilliseconds
             << sample.status;
    argument.endStructure();
    return argument;
}

// Qt D-Bus requires this exact signature, returning the argument it was given.
const QDBusArgument& operator>>(const QDBusArgument& argument, DBusSample& sample) {
    argument.beginStructure();
    argument >> sample.signalId >> sample.value >> sample.unit >> sample.timestampMilliseconds >>
        sample.status;
    argument.endStructure();
    return argument; // NOLINT(bugprone-return-const-ref-from-parameter)
}

QDBusArgument& operator<<(QDBusArgument& argument, const DBusTroubleCode& code) {
    argument.beginStructure();
    argument << code.code << code.description;
    argument.endStructure();
    return argument;
}

const QDBusArgument& operator>>(const QDBusArgument& argument, DBusTroubleCode& code) {
    argument.beginStructure();
    argument >> code.code >> code.description;
    argument.endStructure();
    return argument; // NOLINT(bugprone-return-const-ref-from-parameter)
}

QDBusArgument& operator<<(QDBusArgument& argument, const DBusDiagnostics& diagnostics) {
    argument.beginStructure();
    argument << diagnostics.codesRead << diagnostics.identificationRead << diagnostics.troubleCodes
             << diagnostics.vehicleIdentification << diagnostics.timestampMilliseconds;
    argument.endStructure();
    return argument;
}

const QDBusArgument& operator>>(const QDBusArgument& argument, DBusDiagnostics& diagnostics) {
    argument.beginStructure();
    argument >> diagnostics.codesRead >> diagnostics.identificationRead >>
        diagnostics.troubleCodes >> diagnostics.vehicleIdentification >>
        diagnostics.timestampMilliseconds;
    argument.endStructure();
    return argument; // NOLINT(bugprone-return-const-ref-from-parameter)
}

QDBusArgument& operator<<(QDBusArgument& argument, const DBusConnectionState& state) {
    argument.beginStructure();
    argument << state.state << state.hasTransition << state.from << state.trigger << state.to
             << state.timestampMilliseconds;
    argument.endStructure();
    return argument;
}

// Qt D-Bus requires this exact signature, returning the argument it was given.
const QDBusArgument& operator>>(const QDBusArgument& argument, DBusConnectionState& state) {
    argument.beginStructure();
    argument >> state.state >> state.hasTransition >> state.from >> state.trigger >> state.to >>
        state.timestampMilliseconds;
    argument.endStructure();
    return argument; // NOLINT(bugprone-return-const-ref-from-parameter)
}

} // namespace lexus_head_unit
// NOLINTEND(misc-include-cleaner)
