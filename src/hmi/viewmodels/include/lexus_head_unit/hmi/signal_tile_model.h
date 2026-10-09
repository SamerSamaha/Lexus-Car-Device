#pragma once

#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"

#include <QObject>
#include <QString>

namespace lexus_head_unit {

// One signal as the screen shows it. Lives on the UI thread; applySample is called there.
class SignalTileModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString unitText READ unitText CONSTANT)
    Q_PROPERTY(QString valueText READ valueText NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
    Q_PROPERTY(bool isValid READ isValid NOTIFY changed)
    Q_PROPERTY(bool isStale READ isStale NOTIFY changed)
    Q_PROPERTY(bool isNeverReceived READ isNeverReceived NOTIFY changed)

public:
    explicit SignalTileModel(SignalId signalId, QObject* parent = nullptr);

    [[nodiscard]] SignalId signalId() const;
    [[nodiscard]] QString name() const;
    [[nodiscard]] QString unitText() const;
    [[nodiscard]] QString valueText() const;
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] bool isValid() const;
    [[nodiscard]] bool isStale() const;
    [[nodiscard]] bool isNeverReceived() const;

    void applySample(const SignalSample& sample);

    static QString formatValue(double value, Unit unit);

signals:
    void changed();

private:
    SignalId m_signalId;
    Unit m_unit;
    QString m_name;
    QString m_unitText;
    QString m_valueText;
    SignalStatus m_status = SignalStatus::NeverReceived;
};

} // namespace lexus_head_unit
