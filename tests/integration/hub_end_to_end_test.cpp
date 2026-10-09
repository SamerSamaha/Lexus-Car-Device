// Verifies: REQ-016

#include "lexus_head_unit/hub/hub_control_server.h"

#include <QCoreApplication>
#include <QMap>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QThread>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <unistd.h>

// Qt and glibc declare their symbols in internal headers; the public ones are included.
// NOLINTBEGIN(misc-include-cleaner)
namespace {

constexpr int cycleCount = 20;
constexpr std::int64_t appRunMilliseconds = 100;
constexpr std::int64_t visibleBudgetMilliseconds = 1000;
constexpr int commandTimeoutMilliseconds = 2000;
constexpr int startupTimeoutMilliseconds = 15000;

using StatusFields = QMap<QString, QString>;

std::int64_t millisecondsSince(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                 start)
        .count();
}

class HubEndToEndTest : public ::testing::Test {
protected:
    void SetUp() override {
        const std::string suffix = std::to_string(::getpid());
        m_socketName = QStringLiteral("lexus-hub-test-") + QString::fromStdString(suffix);
        m_registryPath = "/tmp/lexus_hub_test_registry_" + suffix + ".conf";
        std::ofstream registry(m_registryPath);
        registry << "[hub]\napps = fake, forever\nstop_grace_ms = 500\n\n"
                 << "[app.fake]\nname = Fake\nkind = native\ncommand = "
                 << LEXUS_HEAD_UNIT_FAKE_APP_PATH << " --run-ms " << appRunMilliseconds << "\n\n"
                 << "[app.forever]\nname = Forever\nkind = native\ncommand = "
                 << LEXUS_HEAD_UNIT_FAKE_APP_PATH << " --forever\n";
        registry.close();

        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        m_hub.setProcessEnvironment(environment);
        m_hub.setProcessChannelMode(QProcess::ForwardedErrorChannel);
        m_hub.start(QStringLiteral(LEXUS_HEAD_UNIT_HUB_PATH),
                    {QStringLiteral("--registry"),
                     QString::fromStdString(m_registryPath),
                     QStringLiteral("--control-socket"),
                     m_socketName});
        ASSERT_TRUE(m_hub.waitForStarted(startupTimeoutMilliseconds));
        const auto start = std::chrono::steady_clock::now();
        while (millisecondsSince(start) < startupTimeoutMilliseconds) {
            bool answered = false;
            lexus_head_unit::sendHubCommand(m_socketName, QStringLiteral("status"), 200, answered);
            if (answered) {
                return;
            }
            QThread::msleep(50);
        }
        FAIL() << "the hub did not answer on its control socket";
    }

    void TearDown() override {
        if (m_hub.state() != QProcess::NotRunning) {
            m_hub.kill();
            m_hub.waitForFinished(startupTimeoutMilliseconds);
        }
        static_cast<void>(std::remove(m_registryPath.c_str()));
    }

    QString send(const QString& command) {
        bool answered = false;
        const QString reply = lexus_head_unit::sendHubCommand(
            m_socketName, command, commandTimeoutMilliseconds, answered);
        EXPECT_TRUE(answered) << command.toStdString() << ": " << reply.toStdString();
        return reply;
    }

    StatusFields status() {
        const QStringList words = send(QStringLiteral("status")).split(QLatin1Char(' '));
        StatusFields fields;
        for (int index = 0; index + 1 < words.size(); index += 2) {
            fields.insert(words.at(index), words.at(index + 1));
        }
        return fields;
    }

    // Polls the status every 10 ms until the hub reports no foreground app; the elapsed time.
    std::int64_t waitUntilIdle(std::int64_t limitMilliseconds) {
        const auto start = std::chrono::steady_clock::now();
        while (millisecondsSince(start) < limitMilliseconds) {
            if (status().value(QStringLiteral("state")) == QStringLiteral("idle")) {
                return millisecondsSince(start);
            }
            QThread::msleep(10);
        }
        return millisecondsSince(start);
    }

    // One launch of the fake app through the socket; checks the idle status and returns the
    // milliseconds from the launch reply until the hub reported idle with a visible window.
    std::int64_t runOneCycle(int cycle, const QString& hubProcessId) {
        EXPECT_EQ(send(QStringLiteral("launch fake")), QStringLiteral("launch started"))
            << "cycle " << cycle;
        const std::int64_t elapsed =
            waitUntilIdle(appRunMilliseconds + visibleBudgetMilliseconds + 1000);
        const StatusFields fields = status();
        const QStringList observed = {fields.value(QStringLiteral("state")),
                                      fields.value(QStringLiteral("window")),
                                      fields.value(QStringLiteral("hub_pid"))};
        const QStringList expected = {
            QStringLiteral("idle"), QStringLiteral("visible"), hubProcessId};
        EXPECT_EQ(observed, expected) << "cycle " << cycle;
        EXPECT_LE(elapsed, appRunMilliseconds + visibleBudgetMilliseconds) << "cycle " << cycle;
        return elapsed;
    }

    QProcess m_hub;
    QString m_socketName;
    std::string m_registryPath;
};

TEST_F(HubEndToEndTest, TwentyCyclesHubVisibleWithinOneSecondAndProcessIdUnchanged) {
    const QString hubProcessId = QString::number(m_hub.processId());
    std::vector<std::int64_t> elapsedPerCycle;
    elapsedPerCycle.reserve(cycleCount);
    for (int cycle = 0; cycle < cycleCount; ++cycle) {
        elapsedPerCycle.push_back(runOneCycle(cycle, hubProcessId));
    }
    std::sort(elapsedPerCycle.begin(), elapsedPerCycle.end());
    std::cout << "launch to idle with a visible window (ms, " << cycleCount << " cycles, app runs "
              << appRunMilliseconds << " ms): min " << elapsedPerCycle.front() << " median "
              << elapsedPerCycle.at(cycleCount / 2) << " max " << elapsedPerCycle.back() << "\n";
    const StatusFields fields = status();
    EXPECT_EQ(fields.value(QStringLiteral("launches")), QString::number(cycleCount));
    EXPECT_EQ(fields.value(QStringLiteral("exits")), QString::number(cycleCount));
    EXPECT_EQ(m_hub.state(), QProcess::Running);
}

TEST_F(HubEndToEndTest, ReturnStopsTheForegroundAppAndBadCommandsChangeNothing) {
    EXPECT_EQ(send(QStringLiteral("return")), QStringLiteral("return nothing-running"));
    EXPECT_EQ(send(QStringLiteral("launch absent")), QStringLiteral("launch unknown app"));
    EXPECT_EQ(send(QStringLiteral("dance")), QStringLiteral("error unknown command"));
    ASSERT_EQ(send(QStringLiteral("launch forever")), QStringLiteral("launch started"));
    EXPECT_EQ(send(QStringLiteral("launch fake")), QStringLiteral("launch already-running"));
    StatusFields fields = status();
    EXPECT_EQ(fields.value(QStringLiteral("state")), QStringLiteral("running"));
    EXPECT_EQ(fields.value(QStringLiteral("app")), QStringLiteral("forever"));
    EXPECT_EQ(send(QStringLiteral("return")), QStringLiteral("return stopping"));
    EXPECT_LE(waitUntilIdle(2000), visibleBudgetMilliseconds);
    fields = status();
    EXPECT_EQ(fields.value(QStringLiteral("app")), QStringLiteral("-"));
    EXPECT_EQ(fields.value(QStringLiteral("kills")), QStringLiteral("0"));
}

TEST_F(HubEndToEndTest, TermToTheHubStopsTheForegroundAppFirst) {
    ASSERT_EQ(send(QStringLiteral("launch forever")), QStringLiteral("launch started"));
    const QString appProcessId = status().value(QStringLiteral("app_pid"));
    ASSERT_NE(appProcessId, QStringLiteral("-"));
    m_hub.terminate();
    ASSERT_TRUE(m_hub.waitForFinished(startupTimeoutMilliseconds));
    EXPECT_EQ(m_hub.exitStatus(), QProcess::NormalExit);
    EXPECT_EQ(m_hub.exitCode(), 0);
    const std::ifstream stat("/proc/" + appProcessId.toStdString() + "/stat");
    EXPECT_FALSE(stat.good()) << "the app outlived the hub";
}

TEST_F(HubEndToEndTest, SecondHubOnTheSameSocketRefusesToStart) {
    QProcess second;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    second.setProcessEnvironment(environment);
    second.start(QStringLiteral(LEXUS_HEAD_UNIT_HUB_PATH),
                 {QStringLiteral("--registry"),
                  QString::fromStdString(m_registryPath),
                  QStringLiteral("--control-socket"),
                  m_socketName});
    ASSERT_TRUE(second.waitForFinished(startupTimeoutMilliseconds));
    EXPECT_EQ(second.exitCode(), 2);
    EXPECT_EQ(m_hub.state(), QProcess::Running);
    EXPECT_EQ(status().value(QStringLiteral("state")), QStringLiteral("idle"));
}

} // namespace

int main(int argumentCount, char** argumentValues) {
    const QCoreApplication application(argumentCount, argumentValues);
    ::testing::InitGoogleTest(&argumentCount, argumentValues);
    return RUN_ALL_TESTS();
}
// NOLINTEND(misc-include-cleaner)
