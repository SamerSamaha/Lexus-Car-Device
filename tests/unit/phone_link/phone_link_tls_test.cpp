// Verifies: REQ-029

#include "lexus_head_unit/phone_link/phone_link_server.h"
#include "lexus_head_unit/phone_link/phone_position.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QHostAddress>
#include <QIODevice>
#include <QProcess>
#include <QSignalSpy>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslError>
#include <QSslSocket>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

#include <gtest/gtest.h>

// Qt declares its macros in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace {

using lexus_head_unit::PhoneLinkServer;
using lexus_head_unit::PhonePosition;

constexpr int waitMilliseconds = 10000;

// The phone link served with certificates made by the product script
// (deploy/car/phone_link_certificates.sh), and a client that trusts only that authority and
// checks the host name 127.0.0.1, as the phone checks the Pi's address.
class PhoneLinkTlsTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (opensslAvailable()) {
            const QString script = QStringLiteral(LEXUS_HEAD_UNIT_REPOSITORY_ROOT
                                                  "/deploy/car/phone_link_certificates.sh");
            QProcess process;
            process.start(QStringLiteral("sh"), {script, m_folder.path()});
            ASSERT_TRUE(process.waitForFinished(waitMilliseconds));
            ASSERT_EQ(process.exitCode(), 0) << process.readAllStandardError().toStdString();
            m_haveCertificates = true;
        }
    }

    static bool opensslAvailable() {
        QProcess probe;
        probe.start(QStringLiteral("openssl"), {QStringLiteral("version")});
        return probe.waitForFinished(waitMilliseconds) && probe.exitCode() == 0;
    }

    [[nodiscard]] QString file(const char* name) const {
        return m_folder.filePath(QString::fromLatin1(name));
    }

    bool startServer(PhoneLinkServer& server) {
        PhoneLinkServer::Settings settings;
        settings.address = QHostAddress::LocalHost;
        settings.port = 0;
        settings.certificatePath = file("server.crt");
        settings.keyPath = file("server.key");
        settings.caCertificatePath = file("ca.crt");
        QString error;
        const bool started = server.start(settings, error);
        EXPECT_TRUE(started) << error.toStdString();
        return started;
    }

    // Sends one request over TLS and returns the whole reply (the server closes after it).
    QByteArray exchange(quint16 port, const QByteArray& request) {
        QSslSocket socket;
        QSslConfiguration configuration = socket.sslConfiguration();
        QFile authority(file("ca.crt"));
        EXPECT_TRUE(authority.open(QIODevice::ReadOnly));
        configuration.setCaCertificates({QSslCertificate(authority.readAll())});
        configuration.setPeerVerifyMode(QSslSocket::VerifyPeer);
        socket.setSslConfiguration(configuration);
        QList<QSslError> errors;
        QObject::connect(
            &socket, &QSslSocket::sslErrors, [&errors](const QList<QSslError>& reported) {
                errors = reported;
            });
        socket.connectToHostEncrypted(QStringLiteral("127.0.0.1"), port);
        QByteArray reply;
        QElapsedTimer elapsed;
        elapsed.start();
        while (!socket.isEncrypted() && elapsed.elapsed() < waitMilliseconds &&
               socket.state() != QAbstractSocket::UnconnectedState) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        }
        EXPECT_TRUE(socket.isEncrypted()) << "TLS handshake with the product certificates";
        EXPECT_TRUE(errors.isEmpty()) << errors.value(0).errorString().toStdString();
        socket.write(request);
        while (socket.state() != QAbstractSocket::UnconnectedState &&
               elapsed.elapsed() < waitMilliseconds) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            reply += socket.readAll();
        }
        reply += socket.readAll();
        return reply;
    }

    QTemporaryDir m_folder;
    bool m_haveCertificates = false;
};

TEST_F(PhoneLinkTlsTest, ThePageIsServedOverTlsWithTheProductCertificates) {
    if (!m_haveCertificates) {
        GTEST_SKIP() << "openssl is not installed";
    }
    PhoneLinkServer server;
    ASSERT_TRUE(startServer(server));
    const QByteArray reply = exchange(server.port(), "GET / HTTP/1.1\r\nHost: pi\r\n\r\n");
    EXPECT_TRUE(reply.startsWith("HTTP/1.1 200 OK")) << reply.left(80).toStdString();
    EXPECT_TRUE(reply.contains("watchPosition"));
}

QByteArray positionRequest(const QByteArray& body) {
    return "POST /position HTTP/1.1\r\nHost: pi\r\nContent-Length: " +
           QByteArray::number(body.size()) + "\r\n\r\n" + body;
}

TEST_F(PhoneLinkTlsTest, APostedPositionIsReceived) {
    if (!m_haveCertificates) {
        GTEST_SKIP() << "openssl is not installed";
    }
    PhoneLinkServer server;
    ASSERT_TRUE(startServer(server));
    const QSignalSpy received(&server, &PhoneLinkServer::positionReceived);
    const QByteArray reply = exchange(
        server.port(), positionRequest(R"({"latitude":42.5,"longitude":-83.25,"accuracy":4})"));
    EXPECT_TRUE(reply.startsWith("HTTP/1.1 204"));
    ASSERT_EQ(received.count(), 1);
    const auto position = received.at(0).at(0).value<PhonePosition>();
    EXPECT_DOUBLE_EQ(position.latitudeDegrees, 42.5);
    EXPECT_DOUBLE_EQ(position.longitudeDegrees, -83.25);
    EXPECT_EQ(server.positionsAccepted(), 1U);
}

TEST_F(PhoneLinkTlsTest, ABadPositionIsRefusedAndTheAuthorityIsServed) {
    if (!m_haveCertificates) {
        GTEST_SKIP() << "openssl is not installed";
    }
    PhoneLinkServer server;
    ASSERT_TRUE(startServer(server));
    const QSignalSpy received(&server, &PhoneLinkServer::positionReceived);
    EXPECT_TRUE(exchange(server.port(), positionRequest("{}")).startsWith("HTTP/1.1 400"));
    EXPECT_EQ(received.count(), 0);
    EXPECT_EQ(server.requestsRejected(), 1U);
    EXPECT_TRUE(exchange(server.port(), "GET /ca.crt HTTP/1.1\r\nHost: pi\r\n\r\n")
                    .contains("-----BEGIN CERTIFICATE-----"));
}

TEST_F(PhoneLinkTlsTest, MissingCertificatesStopTheServerWithAReason) {
    PhoneLinkServer server;
    PhoneLinkServer::Settings settings;
    settings.certificatePath = QStringLiteral("/nonexistent/server.crt");
    QString error;
    EXPECT_FALSE(server.start(settings, error));
    EXPECT_FALSE(error.isEmpty());
}

QByteArray contentsOf(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

TEST_F(PhoneLinkTlsTest, TheScriptRunAgainKeepsTheAuthority) {
    if (!m_haveCertificates) {
        GTEST_SKIP() << "openssl is not installed";
    }
    const QByteArray before = contentsOf(file("ca.crt"));
    QProcess again;
    again.start(
        QStringLiteral("sh"),
        {QStringLiteral(LEXUS_HEAD_UNIT_REPOSITORY_ROOT "/deploy/car/phone_link_certificates.sh"),
         m_folder.path()});
    ASSERT_TRUE(again.waitForFinished(waitMilliseconds));
    EXPECT_EQ(again.exitCode(), 0);
    EXPECT_TRUE(again.readAllStandardOutput().contains("server certificate kept"));
    EXPECT_FALSE(before.isEmpty());
    EXPECT_EQ(contentsOf(file("ca.crt")), before);
}

TEST_F(PhoneLinkTlsTest, TheKeysAreReadableByTheUserOnly) {
    if (!m_haveCertificates) {
        GTEST_SKIP() << "openssl is not installed";
    }
    const auto keyPermissions = QFile(file("server.key")).permissions();
    EXPECT_FALSE(keyPermissions.testFlag(QFileDevice::ReadOther));
    EXPECT_FALSE(keyPermissions.testFlag(QFileDevice::ReadGroup));
}

} // namespace

int main(int argumentCount, char** argumentValues) {
    const QCoreApplication application(argumentCount, argumentValues);
    ::testing::InitGoogleTest(&argumentCount, argumentValues);
    return RUN_ALL_TESTS();
}
// NOLINTEND(misc-include-cleaner)
