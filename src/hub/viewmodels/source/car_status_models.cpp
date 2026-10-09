#include "lexus_head_unit/hub/car_status_models.h"

#include "lexus_head_unit/hub/ignition_off_shutdown_policy.h"
#include "lexus_head_unit/hub/power_status_model.h"
#include "lexus_head_unit/service/link_detail.h"

#include <QAbstractSocket>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QObject>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <utility>

// Qt declares its macros and types in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace lexus_head_unit {

namespace {

constexpr std::int64_t millisecondsPerSecond = 1000;

} // namespace

IgnitionOffShutdownModel::IgnitionOffShutdownModel(IgnitionOffShutdownSettings settings,
                                                   NowFunction now,
                                                   ShutdownController& shutdown,
                                                   QObject* parent)
    : QObject(parent), m_policy(settings), m_now(std::move(now)), m_shutdown(&shutdown) {
    connect(&m_timer, &QTimer::timeout, this, &IgnitionOffShutdownModel::tick);
}

bool IgnitionOffShutdownModel::countdownActive() const {
    return m_countdownActive;
}

int IgnitionOffShutdownModel::secondsRemaining() const {
    return m_secondsRemaining;
}

QString IgnitionOffShutdownModel::countdownText() const {
    if (!m_countdownActive) {
        return {};
    }
    return QStringLiteral("Vehicle off: shutting down in %1 s").arg(m_secondsRemaining);
}

const IgnitionOffShutdownPolicy& IgnitionOffShutdownModel::policy() const {
    return m_policy;
}

void IgnitionOffShutdownModel::startTicking(int intervalMilliseconds) {
    m_timer.start(intervalMilliseconds);
}

void IgnitionOffShutdownModel::cancel() {
    m_policy.cancel();
    tick();
}

void IgnitionOffShutdownModel::applyLinkDetail(lexus_head_unit::LinkDetail detail) {
    m_policy.onLinkDetail(detail, m_now());
    tick();
}

void IgnitionOffShutdownModel::tick() {
    const std::int64_t now = m_now();
    const IgnitionOffShutdownPhase phase = m_policy.update(now);
    if (phase == IgnitionOffShutdownPhase::ShutdownDue) {
        m_shutdown->shutdownNow(QStringLiteral("Vehicle off: shutting down"));
    }
    const bool active = phase == IgnitionOffShutdownPhase::CountingDown;
    // Rounded up, so the banner never shows 0 while time is left.
    const auto seconds = static_cast<int>(
        (m_policy.millisecondsRemaining(now) + millisecondsPerSecond - 1) / millisecondsPerSecond);
    if (active != m_countdownActive || seconds != m_secondsRemaining) {
        m_countdownActive = active;
        m_secondsRemaining = seconds;
        emit changed();
    }
}

NetworkAddressModel::NetworkAddressModel(QString userName,
                                         AddressProvider provider,
                                         QObject* parent)
    : QObject(parent), m_userName(std::move(userName)), m_provider(std::move(provider)) {
    connect(&m_timer, &QTimer::timeout, this, &NetworkAddressModel::refresh);
    refresh();
}

QString NetworkAddressModel::addressText() const {
    return m_addressText;
}

void NetworkAddressModel::startRefreshing(int intervalMilliseconds) {
    m_timer.start(intervalMilliseconds);
}

void NetworkAddressModel::refresh() {
    const QStringList addresses = m_provider ? m_provider() : QStringList();
    QString text = QStringLiteral("No network");
    if (!addresses.isEmpty()) {
        QStringList targets;
        for (const QString& address : addresses) {
            targets << m_userName + QLatin1Char('@') + address;
        }
        text = QStringLiteral("SSH ") + targets.join(QStringLiteral(", "));
    }
    if (text != m_addressText) {
        m_addressText = text;
        emit changed();
    }
}

NetworkAddressModel::AddressProvider NetworkAddressModel::interfaceAddresses() {
    return []() {
        QStringList addresses;
        for (const QNetworkInterface& networkInterface : QNetworkInterface::allInterfaces()) {
            const QNetworkInterface::InterfaceFlags flags = networkInterface.flags();
            if (!flags.testFlag(QNetworkInterface::IsUp) ||
                flags.testFlag(QNetworkInterface::IsLoopBack)) {
                continue;
            }
            for (const QNetworkAddressEntry& entry : networkInterface.addressEntries()) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    addresses << entry.ip().toString();
                }
            }
        }
        return addresses;
    };
}

} // namespace lexus_head_unit
// NOLINTEND(misc-include-cleaner)
