#pragma once

#include "lexus_head_unit/hmi/value_types.h"
#include "lexus_head_unit/service/diagnostics_report.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

namespace lexus_head_unit {

// What the diagnostics screen binds to (DN-030, REQ-021): the stored trouble codes with their
// texts, the vehicle identification (only its last four characters are shown, so a screen
// recording or a photo never carries the VIN, REQ-026; never written anywhere), and a refresh
// that asks the source for a new read through the requester.
class DiagnosticsViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool codesRead READ codesRead NOTIFY changed)
    Q_PROPERTY(int codeCount READ codeCount NOTIFY changed)
    Q_PROPERTY(QStringList codes READ codes NOTIFY changed)
    Q_PROPERTY(QStringList descriptions READ descriptions NOTIFY changed)
    Q_PROPERTY(QString summaryText READ summaryText NOTIFY changed)
    Q_PROPERTY(QString identificationText READ identificationText NOTIFY changed)
    Q_PROPERTY(QString readTimeText READ readTimeText NOTIFY changed)
    Q_PROPERTY(bool reading READ reading NOTIFY changed)

public:
    using Requester = std::function<void()>;

    explicit DiagnosticsViewModel(QObject* parent = nullptr);

    void setRequester(Requester requester);

    [[nodiscard]] bool codesRead() const;
    [[nodiscard]] int codeCount() const;
    [[nodiscard]] QStringList codes() const;
    [[nodiscard]] QStringList descriptions() const;
    // "Not read yet", "Reading...", "No stored trouble codes", "1 stored trouble code", ...
    [[nodiscard]] QString summaryText() const;
    // The identification with all but its last four characters masked, or "Not read".
    [[nodiscard]] QString identificationText() const;
    [[nodiscard]] QString readTimeText() const;
    [[nodiscard]] bool reading() const;

    Q_INVOKABLE void refresh();

    // Every character but the last four replaced by a bullet; a text of four characters or
    // fewer is masked completely.
    static QString maskedIdentification(const QString& identification);
    static constexpr qsizetype shownIdentificationCharacters = 4;
    static constexpr char16_t maskCharacter = u'\u2022';

public slots:
    void onDiagnostics(lexus_head_unit::DiagnosticsReport report);

signals:
    void changed();

private:
    Requester m_requester;
    DiagnosticsReport m_report;
    bool m_hasReport = false;
    bool m_reading = false;
    QString m_readTimeText;
};

} // namespace lexus_head_unit
