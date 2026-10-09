#pragma once

#include "lexus_head_unit/hmi/diagnostics_view_model.h"
#include "lexus_head_unit/service/diagnostics_report.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace lexus_head_unit::testing {

// Lets a QML test deliver diagnostics reports to the real DiagnosticsViewModel and count the
// refresh requests the screen makes (REQ-021).
class FakeDiagnosticsDriver : public QObject {
    Q_OBJECT
    Q_PROPERTY(int requests READ requests NOTIFY requestsChanged)

public:
    explicit FakeDiagnosticsDriver(DiagnosticsViewModel& model, QObject* parent = nullptr)
        : QObject(parent), m_model(&model) {
        m_model->setRequester([this]() {
            ++m_requests;
            emit requestsChanged();
        });
    }

    [[nodiscard]] int requests() const {
        return m_requests;
    }

    Q_INVOKABLE void deliver(const QStringList& codes,
                             const QStringList& descriptions,
                             const QString& identification) {
        DiagnosticsReport report;
        report.codesRead = true;
        for (int index = 0; index < codes.size(); ++index) {
            report.troubleCodes.push_back(TroubleCode{codes.at(index).toStdString(),
                                                      descriptions.value(index).toStdString()});
        }
        report.identificationRead = !identification.isEmpty();
        report.vehicleIdentification = identification.toStdString();
        m_model->onDiagnostics(report);
    }

signals:
    void requestsChanged();

private:
    DiagnosticsViewModel* m_model;
    int m_requests = 0;
};

} // namespace lexus_head_unit::testing
