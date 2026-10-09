#pragma once

#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/link_detail.h"

#include <QObject>
#include <QString>

namespace lexus_head_unit {

class ConnectionStatusModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString stateText READ stateText NOTIFY changed)
    Q_PROPERTY(QString lastCauseText READ lastCauseText NOTIFY changed)
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY changed)
    Q_PROPERTY(bool isError READ isError NOTIFY changed)
    Q_PROPERTY(quint64 transitionCount READ transitionCount NOTIFY changed)
    // DN-042: the link situation in words for the strip, and its name for the colour.
    Q_PROPERTY(QString detailText READ detailText NOTIFY changed)
    Q_PROPERTY(QString detailName READ detailName NOTIFY changed)

public:
    explicit ConnectionStatusModel(QObject* parent = nullptr);

    [[nodiscard]] ConnectionState state() const;
    [[nodiscard]] QString stateText() const;
    [[nodiscard]] QString lastCauseText() const;
    [[nodiscard]] bool isConnected() const;
    [[nodiscard]] bool isError() const;
    [[nodiscard]] quint64 transitionCount() const;
    [[nodiscard]] LinkDetail linkDetail() const;
    [[nodiscard]] QString detailText() const;
    [[nodiscard]] QString detailName() const;

    // A transition sets the detail that follows from it (linkDetailAfter); a detail reported
    // by the source overrides it.
    void applyTransition(const ConnectionTransition& transition);
    void applyLinkDetail(LinkDetail detail);

signals:
    void changed();

private:
    ConnectionState m_state = ConnectionState::Disconnected;
    QString m_lastCauseText;
    quint64 m_transitionCount = 0;
    LinkDetail m_linkDetail = LinkDetail::Idle;
};

} // namespace lexus_head_unit
