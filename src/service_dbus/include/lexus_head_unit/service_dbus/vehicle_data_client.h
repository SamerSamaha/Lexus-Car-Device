#pragma once

#include "lexus_head_unit/qt/value_types.h"
#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/diagnostics_report.h"
#include "lexus_head_unit/service/link_detail.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"

#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QObject>
#include <QString>
#include <QtGlobal>

#include <array>

class QDBusPendingCallWatcher;

namespace lexus_head_unit {

// Client side of DN-022: subscribes to the service's signals, then fetches the current state,
// and applies every message in arrival order (D-Bus keeps one sender's messages in order, so
// the reply overrides earlier signals and later signals override the reply). Emits each
// applied sample and transition as a Qt signal for the view models.
class VehicleDataClient : public QObject {
    Q_OBJECT

public:
    explicit VehicleDataClient(QDBusConnection connection, QObject* parent = nullptr);

    void start();
    // Asks the service to read the trouble codes and the identification (DN-030).
    void requestDiagnostics();
    [[nodiscard]] DiagnosticsReport latestDiagnostics() const;

    [[nodiscard]] SignalSample latest(SignalId signalId) const;
    [[nodiscard]] ConnectionState connectionState() const;
    [[nodiscard]] LinkDetail linkDetail() const;
    [[nodiscard]] bool hasInitialState() const;
    [[nodiscard]] bool isServiceAvailable() const;
    [[nodiscard]] quint64 samplesReceived() const;
    [[nodiscard]] quint64 malformedMessages() const;

signals:
    void sampleArrived(lexus_head_unit::SignalSample sample);
    void connectionChanged(lexus_head_unit::ConnectionTransition transition);
    void initialStateReceived();
    void serviceAvailabilityChanged(bool available);
    void diagnosticsArrived(lexus_head_unit::DiagnosticsReport report);
    void linkDetailChanged(lexus_head_unit::LinkDetail detail);

private slots:
    void onSampleChanged(
        uint signalId, double value, uint unit, qlonglong timestampMilliseconds, uint status);
    void onConnectionChanged(uint fromState,
                             uint trigger,
                             uint toState,
                             qlonglong timestampMilliseconds);
    void onDiagnosticsChanged();
    void onLinkDetailChanged(uint detail);

private:
    void fetchState();
    void fetchDiagnostics();
    void onDiagnosticsReply(QDBusPendingCallWatcher* watcher);
    void onSamplesReply(QDBusPendingCallWatcher* watcher);
    void onConnectionReply(QDBusPendingCallWatcher* watcher);
    void onLinkDetailReply(QDBusPendingCallWatcher* watcher);
    void applyLinkDetail(LinkDetail detail);
    // Emits initialStateReceived once the samples, the connection and the link detail are in.
    void noteFetched();
    void onServiceRegistered();
    void onServiceUnregistered();
    void setServiceAvailable(bool available);
    void applySample(const SignalSample& sample);
    void applyTransition(const ConnectionTransition& transition);

    QDBusConnection m_connection;
    QDBusServiceWatcher m_watcher;
    std::array<SignalSample, signalCount> m_samples{};
    ConnectionState m_state = ConnectionState::Disconnected;
    LinkDetail m_linkDetail = LinkDetail::Idle;
    bool m_samplesFetched = false;
    bool m_connectionFetched = false;
    bool m_linkDetailFetched = false;
    bool m_serviceAvailable = false;
    quint64 m_samplesReceived = 0;
    quint64 m_malformedMessages = 0;
    DiagnosticsReport m_diagnostics;
};

} // namespace lexus_head_unit
