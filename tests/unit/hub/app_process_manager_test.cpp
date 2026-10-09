// Verifies: REQ-016

#include "fake_process_launcher.h"

#include "lexus_head_unit/hub/app_entry.h"
#include "lexus_head_unit/hub/app_process_manager.h"
#include "lexus_head_unit/hub/process_launcher.h"
#include "lexus_head_unit/service/manual_clock.h"

#include <gtest/gtest.h>

#include <csignal>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

// glibc declares the signal numbers in internal headers; <csignal> is the public one.
// NOLINTBEGIN(misc-include-cleaner)
namespace {

using lexus_head_unit::AppEntry;
using lexus_head_unit::AppExitCause;
using lexus_head_unit::AppExitReport;
using lexus_head_unit::AppManagerSettings;
using lexus_head_unit::AppProcessManager;
using lexus_head_unit::AppRunState;
using lexus_head_unit::describeExit;
using lexus_head_unit::LaunchResult;
using lexus_head_unit::ManualClock;
using lexus_head_unit::ProcessExit;
using lexus_head_unit::ProcessExitKind;
using lexus_head_unit::RestartPolicy;
using lexus_head_unit::toString;
using lexus_head_unit::testing::FakeProcessLauncher;

AppEntry entryNamed(const std::string& appId, RestartPolicy restart = RestartPolicy::Never) {
    AppEntry entry;
    entry.id = appId;
    entry.name = appId;
    entry.arguments = {"/usr/bin/" + appId, "--flag"};
    entry.restart = restart;
    return entry;
}

class AppProcessManagerTest : public ::testing::Test {
protected:
    AppProcessManagerTest() {
        m_manager.setExitListener([this](const AppExitReport& exitReport) {
            m_reports.push_back(exitReport);
        });
    }

    void endWith(ProcessExitKind kind, int code) {
        m_launcher.finish(m_launcher.lastProcessId(), ProcessExit{kind, code});
        m_manager.poll();
    }

    [[nodiscard]] AppExitReport lastReport() const {
        return m_reports.empty() ? AppExitReport{} : m_reports.back();
    }

    struct EndingCase {
        ProcessExitKind kind;
        int code;
        AppExitCause cause;
        int reportedCode;
        std::string text;
    };

    // Launches, ends the app the given way 250 ms later, and compares everything observable in
    // one tuple: cause, code, run time, restarting, text, state, foreground id.
    void checkEnding(const EndingCase& testCase) {
        ASSERT_EQ(m_manager.launch(entryNamed("app")), LaunchResult::Started);
        m_clock.advanceMilliseconds(250);
        endWith(testCase.kind, testCase.code);
        const AppExitReport exitReport = lastReport();
        const auto observed = std::make_tuple(exitReport.cause,
                                              exitReport.code,
                                              exitReport.runMilliseconds,
                                              exitReport.restarting,
                                              describeExit(exitReport, "App"),
                                              m_manager.runState(),
                                              m_manager.foregroundAppId());
        const auto expected = std::make_tuple(testCase.cause,
                                              testCase.reportedCode,
                                              std::int64_t{250},
                                              false,
                                              testCase.text,
                                              AppRunState::Idle,
                                              std::string());
        EXPECT_EQ(observed, expected) << testCase.text;
    }

    void crashAndExpectRestart() {
        m_clock.advanceMilliseconds(1000);
        endWith(ProcessExitKind::Signalled, SIGSEGV);
        EXPECT_TRUE(lastReport().restarting);
        EXPECT_EQ(m_manager.runState(), AppRunState::Running);
    }

    ManualClock m_clock{1000};
    FakeProcessLauncher m_launcher;
    AppProcessManager m_manager{m_launcher, m_clock};
    std::vector<AppExitReport> m_reports;
};

TEST_F(AppProcessManagerTest, LaunchStartsTheArgumentsAndTracksTheProcess) {
    EXPECT_EQ(m_manager.runState(), AppRunState::Idle);
    EXPECT_EQ(m_manager.foregroundProcessId(), std::nullopt);
    EXPECT_EQ(m_manager.launch(entryNamed("music")), LaunchResult::Started);
    EXPECT_EQ(m_manager.runState(), AppRunState::Running);
    EXPECT_EQ(m_manager.foregroundAppId(), "music");
    EXPECT_EQ(m_manager.foregroundProcessId(), m_launcher.lastProcessId());
    ASSERT_EQ(m_launcher.startedArguments().size(), 1U);
    EXPECT_EQ(m_launcher.startedArguments().front(),
              (std::vector<std::string>{"/usr/bin/music", "--flag"}));
    m_manager.poll();
    EXPECT_EQ(m_manager.runState(), AppRunState::Running);
    EXPECT_EQ(m_manager.counters().launches, 1U);
}

TEST_F(AppProcessManagerTest, SecondLaunchWhileRunningIsRefused) {
    ASSERT_EQ(m_manager.launch(entryNamed("music")), LaunchResult::Started);
    EXPECT_EQ(m_manager.launch(entryNamed("video")), LaunchResult::AlreadyRunning);
    EXPECT_EQ(m_manager.foregroundAppId(), "music");
    EXPECT_EQ(m_launcher.startedArguments().size(), 1U);
}

TEST_F(AppProcessManagerTest, EmptyCommandAndStartFailure) {
    AppEntry empty = entryNamed("empty");
    empty.arguments.clear();
    EXPECT_EQ(m_manager.launch(empty), LaunchResult::EmptyCommand);
    m_launcher.setFailStarts(true);
    EXPECT_EQ(m_manager.launch(entryNamed("missing")), LaunchResult::StartFailed);
    EXPECT_EQ(m_manager.runState(), AppRunState::Idle);
    EXPECT_EQ(m_manager.counters().startFailures, 1U);
    ASSERT_EQ(m_reports.size(), 1U);
    EXPECT_EQ(m_reports.front().cause, AppExitCause::StartFailed);
    EXPECT_EQ(describeExit(m_reports.front(), "Missing"), "Missing could not start");
}

TEST_F(AppProcessManagerTest, EachWayOfEndingGivesItsCauseAndRunTime) {
    const std::vector<EndingCase> cases = {
        {ProcessExitKind::Exited, 0, AppExitCause::CleanExit, 0, "App closed"},
        {ProcessExitKind::Exited, 3, AppExitCause::FailedExit, 3, "App failed (exit code 3)"},
        {ProcessExitKind::Signalled,
         SIGSEGV,
         AppExitCause::Signalled,
         SIGSEGV,
         "App ended by signal 11"},
        {ProcessExitKind::Lost, 0, AppExitCause::Lost, 0, "App was lost"},
    };
    for (const EndingCase& testCase : cases) {
        checkEnding(testCase);
    }
    EXPECT_EQ(m_manager.counters().exits, cases.size());
    EXPECT_EQ(m_manager.lastExit().value_or(AppExitReport{}).cause, AppExitCause::Lost);
}

TEST_F(AppProcessManagerTest, StopSendsTermAndReportsStoppedByHub) {
    ASSERT_EQ(m_manager.launch(entryNamed("music")), LaunchResult::Started);
    const auto processId = m_launcher.lastProcessId();
    EXPECT_TRUE(m_manager.requestStop());
    EXPECT_EQ(m_manager.runState(), AppRunState::Stopping);
    EXPECT_FALSE(m_manager.requestStop());
    EXPECT_EQ(m_launcher.terminated(), std::vector<lexus_head_unit::ProcessId>{processId});
    m_manager.poll();
    EXPECT_EQ(m_manager.runState(), AppRunState::Idle);
    ASSERT_EQ(m_reports.size(), 1U);
    EXPECT_EQ(m_reports.front().cause, AppExitCause::StoppedByHub);
    EXPECT_EQ(describeExit(m_reports.front(), "Music"), "Music stopped");
    EXPECT_TRUE(m_launcher.killed().empty());
    EXPECT_FALSE(m_manager.requestStop());
}

TEST_F(AppProcessManagerTest, AppThatIgnoresTermIsKilledExactlyAtTheGracePeriod) {
    m_launcher.setExitOnTerminate(false);
    ASSERT_EQ(m_manager.launch(entryNamed("stubborn")), LaunchResult::Started);
    ASSERT_TRUE(m_manager.requestStop());
    m_clock.advanceMilliseconds(2999);
    m_manager.poll();
    EXPECT_TRUE(m_launcher.killed().empty());
    m_clock.advanceMilliseconds(1);
    m_manager.poll();
    EXPECT_EQ(m_launcher.killed().size(), 1U);
    EXPECT_EQ(m_manager.counters().kills, 1U);
    m_manager.poll();
    EXPECT_EQ(m_manager.runState(), AppRunState::Idle);
    ASSERT_EQ(m_reports.size(), 1U);
    EXPECT_EQ(m_reports.front().cause, AppExitCause::StoppedByHub);
    EXPECT_EQ(m_reports.front().code, SIGKILL);
}

TEST_F(AppProcessManagerTest, OnFailureRestartsThreeTimesInTheWindowThenGivesUp) {
    ASSERT_EQ(m_manager.launch(entryNamed("crashy", RestartPolicy::OnFailure)),
              LaunchResult::Started);
    for (int restart = 0; restart < 3; ++restart) {
        crashAndExpectRestart();
    }
    m_clock.advanceMilliseconds(1000);
    endWith(ProcessExitKind::Exited, 1);
    EXPECT_EQ(describeExit(lastReport(), "Crashy"), "Crashy failed (exit code 1)");
    EXPECT_EQ(m_manager.runState(), AppRunState::Idle);
    EXPECT_EQ(m_manager.counters().restarts, 3U);
    EXPECT_EQ(m_launcher.startedArguments().size(), 4U);
    EXPECT_EQ(describeExit(m_reports.front(), "Crashy"), "Crashy ended by signal 11, restarting");
}

TEST_F(AppProcessManagerTest, RestartBudgetRecoversAfterTheWindow) {
    const AppManagerSettings settings{3000, 10000, 1};
    AppProcessManager manager(m_launcher, m_clock, settings);
    const auto crash = [this, &manager]() {
        m_launcher.finish(m_launcher.lastProcessId(), ProcessExit{ProcessExitKind::Exited, 2});
        manager.poll();
    };
    ASSERT_EQ(manager.launch(entryNamed("crashy", RestartPolicy::OnFailure)),
              LaunchResult::Started);
    crash();
    EXPECT_EQ(manager.runState(), AppRunState::Running);
    m_clock.advanceMilliseconds(10000);
    crash();
    EXPECT_EQ(manager.runState(), AppRunState::Running);
    m_clock.advanceMilliseconds(9999);
    crash();
    EXPECT_EQ(manager.runState(), AppRunState::Idle);
    EXPECT_EQ(manager.counters().restarts, 2U);
}

TEST_F(AppProcessManagerTest, CleanExitsAndHubStopsAreNeverRestarted) {
    ASSERT_EQ(m_manager.launch(entryNamed("app", RestartPolicy::OnFailure)), LaunchResult::Started);
    endWith(ProcessExitKind::Exited, 0);
    EXPECT_EQ(m_manager.runState(), AppRunState::Idle);
    ASSERT_EQ(m_manager.launch(entryNamed("app", RestartPolicy::OnFailure)), LaunchResult::Started);
    ASSERT_TRUE(m_manager.requestStop());
    m_manager.poll();
    EXPECT_EQ(m_manager.runState(), AppRunState::Idle);
    EXPECT_EQ(m_manager.counters().restarts, 0U);
}

TEST_F(AppProcessManagerTest, ShutdownStopsTheForegroundAppAndKillsItIfItStays) {
    ASSERT_EQ(m_manager.launch(entryNamed("app")), LaunchResult::Started);
    m_manager.shutdown(50);
    EXPECT_EQ(m_manager.runState(), AppRunState::Idle);
    EXPECT_TRUE(m_launcher.killed().empty());

    m_launcher.setExitOnTerminate(false);
    ASSERT_EQ(m_manager.launch(entryNamed("stubborn")), LaunchResult::Started);
    m_manager.shutdown(30);
    EXPECT_EQ(m_manager.runState(), AppRunState::Idle);
    EXPECT_EQ(m_launcher.killed().size(), 1U);
    m_manager.shutdown(30);
}

TEST(AppProcessManagerNamesTest, StateResultAndCauseNames) {
    EXPECT_EQ(toString(AppRunState::Idle), "idle");
    EXPECT_EQ(toString(AppRunState::Running), "running");
    EXPECT_EQ(toString(AppRunState::Stopping), "stopping");
    EXPECT_EQ(toString(LaunchResult::Started), "started");
    EXPECT_EQ(toString(LaunchResult::AlreadyRunning), "already-running");
    EXPECT_EQ(toString(LaunchResult::StartFailed), "start-failed");
    EXPECT_EQ(toString(LaunchResult::EmptyCommand), "empty-command");
    EXPECT_EQ(toString(AppExitCause::CleanExit), "clean-exit");
    EXPECT_EQ(toString(AppExitCause::FailedExit), "failed-exit");
    EXPECT_EQ(toString(AppExitCause::Signalled), "signalled");
    EXPECT_EQ(toString(AppExitCause::StoppedByHub), "stopped-by-hub");
    EXPECT_EQ(toString(AppExitCause::StartFailed), "start-failed");
    EXPECT_EQ(toString(AppExitCause::Lost), "lost");
}

} // namespace
// NOLINTEND(misc-include-cleaner)
