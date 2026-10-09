// Verifies: REQ-017, REQ-002, REQ-021

#include "lexus_head_unit/service/connection_state_machine.h"
#include "lexus_head_unit/service/diagnostics_report.h"
#include "lexus_head_unit/service/signal_definition.h"
#include "lexus_head_unit/service/signal_id.h"
#include "lexus_head_unit/service/signal_sample.h"
#include "lexus_head_unit/service_dbus/dbus_names.h"
#include "lexus_head_unit/service_dbus/vehicle_data_client.h"
#include "lexus_head_unit/service_dbus/vehicle_data_service.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QXmlStreamReader>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

// Qt declares its macros and types in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace {

using lexus_head_unit::allSignalIds;
using lexus_head_unit::ConnectionState;
using lexus_head_unit::ConnectionTransition;
using lexus_head_unit::ConnectionTrigger;
using lexus_head_unit::definitionOf;
using lexus_head_unit::SignalId;
using lexus_head_unit::SignalSample;
using lexus_head_unit::SignalStatus;
using lexus_head_unit::VehicleDataClient;
using lexus_head_unit::VehicleDataService;
using Clock = std::chrono::steady_clock;
using SampleFields =
    std::tuple<SignalId, double, lexus_head_unit::Unit, std::int64_t, SignalStatus>;

constexpr int publishedSampleCount = 1000;
constexpr int lateJoinAfterSamples = 100;
constexpr std::int64_t lateJoinBudgetMilliseconds = 500;
constexpr int waitLimitMilliseconds = 10000;

SampleFields fieldsOf(const SignalSample& sample) {
    return {
        sample.signalId, sample.value, sample.unit, sample.timestampMilliseconds, sample.status};
}

// The sample number n of a test run: signals in turn, the value and the timestamp both n.
SignalSample numberedSample(int number) {
    SignalSample sample;
    sample.signalId = allSignalIds.at(static_cast<std::size_t>(number) % allSignalIds.size());
    sample.value = number;
    sample.unit = definitionOf(sample.signalId).unit;
    sample.timestampMilliseconds = 1000 + number;
    sample.status = SignalStatus::Valid;
    return sample;
}

std::int64_t millisecondsSince(Clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
}

// Runs the event loop until the condition holds or the limit passes; true if it held.
bool waitFor(const std::function<bool()>& condition,
             int limitMilliseconds = waitLimitMilliseconds) {
    const auto start = Clock::now();
    while (!condition()) {
        if (millisecondsSince(start) > limitMilliseconds) {
            return false;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }
    return true;
}

// A dbus-daemon of the test's own, so that no test touches the desk's or the runner's bus.
class PrivateBus {
public:
    PrivateBus() {
        m_daemon.start(QStringLiteral("dbus-daemon"),
                       {QStringLiteral("--session"),
                        QStringLiteral("--nofork"),
                        QStringLiteral("--print-address")});
        if (m_daemon.waitForStarted() && m_daemon.waitForReadyRead()) {
            m_address = QString::fromUtf8(m_daemon.readLine()).trimmed();
        }
    }

    PrivateBus(const PrivateBus&) = delete;
    PrivateBus& operator=(const PrivateBus&) = delete;
    PrivateBus(PrivateBus&&) = delete;
    PrivateBus& operator=(PrivateBus&&) = delete;

    ~PrivateBus() {
        m_daemon.kill();
        m_daemon.waitForFinished();
    }

    [[nodiscard]] const QString& address() const {
        return m_address;
    }

    QDBusConnection connect(const QString& name) {
        m_names.append(name);
        return QDBusConnection::connectToBus(m_address, name);
    }

    void disconnectAll() {
        for (const QString& name : m_names) {
            QDBusConnection::disconnectFromBus(name);
        }
        m_names.clear();
    }

private:
    QProcess m_daemon;
    QString m_address;
    QStringList m_names;
};

// A client together with everything it emitted.
struct RecordingClient {
    explicit RecordingClient(QDBusConnection connection) : client(std::move(connection)) {
        QObject::connect(
            &client, &VehicleDataClient::sampleArrived, [this](const SignalSample& sample) {
                samples.push_back(fieldsOf(sample));
                arrivals.push_back(Clock::now());
            });
        QObject::connect(&client,
                         &VehicleDataClient::connectionChanged,
                         [this](const ConnectionTransition& transition) {
                             transitions.push_back(transition);
                         });
        client.start();
    }

    void clear() {
        samples.clear();
        arrivals.clear();
        transitions.clear();
    }

    VehicleDataClient client;
    std::vector<SampleFields> samples;
    std::vector<Clock::time_point> arrivals;
    std::vector<ConnectionTransition> transitions;
};

class VehicleDataDBusTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_FALSE(m_bus.address().isEmpty()) << "dbus-daemon did not start";
        m_service = std::make_unique<VehicleDataService>();
        QString error;
        ASSERT_TRUE(m_service->registerOn(m_bus.connect(QStringLiteral("service")), error))
            << error.toStdString();
    }

    void TearDown() override {
        m_service.reset();
        m_bus.disconnectAll();
    }

    std::unique_ptr<RecordingClient> connectedClient(const QString& name) {
        auto recording = std::make_unique<RecordingClient>(m_bus.connect(name));
        EXPECT_TRUE(waitFor([&recording]() {
            return recording->client.hasInitialState();
        })) << name.toStdString();
        return recording;
    }

    PrivateBus m_bus;
    std::unique_ptr<VehicleDataService> m_service;
};

void printLatency(std::vector<double> microseconds) {
    std::sort(microseconds.begin(), microseconds.end());
    const auto percentile = [&microseconds](double fraction) {
        const auto index =
            static_cast<std::size_t>(fraction * static_cast<double>(microseconds.size() - 1));
        return microseconds.at(index) / 1000.0;
    };
    std::cout << std::fixed << std::setprecision(3) << "D-Bus hop, publish to client ("
              << microseconds.size() << " samples, ms): min " << percentile(0.0) << " median "
              << percentile(0.5) << " p95 " << percentile(0.95) << " p99 " << percentile(0.99)
              << " max " << percentile(1.0) << "\n";
}

TEST_F(VehicleDataDBusTest, TwoClientsReceiveIdenticalSequencesOfAThousandSamples) {
    const auto first = connectedClient(QStringLiteral("client1"));
    const auto second = connectedClient(QStringLiteral("client2"));
    first->clear();
    second->clear();
    std::vector<Clock::time_point> published(publishedSampleCount);
    std::vector<SampleFields> expected;
    expected.reserve(publishedSampleCount);
    for (int number = 0; number < publishedSampleCount; ++number) {
        expected.push_back(fieldsOf(numberedSample(number)));
    }
    // Published from another thread, as the worker loop does, one sample per millisecond (a
    // faster rate than the adapter's), while this thread runs the event loop that sends them.
    std::thread publisher([this, &published]() {
        for (int number = 0; number < publishedSampleCount; ++number) {
            published.at(static_cast<std::size_t>(number)) = Clock::now();
            m_service->publishSample(numberedSample(number));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    const bool allArrived = waitFor([&first, &second]() {
        return first->samples.size() >= publishedSampleCount &&
               second->samples.size() >= publishedSampleCount;
    });
    publisher.join();
    ASSERT_TRUE(allArrived);
    EXPECT_EQ(first->samples, expected);
    EXPECT_EQ(second->samples, expected);
    EXPECT_EQ(first->client.malformedMessages(), 0U);
    std::vector<double> hopMicroseconds;
    hopMicroseconds.reserve(publishedSampleCount);
    for (std::size_t index = 0; index < first->arrivals.size(); ++index) {
        hopMicroseconds.push_back(
            static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(
                                    first->arrivals.at(index) - published.at(index))
                                    .count()));
    }
    printLatency(hopMicroseconds);
}

TEST_F(VehicleDataDBusTest, LateClientHoldsEverySignalWithinHalfASecond) {
    std::array<SampleFields, lexus_head_unit::signalCount> lastPublished{};
    for (int number = 0; number < lateJoinAfterSamples; ++number) {
        const SignalSample sample = numberedSample(number);
        lastPublished.at(lexus_head_unit::indexOf(sample.signalId)) = fieldsOf(sample);
        m_service->publishSample(sample);
    }
    ASSERT_TRUE(waitFor([this]() {
        return m_service->publishedSampleCount() >= lateJoinAfterSamples;
    }));
    const auto start = Clock::now();
    RecordingClient late(m_bus.connect(QStringLiteral("late")));
    const auto complete = [&late, &lastPublished]() {
        return late.client.hasInitialState() &&
               std::all_of(allSignalIds.begin(), allSignalIds.end(), [&](SignalId signalId) {
                   return fieldsOf(late.client.latest(signalId)) ==
                          lastPublished.at(lexus_head_unit::indexOf(signalId));
               });
    };
    ASSERT_TRUE(waitFor(complete, 2000));
    const std::int64_t elapsed = millisecondsSince(start);
    std::cout << "late client complete after " << elapsed << " ms\n";
    EXPECT_LE(elapsed, lateJoinBudgetMilliseconds);
}

TEST_F(VehicleDataDBusTest, TransitionsReachClientsAndALateClientGetsTheCurrentState) {
    const auto early = connectedClient(QStringLiteral("early"));
    early->clear();
    m_service->publishTransition(ConnectionTransition{ConnectionState::Disconnected,
                                                      ConnectionTrigger::StartRequested,
                                                      ConnectionState::Connecting,
                                                      10});
    m_service->publishTransition(ConnectionTransition{ConnectionState::Connecting,
                                                      ConnectionTrigger::HandshakeSucceeded,
                                                      ConnectionState::Connected,
                                                      20});
    ASSERT_TRUE(waitFor([&early]() {
        return early->transitions.size() == 2;
    }));
    EXPECT_EQ(early->client.connectionState(), ConnectionState::Connected);
    EXPECT_EQ(early->transitions.back().trigger, ConnectionTrigger::HandshakeSucceeded);
    const auto late = connectedClient(QStringLiteral("late"));
    EXPECT_EQ(late->client.connectionState(), ConnectionState::Connected);
    ASSERT_FALSE(late->transitions.empty());
    EXPECT_EQ(late->transitions.back().timestampMilliseconds, 20);
}

TEST_F(VehicleDataDBusTest, ServiceGoneMeansStaleAndErrorAndServiceBackMeansCompleteAgain) {
    const auto watcher = connectedClient(QStringLiteral("watcher"));
    m_service->publishTransition(ConnectionTransition{ConnectionState::Connecting,
                                                      ConnectionTrigger::HandshakeSucceeded,
                                                      ConnectionState::Connected,
                                                      5});
    m_service->publishSample(numberedSample(0));
    ASSERT_TRUE(waitFor([&watcher]() {
        return watcher->client.latest(SignalId::VehicleSpeed).status == SignalStatus::Valid &&
               watcher->client.connectionState() == ConnectionState::Connected;
    }));
    m_service.reset();
    QDBusConnection::disconnectFromBus(QStringLiteral("service"));
    ASSERT_TRUE(waitFor([&watcher]() {
        return !watcher->client.isServiceAvailable();
    }));
    EXPECT_EQ(watcher->client.latest(SignalId::VehicleSpeed).status, SignalStatus::Stale);
    EXPECT_EQ(watcher->client.connectionState(), ConnectionState::Error);
    EXPECT_EQ(watcher->transitions.back().trigger, ConnectionTrigger::LinkLost);

    m_service = std::make_unique<VehicleDataService>();
    QString error;
    ASSERT_TRUE(m_service->registerOn(m_bus.connect(QStringLiteral("service2")), error))
        << error.toStdString();
    m_service->publishSample(numberedSample(static_cast<int>(lexus_head_unit::signalCount)));
    ASSERT_TRUE(waitFor([&watcher]() {
        return watcher->client.isServiceAvailable() &&
               watcher->client.latest(SignalId::VehicleSpeed).timestampMilliseconds ==
                   1000 + static_cast<std::int64_t>(lexus_head_unit::signalCount);
    }));
    EXPECT_EQ(watcher->client.latest(SignalId::VehicleSpeed).status, SignalStatus::Valid);
}

TEST_F(VehicleDataDBusTest, SecondServiceCannotTakeTheName) {
    VehicleDataService second;
    QString error;
    EXPECT_FALSE(second.registerOn(m_bus.connect(QStringLiteral("second")), error));
    EXPECT_TRUE(error.contains(lexus_head_unit::dbus_names::serviceName())) << error.toStdString();
}

// "method GetSamples out a(uduxu)" style lines for one interface in an introspection document.
QSet<QString> membersOf(const QString& xml, const QString& interfaceName) {
    QSet<QString> members;
    QXmlStreamReader reader(xml);
    bool inInterface = false;
    QString current;
    while (!reader.atEnd()) {
        reader.readNext();
        const QString name = reader.name().toString();
        if (reader.isStartElement() && name == QStringLiteral("interface")) {
            inInterface = reader.attributes().value(QStringLiteral("name")) == interfaceName;
        } else if (inInterface && reader.isStartElement() &&
                   (name == QStringLiteral("method") || name == QStringLiteral("signal"))) {
            current = name + QLatin1Char(' ') +
                      reader.attributes().value(QStringLiteral("name")).toString();
        } else if (inInterface && reader.isStartElement() && name == QStringLiteral("arg")) {
            const QString direction =
                reader.attributes().value(QStringLiteral("direction")).toString();
            current +=
                QLatin1Char(' ') + (direction.isEmpty() ? QStringLiteral("out") : direction) +
                QLatin1Char(' ') + reader.attributes().value(QStringLiteral("type")).toString();
        } else if (inInterface && reader.isEndElement() &&
                   (name == QStringLiteral("method") || name == QStringLiteral("signal"))) {
            members.insert(current);
        } else if (reader.isEndElement() && name == QStringLiteral("interface")) {
            inInterface = false;
        }
    }
    return members;
}

lexus_head_unit::DiagnosticsReport twoCodeReport() {
    lexus_head_unit::DiagnosticsReport report;
    report.codesRead = true;
    report.troubleCodes = {{"P0133", "Oxygen sensor slow response, bank 1 sensor 1"},
                           {"U0100", "Lost communication with the engine control module"}};
    report.identificationRead = true;
    report.vehicleIdentification = "DEMO-NOT-A-VIN";
    report.timestampMilliseconds = 1234;
    return report;
}

void expectTheTwoCodeReport(const lexus_head_unit::DiagnosticsReport& arrived) {
    EXPECT_TRUE(arrived.codesRead);
    ASSERT_EQ(arrived.troubleCodes.size(), 2U);
    EXPECT_EQ(arrived.troubleCodes.at(1).code, "U0100");
    EXPECT_EQ(arrived.troubleCodes.at(1).description,
              "Lost communication with the engine control module");
    EXPECT_EQ(arrived.vehicleIdentification, "DEMO-NOT-A-VIN");
    EXPECT_EQ(arrived.timestampMilliseconds, 1234);
}

TEST_F(VehicleDataDBusTest, DiagnosticsRequestReachesTheSourceAndTheReportReachesEveryClient) {
    int requests = 0;
    m_service->setDiagnosticsRequester([&requests]() {
        ++requests;
    });
    auto first = connectedClient(QStringLiteral("diagnostics-first"));
    auto second = connectedClient(QStringLiteral("diagnostics-second"));
    std::vector<lexus_head_unit::DiagnosticsReport> received;
    QObject::connect(&second->client,
                     &VehicleDataClient::diagnosticsArrived,
                     [&received](const lexus_head_unit::DiagnosticsReport& report) {
                         received.push_back(report);
                     });

    first->client.requestDiagnostics();
    ASSERT_TRUE(waitFor([&requests]() {
        return requests == 1;
    }));

    const lexus_head_unit::DiagnosticsReport report = twoCodeReport();
    std::thread publisher([this, &report]() {
        m_service->publishDiagnostics(report);
    });
    publisher.join();
    ASSERT_TRUE(waitFor([&received]() {
        return !received.empty();
    }));
    expectTheTwoCodeReport(received.front());
    EXPECT_TRUE(waitFor([&first]() {
        return first->client.latestDiagnostics().codesRead;
    }));

    // A client that joins later reads the last report without a new request.
    auto late = connectedClient(QStringLiteral("diagnostics-late"));
    EXPECT_TRUE(waitFor([&late]() {
        return late->client.latestDiagnostics().troubleCodes.size() == 2U;
    }));
    EXPECT_EQ(requests, 1);
}

TEST_F(VehicleDataDBusTest, LiveIntrospectionMatchesTheCommittedInterfaceFile) {
    const QDBusConnection connection = m_bus.connect(QStringLiteral("introspector"));
    // Asynchronous: the service object answers from this same thread's event loop.
    QDBusPendingCall pending = connection.asyncCall(
        QDBusMessage::createMethodCall(lexus_head_unit::dbus_names::serviceName(),
                                       lexus_head_unit::dbus_names::objectPath(),
                                       QStringLiteral("org.freedesktop.DBus.Introspectable"),
                                       QStringLiteral("Introspect")));
    ASSERT_TRUE(waitFor([&pending]() {
        return pending.isFinished();
    }));
    const QDBusMessage reply = pending.reply();
    ASSERT_EQ(reply.type(), QDBusMessage::ReplyMessage);
    QFile file(QStringLiteral(LEXUS_HEAD_UNIT_INTERFACE_XML));
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    const QString committed = QString::fromUtf8(file.readAll());
    const QString live = reply.arguments().value(0).toString();
    const QSet<QString> expected =
        membersOf(committed, lexus_head_unit::dbus_names::interfaceName());
    EXPECT_EQ(expected.size(), 8);
    EXPECT_EQ(membersOf(live, lexus_head_unit::dbus_names::interfaceName()), expected);
}

// Starts a second copy of the service on the same bus and returns its exit code.
int exitCodeOfSecondService(const QStringList& arguments) {
    QProcess second;
    second.start(QStringLiteral(LEXUS_HEAD_UNIT_SERVICE_PATH), arguments);
    return second.waitForFinished(5000) ? second.exitCode() : -1;
}

// SIGTERM, then the exit code of a normal exit, or -1 for a crash or a timeout.
int terminateAndGetExitCode(QProcess& process) {
    process.terminate();
    if (!process.waitForFinished(5000) || process.exitStatus() != QProcess::NormalExit) {
        return -1;
    }
    return process.exitCode();
}

// End to end: the service executable with the fake source, on the private bus.
TEST(VehicleDataServiceProcessTest, ServiceExecutablePublishesEverySignalAndRefusesASecondCopy) {
    PrivateBus bus;
    ASSERT_FALSE(bus.address().isEmpty());
    const QStringList arguments = {QStringLiteral("--source"),
                                   QStringLiteral("fake"),
                                   QStringLiteral("--config"),
                                   QStringLiteral("/nonexistent/head_unit.conf"),
                                   QStringLiteral("--bus-address"),
                                   bus.address()};
    QProcess service;
    service.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    service.start(QStringLiteral(LEXUS_HEAD_UNIT_SERVICE_PATH), arguments);
    ASSERT_TRUE(service.waitForStarted());
    RecordingClient watcher(bus.connect(QStringLiteral("watcher")));
    // Every measured signal from the demo source, and the derived signals that need no more
    // than a few seconds of demo driving (the trip average needs 0.1 km, the warm-up 30 s).
    const auto everySignalValid = [&watcher]() {
        const auto valid = [&watcher](SignalId signalId) {
            return watcher.client.latest(signalId).status == SignalStatus::Valid;
        };
        return watcher.client.connectionState() == ConnectionState::Connected &&
               std::all_of(lexus_head_unit::measuredSignalIds.begin(),
                           lexus_head_unit::measuredSignalIds.end(),
                           valid) &&
               valid(SignalId::InstantFuelEconomy) && valid(SignalId::TripDistance) &&
               valid(SignalId::TimeBelow1000Rpm);
    };
    EXPECT_TRUE(waitFor(everySignalValid, 5000));
    EXPECT_EQ(exitCodeOfSecondService(arguments), 2);
    EXPECT_EQ(terminateAndGetExitCode(service), 0);
    EXPECT_TRUE(waitFor([&watcher]() {
        return !watcher.client.isServiceAvailable() &&
               watcher.client.connectionState() == ConnectionState::Error;
    }));
    bus.disconnectAll();
}

} // namespace

int main(int argumentCount, char** argumentValues) {
    const QCoreApplication application(argumentCount, argumentValues);
    ::testing::InitGoogleTest(&argumentCount, argumentValues);
    return RUN_ALL_TESTS();
}
// NOLINTEND(misc-include-cleaner)
