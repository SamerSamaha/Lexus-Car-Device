#include "lexus_head_unit/hmi/diagnostics_view_model.h"

#include "lexus_head_unit/service/diagnostics_report.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTime>

#include <utility>

// Qt declares its macros and QStringLiteral in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace lexus_head_unit {

DiagnosticsViewModel::DiagnosticsViewModel(QObject* parent) : QObject(parent) {}

void DiagnosticsViewModel::setRequester(Requester requester) {
    m_requester = std::move(requester);
}

bool DiagnosticsViewModel::codesRead() const {
    return m_hasReport && m_report.codesRead;
}

int DiagnosticsViewModel::codeCount() const {
    return codesRead() ? static_cast<int>(m_report.troubleCodes.size()) : 0;
}

QStringList DiagnosticsViewModel::codes() const {
    QStringList result;
    if (codesRead()) {
        for (const TroubleCode& code : m_report.troubleCodes) {
            result << QString::fromStdString(code.code);
        }
    }
    return result;
}

QStringList DiagnosticsViewModel::descriptions() const {
    QStringList result;
    if (codesRead()) {
        for (const TroubleCode& code : m_report.troubleCodes) {
            result << QString::fromStdString(code.description);
        }
    }
    return result;
}

QString DiagnosticsViewModel::summaryText() const {
    if (m_reading) {
        return QStringLiteral("Reading...");
    }
    if (!m_hasReport) {
        return QStringLiteral("Not read yet");
    }
    if (!m_report.codesRead) {
        return QStringLiteral("Trouble codes could not be read");
    }
    const int count = codeCount();
    if (count == 0) {
        return QStringLiteral("No stored trouble codes");
    }
    return count == 1 ? QStringLiteral("1 stored trouble code")
                      : QStringLiteral("%1 stored trouble codes").arg(count);
}

QString DiagnosticsViewModel::identificationText() const {
    if (!m_hasReport || !m_report.identificationRead) {
        return QStringLiteral("Not read");
    }
    return QString::fromStdString(m_report.vehicleIdentification);
}

QString DiagnosticsViewModel::readTimeText() const {
    return m_readTimeText;
}

bool DiagnosticsViewModel::reading() const {
    return m_reading;
}

void DiagnosticsViewModel::refresh() {
    if (!m_requester) {
        return;
    }
    m_reading = true;
    emit changed();
    m_requester();
}

void DiagnosticsViewModel::onDiagnostics(DiagnosticsReport report) {
    m_report = std::move(report);
    m_hasReport = true;
    m_reading = false;
    m_readTimeText =
        QStringLiteral("Read at %1").arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")));
    emit changed();
}

} // namespace lexus_head_unit
// NOLINTEND(misc-include-cleaner)
