#include "lexus_head_unit/hub/hub_control_server.h"

#include "lexus_head_unit/hub/app_process_manager.h"
#include "lexus_head_unit/hub/hub_view_model.h"
#include "lexus_head_unit/hub/process_launcher.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>
#include <QString>
#include <QStringList>

#include <optional>
#include <utility>

// Qt declares its macros, enumerations and QStringLiteral in internal headers; the public
// ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace lexus_head_unit {

namespace {

constexpr int probeTimeoutMilliseconds = 200;

QString numberText(unsigned long long value) {
    return QString::number(value);
}

} // namespace

HubControlServer::HubControlServer(HubViewModel& hub, QObject* parent)
    : QObject(parent), m_hub(&hub), m_server(this) {
    connect(&m_server, &QLocalServer::newConnection, this, &HubControlServer::acceptConnections);
}

bool HubControlServer::listen(const QString& name, QString& error) {
    QLocalSocket probe;
    probe.connectToServer(name);
    if (probe.waitForConnected(probeTimeoutMilliseconds)) {
        probe.disconnectFromServer();
        error = QStringLiteral("another hub is already listening on ") + name;
        return false;
    }
    QLocalServer::removeServer(name);
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server.listen(name)) {
        error = m_server.errorString();
        return false;
    }
    return true;
}

void HubControlServer::setWindowVisibilityProvider(VisibilityProvider provider) {
    m_windowVisible = std::move(provider);
}

void HubControlServer::acceptConnections() {
    while (QLocalSocket* connection = m_server.nextPendingConnection()) {
        connect(connection, &QLocalSocket::disconnected, connection, &QObject::deleteLater);
        connect(connection, &QLocalSocket::readyRead, this, [this, connection]() {
            while (connection->canReadLine()) {
                const QString line = QString::fromUtf8(connection->readLine()).trimmed();
                const QByteArray reply = handleCommand(line).toUtf8() + '\n';
                connection->write(reply);
                connection->flush();
            }
        });
    }
}

QString HubControlServer::handleCommand(const QString& line) {
    const QStringList words = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (words.isEmpty()) {
        return QStringLiteral("error empty command");
    }
    const QString& command = words.front();
    if (command == QStringLiteral("launch") && words.size() == 2) {
        return QStringLiteral("launch ") + m_hub->launchById(words.at(1));
    }
    if (command == QStringLiteral("return") && words.size() == 1) {
        return m_hub->requestReturn() ? QStringLiteral("return stopping")
                                      : QStringLiteral("return nothing-running");
    }
    if (command == QStringLiteral("status") && words.size() == 1) {
        return statusLine();
    }
    return QStringLiteral("error unknown command");
}

QString HubControlServer::statusLine() const {
    const AppProcessManager& manager = m_hub->manager();
    const std::optional<ProcessId> appProcessId = manager.foregroundProcessId();
    const AppManagerCounters counters = manager.counters();
    const QString appId = QString::fromStdString(manager.foregroundAppId());
    const bool windowVisible = m_windowVisible ? m_windowVisible() : false;
    QStringList fields;
    fields << QStringLiteral("state")
           << QString::fromUtf8(toString(manager.runState()).data(),
                                static_cast<int>(toString(manager.runState()).size()))
           << QStringLiteral("app") << (appId.isEmpty() ? QStringLiteral("-") : appId)
           << QStringLiteral("hub_pid") << QString::number(QCoreApplication::applicationPid())
           << QStringLiteral("app_pid")
           << (appProcessId.has_value() ? QString::number(*appProcessId) : QStringLiteral("-"))
           << QStringLiteral("window")
           << (windowVisible ? QStringLiteral("visible") : QStringLiteral("hidden"))
           << QStringLiteral("launches") << numberText(counters.launches) << QStringLiteral("exits")
           << numberText(counters.exits) << QStringLiteral("restarts")
           << numberText(counters.restarts) << QStringLiteral("kills")
           << numberText(counters.kills);
    return fields.join(QLatin1Char(' '));
}

QString sendHubCommand(const QString& name,
                       const QString& command,
                       int timeoutMilliseconds,
                       bool& answered) {
    answered = false;
    QLocalSocket socket;
    socket.connectToServer(name);
    if (!socket.waitForConnected(timeoutMilliseconds)) {
        return socket.errorString();
    }
    socket.write(command.toUtf8() + '\n');
    if (!socket.waitForBytesWritten(timeoutMilliseconds)) {
        return socket.errorString();
    }
    while (!socket.canReadLine()) {
        if (!socket.waitForReadyRead(timeoutMilliseconds)) {
            return socket.errorString();
        }
    }
    answered = true;
    return QString::fromUtf8(socket.readLine()).trimmed();
}

} // namespace lexus_head_unit
// NOLINTEND(misc-include-cleaner)
