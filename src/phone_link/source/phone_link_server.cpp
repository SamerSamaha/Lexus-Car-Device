#include "lexus_head_unit/phone_link/phone_link_server.h"

#include "lexus_head_unit/phone_link/http_request.h"
#include "lexus_head_unit/phone_link/phone_position.h"

#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QObject>
#include <QSsl>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>
#include <QSslSocket>
#include <QString>
#include <QTcpSocket>
#include <QTimer>

#include <memory>
#include <optional>

// Qt declares its macros and basic types in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace lexus_head_unit {

namespace {

constexpr int statusOk = 200;
constexpr int statusNoContent = 204;
constexpr int statusBadRequest = 400;
constexpr int statusNotFound = 404;
constexpr int statusMethodNotAllowed = 405;
constexpr int statusPayloadTooLarge = 413;
constexpr int statusUnavailable = 503;

QByteArray reasonOf(int statusCode) {
    switch (statusCode) {
    case statusOk:
        return QByteArrayLiteral("OK");
    case statusNoContent:
        return QByteArrayLiteral("No Content");
    case statusBadRequest:
        return QByteArrayLiteral("Bad Request");
    case statusNotFound:
        return QByteArrayLiteral("Not Found");
    case statusMethodNotAllowed:
        return QByteArrayLiteral("Method Not Allowed");
    case statusPayloadTooLarge:
        return QByteArrayLiteral("Payload Too Large");
    default:
        return QByteArrayLiteral("Service Unavailable");
    }
}

std::optional<QByteArray> readFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    return file.readAll();
}

} // namespace

PhoneLinkServer::PhoneLinkServer(QObject* parent) : QObject(parent), m_page(page()) {
    connect(&m_server, &QSslServer::pendingConnectionAvailable, this, [this]() {
        acceptConnections();
    });
    // A client that refuses the certificate (the phone before it trusts the authority) is
    // simply dropped; the browser shows its own warning.
    m_server.setHandshakeTimeout(connectionTimeoutMilliseconds);
}

bool PhoneLinkServer::start(const Settings& settings, QString& error) {
    const std::optional<QByteArray> certificate = readFile(settings.certificatePath);
    const std::optional<QByteArray> key = readFile(settings.keyPath);
    const std::optional<QByteArray> caCertificate = readFile(settings.caCertificatePath);
    if (!certificate || !key || !caCertificate) {
        error = QStringLiteral("cannot read the certificate, its key or the authority's "
                               "certificate");
        return false;
    }
    const QSslCertificate parsedCertificate(*certificate, QSsl::Pem);
    const QSslKey parsedKey(*key, QSsl::Rsa, QSsl::Pem);
    if (parsedCertificate.isNull() || parsedKey.isNull()) {
        error = QStringLiteral("the certificate or its key is not a PEM RSA pair");
        return false;
    }
    QSslConfiguration configuration = QSslConfiguration::defaultConfiguration();
    configuration.setLocalCertificateChain(
        {parsedCertificate, QSslCertificate(*caCertificate, QSsl::Pem)});
    configuration.setPrivateKey(parsedKey);
    configuration.setPeerVerifyMode(QSslSocket::VerifyNone);
    configuration.setProtocol(QSsl::TlsV1_2OrLater);
    m_server.setSslConfiguration(configuration);
    m_caCertificate = *caCertificate;
    if (!m_server.listen(settings.address, settings.port)) {
        error = QStringLiteral("cannot listen on port %1: %2")
                    .arg(settings.port)
                    .arg(m_server.errorString());
        return false;
    }
    return true;
}

quint16 PhoneLinkServer::port() const {
    return m_server.serverPort();
}

std::uint64_t PhoneLinkServer::positionsAccepted() const {
    return m_positionsAccepted;
}

std::uint64_t PhoneLinkServer::requestsRejected() const {
    return m_requestsRejected;
}

QByteArray PhoneLinkServer::page() {
    return readFile(QStringLiteral(":/phone_link/phone_page.html")).value_or(QByteArray());
}

PhoneLinkResponse
PhoneLinkServer::response(int statusCode, const QByteArray& contentType, const QByteArray& body) {
    QByteArray bytes = "HTTP/1.1 " + QByteArray::number(statusCode) + ' ' + reasonOf(statusCode) +
                       "\r\nConnection: close\r\nCache-Control: no-store\r\n"
                       "X-Content-Type-Options: nosniff\r\nContent-Length: " +
                       QByteArray::number(body.size()) + "\r\n";
    if (!contentType.isEmpty()) {
        bytes += "Content-Type: " + contentType + "\r\n";
    }
    bytes += "\r\n" + body;
    return PhoneLinkResponse{statusCode, bytes, std::nullopt};
}

PhoneLinkResponse PhoneLinkServer::respond(const HttpRequest& request,
                                           const QByteArray& page,
                                           const QByteArray& caCertificate) {
    const qsizetype query = request.path.indexOf('?');
    const QByteArray path = query < 0 ? request.path : request.path.left(query);
    if (path == "/") {
        if (request.method != "GET") {
            return response(statusMethodNotAllowed, {}, {});
        }
        return response(statusOk, QByteArrayLiteral("text/html; charset=utf-8"), page);
    }
    if (path == "/ca.crt") {
        if (request.method != "GET") {
            return response(statusMethodNotAllowed, {}, {});
        }
        return response(statusOk, QByteArrayLiteral("application/x-x509-ca-cert"), caCertificate);
    }
    if (path == "/position") {
        if (request.method != "POST") {
            return response(statusMethodNotAllowed, {}, {});
        }
        QString error;
        const std::optional<PhonePosition> position = parsePhonePosition(request.body, error);
        if (!position.has_value()) {
            return response(statusBadRequest, QByteArrayLiteral("text/plain"), error.toUtf8());
        }
        PhoneLinkResponse accepted = response(statusNoContent, {}, {});
        accepted.position = position;
        return accepted;
    }
    return response(statusNotFound, {}, {});
}

void PhoneLinkServer::acceptConnections() {
    while (m_server.hasPendingConnections()) {
        QTcpSocket* socket = m_server.nextPendingConnection();
        if (socket == nullptr) {
            return;
        }
        if (m_openConnections >= maximumConnections) {
            socket->write(response(statusUnavailable, {}, {}).bytes);
            socket->disconnectFromHost();
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            ++m_requestsRejected;
            continue;
        }
        ++m_openConnections;
        auto parser = std::make_shared<HttpRequestParser>();
        // Qt parent ownership: the timer is deleted with the socket.
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
        auto* timeout = new QTimer(socket);
        timeout->setSingleShot(true);
        connect(timeout, &QTimer::timeout, socket, &QTcpSocket::abort);
        timeout->start(connectionTimeoutMilliseconds);
        connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
            --m_openConnections;
            socket->deleteLater();
        });
        connect(socket, &QTcpSocket::readyRead, this, [this, socket, parser]() {
            const HttpRequestParser::Result result = parser->feed(socket->readAll());
            if (result == HttpRequestParser::Result::Incomplete) {
                return;
            }
            PhoneLinkResponse answer = response(statusBadRequest, {}, {});
            if (result == HttpRequestParser::Result::Complete) {
                answer = respond(parser->request(), m_page, m_caCertificate);
            } else if (result == HttpRequestParser::Result::TooLarge) {
                answer = response(statusPayloadTooLarge, {}, {});
            }
            // Only once per connection: the parser keeps its result.
            disconnect(socket, &QTcpSocket::readyRead, this, nullptr);
            if (answer.position.has_value()) {
                ++m_positionsAccepted;
                emit positionReceived(*answer.position);
            } else if (answer.statusCode >= statusBadRequest) {
                ++m_requestsRejected;
            }
            socket->write(answer.bytes);
            socket->disconnectFromHost();
        });
    }
}

} // namespace lexus_head_unit
// NOLINTEND(misc-include-cleaner)
