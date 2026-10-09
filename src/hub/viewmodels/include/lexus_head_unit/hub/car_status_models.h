#pragma once

#include "lexus_head_unit/hub/ignition_off_shutdown_policy.h"
#include "lexus_head_unit/service/link_detail.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <cstdint>
#include <functional>

namespace lexus_head_unit {

class ShutdownController;

// DN-043: the countdown banner of the hub. Fed the link detail from the vehicle-data service;
// when the policy says the shutdown is due it asks the ShutdownController to shut down, the
// same path as the button's second tap. The time comes from a function returning monotonic
// milliseconds (the hub passes its steady clock, tests a manual one), so the HMI layer needs no
// service header beyond the value types (REQ-011).
class IgnitionOffShutdownModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool countdownActive READ countdownActive NOTIFY changed)
    Q_PROPERTY(int secondsRemaining READ secondsRemaining NOTIFY changed)
    Q_PROPERTY(QString countdownText READ countdownText NOTIFY changed)

public:
    using NowFunction = std::function<std::int64_t()>;

    IgnitionOffShutdownModel(IgnitionOffShutdownSettings settings,
                             NowFunction now,
                             ShutdownController& shutdown,
                             QObject* parent = nullptr);

    [[nodiscard]] bool countdownActive() const;
    [[nodiscard]] int secondsRemaining() const;
    [[nodiscard]] QString countdownText() const;
    [[nodiscard]] const IgnitionOffShutdownPolicy& policy() const;

    // Starts a timer that calls tick() every intervalMilliseconds.
    void startTicking(int intervalMilliseconds);
    Q_INVOKABLE void cancel();

public slots:
    void applyLinkDetail(lexus_head_unit::LinkDetail detail);
    void tick();

signals:
    void changed();

private:
    IgnitionOffShutdownPolicy m_policy;
    NowFunction m_now;
    ShutdownController* m_shutdown;
    QTimer m_timer;
    bool m_countdownActive = false;
    int m_secondsRemaining = 0;
};

// The Pi's addresses for SSH from a phone on the hotspot (DN-043): "SSH lexus@<address>", or
// "No network". The provider returns the IPv4 addresses; refresh() re-reads them.
class NetworkAddressModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString addressText READ addressText NOTIFY changed)

public:
    using AddressProvider = std::function<QStringList()>;

    NetworkAddressModel(QString userName, AddressProvider provider, QObject* parent = nullptr);

    [[nodiscard]] QString addressText() const;
    void startRefreshing(int intervalMilliseconds);
    Q_INVOKABLE void refresh();

    // The product provider: IPv4 addresses of interfaces that are up and not loopback.
    static AddressProvider interfaceAddresses();

signals:
    void changed();

private:
    QString m_userName;
    AddressProvider m_provider;
    QTimer m_timer;
    QString m_addressText;
};

} // namespace lexus_head_unit
