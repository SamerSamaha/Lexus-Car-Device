#pragma once

#include "lexus_head_unit/hub/hub_view_model.h"

#include <QLocalServer>
#include <QObject>
#include <QString>

#include <functional>

namespace lexus_head_unit {

// One-line text commands on a local socket (DN-021): "launch <id>", "return", "status".
// Each command gets one reply line. Used by the panel launcher ("lexus-hub --send return") and
// by the end-to-end tests.
class HubControlServer : public QObject {
    Q_OBJECT

public:
    using VisibilityProvider = std::function<bool()>;

    explicit HubControlServer(HubViewModel& hub, QObject* parent = nullptr);

    // Fails when another hub already answers on the name; a stale socket file is replaced.
    bool listen(const QString& name, QString& error);
    void setWindowVisibilityProvider(VisibilityProvider provider);
    // The reply to one command line, without the newline.
    QString handleCommand(const QString& line);

private:
    void acceptConnections();
    QString statusLine() const;

    HubViewModel* m_hub;
    QLocalServer m_server;
    VisibilityProvider m_windowVisible;
};

// Client side of the same protocol: connects, sends one command, returns the reply line.
// answered is false, and the reply is the socket error, when no hub answered within the timeout.
QString sendHubCommand(const QString& name,
                       const QString& command,
                       int timeoutMilliseconds,
                       bool& answered);

} // namespace lexus_head_unit
