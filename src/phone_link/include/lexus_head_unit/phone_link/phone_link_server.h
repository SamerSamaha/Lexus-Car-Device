#pragma once

#include "lexus_head_unit/phone_link/http_request.h"
#include "lexus_head_unit/phone_link/phone_position.h"

#include <QByteArray>
#include <QHostAddress>
#include <QObject>
#include <QSslServer>
#include <QString>
#include <QtGlobal>

#include <cstdint>
#include <optional>

namespace lexus_head_unit {

// What the server answers to one request, and the position it carried, if any.
struct PhoneLinkResponse {
    int statusCode = 0;
    QByteArray bytes;
    std::optional<PhonePosition> position;
};

// The phone link (DN-053, REQ-029): an HTTPS server in the vehicle-data service that serves one
// page to the phone's browser and takes the positions the page posts. HTTPS because Safari
// gives a page its position only over a secure connection; the certificate is the Pi's own,
// signed by a private certificate authority that the phone trusts once (A27).
//
//   GET  /          the page (resources/phone_page.html)
//   GET  /ca.crt    the authority's certificate, for installing it on the phone
//   POST /position  {"latitude": d, "longitude": d, "accuracy": d}: 204, or 400 with the reason
//   anything else   404 (405 for a wrong method on a known path)
//
// One request per connection (Connection: close); a connection that has not sent a whole
// request within connectionTimeoutMilliseconds is closed. Lives on the thread that created it.
class PhoneLinkServer : public QObject {
    Q_OBJECT

public:
    struct Settings {
        QHostAddress address = QHostAddress::Any;
        quint16 port = 0;
        QString certificatePath;
        QString keyPath;
        QString caCertificatePath;
    };

    static constexpr int connectionTimeoutMilliseconds = 10000;
    static constexpr int maximumConnections = 8;

    explicit PhoneLinkServer(QObject* parent = nullptr);

    // Loads the certificate, its key and the authority's certificate, then listens. False with
    // the reason when a file cannot be read or the port cannot be opened.
    bool start(const Settings& settings, QString& error);
    [[nodiscard]] quint16 port() const;
    [[nodiscard]] std::uint64_t positionsAccepted() const;
    [[nodiscard]] std::uint64_t requestsRejected() const;

    // The answer to one request; pure, so the routes are tested without TLS.
    static PhoneLinkResponse
    respond(const HttpRequest& request, const QByteArray& page, const QByteArray& caCertificate);
    static PhoneLinkResponse
    response(int statusCode, const QByteArray& contentType, const QByteArray& body);
    // The page from the resources.
    static QByteArray page();

signals:
    void positionReceived(lexus_head_unit::PhonePosition position);

private:
    void acceptConnections();

    QSslServer m_server;
    QByteArray m_page;
    QByteArray m_caCertificate;
    int m_openConnections = 0;
    std::uint64_t m_positionsAccepted = 0;
    std::uint64_t m_requestsRejected = 0;
};

} // namespace lexus_head_unit
